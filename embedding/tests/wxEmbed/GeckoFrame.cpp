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
 *   Adam Lock <adamlock@netscape.com>
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

