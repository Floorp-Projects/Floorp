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

// nsIChannelTests.h : implementation file for nsIChannel test cases

// nsIChannelTests.cpp : implementation file
//

#include "stdafx.h"
#include "testembed.h"
#include "nsIChannelTests.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "QaUtils.h"
#include "Tests.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// nsIChannelTests

CnsIChannelTests::CnsIChannelTests(nsIWebBrowser *mWebBrowser,
								   CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser;
	qaBrowserImpl = mpBrowserImpl;
}

CnsIChannelTests::~CnsIChannelTests()
{
}


/////////////////////////////////////////////////////////////////////////////
// nsIChannelTests message handlers

nsIChannel * CnsIChannelTests::GetChannelObject(nsCAutoString theSpec)
{
	theURI = GetURIObject(theSpec);
	if (!theURI) 	
	{
	   QAOutput("Didn't get URI object. Test failed.", 2);
	   return nsnull;
	}
	rv = NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, nsnull);
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. GetChannelObject Test failed.", 2);
	   return nsnull;
	}
	return theChannel;
}

nsIURI * CnsIChannelTests::GetURIObject(nsCAutoString theSpec)
{
	rv = NS_NewURI(getter_AddRefs(theURI), theSpec);
	if (!theURI)
	{
	   QAOutput("Didn't get URI object. GetURIObject Test failed.", 2);
	   return nsnull;
	}
//	nsIURI *retVal = theURI;
//	NS_ADDREF(retVal);
//	return retVal;
	return theURI;
}

void CnsIChannelTests::SetOriginalURITest(nsIChannel *theChannel, nsCAutoString theSpec,
										  PRInt16 displayMode)
{
	theURI = GetURIObject(theSpec);
	if (!theURI)
	{
	   QAOutput("Didn't get URI object. SetOriginalURITest failed.", displayMode);
	   return;
	}
	rv = theChannel->SetOriginalURI(theURI);
	RvTestResult(rv, "SetOriginalURITest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetOriginalURITest", true);
}

void CnsIChannelTests::GetOriginalURITest(nsIChannel *theChannel, PRInt16 displayMode)
{
	rv = theChannel->GetOriginalURI(getter_AddRefs(theURI));
	if (!theURI)
	{
	   QAOutput("Didn't get URI object. GetOriginalURITest failed.", displayMode);
	   return;
	}
	RvTestResult(rv, "GetOriginalURITest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetOriginalURITest");
	GetTheUri(theURI);
}

void CnsIChannelTests::GetURITest(nsIChannel *theChannel, PRInt16 displayMode)
{
	rv = theChannel->GetURI(getter_AddRefs(theURI));
	if (!theURI)
	{
	   QAOutput("Didn't get URI object. GetURITest failed.", displayMode);
	   return;
	}
	RvTestResult(rv, "GetURITest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetURITest");
	GetTheUri(theURI);
}

void CnsIChannelTests::SetOwnerTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	theSupports = do_QueryInterface(theChannel);
	if (!theSupports)
	{
	   QAOutput("Didn't get nsISupports object. SetOwnerTest failed.", displayMode);
	   return;
	}
	theChannel->SetOwner(theSupports);
	RvTestResult(rv, "SetOwner", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetOwner");
}

void CnsIChannelTests::GetOwnerTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	rv = theChannel->GetOwner(getter_AddRefs(theSupports));
	if (!theSupports)
	{
	   QAOutput("Didn't get nsISupports object. GetOwnerTest failed.", displayMode);
	   return;
	}
	RvTestResult(rv, "GetOwner", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetOwner");
}

void CnsIChannelTests::SetNotificationsTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!qaWebBrowser)
	{
	   QAOutput("Didn't get nsIWebBrowser object. SetNotificationsTest failed.", displayMode);
	   return;
	}
	theIRequestor = do_QueryInterface(qaWebBrowser);
	if (!theIRequestor)
	{
	   QAOutput("Didn't get nsIInterfaceRequestor object. SetNotificationsTest failed.", displayMode);
	   return;
	}
	rv = theChannel->SetNotificationCallbacks(theIRequestor);
	RvTestResult(rv, "SetNotificationCallbacks", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetNotificationCallbacks");
}

void CnsIChannelTests::GetNotificationsTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	rv = theChannel->GetNotificationCallbacks(getter_AddRefs(theIRequestor));
	if (!theIRequestor)
	{
	   QAOutput("Didn't get nsIInterfaceRequestor object. GetNotificationsTest failed.", displayMode);
	   return;
	}
	RvTestResult(rv, "GetNotificationCallbacks", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetNotificationCallbacks");
}

void CnsIChannelTests::GetSecurityInfoTest(nsIChannel *theChannel, PRInt16 displayMode)
{
//	theSupports = do_QueryInterface(theChannel);
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. GetSecurityInfoTest failed.", displayMode);
	   return;
	}
	rv = theChannel->GetSecurityInfo(getter_AddRefs(theSupports));
	if (!theSupports)
	{
	   QAOutput("Didn't get nsISupports object. GetSecurityInfoTest failed.", displayMode);
	   return;
	}	
	RvTestResult(rv, "GetSecurityInfo", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetSecurityInfo");
}

void CnsIChannelTests::SetContentTypeTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	rv = theChannel->SetContentType(NS_LITERAL_CSTRING("text/html"));
	RvTestResult(rv, "SetContentType", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetContentType");	
}

void CnsIChannelTests::GetContentTypeTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCAutoString contentType;

	rv = theChannel->GetContentType(contentType);
	RvTestResult(rv, "GetContentType", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetContentType");
	FormatAndPrintOutput("the content type = ", contentType, displayMode);
}

void CnsIChannelTests::SetContentCharsetTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCAutoString charsetType;

	rv = theChannel->SetContentCharset(NS_LITERAL_CSTRING("ISO-8859-1"));
	RvTestResult(rv, "SetContentCharset", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetContentCharset");
}

void CnsIChannelTests::GetContentCharsetTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCAutoString charsetType;

	rv = theChannel->GetContentCharset(charsetType);
	RvTestResult(rv, "GetContentCharset", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetContentCharset");
	FormatAndPrintOutput("the charset type = ", charsetType, displayMode);
}

void CnsIChannelTests::SetContentLengthTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	PRInt32 contentLength;

	contentLength = 100;
	rv = theChannel->SetContentLength(contentLength);
	RvTestResult(rv, "SetContentLength", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetContentLength");
}

void CnsIChannelTests::GetContentLengthTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	PRInt32 contentLength;

	rv = theChannel->GetContentLength(&contentLength);
	RvTestResult(rv, "GetContentLength", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetContentLength");
	FormatAndPrintOutput("the content length = ", contentLength, displayMode);
}

void CnsIChannelTests::OpenTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	rv =  theChannel->Open(getter_AddRefs(theInputStream));
	if (!theInputStream)
	{
	   QAOutput("Didn't get theInputStream object. OpenTest failed.", displayMode);
	   return;
	}
	RvTestResult(rv, "OpenTest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "OpenTest");
}

void CnsIChannelTests::AsyncOpenTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCOMPtr<nsIStreamListener> listener(NS_STATIC_CAST(nsIStreamListener*, qaBrowserImpl));
	nsCOMPtr<nsIWeakReference> thisListener(dont_AddRef(NS_GetWeakReference(listener)));
	qaWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsIStreamListener));

	if (!listener) {
	   QAOutput("Didn't get the stream listener object. AsyncOpenTest failed.", displayMode);
	   return;
	}
	// this calls nsIStreamListener::OnDataAvailable()
	rv = theChannel->AsyncOpen(listener, nsnull);
	RvTestResult(rv, "AsyncOpen()", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "AsyncOpen()");
}


void CnsIChannelTests::OnStartTests(UINT nMenuID)
{
	theSpec = "http://www.netscape.com";
	theChannel = GetChannelObject(theSpec);
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. Test not run.", 1);
	   return;
	}
	switch(nMenuID)
	{
		case ID_INTERFACES_NSICHANNEL_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSICHANNEL_SETORIGINALURI :
			SetOriginalURITest(theChannel, theSpec, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETORIGINALURI :
			GetOriginalURITest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETURI  :
			GetURITest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETOWNER :
			SetOwnerTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETOWNER :
			GetOwnerTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETNOTIFICATIONS :
			SetNotificationsTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETNOTIFICATIONS  :
			GetNotificationsTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETSECURITYINFO  :
			GetSecurityInfoTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETCONTENTTYPE :
			SetContentTypeTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETCONTENTTYPE :
			GetContentTypeTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETCONTENTCHARSET  :
			SetContentCharsetTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETCONTENTCHARSET :
			GetContentCharsetTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETCONTENTLENGTH :
			SetContentLengthTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETCONTENTLENGTH :
			GetContentLengthTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_OPEN  :
			OpenTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_ASYNCOPEN  :
			AsyncOpenTest(theChannel, 2);
			break ;
	}
}

void CnsIChannelTests::RunAllTests()
{
	theSpec = "http://www.netscape.com";
	theChannel = GetChannelObject(theSpec);
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. RunAllTests not run.", 2);
	   return;
	}

	SetOriginalURITest(theChannel, theSpec, 1);
	GetOriginalURITest(theChannel, 1);
	GetURITest(theChannel, 1);
	SetOwnerTest(theChannel, 1);
	GetOwnerTest(theChannel, 1);
	SetNotificationsTest(theChannel, 1);
	GetNotificationsTest(theChannel, 1);
	GetSecurityInfoTest(theChannel, 1);
	SetContentTypeTest(theChannel, 1);
	GetContentTypeTest(theChannel, 1);
	SetContentCharsetTest(theChannel, 1);
	GetContentCharsetTest(theChannel, 1);
	SetContentLengthTest(theChannel, 1);
	GetContentLengthTest(theChannel, 1);
	OpenTest(theChannel, 1);
	AsyncOpenTest(theChannel, 1);
}