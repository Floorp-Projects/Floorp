/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "global.h"

#include "wx/strconv.h"

#include "BrowserFrame.h"

#include "nsIURI.h"

BEGIN_EVENT_TABLE(BrowserFrame, GeckoFrame)

    // View menu
    EVT_MENU(XRCID("view_pagesource"),  BrowserFrame::OnViewPageSource)
    EVT_UPDATE_UI(XRCID("view_pagesource"),
                                        BrowserFrame::OnUpdateViewPageSource)

    // Browser operations, back / forward etc.
    // TODO some of these can go in GeckoFrame
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
    EVT_MENU(XRCID("browser_open_in_new_window"),
                                        BrowserFrame::OnBrowserOpenLinkInNewWindow)
END_EVENT_TABLE()

BrowserFrame::BrowserFrame(wxWindow* aParent)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("browser_frame"));

    SetIcon(wxICON(appicon));

    SetName("browser");

    SetupDefaultGeckoWindow();

    CreateStatusBar();
}


nsresult BrowserFrame::LoadURI(const wchar_t *aURI)
{
    if (mWebBrowser)
    {
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
        if (webNav)
        {
            return webNav->LoadURI(aURI,
                    nsIWebNavigation::LOAD_FLAGS_NONE,
                    nsnull,
                    nsnull,
                    nsnull);
        }
    }
    return NS_ERROR_FAILURE;
}


nsresult BrowserFrame::LoadURI(const char *aURI)
{
    wxMBConv conv;
    return LoadURI(conv.cWX2WC(aURI));
}


///////////////////////////////////////////////////////////////////////////////
// Browser specific handlers


void BrowserFrame::OnFileSave(wxCommandEvent & WXUNUSED(event))
{
}

void BrowserFrame::OnFilePrint(wxCommandEvent & WXUNUSED(event))
{
}

void BrowserFrame::OnViewPageSource(wxCommandEvent &event)
{
    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
    if(!webNav)
        return;

    // Get the URI object whose source we want to view.
    nsresult rv = NS_OK;
    nsCOMPtr<nsIURI> currentURI;
    rv = webNav->GetCurrentURI(getter_AddRefs(currentURI));
    if(NS_FAILED(rv) || !currentURI)
        return;

    // Get the uri string associated with the nsIURI object
    nsCAutoString uriString;
    rv = currentURI->GetSpec(uriString);
    if(NS_FAILED(rv))
        return;

    // Build the view-source: url
    nsAutoString viewSrcUrl(L"view-source:");
    viewSrcUrl.AppendWithConversion(uriString.get());

    BrowserFrame *frame = new BrowserFrame(NULL);
    if (frame)
    {
        frame->Show(TRUE);
        frame->LoadURI(viewSrcUrl.get());
    }

}

void BrowserFrame::OnUpdateViewPageSource(wxUpdateUIEvent &event)
{
}

void BrowserFrame::OnBrowserGo(wxCommandEvent & WXUNUSED(event))
{
    wxTextCtrl *txtCtrl = (wxTextCtrl *) FindWindowById(XRCID("url"), this);
    wxString url = txtCtrl->GetValue();
    if (!url.IsEmpty())
    {
        LoadURI(url);
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
    event.Enable(canGoBack ? true : false);
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
    event.Enable(canGoForward ? true : false);
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
    event.Enable(mBusy ? true : false);
}

void BrowserFrame::OnBrowserHome(wxCommandEvent & WXUNUSED(event))
{
    LoadURI("http://www.mozilla.org/projects/embedding/");
}

void BrowserFrame::OnBrowserOpenLinkInNewWindow(wxCommandEvent & WXUNUSED(event))
{
    BrowserFrame* frame = new BrowserFrame(NULL);
    frame->Show(TRUE);
    frame->LoadURI(mContextLinkUrl.get());
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

    mContextLinkUrl.SetLength(0);

    nsCOMPtr<nsIDOMMouseEvent> mouseEvent = do_QueryInterface(event);
    if (mouseEvent)
    {
        PRInt32 x, y;
        mouseEvent->GetScreenX(&x);
        mouseEvent->GetScreenY(&y);

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

            aContextMenuInfo->GetAssociatedLink(mContextLinkUrl);
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
        {
            int fX = 0, fY = 0;
            // Make screen coords relative to the frame window for accurate
            // popup menu position
            GetPosition(&fX, &fY);
            PopupMenu(menu, x - fX, y - fY);
        }
    }
}


