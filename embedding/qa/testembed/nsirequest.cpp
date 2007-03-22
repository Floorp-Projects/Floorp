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
 *   Ashish Bhatt <ashishbhatt@netscape.com>
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

// File Overview....
//
// Test cases for the nsiRequest Interface

#include "stdafx.h"
#include "TestEmbed.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "QaUtils.h"
#include "nsirequest.h"
#include <stdio.h>


/////////////////////////////////////////////////////////////////////////////
// CNsIRequest


CNsIRequest::CNsIRequest(nsIWebBrowser* mWebBrowser,CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser ;
	qaBrowserImpl = mpBrowserImpl ;
}


CNsIRequest::~CNsIRequest()
{
}


/////////////////////////////////////////////////////////////////////////////
// CNsIRequest message handlers
// ***********************************************************************
// ***********************************************************************
//  nsIRequest iface

//  table columns corrsp to: pending, status, suspend, resume, cancel,
//  setLoadGroup & getLoadGroup tests respectively.

Element ReqTable[] = {
	{"http://www.netscape.com/", 1, 1, 0, 0, 0, 1, 1},
	{"http://www.yahoo.com/",    0, 0, 1, 1, 0, 0, 0},
	{"http://www.cisco.com/",    0, 0, 0, 0, 1, 0, 0},
	{"http://www.sun.com/",      0, 0, 0, 0, 0, 1, 1},
	{"http://www.intel.com/",    1, 1, 1, 0, 0, 0, 0},
	{"http://www.aol.com/",      0, 1, 0, 0, 0, 1, 1},
	{"https://www.yahoo.com/",   1, 1, 1, 1, 0, 1, 1},
	{"data:text/plain;charset=iso-8859-7,%be%fg%be",
								 1, 1, 1, 1, 0, 1, 1},
	{"file://C|/Program Files/", 1, 1, 1, 1, 0, 1, 1},
	{"ftp://ftp.netscape.com/",  1, 1, 1, 1, 0, 1, 1},
	{"ftp://ftp.mozilla.org/",   0, 0, 0, 0, 1, 0, 0},
};

void CNsIRequest::OnStartTests(UINT nMenuID)
{
	if (nMenuID == ID_INTERFACES_NSIREQUEST_RUNALLTESTS)
		RunAllTests(9);
	else
		RunIndividualTests(nMenuID, 9);
}
 
void CNsIRequest::RunIndividualTests(UINT nMenuID, int reqTotal)
{
	nsCOMPtr<nsIChannel> theChannel;
	nsCOMPtr<nsILoadGroup> theLoadGroup(do_CreateInstance(NS_LOADGROUP_CONTRACTID));
	nsCOMPtr<nsIURI> theURI;
	if (!theLoadGroup)
	{
		QAOutput("We didn't get the Load Group. Test failed.", 2);
		return;
	}
	int i=0;

    QAOutput("Start selected nsIRequest test.", 2);	

	for (i=0; i<reqTotal; i++)
	{
		// nsIRequest individual tests

		QAOutput("********** Individual nsIRequest test begins. **********");

		theChannel = GetTheChannel(i, theLoadGroup);
		nsCOMPtr<nsIRequest> theRequest = do_QueryInterface(theChannel);

		switch(nMenuID)
		{
			
		case ID_INTERFACES_NSIREQUEST_GETNAME :
			break;
		case ID_INTERFACES_NSIREQUEST_ISPENDING :
			IsPendingReqTest(theRequest);
			break;
		case ID_INTERFACES_NSIREQUEST_GETSTATUS :
			GetStatusReqTest(theRequest);
			break;
		case ID_INTERFACES_NSIREQUEST_CANCEL :
			CancelReqTest(theRequest);	
			break;
		case ID_INTERFACES_NSIREQUEST_SUSPEND :
			SuspendReqTest(theRequest);	
			break;
		case ID_INTERFACES_NSIREQUEST_RESUME :
			ResumeReqTest(theRequest);	
			break;
		case ID_INTERFACES_NSIREQUEST_GETLOADGROUP :
			GetLoadGroupTest(theRequest);
			break;
		case ID_INTERFACES_NSIREQUEST_SETLOADGROUP :
			SetLoadGroupTest(theRequest, theLoadGroup);	
			break;
		case ID_INTERFACES_NSIREQUEST_GETLOADFLAGS :
			break;
		case ID_INTERFACES_NSIREQUEST_SETLOADFLAGS :
			break;
		}

	} // end for loop
}


void CNsIRequest::RunAllTests(int reqTotal) 
{
	// note: nsIRequest tests are called:
	// 1) in BrowserImpl.cpp, nsIStreamListener::OnDataAvailable()
	// 2) as individual tests below

	nsCOMPtr<nsIChannel> theChannel;
	nsCOMPtr<nsILoadGroup> theLoadGroup(do_CreateInstance(NS_LOADGROUP_CONTRACTID));
	if (!theLoadGroup)
	{
		QAOutput("We didn't get the Load Group. Test failed.", 2);
		return;
	}

	int i=0;

    QAOutput("Start nsIRequest tests.", 1);	

	for (i=0; i<reqTotal; i++)
	{
		// nsIRequest individual tests

		QAOutput("********** Individual nsIRequest test begins. **********");

		theChannel = GetTheChannel(i, theLoadGroup);
		nsCOMPtr<nsIRequest> theRequest = do_QueryInterface(theChannel);

		if (ReqTable[i].reqPend == TRUE)
			IsPendingReqTest(theRequest);

		if (ReqTable[i].reqStatus == TRUE)
			GetStatusReqTest(theRequest);

		if (ReqTable[i].reqSuspend == TRUE)
			SuspendReqTest(theRequest);	

		if (ReqTable[i].reqResume == TRUE)
			ResumeReqTest(theRequest);	

		if (ReqTable[i].reqCancel == TRUE)
			CancelReqTest(theRequest);	

		if (ReqTable[i].reqSetLoadGroup == TRUE)
			SetLoadGroupTest(theRequest, theLoadGroup);	

		if (ReqTable[i].reqGetLoadGroup == TRUE)
			GetLoadGroupTest(theRequest);

		QAOutput("- - - - - - - - - - - - - - - - - - - - -", 1);
	} // end for loop
    QAOutput("End nsIRequest tests.", 1);
}


nsIChannel * CNsIRequest::GetTheChannel(int i, nsILoadGroup *theLoadGroup)
{
	nsCAutoString theSpec, retURI;
	nsCOMPtr<nsIURI> theURI;
	nsCOMPtr<nsIChannel> theChannel;

	theSpec = ReqTable[i].theUrl;
	FormatAndPrintOutput("the input uri = ", theSpec, 1);

	rv = NS_NewURI(getter_AddRefs(theURI), theSpec);

	if (!theURI)
	{
	   QAOutput("We didn't get the URI. Test failed.", 1);
	   return nsnull;
	}
	else {
	   retURI = GetTheURI(theURI, 1);
	   // simple string compare to see if input & output URLs match
	   if (strcmp(ReqTable[i].theUrl, retURI.get()) == 0)
		  QAOutput("The in/out URIs MATCH.", 1);
	   else
		  QAOutput("The in/out URIs don't MATCH.", 1);
	   RvTestResult(rv, "NS_NewURI", 1);
	   RvTestResultDlg(rv, "NS_NewURI", true);
	}

	rv = NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, theLoadGroup);
	if (!theChannel)
	{
	   QAOutput("We didn't get the Channel. Test failed.", 1);
	   return nsnull;
	}
	else {
	   RvTestResult(rv, "NS_NewChannel", 1);
	   RvTestResultDlg(rv, "NS_NewChannel");
	}

	nsCOMPtr<nsIStreamListener> listener(NS_STATIC_CAST(nsIStreamListener*, qaBrowserImpl));
	nsCOMPtr<nsIWeakReference> thisListener(do_GetWeakReference(listener));
	qaWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsIStreamListener));

	if (!listener) {
	   QAOutput("We didn't get the listener for AsyncOpen(). Test failed.", 1);
	   return nsnull;
	}
	// this calls nsIStreamListener::OnDataAvailable()
	rv = theChannel->AsyncOpen(listener, nsnull);
	RvTestResult(rv, "AsyncOpen()", 1);
	RvTestResultDlg(rv, "AsyncOpen()");

	return theChannel;
}


void CNsIRequest::IsPendingReqTest(nsIRequest *request)
{
	PRBool	  reqPending;
	nsresult rv;  

	rv = request->IsPending(&reqPending);
    RvTestResult(rv, "nsIRequest::IsPending() rv test", 1);
	RvTestResultDlg(rv, "nsIRequest::IsPending() rv test()");

	if (!reqPending)
		QAOutput("Pending request = false.", 1);
	else
		QAOutput("Pending request = true.", 1);
}

void CNsIRequest::GetStatusReqTest(nsIRequest *request)
{
	nsresult	theStatusError;
	nsresult rv;

	rv = request->GetStatus(&theStatusError);
    RvTestResult(rv, "nsIRequest::GetStatus() test", 1);
	RvTestResultDlg(rv, "nsIRequest::GetStatus() test");
    RvTestResult(theStatusError, "the returned status error test", 1);
    RvTestResultDlg(theStatusError, "the returned status error test");
} 

void CNsIRequest::SuspendReqTest(nsIRequest *request)
{
	nsresult	rv;

	rv = request->Suspend();
    RvTestResult(rv, "nsIRequest::Suspend() test", 1);
    RvTestResultDlg(rv, "nsIRequest::Suspend() test");
}

void CNsIRequest::ResumeReqTest(nsIRequest *request)
{
	nsresult	rv;

	rv = request->Resume();
    RvTestResult(rv, "nsIRequest::Resume() test", 1);
    RvTestResultDlg(rv, "nsIRequest::Resume() test");
}

void CNsIRequest::CancelReqTest(nsIRequest *request)
{
	nsresult	rv;
	nsresult	status = NS_BINDING_ABORTED;

	rv = request->Cancel(status);
    RvTestResult(rv, "nsIRequest::Cancel() rv test", 1);
    RvTestResultDlg(rv, "nsIRequest::Cancel() test");
    RvTestResult(status, "nsIRequest::Cancel() status test", 1);
    RvTestResultDlg(status, "nsIRequest::Cancel() status test");
}

void CNsIRequest::SetLoadGroupTest(nsIRequest *request,
							  nsILoadGroup *theLoadGroup)
{
	nsresult	rv;
	nsCOMPtr<nsISimpleEnumerator> theSimpEnum;

	rv = request->SetLoadGroup(theLoadGroup);
    RvTestResult(rv, "nsIRequest::SetLoadGroup() rv test", 1);
    RvTestResultDlg(rv, "nsIRequest::SetLoadGroup() rv test");
}

void CNsIRequest::GetLoadGroupTest(nsIRequest *request)
{
	nsCOMPtr<nsILoadGroup> theLoadGroup;
	nsresult	rv;
	nsCOMPtr<nsISimpleEnumerator> theSimpEnum;

	rv = request->GetLoadGroup(getter_AddRefs(theLoadGroup));
    RvTestResult(rv, "nsIRequest::GetLoadGroup() rv test", 1);
    RvTestResultDlg(rv, "nsIRequest::GetLoadGroup() rv test");

	rv = theLoadGroup->GetRequests(getter_AddRefs(theSimpEnum));
    RvTestResult(rv, "nsIRequest:: LoadGroups' GetRequests() rv test", 1);
    RvTestResultDlg(rv, "nsIRequest:: LoadGroups' GetRequests() rv test");
}

