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
 *   Adam Lock <adamlock@netscape.com>
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

#include "GeckoFrame.h"
#include "GeckoContainer.h"

#include "nsIWebBrowserFocus.h"
#include "nsIClipboardCommands.h"

GeckoFrame::GeckoFrame() :
    mGeckoWnd(NULL)
{
}

BEGIN_EVENT_TABLE(GeckoFrame, wxFrame)
    EVT_ACTIVATE(GeckoFrame::OnActivate) 
    // Clipboard functions
    EVT_MENU(XRCID("edit_cut"),         GeckoFrame::OnEditCut)
    EVT_UPDATE_UI(XRCID("edit_cut"),    GeckoFrame::OnUpdateEditCut)
    EVT_MENU(XRCID("edit_copy"),        GeckoFrame::OnEditCopy)
    EVT_UPDATE_UI(XRCID("edit_copy"),   GeckoFrame::OnUpdateEditCopy)
    EVT_MENU(XRCID("edit_paste"),       GeckoFrame::OnEditPaste) 
    EVT_UPDATE_UI(XRCID("edit_paste"),  GeckoFrame::OnUpdateEditPaste)
    EVT_MENU(XRCID("edit_selectall"),   GeckoFrame::OnEditSelectAll)
END_EVENT_TABLE()


bool GeckoFrame::SetupDefaultGeckoWindow()
{
    mGeckoWnd  = (GeckoWindow *) FindWindowById(XRCID("gecko"), this);
    if (!mGeckoWnd)
        return FALSE;
    return SetupGeckoWindow(mGeckoWnd, this, getter_AddRefs(mWebBrowser));
}

bool GeckoFrame::SetupGeckoWindow(GeckoWindow *aGeckoWindow, GeckoContainerUI *aUI, nsIWebBrowser **aWebBrowser) const
{
    if (!aGeckoWindow || !aUI)
        return FALSE;

    GeckoContainer *geckoContainer = new GeckoContainer(aUI);
    if (!geckoContainer)
        return FALSE;

    mGeckoWnd->SetGeckoContainer(geckoContainer);

    PRUint32 aChromeFlags = nsIWebBrowserChrome::CHROME_ALL;
    geckoContainer->SetChromeFlags(aChromeFlags);
    geckoContainer->SetParent(nsnull);
    wxSize size = mGeckoWnd->GetClientSize();

    // Insert the browser
    geckoContainer->CreateBrowser(0, 0, size.GetWidth(), size.GetHeight(),
        (nativeWindow) aGeckoWindow->GetHWND(), aWebBrowser);

    nsCOMPtr<nsIBaseWindow> webBrowserAsWin = do_QueryInterface(*aWebBrowser);
    if (webBrowserAsWin)
    {
        webBrowserAsWin->SetVisibility(PR_TRUE);
    }

    return TRUE;
}

void GeckoFrame::OnActivate(wxActivateEvent &event)
{
    nsCOMPtr<nsIWebBrowserFocus> focus(do_GetInterface(mWebBrowser));
    if (focus)
    {
        if (event.GetActive())
            focus->Activate();
        else
            focus->Deactivate();
    }
    wxFrame::OnActivate(event);
}

void GeckoFrame::OnEditCut(wxCommandEvent &event)
{
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->CutSelection();
}

void GeckoFrame::OnUpdateEditCut(wxUpdateUIEvent &event)
{
    PRBool canCut = PR_FALSE;
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->CanCutSelection(&canCut);
    event.Enable(canCut ? true : false);
}

void GeckoFrame::OnEditCopy(wxCommandEvent &event)
{
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->CopySelection();
}

void GeckoFrame::OnUpdateEditCopy(wxUpdateUIEvent &event)
{
    PRBool canCopy = PR_FALSE;
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->CanCopySelection(&canCopy);
    event.Enable(canCopy ? true : false);
}

void GeckoFrame::OnEditPaste(wxCommandEvent &event)
{
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->Paste();
}

void GeckoFrame::OnUpdateEditPaste(wxUpdateUIEvent &event)
{
    PRBool canPaste = PR_FALSE;
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->CanPaste(&canPaste);
    event.Enable(canPaste ? true : false);
}

void GeckoFrame::OnEditSelectAll(wxCommandEvent &event)
{
    nsCOMPtr<nsIClipboardCommands> clipCmds = do_GetInterface(mWebBrowser);
    if(clipCmds)
        clipCmds->SelectAll();
}

///////////////////////////////////////////////////////////////////////////////
// GeckoContainerUI overrides

void GeckoFrame::SetFocus()
{
    mGeckoWnd->SetFocus();
}

