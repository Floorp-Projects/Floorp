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


// 1st column: command; 2nd column: DoCommand state, 3rd column: CmdParam state; 
CommandTest CommandTable[] = {
	{"cmd_bold",   "",    "state_all", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_italic", "",    "state_all", PR_TRUE, 10000000, 2.27,      "This is a complete sentence", "This isn't!"},
	{"cmd_underline", "", "state_all", PR_TRUE, 17,		  7839.3480, "A longer string than the previous one.", "Shorter string, but still pretty long."},
	{"cmd_indent", "",	  "state_enabled", PR_TRUE, 7480, -1.487, "Another string input for testing ...", "How about them Giants?!"},
	{"cmd_outdent", "",   "state_enabled", PR_FALSE, 0, 24987.2465, "A few numbers: 1 2 3,  A few letters: C A B,  A few characters: Mickey Goofy $%*&@", "nothing here"},
	{"cmd_increaseFont", "", "state_enabled", PR_TRUE, 500000000, 16, "hi", "HI"},
	{"cmd_undo", "",	  "state_enabled", PR_TRUE, 987352487, 36.489, "x ", "x"},
	{"cmd_redo", "",	  "state_enabled", PR_FALSE, 90, -24, "", " "},
	{"cmd_decreaseFont", "", "", PR_TRUE, 0.0,    0.0,     "hello",						 "HELLO"},
	{"cmd_fontColor", "state_attribute", "state_attribute", PR_TRUE, 25,    100,     "#FF0000",						 "#000000"},
	{"cmd_backgroundColor", "state_attribute", "state_attribute", PR_TRUE, -35871678,    15.345363645,     "#FF1234",						 "#001234"},
	{"cmd_fontFace", "state_attribute", "state_attribute", PR_TRUE, 50000,    5.798,     "Times New Roman, Times, serif",						 "Courier New, Courier, monospace"},
	{"cmd_align", "state_attribute", "state_attribute", PR_TRUE, 10000,    5.798,     "right",						 "center"},
	{"cmd_charSet", "state_attribute", "state_attribute", PR_TRUE, 20000,    5.798,     "hello",						 "HELLO"},
	{"cmd_copy", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_delete", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_deleteCharBackward", "", "state_enabled", PR_TRUE, 30000,    245.2323,     "hello",						 "HELLO"},
	{"cmd_deleteCharForward", "", "state_enabled", PR_TRUE, 50000,    -24235.2346,     "a very very very very very very very very looooooooooooooooooooong stringgggggggggggggggggggggggggggggggggggggggggggg!!!",						 "HELLO"},
	{"cmd_deleteWordForward", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_deleteWordBackward", "", "state_enabled", PR_TRUE, 6034600,    5.798,     "hello",						 "HELLO"},
	{"cmd_deleteToBeginningOfLine", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_deleteToEndOfLine", "", "state_enabled", PR_TRUE, -5434,    5.798,     "hello",						 "HELLO"},
	{"cmd_scrollTop", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_scrollBottom", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_scrollPageUp", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_scrollPageDown", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_movePageUp", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_movePageDown", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_moveTop", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_moveBottom", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectTop", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectBottom", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_lineNext", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_linePrevious", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectLineNext", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectLinePrevious", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_charPrevious", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_charNext", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectCharPrevious", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectCharNext", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_beginLine", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_endLine", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectBeginLine", "", "state_enabled", PR_FALSE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectEndLine", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_wordPrevious", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_wordNext", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectWordPrevious", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_selectWordNext", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_cut", "", "state_enabled", PR_TRUE, 50000,    5.798,     "hello",						 "HELLO"},
	{"cmd_cutOrDelete", "", "state_enabled", PR_FALSE, 50000,    5.798,     "hello",						 "HELLO"},
};

nsICommandManager * CnsICommandMgr::GetCommandMgrObject(nsIWebBrowser *aWebBrowser, PRInt16 displayMethod)
{
	nsCOMPtr<nsIWebBrowser> wb(aWebBrowser);
	nsCOMPtr<nsICommandManager> cmdMgrObj = do_GetInterface(wb, &rv);
	RvTestResult(rv, "GetCommandMgrObject() test", displayMethod);
    if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return nsnull;
	}
	return cmdMgrObj;
}

nsICommandManager * CnsICommandMgr::GetCommandMgrWithContractIDObject(PRInt16 displayMethod)
{
	nsCOMPtr<nsICommandManager> cmdMgrObj = do_CreateInstance(NS_COMMAND_MANAGER_CONTRACTID, &rv);
	RvTestResult(rv, "GetCommandMgrWithContractIDObject() test", 1); 
    if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return nsnull;
	}
	return cmdMgrObj;
}

void CnsICommandMgr::IsCommandSupportedTest(const char *aCommandName, PRInt16 displayMethod)
{
	PRBool isSupported;

	FormatAndPrintOutput("the Command input = ", aCommandName, displayMethod);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser, displayMethod);
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object. Test failed.");
		return;
	}
	rv = cmdMgrObj->IsCommandSupported(aCommandName, nsnull, &isSupported);
	RvTestResult(rv, "IsCommandSupported() test", displayMethod);
	if (displayMethod == 1)
		RvTestResultDlg(rv, "IsCommandSupported() test", true);
	FormatAndPrintOutput("isSupported boolean = ", isSupported, displayMethod);
}

void CnsICommandMgr::IsCommandEnabledTest(const char *aCommandName, PRInt16 displayMethod)
{
	PRBool isEnabled;

	FormatAndPrintOutput("the Command input = ", aCommandName, displayMethod);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser, displayMethod);
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return;
	}
	rv = cmdMgrObj->IsCommandEnabled(aCommandName, nsnull, &isEnabled);
	RvTestResult(rv, "IsCommandEnabled() test", displayMethod);
	if (displayMethod == 1)
		RvTestResultDlg(rv, "IsCommandEnabled() test");
	FormatAndPrintOutput("isEnabled boolean = ", isEnabled, displayMethod);
}

void CnsICommandMgr::GetCommandStateTest(const char *aCommandName, PRInt16 displayMethod)
{
	PRBool enabled = PR_FALSE;

	FormatAndPrintOutput("the Command input = ", aCommandName, displayMethod);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser, displayMethod);
	cmdParamObj = CnsICmdParams::GetCommandParamObject();
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object.");
		return;
	}
	rv = cmdMgrObj->GetCommandState(aCommandName, nsnull, cmdParamObj);
	RvTestResult(rv, "GetCommandState() test", displayMethod);
	if (displayMethod == 1)
		RvTestResultDlg(rv, "GetCommandState() test");
	if (!cmdParamObj) 
        QAOutput("Didn't get nsICommandParams object for GetCommandStateTest.");
}

void CnsICommandMgr::DoCommandTest(const char *aCommandName,
								   const char *doCommandState,
								   PRInt16 displayMethod)
{
	nsCAutoString value;

	FormatAndPrintOutput("the Command input = ", aCommandName, displayMethod);
	cmdMgrObj = GetCommandMgrObject(qaWebBrowser, displayMethod);
	cmdParamObj = CnsICmdParams::GetCommandParamObject();
	if (!cmdMgrObj) {
        QAOutput("Didn't get nsICommandManager object. Tests fail");
		return;
	}

	if (strcmp(doCommandState, "state_attribute") == 0)
	{
		if (strcmp(aCommandName,"cmd_fontColor") == 0 ||
		    strcmp(aCommandName,"cmd_backgroundColor") == 0)
			value = "#FF0000";
		else if (strcmp(aCommandName,"cmd_fontFace") == 0)
			value = "Helvetica, Ariel, san-serif";
		else
			value = "left";

		if (cmdParamObj)
			cmdParamObj->SetCStringValue("state_attribute", value.get());
		else
			QAOutput("Didn't get nsICommandParam object for nsICommandMgr test.");
	}
	rv = cmdMgrObj->DoCommand(aCommandName, cmdParamObj, nsnull);
	RvTestResult(rv, "DoCommand() test", displayMethod);
	if (displayMethod == 1)
		RvTestResultDlg(rv, "DoCommand() test");
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
			IsCommandSupportedTest("cmd_bold", 2);
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_ISCOMMANDENABLED :
			IsCommandEnabledTest("cmd_bold", 2);
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_GETCOMMANDSTATE :
			GetCommandStateTest("cmd_charSet", 2);
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_DOCOMMAND :
			DoCommandTest("cmd_fontColor", "state_attribute", 2);
			break;
	}
}

void CnsICommandMgr::RunAllTests()
{
	PRInt16 i;

	QAOutput("Run All nsICommandManager tests. Check C:/Temp/TestOutput.txt for test results.", 2);
	for (i=0; i < 50; i++)
	{
		FormatAndPrintOutput("loop cnt = ", i, 1);
		IsCommandSupportedTest(CommandTable[i].mCmdName, 1);
		IsCommandEnabledTest(CommandTable[i].mCmdName, 1);
		GetCommandStateTest(CommandTable[i].mCmdName, 1);
		DoCommandTest(CommandTable[i].mCmdName,
					  CommandTable[i].mDoCmdState, 1);
	}
}


/////////////////////////////////////////////////////////////////////////////
// CnsICommandMgr message handlers
