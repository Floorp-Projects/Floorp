#include "global.h"

#include "BrowserFrame.h"

#include "nsIURI.h"

BEGIN_EVENT_TABLE(BrowserFrame, wxFrame)
    EVT_MENU(XRCID("browse_back"),   BrowserFrame::OnBrowserBack)
    EVT_MENU(XRCID("browse_fwd"),    BrowserFrame::OnBrowserForward)
    EVT_MENU(XRCID("browse_reload"), BrowserFrame::OnBrowserReload)
    EVT_MENU(XRCID("browse_stop"),   BrowserFrame::OnBrowserStop)
    EVT_MENU(XRCID("browse_home"),   BrowserFrame::OnBrowserHome)
    EVT_BUTTON(XRCID("browser_go"),  BrowserFrame::OnBrowserGo)
    EVT_TEXT_ENTER(XRCID("url"),     BrowserFrame::OnBrowserUrl)
END_EVENT_TABLE()

BrowserFrame::BrowserFrame(wxWindow* aParent) :
    mGeckoWnd(NULL)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("browser_frame"));

    SetIcon(wxICON(appicon));

    mGeckoWnd = (GeckoWindow *) FindWindowById(XRCID("gecko"), this);
    if (mGeckoWnd)
    {
        GeckoContainer *geckoContainer = new GeckoContainer(this);
        if (geckoContainer)
        {
            mGeckoWnd->SetGeckoContainer(geckoContainer);

            PRUint32 aChromeFlags = nsIWebBrowserChrome::CHROME_ALL;
            geckoContainer->SetChromeFlags(aChromeFlags);
            geckoContainer->SetParent(nsnull);

            wxSize size = mGeckoWnd->GetClientSize();

            // Insert the browser
            geckoContainer->CreateBrowser(0, 0, size.GetWidth(), size.GetHeight(),
                (nativeWindow) mGeckoWnd->GetHWND(), getter_AddRefs(mWebbrowser));

            GeckoContainerUI::ShowWindow(PR_TRUE);

        }
    }

    CreateStatusBar();
}


///////////////////////////////////////////////////////////////////////////////
// Browser specific handlers


void BrowserFrame::OnBrowserGo(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        wxTextCtrl *txtCtrl = (wxTextCtrl *) FindWindowById(XRCID("url"), this);
        wxString url = txtCtrl->GetValue();
        if (!url.IsEmpty())
        {
            nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
            webNav->LoadURI(NS_ConvertASCIItoUCS2(url.c_str()).get(),
                                   nsIWebNavigation::LOAD_FLAGS_NONE,
                                   nsnull,
                                   nsnull,
                                   nsnull);
        }
    }
}

void BrowserFrame::OnBrowserUrl(wxCommandEvent & event)
{
    OnBrowserGo(event);
}

void BrowserFrame::OnBrowserBack(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->GoBack();
    }
}

void BrowserFrame::OnBrowserForward(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->GoForward();
    }
}

void BrowserFrame::OnBrowserReload(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
    }
}

void BrowserFrame::OnBrowserStop(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->Stop(nsIWebNavigation::STOP_ALL);
    }
}

void BrowserFrame::OnBrowserHome(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->LoadURI(NS_ConvertASCIItoUCS2("www.cnn.com").get(),
                               nsIWebNavigation::LOAD_FLAGS_NONE,
                               nsnull,
                               nsnull,
                               nsnull);
    }
}


///////////////////////////////////////////////////////////////////////////////
// GeckoContainerUI overrides

void BrowserFrame::UpdateStatusBarText(const PRUnichar* aStatusText)
{
    SetStatusText(aStatusText);
}

void BrowserFrame::UpdateCurrentURI()
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        nsCOMPtr<nsIURI> currentURI;
        webNav->GetCurrentURI(getter_AddRefs(currentURI));
        nsCAutoString spec;
        currentURI->GetSpec(spec);

        wxTextCtrl *txtCtrl = (wxTextCtrl *) FindWindowById(XRCID("url"), this);
        if (txtCtrl)
        {
            txtCtrl->SetValue(spec.get());
        }
    }
}
