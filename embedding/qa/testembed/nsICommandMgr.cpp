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


nsICommandManager * CnsICommandMgr::GetCommandMgrObject()
{
	nsCOMPtr<nsICommandManager> cmdMgrObj = do_GetInterface(qaWebBrowser, &rv);
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

nsICommandParams * CnsICommandMgr::GetCommandParamsObject()
{
    nsCOMPtr<nsICommandParams> cmdParamsObj = do_CreateInstance(NS_COMMAND_PARAMS_CONTRACTID,&rv);
	RvTestResult(rv, "GetCommandParamsObject() test", 1); 
    if (!cmdParamsObj) {
        QAOutput("Didn't get nsICommandParams object.");
		return nsnull;
	}
	nsICommandParams *retVal = cmdParamsObj;
	NS_ADDREF(retVal);
	return retVal;
}

void CnsICommandMgr::IsCommandSupportedTest(const char *aCommandName)
{
	PRBool isSupported;

	cmdMgrObj = GetCommandMgrObject();
	rv = cmdMgrObj->IsCommandSupported(aCommandName, &isSupported);
	RvTestResult(rv, "IsCommandSupported() test", 2);
	FormatAndPrintOutput("isSupported boolean = ", isSupported, 2);
}

void CnsICommandMgr::IsCommandEnabledTest(const char *aCommandName)
{
	PRBool isEnabled;

	cmdMgrObj = GetCommandMgrObject();
	rv = cmdMgrObj->IsCommandEnabled(aCommandName, &isEnabled);
	RvTestResult(rv, "IsCommandEnabled() test", 2);
	FormatAndPrintOutput("isEnabled boolean = ", isEnabled, 2);
}

void CnsICommandMgr::GetCommandStateTest(const char *aCommandName, const char *aParameter)
{
	PRBool enabled = PR_FALSE;

	cmdMgrObj = GetCommandMgrObject();
	cmdParamObj = GetCommandParamsObject();
	rv = cmdMgrObj->GetCommandState(aCommandName, cmdParamObj);
	RvTestResult(rv, "GetCommandState() test", 2);
	cmdParamObj->GetBooleanValue(aParameter, &enabled);
	FormatAndPrintOutput("isEnabled boolean = ", enabled, 2);
}

void CnsICommandMgr::DoCommandTest(const char *aCommandName, const char *value)
{
	cmdMgrObj = GetCommandMgrObject();
	cmdParamObj = GetCommandParamsObject();
	rv = cmdMgrObj->DoCommand(aCommandName, cmdParamObj);
	RvTestResult(rv, "DoCommand() test", 2);
	cmdParamObj->SetCStringValue("state_attribute", value);
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
			GetCommandStateTest("cmd_bold", "state_enabled");
			break;
		case ID_INTERFACES_NSICOMMANDMANAGER_DOCOMMAND :
			nsCAutoString value;
			DoCommandTest("cmd_bold", value.get());
			break;
	}
}

void CnsICommandMgr::RunAllTests()
{
	QAOutput("Not implemented yet.", 2);
}


/////////////////////////////////////////////////////////////////////////////
// CnsICommandMgr message handlers
