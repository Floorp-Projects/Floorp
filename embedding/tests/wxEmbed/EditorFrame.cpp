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

#include "nsIWebNavigation.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMWindow.h"
#include "nsIEditingSession.h"

#include "EditorFrame.h"

BEGIN_EVENT_TABLE(EditorFrame, GeckoFrame)
    EVT_MENU(XRCID("edit_bold"),           EditorFrame::OnEditBold)
    EVT_MENU(XRCID("edit_italic"),         EditorFrame::OnEditItalic)
    EVT_MENU(XRCID("edit_underline"),      EditorFrame::OnEditUnderline)
    EVT_UPDATE_UI(XRCID("edit_bold"),      EditorFrame::OnUpdateToggleCmd)
    EVT_UPDATE_UI(XRCID("edit_italic"),    EditorFrame::OnUpdateToggleCmd)
    EVT_UPDATE_UI(XRCID("edit_underline"), EditorFrame::OnUpdateToggleCmd)
    EVT_MENU(XRCID("edit_indent"),         EditorFrame::OnEditIndent)
    EVT_MENU(XRCID("edit_outdent"),        EditorFrame::OnEditOutdent)
END_EVENT_TABLE()

EditorFrame::EditorFrame(wxWindow* aParent)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("editor_frame"));

    SetName("editor");

    SetIcon(wxICON(appicon));

    SetupDefaultGeckoWindow();

    SendSizeEvent(); // 

    
    MakeEditable();

    mCommandManager = do_GetInterface(mWebBrowser);

    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
    webNav->LoadURI(NS_ConvertASCIItoUCS2("www.mozilla.org").get(),
        nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);
}



void EditorFrame::MakeEditable()
{
    nsCOMPtr<nsIDOMWindow> domWindow;
    mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));

    nsCOMPtr<nsIEditingSession> editingSession = do_GetInterface(mWebBrowser);
    if (!editingSession)
        return;// NS_ERROR_FAILURE;
    editingSession->MakeWindowEditable(domWindow, NULL, PR_TRUE);
}

void EditorFrame::DoCommand(const char *aCommand, nsICommandParams *aCommandParams)
{
    if (mCommandManager)
    {
        nsCOMPtr<nsIDOMWindow> domWindow;
        mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
        mCommandManager->DoCommand(aCommand, aCommandParams, domWindow);
    }
}

void EditorFrame::IsCommandEnabled(const char *aCommand, PRBool *retval)
{
    if (mCommandManager)
    {
        nsCOMPtr<nsIDOMWindow> domWindow;
        mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
        mCommandManager->IsCommandEnabled(aCommand, domWindow, retval);
    }
}


void EditorFrame::GetCommandState(const char *aCommand, nsICommandParams *aCommandParams)
{
    if (mCommandManager)
    {
        nsCOMPtr<nsIDOMWindow> domWindow;
        mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
        mCommandManager->GetCommandState(aCommand, domWindow, aCommandParams);
    }
}


static
nsresult MakeCommandParams(nsICommandParams **aParams)
{
    nsresult rv;
    nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
    if (NS_FAILED(rv))
        return rv;
    if (!params)
        return NS_ERROR_FAILURE;
    *aParams = params;
    NS_ADDREF(*aParams);
    return rv;
}

const char kCmdBold[]      = "cmd_bold";
const char kCmdItalic[]    = "cmd_italic";
const char kCmdUnderline[] = "cmd_underline";
const char kCmdIndent[]    = "cmd_indent";
const char kCmdOutdent[]   = "cmd_outdent";


void EditorFrame::OnEditBold(wxCommandEvent &event)
{
    // command manager cmd_bold
    DoCommand(kCmdBold, nsnull);
}

void EditorFrame::OnEditItalic(wxCommandEvent &event)
{
    // command manager cmd_bold
    DoCommand(kCmdItalic, nsnull);
}

void EditorFrame::OnEditUnderline(wxCommandEvent &event)
{
    // command manager cmd_bold
    DoCommand(kCmdUnderline, nsnull);
}

void EditorFrame::OnEditIndent(wxCommandEvent &event)
{
    // command manager cmd_bold
    DoCommand(kCmdIndent, nsnull);
}

void EditorFrame::OnEditOutdent(wxCommandEvent &event)
{
    // command manager cmd_bold
    DoCommand(kCmdOutdent, nsnull);
}


#define STATE_ALL           "state_all"
#define STATE_MIXED         "state_mixed"
#define STATE_ATTRIBUTE     "state_attribute"
#define STATE_ENABLED       "state_enabled"

void EditorFrame::OnUpdateToggleCmd(wxUpdateUIEvent &event)
{
    const char *cmd = NULL;
    if (event.GetId() == XRCID("edit_bold"))
    {
        cmd = kCmdBold;
    }
    else if (event.GetId() == XRCID("edit_italic"))
    {
        cmd = kCmdItalic;
    }
    else if (event.GetId() == XRCID("edit_underline"))
    {
        cmd = kCmdUnderline;
    }

    // command manager GetCommandState cmd_bold
    nsCOMPtr<nsICommandParams> params;
    if (NS_SUCCEEDED(MakeCommandParams(getter_AddRefs(params))))
    {
        GetCommandState(cmd, params);

        // Does our current selection span mixed styles?
        // If so, set the toolbar button to an indeterminate
        // state
        //
        PRBool bMixedStyle = PR_FALSE;
        params->GetBooleanValue(STATE_MIXED, &bMixedStyle);

        // We're not in STATE_MIXED. Enable/Disable the
        // toolbar button based on it's current state
        //
        PRBool bCmdEnabled = PR_FALSE;
        params->GetBooleanValue(STATE_ALL, &bCmdEnabled);

        event.Check((bMixedStyle || bCmdEnabled) ? TRUE : FALSE);
    }
}
