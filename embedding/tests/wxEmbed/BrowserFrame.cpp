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

BEGIN_EVENT_TABLE(BrowserFrame, GeckoFrame)
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

BrowserFrame::BrowserFrame(wxWindow* aParent)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("browser_frame"));

    SetIcon(wxICON(appicon));

    SetupDefaultGeckoWindow();

    CreateStatusBar();
}


///////////////////////////////////////////////////////////////////////////////
// Browser specific handlers


void BrowserFrame::OnBrowserGo(wxCommandEvent & WXUNUSED(event))
{
    if (mWebBrowser)
    {
        wxTextCtrl *txtCtrl = (wxTextCtrl *) FindWindowById(XRCID("url"), this);
        wxString url = txtCtrl->GetValue();
        if (!url.IsEmpty())
        {
            nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
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
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        webNav->GoBack();
    }
}

void BrowserFrame::OnUpdateBrowserBack(wxUpdateUIEvent &event)
{
    PRBool canGoBack = PR_FALSE;
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        webNav->GetCanGoBack(&canGoBack);
    }
    event.Enable(canGoBack);
}

void BrowserFrame::OnBrowserForward(wxCommandEvent & WXUNUSED(event))
{
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        webNav->GoForward();
    }
}

void BrowserFrame::OnUpdateBrowserForward(wxUpdateUIEvent &event)
{
    PRBool canGoForward = PR_FALSE;
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        webNav->GetCanGoForward(&canGoForward);
    }
    event.Enable(canGoForward);
}

void BrowserFrame::OnBrowserReload(wxCommandEvent & WXUNUSED(event))
{
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        webNav->Reload(nsIWebNavigation::LOAD_FLAGS_NONE);
    }
}

void BrowserFrame::OnBrowserStop(wxCommandEvent & WXUNUSED(event))
{
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        webNav->Stop(nsIWebNavigation::STOP_ALL);
    }
}

void BrowserFrame::OnUpdateBrowserStop(wxUpdateUIEvent &event)
{
    event.Enable(mBusy);
}

void BrowserFrame::OnBrowserHome(wxCommandEvent & WXUNUSED(event))
{
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
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
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
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

#include "nsIDOMMouseEvent.h"

void BrowserFrame::ShowContextMenu(PRUint32 aContextFlags, nsIContextMenuInfo *aContextMenuInfo)
{
    nsCOMPtr<nsIDOMEvent> event;
    aContextMenuInfo->GetMouseEvent(getter_AddRefs(event));
    if (!event)
    {
        return;
    }
    nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(event);
    if (mouseEvent)
    {
        PRInt32 x, y;
        mouseEvent->GetClientX(&x);
        mouseEvent->GetClientY(&y);

        char *menuName = NULL;

        // CONTEXT_LINK                   <A>
        // CONTEXT_IMAGE                  <IMG>
        // CONTEXT_IMAGE | CONTEXT_LINK   <IMG> with <A> as an ancestor
        // CONTEXT_INPUT                  <INPUT>
        // CONTEXT_INPUT | CONTEXT_IMAGE  <INPUT> with type=image
        // CONTEXT_TEXT                   <TEXTAREA>
        // CONTEXT_DOCUMENT               <HTML>
        // CONTEXT_BACKGROUND_IMAGE       <HTML> with background image
        
        if (aContextFlags & nsIContextMenuListener2::CONTEXT_IMAGE)
        {
            if (aContextFlags & nsIContextMenuListener2::CONTEXT_LINK)
                menuName = "context_browser_image"; // TODO
            else if (aContextFlags & nsIContextMenuListener2::CONTEXT_INPUT)
                menuName = "context_browser_image"; // TODO
            else
                menuName = "context_browser_image"; // TODO
        }
        else if (aContextFlags & nsIContextMenuListener2::CONTEXT_LINK)
        {
            menuName = "context_browser_link";
        }
        else if (aContextFlags & nsIContextMenuListener2::CONTEXT_INPUT)
        {
            menuName = "context_browser_input";
        }
        else if (aContextFlags & nsIContextMenuListener2::CONTEXT_TEXT)
        {
            menuName = "context_browser_text";
        }
        else if (aContextFlags & nsIContextMenuListener2::CONTEXT_DOCUMENT)
        {
            menuName = "context_browser_document";
        }
        else if (aContextFlags & nsIContextMenuListener2::CONTEXT_BACKGROUND_IMAGE)
        {
            menuName = "context_browser_document";
        }

        if (!menuName)
            return;

// Hack for Win32 that has a #define for LoadMenu
#undef LoadMenu
        wxMenu *menu = wxXmlResource::Get()->LoadMenu(menuName);
        if (menu)
            PopupMenu(menu, x, y);
    }
}


