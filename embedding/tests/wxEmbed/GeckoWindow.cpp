#include "global.h"

#include "GeckoWindow.h"

IMPLEMENT_DYNAMIC_CLASS(GeckoWindow, wxPanel)

GeckoWindow::GeckoWindow(void) :
    mGeckoContainer(NULL)
{
}

GeckoWindow::~GeckoWindow()
{
    SetGeckoContainer(NULL);
}

void GeckoWindow::SetGeckoContainer(GeckoContainer *aGeckoContainer)
{
    if (aGeckoContainer != mGeckoContainer)
    {
        NS_IF_RELEASE(mGeckoContainer);
        mGeckoContainer = aGeckoContainer;
        NS_IF_ADDREF(mGeckoContainer);
    }
}

BEGIN_EVENT_TABLE(GeckoWindow, wxPanel)
    EVT_SIZE(GeckoWindow::OnSize)
END_EVENT_TABLE()

void GeckoWindow::OnSize(wxSizeEvent &event)
{
    if (!mGeckoContainer)
    {
        return;
    }
    // Make sure the browser is visible and sized 
    nsCOMPtr<nsIWebBrowser> webBrowser;
    mGeckoContainer->GetWebBrowser(getter_AddRefs(webBrowser));
    nsCOMPtr<nsIBaseWindow> webBrowserAsWin = do_QueryInterface(webBrowser);
    if (webBrowserAsWin)
    {
        wxSize size = GetClientSize();
        webBrowserAsWin->SetPositionAndSize(
            0, 0, size.GetWidth(), size.GetHeight(), PR_TRUE);
        webBrowserAsWin->SetVisibility(PR_TRUE);
    }
}
