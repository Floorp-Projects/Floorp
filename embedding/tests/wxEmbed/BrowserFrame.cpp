/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "global.h"

#include "BrowserFrame.h"

#include "nsIURI.h"

BEGIN_EVENT_TABLE(BrowserFrame, wxFrame)
    EVT_MENU(XRCID("browse_back"),      BrowserFrame::OnBrowserBack)
    EVT_UPDATE_UI(XRCID("browse_back"), BrowserFrame::OnUpdateBrowserBack)
    EVT_MENU(XRCID("browse_fwd"),       BrowserFrame::OnBrowserForward)
    EVT_UPDATE_UI(XRCID("browse_fwd"),  BrowserFrame::OnUpdateBrowserForward)
    EVT_MENU(XRCID("browse_reload"),    BrowserFrame::OnBrowserReload)
    EVT_MENU(XRCID("browse_stop"),      BrowserFrame::OnBrowserStop)
    EVT_UPDATE_UI(XRCID("browse_stop"), BrowserFrame::OnUpdateBrowserStop)
    EVT_MENU(XRCID("browse_home"),      BrowserFrame::OnBrowserHome)
    EVT_BUTTON(XRCID("browser_go"),     BrowserFrame::OnBrowserGo)
    EVT_TEXT_ENTER(XRCID("url"),        BrowserFrame::OnBrowserUrl)
END_EVENT_TABLE()

BrowserFrame::BrowserFrame(wxWindow* aParent) :
    mGeckoWnd(NULL)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("browser_frame"));

    SetIcon(wxICON(appicon));

    mGeckoWnd = (GeckoWindow *) FindWindowById(XRCID("gecko"), this);
    if (mGeckoWnd)
    {
        // Note we pass in 'browser' as the role so that when the content
        // wants to open a popup the window creator can say okay but deny it
        // for other roles such as 'mail' or whatever.
        GeckoContainer *geckoContainer = new GeckoContainer(this, "browser");
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

void BrowserFrame::OnUpdateBrowserBack(wxUpdateUIEvent &event)
{
    PRBool canGoBack = PR_FALSE;
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->GetCanGoBack(&canGoBack);
    }
    event.Enable(canGoBack);
}

void BrowserFrame::OnBrowserForward(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->GoForward();
    }
}

void BrowserFrame::OnUpdateBrowserForward(wxUpdateUIEvent &event)
{
    PRBool canGoForward = PR_FALSE;
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->GetCanGoForward(&canGoForward);
    }
    event.Enable(canGoForward);
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

void BrowserFrame::OnUpdateBrowserStop(wxUpdateUIEvent &event)
{
    event.Enable(mBusy);
}

void BrowserFrame::OnBrowserHome(wxCommandEvent & WXUNUSED(event))
{
    if (mWebbrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
        webNav->LoadURI(NS_ConvertASCIItoUCS2("http://www.mozilla.org/projects/embedding/").get(),
                               nsIWebNavigation::LOAD_FLAGS_NONE,
                               nsnull,
                               nsnull,
                               nsnull);
    }
}


///////////////////////////////////////////////////////////////////////////////
// GeckoContainerUI overrides

nsresult BrowserFrame::CreateBrowserWindow(PRUint32 aChromeFlags,
         nsIWebBrowserChrome *aParent, nsIWebBrowserChrome **aNewWindow)
{
    // Create the main frame window
    BrowserFrame* frame = new BrowserFrame(NULL);
    if (!frame)
        return NS_ERROR_OUT_OF_MEMORY;
    frame->Show(TRUE);
    GeckoContainer *container = frame->mGeckoWnd->GetGeckoContainer();
    return container->QueryInterface(NS_GET_IID(nsIWebBrowserChrome), (void **) aNewWindow);
}

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
