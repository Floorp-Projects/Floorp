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

//------------------------------------------------------------
//              Editor Command/Parameter Names
//------------------------------------------------------------
#define BOLD_COMMAND            NS_LITERAL_STRING("cmd_bold")
#define ITALIC_COMMAND          NS_LITERAL_STRING("cmd_italic")
#define UNDERLINE_COMMAND       NS_LITERAL_STRING("cmd_underline")
#define INDENT_COMMAND          NS_LITERAL_STRING("cmd_indent")
#define OUTDENT_COMMAND         NS_LITERAL_STRING("cmd_outdent")
#define FONTCOLOR_COMMAND       NS_LITERAL_STRING("cmd_fontColor")
#define BACKGROUNDCOLOR_COMMAND NS_LITERAL_STRING("cmd_backgroundColor")
#define COMMAND_NAME            NS_LITERAL_STRING("cmd_name")
#define INCREASEFONT_COMMAND    NS_LITERAL_STRING("cmd_increaseFont")
#define DECREASEFONT_COMMAND    NS_LITERAL_STRING("cmd_decreaseFont")
#define FONTFACE_COMMAND        NS_LITERAL_STRING("cmd_fontFace")
#define ALIGN_COMMAND           NS_LITERAL_STRING("cmd_align")
#define UNDO_COMMAND            NS_LITERAL_STRING("cmd_undo")
#define REDO_COMMAND            NS_LITERAL_STRING("cmd_redo")


//states
#define STATE_ALL           NS_LITERAL_STRING("state_all")
#define STATE_MIXED         NS_LITERAL_STRING("state_mixed")
#define STATE_ATTRIBUTE     NS_LITERAL_STRING("state_attribute")
#define STATE_ENABLED       NS_LITERAL_STRING("state_enabled")

//colors
#define COLOR_RED       NS_LITERAL_STRING("#FF0000")
#define COLOR_BLACK     NS_LITERAL_STRING("#000000")

//fonts
#define FONT_ARIAL			NS_LITERAL_STRING("Helvetica, Arial, sans-serif")
#define FONT_TIMES			NS_LITERAL_STRING("Times New Roman, Times, serif")
#define FONT_COURIER		NS_LITERAL_STRING("Courier New, Courier, monospace")

//align
#define ALIGN_LEFT      NS_LITERAL_STRING("left")
#define ALIGN_RIGHT     NS_LITERAL_STRING("right")
#define ALIGN_CENTER    NS_LITERAL_STRING("center")


//value
#define STATE_EMPTY		NS_LITERAL_STRING("")

IMPLEMENT_DYNAMIC(CEditorFrame, CBrowserFrame)

BEGIN_MESSAGE_MAP(CEditorFrame, CBrowserFrame)
	//{{AFX_MSG_MAP(CEditorFrame)
    ON_COMMAND(ID_BOLD, OnBold)
    ON_UPDATE_COMMAND_UI(ID_BOLD, OnUpdateBold)
    ON_COMMAND(ID_ITALICS, OnItalics)
    ON_UPDATE_COMMAND_UI(ID_ITALICS, OnUpdateItalics)
    ON_COMMAND(ID_UNDERLINE, OnUnderline)
    ON_UPDATE_COMMAND_UI(ID_UNDERLINE, OnUpdateUnderline)
	ON_COMMAND(ID_INDENT, OnIndent)
	ON_UPDATE_COMMAND_UI(ID_INDENT, OnUpdateIndent)
	ON_COMMAND(ID_OUTDENT, OnOutdent)
	ON_UPDATE_COMMAND_UI(ID_OUTDENT, OnUpdateOutdent)
	ON_COMMAND(ID_FONTRED, OnFontred)
	ON_UPDATE_COMMAND_UI(ID_FONTRED, OnUpdateFontred)
	ON_COMMAND(ID_FONTBLACK, OnFontblack)
	ON_UPDATE_COMMAND_UI(ID_FONTBLACK, OnUpdateFontblack)
	ON_COMMAND(ID_BGCOLOR, OnBgcolor)
	ON_UPDATE_COMMAND_UI(ID_BGCOLOR, OnUpdateBgcolor)
	ON_COMMAND(ID_NOBGCOLOR, OnNobgcolor)
	ON_UPDATE_COMMAND_UI(ID_NOBGCOLOR, OnUpdateNobgcolor)
	ON_COMMAND(ID_FONTSIZEINCREASE, OnFontsizeincrease)
	ON_COMMAND(ID_FONTSIZEDECREASE, OnFontsizedecrease)
	ON_COMMAND(ID_ARIAL, OnArial)
	ON_COMMAND(ID_TIMES, OnTimes)
	ON_COMMAND(ID_COURIER, OnCourier)
	ON_COMMAND(ID_ALIGNLEFT, OnAlignleft)
	ON_UPDATE_COMMAND_UI(ID_ALIGNLEFT, OnUpdateAlignleft)
	ON_COMMAND(ID_ALIGNRIGHT, OnAlignright)
	ON_UPDATE_COMMAND_UI(ID_ALIGNRIGHT, OnUpdateAlignright)
	ON_COMMAND(ID_ALIGNCENTER, OnAligncenter)
	ON_UPDATE_COMMAND_UI(ID_ALIGNCENTER, OnUpdateAligncenter)
	ON_COMMAND(ID_INSERTLINK, OnInsertlink)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_COMMAND(ID_EDIT_REDO, OnEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_REDO, OnUpdateEditRedo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CEditorFrame::CEditorFrame(PRUint32 chromeMask)
{
    m_chromeMask = chromeMask;

    mCommandManager = nsnull;
}

CEditorFrame::~CEditorFrame()
{
}

BOOL CEditorFrame::InitEditor()
{
    // Get and save off nsICommandManager for later use
    //
    nsresult rv;
    mCommandManager = do_GetInterface(m_wndBrowserView.mWebBrowser, &rv);

    rv = MakeEditable();
 
    return (NS_SUCCEEDED(rv));
}

void CEditorFrame::OnBold() 
{
    ExecuteStyleCommand(BOLD_COMMAND);
}

void CEditorFrame::OnUpdateBold(CCmdUI* pCmdUI) 
{
    UpdateStyleToolBarBtn(BOLD_COMMAND, pCmdUI);
}

void CEditorFrame::OnItalics() 
{
    ExecuteStyleCommand(ITALIC_COMMAND);
}

void CEditorFrame::OnUpdateItalics(CCmdUI* pCmdUI) 
{
    UpdateStyleToolBarBtn(ITALIC_COMMAND, pCmdUI);
}

void CEditorFrame::OnUnderline() 
{
    ExecuteStyleCommand(UNDERLINE_COMMAND);
}

void CEditorFrame::OnUpdateUnderline(CCmdUI* pCmdUI) 
{
    UpdateStyleToolBarBtn(UNDERLINE_COMMAND, pCmdUI);
}


//Called to make a nsICommandParams with the 1 value pair of command name
NS_METHOD
CEditorFrame::MakeCommandParams(const nsAString &aCommand,nsICommandParams **aParams)
{
    nsresult rv;
    nsCOMPtr<nsICommandParams> params = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
	if (NS_FAILED(rv))
		return rv;
	if (!params)
		return NS_ERROR_FAILURE;
    rv = params->SetStringValue(COMMAND_NAME, aCommand);
	*aParams = params;
	NS_ADDREF(*aParams);
	return rv;
}


// Called in response to the ON_COMMAND messages generated
// when style related toolbar buttons(bold, italic etc)
// are clicked
//
NS_METHOD
CEditorFrame::ExecuteStyleCommand(const nsAString &aCommand)
{
	nsCOMPtr<nsICommandParams> params;
	nsresult rv = MakeCommandParams(aCommand,getter_AddRefs(params));
	if (NS_FAILED(rv))
		return rv;
	if (!params)
		return NS_ERROR_FAILURE;
    params->SetBooleanValue(STATE_ALL, true);

    return DoCommand(params);
}

// Called in response to the UPDATE_COMMAND_UI messages for 
// style related toolbar buttons(bold, italic etc.)
// to update their current state
//
void CEditorFrame::UpdateStyleToolBarBtn(const nsAString &aCommand, CCmdUI* pCmdUI)
{
	nsCOMPtr<nsICommandParams> params;
    nsresult rv;
	rv = MakeCommandParams(aCommand,getter_AddRefs(params));
	if (NS_FAILED(rv) || !params)
		return;
    rv = GetCommandState(params);
    if (NS_SUCCEEDED(rv))
    {
        // Does our current selection span mixed styles?
        // If so, set the toolbar button to an indeterminate
        // state
        //
        PRBool bMixedStyle = PR_FALSE;
        rv = params->GetBooleanValue(STATE_MIXED, &bMixedStyle);
        if (NS_SUCCEEDED(rv))
        {
            if(bMixedStyle)
            {
                pCmdUI->SetCheck(2);
                return;
            }
        }

        // We're not in STATE_MIXED. Enable/Disable the
        // toolbar button based on it's current state
        //
        PRBool bCmdEnabled = PR_FALSE;
        rv = params->GetBooleanValue(STATE_ALL, &bCmdEnabled);
        if (NS_SUCCEEDED(rv))
        {
            bCmdEnabled ? pCmdUI->SetCheck(1) : pCmdUI->SetCheck(0);
        }
    }
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
    return mCommandManager ? mCommandManager->DoCommand(aCommandParams) : NS_ERROR_FAILURE;
}

NS_METHOD
CEditorFrame::IsCommandEnabled(const nsAString &aCommand, PRBool *retval)
{
    return mCommandManager ? mCommandManager->IsCommandEnabled(aCommand, retval) : NS_ERROR_FAILURE;
}


NS_METHOD
CEditorFrame::GetCommandState(nsICommandParams *aCommandParams)
{
    return mCommandManager ? mCommandManager->GetCommandState(aCommandParams) : NS_ERROR_FAILURE;
}

NS_METHOD
CEditorFrame::ExecuteNoParam(const nsAString &aCommand)
{
    nsresult rv;
	nsCOMPtr<nsICommandParams> params;
	rv = MakeCommandParams(aCommand,getter_AddRefs(params));
	if (NS_FAILED(rv))
		return rv;
	if (!params)
		return NS_ERROR_FAILURE;
    return DoCommand(params);
}

void CEditorFrame::OnIndent() 
{
	// TODO: Add your command handler code here
	ExecuteNoParam(INDENT_COMMAND);	
}

void CEditorFrame::OnUpdateIndent(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CEditorFrame::OnOutdent() 
{
	// TODO: Add your command handler code here
	ExecuteNoParam(OUTDENT_COMMAND);	
}

void CEditorFrame::OnUpdateOutdent(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

NS_METHOD
CEditorFrame::ExecuteAttribParam(const nsAString &aCommand, const nsAString &aAttribute)
{
    nsresult rv;
	nsCOMPtr<nsICommandParams> params;
	rv = MakeCommandParams(aCommand,getter_AddRefs(params));
	if (NS_FAILED(rv))
		return rv;
	if (!params)
		return NS_ERROR_FAILURE;
    params->SetStringValue(STATE_ATTRIBUTE, aAttribute);
    return DoCommand(params);
}

NS_METHOD
CEditorFrame::GetAttributeParamValue(const nsAString &aCommand, nsString &aValue)
{
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(aCommand,getter_AddRefs(params));
  if (NS_FAILED(rv))
    return rv;
  if (!params)
    return NS_ERROR_FAILURE;
  rv = GetCommandState(params);
  if (NS_SUCCEEDED(rv))
  {
    return params->GetStringValue(STATE_ATTRIBUTE,aValue);
  }
  return rv;
}


void CEditorFrame::OnFontred() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(FONTCOLOR_COMMAND,COLOR_RED);	
}

void CEditorFrame::OnUpdateFontred(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CEditorFrame::OnFontblack() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(FONTCOLOR_COMMAND,COLOR_BLACK);		
}

void CEditorFrame::OnUpdateFontblack(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CEditorFrame::OnBgcolor() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(BACKGROUNDCOLOR_COMMAND,COLOR_RED);		
	
}

void CEditorFrame::OnUpdateBgcolor(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CEditorFrame::OnNobgcolor() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(BACKGROUNDCOLOR_COMMAND,STATE_EMPTY);
}

void CEditorFrame::OnUpdateNobgcolor(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	
}

void CEditorFrame::OnFontsizeincrease() 
{
	// TODO: Add your command handler code here
	ExecuteNoParam(INCREASEFONT_COMMAND);
}

void CEditorFrame::OnFontsizedecrease() 
{
	// TODO: Add your command handler code here
	ExecuteNoParam(DECREASEFONT_COMMAND);	
}

void CEditorFrame::OnArial() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(FONTFACE_COMMAND,FONT_ARIAL);
}

void CEditorFrame::OnTimes() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(FONTFACE_COMMAND,FONT_TIMES);	
}

void CEditorFrame::OnCourier() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(FONTFACE_COMMAND,FONT_COURIER);
}

void CEditorFrame::OnAlignleft() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(ALIGN_COMMAND,ALIGN_LEFT);
	
}


void CEditorFrame::OnUpdateAlignleft(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
  nsString tValue;

  nsresult rv = GetAttributeParamValue(ALIGN_COMMAND,tValue);
  if (NS_SUCCEEDED(rv))
  {
    if (tValue == ALIGN_LEFT)
      pCmdUI->SetCheck(1);
    else
      pCmdUI->SetCheck(0);
  }
}

void CEditorFrame::OnAlignright() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(ALIGN_COMMAND,ALIGN_RIGHT);
	
}

void CEditorFrame::OnUpdateAlignright(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
	// TODO: Add your command update UI handler code here
  nsString tValue;
  nsresult rv = GetAttributeParamValue(ALIGN_COMMAND,tValue);
  if (NS_SUCCEEDED(rv))
  {
    if (tValue == ALIGN_RIGHT)
      pCmdUI->SetCheck(1);
    else
      pCmdUI->SetCheck(0);
  }
}

void CEditorFrame::OnAligncenter() 
{
	// TODO: Add your command handler code here
	ExecuteAttribParam(ALIGN_COMMAND,ALIGN_CENTER);
	
}

void CEditorFrame::OnUpdateAligncenter(CCmdUI* pCmdUI) 
{
 	// TODO: Add your command update UI handler code here
	// TODO: Add your command update UI handler code here
  nsString tValue;
  nsresult rv = GetAttributeParamValue(ALIGN_COMMAND,tValue);
  if (NS_SUCCEEDED(rv))
  {
    if (tValue == ALIGN_CENTER)
      pCmdUI->SetCheck(1);
    else
      pCmdUI->SetCheck(0);
  }
}

void CEditorFrame::OnInsertlink() 
{
	// TODO: Add your command handler code here
	
}

void CEditorFrame::OnEditUndo() 
{
	// TODO: Add your command handler code here
	ExecuteNoParam(UNDO_COMMAND);
}

void CEditorFrame::OnEditRedo() 
{
	// TODO: Add your command handler code here
	ExecuteNoParam(REDO_COMMAND);	
}

void CEditorFrame::OnUpdateEditRedo(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(REDO_COMMAND,getter_AddRefs(params));
  if (NS_FAILED(rv))
    return;
  if (!params)
    return;
  rv = GetCommandState(params);
  if (NS_SUCCEEDED(rv))
  {
    PRBool tValue;
    params->GetBooleanValue(STATE_ENABLED,&tValue);
    if (tValue)
    {
      pCmdUI->Enable(TRUE);
      return;
    }
  }
  pCmdUI->Enable(FALSE);	
}

void CEditorFrame::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
	// TODO: Add your command update UI handler code here
  nsresult rv;
  nsCOMPtr<nsICommandParams> params;
  rv = MakeCommandParams(UNDO_COMMAND,getter_AddRefs(params));
  if (NS_FAILED(rv))
    return;
  if (!params)
    return;
  rv = GetCommandState(params);
  if (NS_SUCCEEDED(rv))
  {
    PRBool tValue;
    params->GetBooleanValue(STATE_ENABLED,&tValue);
    if (tValue)
    {
      pCmdUI->Enable(TRUE);
      return;
    }
  }
  pCmdUI->Enable(FALSE);		
}
