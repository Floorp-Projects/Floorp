/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Judge <mjudge@netscape.com>
 *   Chak Nanga <chak@netscape.com> 
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "stdafx.h"
#include "MfcEmbed.h"
#include "BrowserFrm.h"
#include "EditorFrm.h"

IMPLEMENT_DYNAMIC(CEditorFrame, CBrowserFrame)

BEGIN_MESSAGE_MAP(CEditorFrame, CBrowserFrame)
	//{{AFX_MSG_MAP(CEditorFrame)
    ON_COMMAND(ID_BOLD, OnBold)
    ON_UPDATE_COMMAND_UI(ID_BOLD, OnUpdateBold)
    ON_COMMAND(ID_ITALICS, OnItalics)
    ON_UPDATE_COMMAND_UI(ID_ITALICS, OnUpdateItalics)
    ON_COMMAND(ID_UNDERLINE, OnUnderline)
    ON_UPDATE_COMMAND_UI(ID_UNDERLINE, OnUpdateUnderline)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CEditorFrame::CEditorFrame(PRUint32 chromeMask)
{
    m_chromeMask = chromeMask;

    NS_ADDREF(&mToolBarObserver);//make sure no one releases this
}

CEditorFrame::~CEditorFrame()
{
}

BOOL CEditorFrame::InitEditor()
{
    mToolBarObserver.SetFrame(this,ID_TOOLBAR_UPDATE,100); //update if 100 ticks goes by and no more changes

    AddEditorObservers();

    MakeEditable();
 
    return TRUE;
}

/*
"bold"
state_all   //setter and getter
state_begin //getter
state_end   //getter
state_mixed //getter
*/
#define COMMAND_NAME NS_ConvertASCIItoUCS2("cmd_name")
#define STATE_ALL NS_ConvertASCIItoUCS2("state_all")

#define BOLD_COMMAND NS_ConvertASCIItoUCS2("cmd_bold")

void CEditorFrame::OnBold() 
{
    nsresult rv;
    nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID, &rv);
	if (NS_FAILED(rv) || !params)
        return;

    params->SetBooleanValue(STATE_ALL, true);
    params->SetStringValue(COMMAND_NAME, BOLD_COMMAND);

    DoCommand(params);
}

void CEditorFrame::OnUpdateBold(CCmdUI* pCmdUI) 
{
    nsresult rv;
    nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
    params->SetStringValue(COMMAND_NAME,BOLD_COMMAND);

    rv = GetCommandState(params);
    if (NS_SUCCEEDED(rv))
    {
      //set tri state of button here if we need to
    }

    //just return true for now
    pCmdUI->Enable();
}

void CEditorFrame::OnItalics() 
{
	// TODO: Add your command handler code here
	
}

void CEditorFrame::OnUpdateItalics(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CEditorFrame::OnUnderline() 
{
	// TODO: Add your command handler code here
	
}

void CEditorFrame::OnUpdateUnderline(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

NS_METHOD
CEditorFrame::AddEditorObservers()
{
    nsCOMPtr<nsICommandManager> commandManager;
    nsresult rv;
    commandManager = do_GetInterface(m_wndBrowserView.mWebBrowser,&rv);
    if (commandManager)
    {
        nsAutoString tString(NS_LITERAL_STRING("cmd_bold"));
        rv = commandManager->AddCommandObserver(&mToolBarObserver,tString);

        tString = NS_LITERAL_STRING("cmd_italic");
        rv = commandManager->AddCommandObserver(&mToolBarObserver,tString);

        tString = NS_LITERAL_STRING("cmd_underline");
        rv = commandManager->AddCommandObserver(&mToolBarObserver,tString);
    }

    return rv;
}

NS_METHOD
CEditorFrame::MakeEditable()
{
    nsresult rv;
    nsCOMPtr<nsIDOMWindow> domWindow;
    m_wndBrowserView.mWebBrowser->GetContentDOMWindow(getter_AddRefs(domWindow));
    if (!domWindow)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject = do_QueryInterface(domWindow);
    if (!scriptGlobalObject)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDocShell> docShell;
    rv  = scriptGlobalObject->GetDocShell(getter_AddRefs(docShell));
    if (NS_FAILED(rv))
        return rv;
    if (!docShell)
        return NS_ERROR_FAILURE;
  
    nsCOMPtr<nsIEditingSession> editingSession = do_GetInterface(docShell);
    if (!editingSession)
        return NS_ERROR_FAILURE;
  
    rv= editingSession->MakeWindowEditable(domWindow, PR_TRUE);
    // this can fail for the root (if it's a frameset), but we still want
    // to make children editable
  
    nsCOMPtr<nsISimpleEnumerator> docShellEnumerator;
    docShell->GetDocShellEnumerator( nsIDocShellTreeItem::typeContent,
                                    nsIDocShell::ENUMERATE_FORWARDS,
                                    getter_AddRefs(docShellEnumerator));  
    if (docShellEnumerator)
    {
        PRBool hasMore;
        while (NS_SUCCEEDED(docShellEnumerator->HasMoreElements(&hasMore)) && hasMore)
        {
            nsCOMPtr<nsISupports> curSupports;
            rv = docShellEnumerator->GetNext(getter_AddRefs(curSupports));
            if (NS_FAILED(rv)) break;

            nsCOMPtr<nsIDocShell> curShell = do_QueryInterface(curSupports, &rv);
            if (NS_FAILED(rv)) break;

            nsCOMPtr<nsIDOMWindow> childWindow = do_GetInterface(curShell,&rv);
            if (childWindow)
                editingSession->MakeWindowEditable(childWindow, PR_FALSE);
        }
    }

    return NS_OK;  
}

NS_METHOD
CEditorFrame::DoCommand(nsICommandParams *aCommandParams)
{
    nsCOMPtr<nsICommandManager> commandManager;
    nsresult rv = NS_ERROR_FAILURE;
    commandManager = do_GetInterface(m_wndBrowserView.mWebBrowser,&rv);
    if (commandManager)
    {
        rv = commandManager->DoCommand(aCommandParams);
    }

    return rv;
}

NS_METHOD
CEditorFrame::IsCommandEnabled(const nsAString &aCommand, PRBool *retval)
{
    nsCOMPtr<nsICommandManager> commandManager;
    nsresult rv = NS_ERROR_FAILURE;
    commandManager = do_GetInterface(m_wndBrowserView.mWebBrowser,&rv);
    if (commandManager)
    {
        rv = commandManager->IsCommandEnabled(aCommand,retval);
    }

    return rv;
}


NS_METHOD
CEditorFrame::GetCommandState(nsICommandParams *aCommandParams)
{
    nsCOMPtr<nsICommandManager> commandManager;
    nsresult rv = NS_ERROR_FAILURE;
    commandManager = do_GetInterface(m_wndBrowserView.mWebBrowser,&rv);
    if (commandManager)
    {
        rv = commandManager->GetCommandState(aCommandParams);
    }

    return rv;
}
