// nsICommandMgr.cpp : implementation file
//
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
 *   David Epstein <depstein@netscape.com> 
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

// File Overview....
//
// nsICommandMgr.cpp : test implementations for nsICommandManager interface
//
#include "stdafx.h"
#include "testembed.h"
#include "nsICommandMgr.h"
#include "nsICmdParams.h"
#include "QaUtils.h"
#include "BrowserFrm.h"
#include "BrowserImpl.h"
#include "BrowserView.h"
#include "Tests.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// nsICommandMgr

CnsICommandMgr::CnsICommandMgr(nsIWebBrowser *mWebBrowser)
{
	qaWebBrowser = mWebBrowser;
}

CnsICommandMgr::~CnsICommandMgr()
{
}


CommandTest CommandTable[] = {
	{"cmd_bold", "", "state_all", "state_begin", "state_end", "state_mixed", 1},
	{"cmd_italic", "", "state_all", "state_begin", "state_end", "state_mixed", 1},
	{"cmd_underline", "", "state_all", "state_begin", "state_end", "state_mixed", 1},
	{"cmd_indent", "", "state_enabled", "", "", "", 1},
	{"cmd_outdent", "", "state_enabled", "", "", "", 1},
	{"cmd_increaseFont", "", "state_enabled", "", "", "", 1},
	{"cmd_undo", "", "state_enabled", "", "", "", 1},
	{"cmd_redo", "", "state_enabled", "", "", "", 1},
	{"cmd_decreaseFont", "", "", "", "", "", 1},
	{"cmd_fontColor", "state_attribute", "state_attribute", "", "", "", 1},
	{"cmd_backgroundColor", "state_attribute", "state_attribute", "", "", "", 1},
	{"cmd_fontFace", "state_attribute", "state_attribute", "", "", "", 1},
	{"cmd_align", "state_attribute", "state_attribute", "", "", "", 1},
	{"cmd_charSet", "state_attribute", "state_attribute", "", "", "", 1},
	{"cmd_copy", "", "state_enabled", "", "", "", 1},
	{"cmd_delete", "", "state_enabled", "", "", "", 1},
	{"cmd_deleteCharBackward", "", "state_enabled", "", "", "", 1},
	{"cmd_deleteCharForward", "", "state_enabled", "", "", "", 1},
	{"cmd_deleteWordForward", "", "state_enabled", "", "", "", 1},
	{"cmd_deleteWordBackward", "", "state_enabled", "", "", "", 1},
	{"cmd_deleteToBeginningOfLine", "", "state_enabled", "", "", "", 1},
	{"cmd_deleteToEndOfLine", "", "state_enabled", "", "", "", 1},
	{"cmd_scrollTop", "", "state_enabled", "", "", "", 1},
	{"cmd_scrollBottom", "", "state_enabled", "", "", "", 1},
	{"cmd_scrollPageUp", "", "state_enabled", "", "", "", 1},
	{"cmd_scrollPageDown", "", "state_enabled", "", "", "", 1},
	{"cmd_movePageUp", "", "state_enabled", "", "", "", 1},
	{"cmd_movePageDown", "", "state_enabled", "", "", "", 1},
	{"cmd_moveTop", "", "state_enabled", "", "", "", 1},
	{"cmd_moveBottom", "", "state_enabled", "", "", "", 1},
	{"cmd_selectTop", "", "state_enabled", "", "", "", 1},
	{"cmd_selectBottom", "", "state_enabled", "", "", "", 1},
	{"cmd_lineNext", "", "state_enabled", "", "", "", 1},
	{"cmd_linePrevious", "", "state_enabled", "", "", "", 1},
	{"cmd_selectLineNext", "", "state_enabled", "", "", "", 1},
	{"cmd_selectLinePrevious", "", "state_enabled", "", "", "", 1},
	{"cmd_charPrevious", "", "state_enabled", "", "", "", 1},
	{"cmd_charNext", "", "state_enabled", "", "", "", 1},
	{"cmd_selectCharPrevious", "", "state_enabled", "", "", "", 1},
	{"cmd_selectCharNext", "", "state_enabled", "", "", "", 1},
	{"cmd_beginLine", "", "state_enabled", "", "", "", 1},
	{"cmd_endLine", "", "state_enabled", "", "", "", 1},
	{"cmd_selectBeginLine", "", "state_enabled", "", "", "", 1},
	{"cmd_selectEndLine", "", "state_enabled", "", "", "", 1},
	{"cmd_wordPrevious", "", "state_enabled", "", "", "", 1},
	{"cmd_wordNext", "", "state_enabled", "", "", "", 1},
	{"cmd_selectWordPrevious", "", "state_enabled", "", "", "", 1},
	{"cmd_selectWordNext", "", "state_enabled", "", "", "", 1},
	{"cmd_cut", "", "state_enabled", "", "", "", 1},
	{"cmd_cutOrDelete", "", "state_enabled", "", "", "", 1},
};

nsICommandManager * CnsICommandMgr::GetCommandMgrObject(nsIWebBrowser *aWebBrowser)
{
	nsCOMPtr<nsIWebBrowser> wb(aWebBrowser);
	nsCOMPtr<nsICommandManager> cmdMgrObj = do_GetInterface(wb, &rv);
	RvTestResult(rv, "GetCommandMgrObject() test", 1); 
    if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return nsnull;
	}
	return cmdMgrObj;
}

nsICommandManager * CnsICommandMgr::GetCommandMgrWithContractIDObject()
{
	nsCOMPtr<nsICommandManager> cmdMgrObj = do_CreateInstance(NS_COMMAND_MANAGER_CONTRACTID, &rv);
	RvTestResult(rv, "GetCommandMgrWithContractIDObject() test", 1); 
    if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return nsnull;
	}
	return cmdMgrObj;
}

void CnsICommandMgr::IsCommandSupportedTest(const char *aCommandName)
{
	PRBool isSupported;

	FormatAndPrintOutput("the Command input = ", aCommandName, 2);
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return;
	}
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser);
	rv = cmdMgrObj->IsCommandSupported(aCommandName, &isSupported);
	RvTestResult(rv, "IsCommandSupported() test", 2);
	FormatAndPrintOutput("isSupported boolean = ", isSupported, 2);
}

void CnsICommandMgr::IsCommandEnabledTest(const char *aCommandName)
{
	PRBool isEnabled;

	FormatAndPrintOutput("the Command input = ", aCommandName, 2);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser);
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return;
	}
	rv = cmdMgrObj->IsCommandEnabled(aCommandName, &isEnabled);
	RvTestResult(rv, "IsCommandEnabled() test", 2);
	FormatAndPrintOutput("isEnabled boolean = ", isEnabled, 2);
}

void CnsICommandMgr::GetCommandStateTest(const char *aCommandName)
{
	PRBool enabled = PR_FALSE;

	FormatAndPrintOutput("the Command input = ", aCommandName, 2);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser);
	cmdParamObj = CnsICmdParams::GetCommandParamObject();
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return;
	}
	else if (!cmdParamObj) {
        QAOutput("Didn't get nsICommandParams object.");
		return;
	}
	else {
		rv = cmdMgrObj->GetCommandState(aCommandName, cmdParamObj);
		RvTestResult(rv, "GetCommandState() test", 2);
	}
}

void CnsICommandMgr::DoCommandTest(const char *aCommandName)
{
	nsCAutoString value;

	FormatAndPrintOutput("the Command input = ", aCommandName, 2);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser);
	cmdParamObj = CnsICmdParams::GetCommandParamObject();
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object. Tests fail");
		return;
	}
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandParam object. Test fail");
		return;
	}
	if  (strcmp(aCommandName,"cmd_fontColor") == 0 ||
		 strcmp(aCommandName,"cmd_backgroundColor") == 0 ||
		 strcmp(aCommandName,"cmd_fontFace") == 0 ||
		 strcmp(aCommandName,"cmd_align") == 0)
	{
		if (strcmp(aCommandName,"cmd_fontColor") == 0 ||
		    strcmp(aCommandName,"cmd_backgroundColor") == 0)
			value = "#FF0000";
		else if (strcmp(aCommandName,"cmd_fontFace") == 0)
			value = "Helvetica, Ariel, san-serif";
		else
			value = "left";
		cmdParamObj->SetCStringValue("state_attribute", value.get());
	}
	rv = cmdMgrObj->DoCommand(aCommandName, cmdParamObj);
	RvTestResult(rv, "DoCommand() test", 2);
}


void CnsICommandMgr::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSICOMMANDMANAGER_RUNALLTESTS :
			RunAllTests();
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_ADDCOMMANDOBSERVER :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_REMOVECOMMANDOBSERVER :
			QAOutput("Not implemented yet.", 2);
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_ISCOMMANDESUPPORTED  :
			IsCommandSupportedTest("cmd_bold");
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_ISCOMMANDENABLED :
			IsCommandEnabledTest("cmd_bold");
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_GETCOMMANDSTATE :
			GetCommandStateTest("cmd_charSet");
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_DOCOMMAND :
			DoCommandTest("cmd_fontColor");
			break;
	}
}

void CnsICommandMgr::RunAllTests()
{
	PRInt16 i;

	for (i=0; i < 50; i++)
	{
		FormatAndPrintOutput("loop cnt = ", i, 2);
		IsCommandSupportedTest(CommandTable[i].mCmdName);
		IsCommandEnabledTest(CommandTable[i].mCmdName);
		GetCommandStateTest(CommandTable[i].mCmdName);
		DoCommandTest(CommandTable[i].mCmdName);
	}
}


/////////////////////////////////////////////////////////////////////////////
// CnsICommandMgr message handlers
