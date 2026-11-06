#include "WebMediaController.hpp"
#include "IPrinterAgent.hpp"

#include <wx/gdicmn.h>
#include <wx/uri.h>
#include <wx/webview.h>

namespace Slic3r { namespace GUI {

namespace {

void initialize_webview(wxWebView* webview)
{
    if (!webview)
        return;

    webview->SetBackgroundColour(*wxBLACK);
    webview->SetPage("<html><head><style>html,body{margin:0;height:100%;background:#000;}</style></head><body></body></html>", "");
}

// The printer's page sizes its <video> itself and leaves the background transparent, which shows white bands
// around it. Adds a stylesheet once per document, so a page that loads again gets it again.
const char* const fit_camera_page_script =
    "(function(){"
    "if(document.getElementById('orca-camera-fit'))return;"
    "var style=document.createElement('style');"
    "style.id='orca-camera-fit';"
    "style.textContent='html,body{margin:0!important;padding:0!important;width:100%!important;height:100%!important;"
    "overflow:hidden!important;background:#000!important;}"
    "video{position:fixed!important;top:0!important;left:0!important;width:100%!important;height:100%!important;"
    "max-width:none!important;max-height:none!important;object-fit:contain!important;background:#000!important;}';"
    "(document.head||document.documentElement).appendChild(style);"
    "})();";

} // namespace

WebMediaController::WebMediaController(wxWebView* webview)
    : m_webview(webview)
{
    initialize_webview(m_webview);
    // why: no Unbind - StatusBasePanel destroys the webview before this controller, so no event outlives it
    if (m_webview)
        m_webview->Bind(wxEVT_WEBVIEW_LOADED, [this](wxWebViewEvent& evt) {
            evt.Skip();
            on_page_loaded(evt.GetURL());
        });
}

void WebMediaController::on_page_loaded(const wxString& url)
{
    // Only the camera page: not a snapshot or stream document, nor the blank page Stop() leaves
    if (m_stream_mode != CameraStreamMode::web_page || m_url.empty() || url.StartsWith("about:"))
        return;
    m_webview->RunScript(fit_camera_page_script);
}

void WebMediaController::Load(wxURI url)
{
    m_url = url.BuildURI().ToStdString();
}

void WebMediaController::set_mode(CameraStreamMode mode)
{
    m_stream_mode = mode;
}

void WebMediaController::Play()
{
    if (!m_webview)
        return;

    wxString url  = wxString::FromUTF8(m_url);
    wxString html = "<html><head><style>"
                    "html,body{margin:0;height:100%;background:#000;overflow:hidden;}"
                    "img{width:100%;height:100%;object-fit:contain;display:block;}"
                    "</style></head><body><img id=\"camera-frame\"";
    if (m_stream_mode == CameraStreamMode::http_snapshot) {
        html += " data-camera-url=\"" + url +
                "\"><script>"
                "const cameraFrame=document.getElementById('camera-frame');"
                "const cameraUrl=cameraFrame.dataset.cameraUrl;"
                "let cameraFrameLoading=false;"
                "function refreshCameraFrame(){"
                "if(cameraFrameLoading)return;"
                "cameraFrameLoading=true;"
                "const nextFrame=new Image();"
                "nextFrame.onload=function(){cameraFrame.src=nextFrame.src;cameraFrameLoading=false;};"
                "nextFrame.onerror=function(){cameraFrameLoading=false;};"
                "nextFrame.src=cameraUrl+(cameraUrl.indexOf('?')>=0?'&':'?')+'_orca_frame='+Date.now();"
                "}"
                "let cameraRefreshInterval=null;"
                "function stopCameraRefresh(){"
                "if(cameraRefreshInterval!==null){"
                "clearInterval(cameraRefreshInterval);"
                "cameraRefreshInterval=null;"
                "}"
                "}"
                "refreshCameraFrame();"
                "cameraRefreshInterval = setInterval(refreshCameraFrame,200);"
                "</script></body></html>";
        m_webview->SetPage(html, url);
    } else {
        // Load MJPEG streams as the top-level document.  Some embedded WebView
        // backends buffer a multipart stream when it is used as an <img> resource,
        // which introduces noticeable live-view latency.
        // A web_page camera is loaded the same way, so the printer's page runs its own player.
        m_webview->LoadURL(url);
    }
}

void WebMediaController::Stop()
{
    if (m_webview) {
        m_webview->RunScript("if(typeof stopCameraRefresh==='function') stopCameraRefresh();");
        m_webview->Stop();
        m_webview->SetPage("", "about:blank");
        m_webview->ClearHistory();
    }
    m_url.clear();
}

}} // namespace Slic3r::GUI
