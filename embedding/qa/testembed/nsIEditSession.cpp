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
// nsIEditSession.cpp : test implementations for nsIEditingSession interface
//

#include "stdafx.h"
#include "testembed.h"
#include "nsIEditSession.h"
#include "QaUtils.h"
#include "BrowserFrm.h"
#include "BrowserImpl.h"
#include "BrowserView.h"
#include "Tests.h"
#include "nsIEditor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// nsIEditSession

CnsIEditSession::CnsIEditSession(nsIWebBrowser *mWebBrowser)
{
	qaWebBrowser = mWebBrowser;
}

CnsIEditSession::~CnsIEditSession()
{
}

nsIEditingSession * CnsIEditSession::GetEditSessionObject()
{
    editingSession = do_GetInterface(qaWebBrowser);
    if (!editingSession) {
        QAOutput("Didn't get nsIEditingSession object.");
		return nsnull;
	}

    return (editingSession);
}

void CnsIEditSession::InitTest()
{
	editingSession = GetEditSessionObject();
	domWindow = GetTheDOMWindow(qaWebBrowser);
	if (domWindow && editingSession) {
		rv = editingSession->Init(domWindow);
		RvTestResult(rv, "Init() test", 2);
	}
	else
		QAOutput("Didn't get object(s) for InitTest() test. Test failed.", 1);
}

void CnsIEditSession::MakeWinEditTest(PRBool afterUriLoad)
{
	editingSession = GetEditSessionObject();
	domWindow = GetTheDOMWindow(qaWebBrowser);
	if (domWindow && editingSession) {
		rv= editingSession->MakeWindowEditable(domWindow, NULL, afterUriLoad);
		RvTestResult(rv, "MakeWindowEditable() test", 2);
	}
	else
		QAOutput("Didn't get object(s) for MakeWindowEditable() test. Test failed.", 1);
}

void CnsIEditSession::WinIsEditTest(PRBool outIsEditable)
{
	editingSession = GetEditSessionObject();
	domWindow = GetTheDOMWindow(qaWebBrowser);
	if (domWindow && editingSession) {
		rv = editingSession->WindowIsEditable(domWindow, &outIsEditable);
		RvTestResult(rv, "WindowIsEditable() test", 2);
		FormatAndPrintOutput("the outIsEditable boolean = ", outIsEditable, 2);
	}
	else
		QAOutput("Didn't get object(s) for WinIsEditTest() test. Test failed.", 1);
}

void CnsIEditSession::GetEditorWinTest()
{
	nsCOMPtr<nsIEditor> theEditor;
//	nsIEditor *theEditor = nsnull;
	editingSession = GetEditSessionObject();
	domWindow = GetTheDOMWindow(qaWebBrowser);
	if (domWindow && editingSession) {
		rv = editingSession->GetEditorForWindow(domWindow, getter_AddRefs(theEditor));
		RvTestResult(rv, "GetEditorForWindow() test", 2);
		if (!theEditor) 
			QAOutput("Didn't get the Editor object.");
	}
	else
		QAOutput("Didn't get object(s) for WinIsEditTest() test. Test failed.", 1);
}

void CnsIEditSession::SetEditorWinTest()
{
	editingSession = GetEditSessionObject();
	domWindow = GetTheDOMWindow(qaWebBrowser);
	if (domWindow && editingSession) {
		rv = editingSession->SetupEditorOnWindow(domWindow);
		RvTestResult(rv, "SetupEditorOnWindow() test", 2);
	}
	else
		QAOutput("Didn't get object(s) for SetEditorWinTest() test. Test failed.", 1);
}

void CnsIEditSession::TearEditorWinTest()
{
	editingSession = GetEditSessionObject();
	domWindow = GetTheDOMWindow(qaWebBrowser);
	if (domWindow && editingSession) {
		rv = editingSession->TearDownEditorOnWindow(domWindow);
		RvTestResult(rv, "TearDownEditorOnWindow() test", 2);
	}
	else
		QAOutput("Didn't get object(s) for TearEditorWinTest() test. Test failed.", 1);
}

void CnsIEditSession::OnStartTests(UINT nMenuID)
{
	switch(nMenuID)
	{
		case ID_INTERFACES_NSIEDITINGSESSION_RUNALLTESTS :
			RunAllTests();
			break;
		case ID_INTERFACES_NSIEDITINGSESSION_INIT :
			InitTest();
			break;
		case ID_INTERFACES_NSIEDITINGSESSION_MAKEWINDOWEDITABLE :
			MakeWinEditTest(PR_FALSE);
			break;
		case ID_INTERFACES_NSIEDITINGSESSION_WINDOWISEDITABLE  :
			WinIsEditTest(PR_TRUE);
			break;
		case ID_INTERFACES_NSIEDITINGSESSION_GETEDITORFORWINDOW :
			GetEditorWinTest();
			break;
		case ID_INTERFACES_NSIEDITINGSESSION_SETUPEDITORONWINDOW :
			SetEditorWinTest();
			break;
		case ID_INTERFACES_NSIEDITINGSESSION_TEARDOWNEDITORONWINDOW :
			TearEditorWinTest();
			break;
	}
}

void CnsIEditSession::RunAllTests()
{
	InitTest();
	MakeWinEditTest(PR_FALSE);
	WinIsEditTest(PR_TRUE);
	GetEditorWinTest();
//	SetEditorWinTest();		 
	TearEditorWinTest();
}

/////////////////////////////////////////////////////////////////////////////
// CnsIEditSession message handlers
