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
 *   Ashish Bhatt <ashishbhatt@netscape.com> 
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

/*CNsIRequest::CNsIRequest(nsIWebBrowser* mWebBrowser,
			   nsIBaseWindow* mBaseWindow,
			   nsIWebNavigation* mWebNav,
			   CBrowserImpl *mpBrowserImpl):CTests(mWebBrowser,mBaseWindow,mWebNav,mpBrowserImpl)
{

}*/


CNsIRequest::CNsIRequest(nsIWebBrowser* mWebBrowser,CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser ;
	qaBrowserImpl = mpBrowserImpl ;
}


CNsIRequest::~CNsIRequest()
{
}


/*BEGIN_MESSAGE_MAP(CNsIRequest, CTests)
	//{{AFX_MSG_MAP(CNsIRequest)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()*/


/////////////////////////////////////////////////////////////////////////////
// CNsIRequest message handlers
// ***********************************************************************
// ***********************************************************************
//  nsIRequest iface

//  table columns corrsp to: pending, status, suspend, resume, cancel,
//  setLoadGroup & getLoadGroup tests respectively.

Element UrlTable_Temp[] = {
	{"http://www.netscape.com", 1, 1, 0, 0, 0, 1, 1},
	{"http://www.yahoo.com",    0, 0, 1, 1, 0, 0, 0},
	{"http://www.cisco.com",    0, 0, 0, 0, 1, 0, 0},
	{"http://www.sun.com",      0, 0, 0, 0, 0, 1, 1},
	{"http://www.intel.com",    1, 1, 1, 0, 0, 0, 0},
	{"http://www.aol.com",      0, 1, 0, 0, 0, 1, 1}
}; 

void CNsIRequest::OnInterfacesNsirequest() 
{
	// note: nsIRequest tests are called:
	// 1) in BrowserImpl.cpp, nsIStreamListener::OnDataAvailable()
	// 2) as individual tests below

	nsCString theSpec;
	nsCOMPtr<nsIURI> theURI;
	nsCOMPtr<nsIChannel> theChannel;
	nsCOMPtr<nsILoadGroup> theLoadGroup(do_CreateInstance(NS_LOADGROUP_CONTRACTID));

	int i=0;

    QAOutput("Start nsIRequest tests.", 2);	

//	theSpec = "http://www.netscape.com";
	for (i=0; i<6; i++)
	{
		theSpec = UrlTable_Temp[i].theUrl;
		FormatAndPrintOutput("the uri spec = ", theSpec, 2);

		rv = NS_NewURI(getter_AddRefs(theURI), theSpec);
		if (!theURI)
		{
		   QAOutput("We didn't get the URI. Test failed.", 1);
		   return;
		}
		else
		   RvTestResult(rv, "NS_NewURI", 1);

		rv = NS_OpenURI(getter_AddRefs(theChannel), theURI, nsnull, theLoadGroup);
		if (!theChannel)
		{
		   QAOutput("We didn't get the Channel. Test failed.", 1);
		   return;
		}
		else if (!theLoadGroup)
		{
		   QAOutput("We didn't get the Load Group. Test failed.", 2);
		   return;
		}
		else
		   RvTestResult(rv, "NS_OpenURI", 1);

		nsCOMPtr<nsIStreamListener> listener(NS_STATIC_CAST(nsIStreamListener*, qaBrowserImpl));
		nsCOMPtr<nsIWeakReference> thisListener(dont_AddRef(NS_GetWeakReference(listener)));
		qaWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsIStreamListener));

		// this calls nsIStreamListener::OnDataAvailable()
		rv = theChannel->AsyncOpen(listener, nsnull);
		RvTestResult(rv, "AsyncOpen()", 1);

		// nsIRequest individual tests

		QAOutput("***** Individual nsIRequest test begins. *****");

		nsCOMPtr<nsIRequest> theRequest = do_QueryInterface(theChannel);

		if (UrlTable_Temp[i].reqPend == TRUE)
			IsPendingReqTest(theRequest);

		if (UrlTable_Temp[i].reqStatus == TRUE)
			GetStatusReqTest(theRequest);

		if (UrlTable_Temp[i].reqSuspend == TRUE)
			SuspendReqTest(theRequest);	

		if (UrlTable_Temp[i].reqResume == TRUE)
			ResumeReqTest(theRequest);	

		if (UrlTable_Temp[i].reqCancel == TRUE)
			CancelReqTest(theRequest);	

		if (UrlTable_Temp[i].reqSetLoadGroup == TRUE)
			SetLoadGroupTest(theRequest, theLoadGroup);	

		if (UrlTable_Temp[i].reqGetLoadGroup == TRUE)
			GetLoadGroupTest(theRequest);

		QAOutput("- - - - - - - - - - - - - - - - - - - - -", 1);
	} // end for loop
    QAOutput("End nsIRequest tests.", 2);
}

void CNsIRequest::IsPendingReqTest(nsIRequest *request)
{
	PRBool	  reqPending;
	nsresult rv;  

	rv = request->IsPending(&reqPending);
    RvTestResult(rv, "nsIRequest::IsPending() rv test", 1);

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
    RvTestResult(rv, "the returned status error test", 1);

} 

void CNsIRequest::SuspendReqTest(nsIRequest *request)
{
	nsresult	rv;

	rv = request->Suspend();
    RvTestResult(rv, "nsIRequest::Suspend() test", 1);
}

void CNsIRequest::ResumeReqTest(nsIRequest *request)
{
	nsresult	rv;

	rv = request->Resume();
    RvTestResult(rv, "nsIRequest::Resume() test", 1);
}

void CNsIRequest::CancelReqTest(nsIRequest *request)
{
	nsresult	rv;
	nsresult	status = NS_BINDING_ABORTED;

	rv = request->Cancel(status);
    RvTestResult(rv, "nsIRequest::Cancel() rv test", 1);
    RvTestResult(status, "nsIRequest::Cancel() status test", 1);
}

void CNsIRequest::SetLoadGroupTest(nsIRequest *request,
							  nsILoadGroup *theLoadGroup)
{
	nsresult	rv;
	nsCOMPtr<nsISimpleEnumerator> theSimpEnum;

	rv = request->SetLoadGroup(theLoadGroup);
    RvTestResult(rv, "nsIRequest::SetLoadGroup() rv test", 1);
}

void CNsIRequest::GetLoadGroupTest(nsIRequest *request)
{
	nsCOMPtr<nsILoadGroup> theLoadGroup;
	nsresult	rv;
	nsCOMPtr<nsISimpleEnumerator> theSimpEnum;

	rv = request->GetLoadGroup(getter_AddRefs(theLoadGroup));
    RvTestResult(rv, "nsIRequest::GetLoadGroup() rv test", 1);

	rv = theLoadGroup->GetRequests(getter_AddRefs(theSimpEnum));
    RvTestResult(rv, "nsIRequest:: LoadGroups' GetRequests() rv test", 1);
}

