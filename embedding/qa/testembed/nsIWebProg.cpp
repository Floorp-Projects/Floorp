/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

// nsiWebProg.cpp : implementation file
//

#include "Stdafx.h"
#include "TestEmbed.h"
#include "nsIWebProg.h"
#include "Tests.h"
#include "QaUtils.h"
#include "BrowserFrm.h"
#include "BrowserImpl.h"
#include "BrowserView.h"
#include "domwindow.h"
#include "WebProgDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CnsiWebProg

// constructor for CnsiWebProg
CnsiWebProg::CnsiWebProg(nsIWebBrowser *mWebBrowser, 
						 CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser;
	qaBrowserImpl = mpBrowserImpl;
}

// destructor for CnsiWebProg
CnsiWebProg::~CnsiWebProg()
{
}

	// nsIWebProgress test cases

		// get webProg object
nsIWebProgress * CnsiWebProg::GetWebProgObject()
{
	nsCOMPtr<nsIInterfaceRequestor> qaIReq(do_QueryInterface(qaWebBrowser));
	nsCOMPtr<nsIWebProgress> qaWebProgress(do_GetInterface(qaIReq));

	if (!qaWebProgress) {
		QAOutput("Didn't get web progress object.", 2);
		return NULL;
	}
	else {
		QAOutput("We got web progress object.", 1);
		return (qaWebProgress);
	}
}

void CnsiWebProg::AddWebProgLstnr(PRUint32 theFlag, PRInt16 displayMode)
{
	nsCAutoString flagName(NS_LITERAL_CSTRING("xxxx"));

	ConvertWPFlagToString(theFlag, flagName);

			// addWebProgListener
	qaWebProgress = GetWebProgObject();
	nsCOMPtr<nsIWebProgressListener> listener(NS_STATIC_CAST(nsIWebProgressListener*, qaBrowserImpl));
	rv = qaWebProgress->AddProgressListener(listener, theFlag);
//	StoreWebProgFlag(theFlag);
	RvTestResult(rv, "nsIWebProgress::AddProgressListener() test", displayMode);
	RvTestResultDlg(rv, "nsIWebProgress::AddProgressListener() test", true);
	FormatAndPrintOutput("WebProgressListener flag = ", flagName, displayMode);
}

void CnsiWebProg::RemoveWebProgLstnr(PRInt16 displayMode)
{
		// removeWebProgListener
	qaWebProgress = GetWebProgObject();
	nsCOMPtr<nsIWebProgressListener> listener(NS_STATIC_CAST(nsIWebProgressListener*, qaBrowserImpl));
	rv = qaWebProgress->RemoveProgressListener(listener);
	RvTestResult(rv, "nsIWebProgress::RemoveProgressListener() test", displayMode);
	RvTestResultDlg(rv, "nsIWebProgress::RemoveProgressListener() test");
}

void CnsiWebProg::GetTheDOMWindow(PRInt16 displayMode)
{
		// getTheDOMWindow
	qaWebProgress = GetWebProgObject();
	nsCOMPtr<nsIDOMWindow> qaDOMWindow;
	rv = qaWebProgress->GetDOMWindow(getter_AddRefs(qaDOMWindow));
	RvTestResult(rv, "nsIWebProgress::GetDOMWindow() test", displayMode);
	RvTestResultDlg(rv, "nsIWebProgress::GetDOMWindow() test");
	if (!qaDOMWindow)
		QAOutput("Didn't get DOM Window object.", displayMode);
}

void CnsiWebProg::GetIsLoadingDocTest(PRInt16 displayMode)
{
	PRBool docLoading;
	qaWebProgress = GetWebProgObject();
	rv = qaWebProgress->GetIsLoadingDocument(&docLoading);
	RvTestResult(rv, "nsIWebProgress::GetisLoadingDocument() test", displayMode);
	RvTestResultDlg(rv, "nsIWebProgress::GetisLoadingDocument() test");
	FormatAndPrintOutput("GetisLoadingDocument return value = ", docLoading, displayMode);
}

void CnsiWebProg::ConvertWPFlagToString(PRUint32 theFlag,
										nsCAutoString& flagName)
{
	switch(theFlag)
	{
	case nsIWebProgress::NOTIFY_STATE_REQUEST:
		flagName.AssignLiteral("NOTIFY_STATE_REQUEST");
		break;
	case nsIWebProgress::NOTIFY_STATE_DOCUMENT:
		flagName.AssignLiteral("NOTIFY_STATE_DOCUMENT");
		break;
	case nsIWebProgress::NOTIFY_STATE_NETWORK:
		flagName.AssignLiteral("NOTIFY_STATE_NETWORK");
		break;
	case nsIWebProgress::NOTIFY_STATE_WINDOW:
		flagName.AssignLiteral("NOTIFY_STATE_WINDOW");
		break;
	case nsIWebProgress::NOTIFY_STATE_ALL:
		flagName.AssignLiteral("NOTIFY_STATE_ALL");
		break;
	case nsIWebProgress::NOTIFY_PROGRESS:
		flagName.AssignLiteral("NOTIFY_PROGRESS");
		break;
	case nsIWebProgress::NOTIFY_STATUS:
		flagName.AssignLiteral("NOTIFY_STATUS");
		break;
	case nsIWebProgress::NOTIFY_SECURITY:
		flagName.AssignLiteral("NOTIFY_SECURITY");
		break;
	case nsIWebProgress::NOTIFY_LOCATION:
		flagName.AssignLiteral("NOTIFY_LOCATION");
		break;
	case nsIWebProgress::NOTIFY_ALL:
		flagName.AssignLiteral("NOTIFY_ALL");
		break;
	case nsIWebProgress::NOTIFY_STATE_DOCUMENT | nsIWebProgress::NOTIFY_STATE_REQUEST:
		flagName.AssignLiteral("NOTIFY_STATE_DOCUMENT&REQUEST");
		break;
	case nsIWebProgress::NOTIFY_STATE_DOCUMENT | nsIWebProgress::NOTIFY_STATE_REQUEST
					| nsIWebProgress::NOTIFY_STATE_NETWORK	:
		flagName.AssignLiteral("NOTIFY_STATE_DOCUMENT&REQUEST&NETWORK");
		break;
	case nsIWebProgress::NOTIFY_STATE_NETWORK | nsIWebProgress::NOTIFY_STATE_WINDOW:
		flagName.AssignLiteral("NOTIFY_STATE_NETWORK&WINDOW");
		break;
	}
}

void CnsiWebProg::StoreWebProgFlag(PRUint32 theFlag)
{
	theStoredFlag = theFlag;
}

void CnsiWebProg::RetrieveWebProgFlag()
{
	PRUint32 theFlag;
	nsCAutoString flagName(NS_LITERAL_CSTRING("NOTIFY_ALL"));

	theFlag = theStoredFlag;
	ConvertWPFlagToString(theFlag, flagName);
	FormatAndPrintOutput("WebProgressListener flag = ", flagName, 2);
}

void CnsiWebProg::OnStartTests(UINT nMenuID)
{
	CWebProgDlg myDialog;

	switch(nMenuID)
	{
		case ID_INTERFACES_NSIWEBPROGRESS_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIWEBPROGRESS_ADDPROGRESSLISTENER :
			if (myDialog.DoModal() == IDOK)
				AddWebProgLstnr(myDialog.m_wpFlagValue, 2);
			break ;
		case ID_INTERFACES_NSIWEBPROGRESS_REMOVEPROGRESSLISTENER :
			RemoveWebProgLstnr(2);
			break ;
		case ID_INTERFACES_NSIWEBPROGRESS_GETDOMWINDOW  :
			GetTheDOMWindow(2);
			break ;
		case ID_INTERFACES_NSIWEBPROGRESS_ISLOADINGDOCUMENT :
			GetIsLoadingDocTest(2);
			break;
	}
}

void CnsiWebProg::RunAllTests(void)
{
	int i;
	PRUint32 theFlag = 0x000000ff;
	for (i = 0; i < 10; i++) {
		switch(i) {
		case 0:
			theFlag = nsIWebProgress::NOTIFY_STATE_REQUEST;
			break;
		case 1:
			theFlag = nsIWebProgress::NOTIFY_STATE_DOCUMENT;
			break;
		case 2:
			theFlag = nsIWebProgress::NOTIFY_STATE_NETWORK;
			break;
		case 3:
			theFlag = nsIWebProgress::NOTIFY_STATE_WINDOW;
			break;
		case 4:
			theFlag = nsIWebProgress::NOTIFY_STATE_ALL;
			break;
		case 5:
			theFlag = nsIWebProgress::NOTIFY_PROGRESS;
			break;
		case 6:
			theFlag = nsIWebProgress::NOTIFY_STATUS;
			break;
		case 7:
			theFlag = nsIWebProgress::NOTIFY_SECURITY;
			break;
		case 8:
			theFlag = nsIWebProgress::NOTIFY_LOCATION;
			break;
		case 9:
			theFlag = nsIWebProgress::NOTIFY_ALL;
			break;
		}
		AddWebProgLstnr(theFlag, 1);
		RemoveWebProgLstnr(1);
	}
	GetTheDOMWindow(1);
	GetIsLoadingDocTest(1);
}

/////////////////////////////////////////////////////////////////////////////
// CnsiWebProg message handlers
