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

ChannelRow ChannelTable[] = {
	{"http://www.netscape.com/", "text/plain"},
	{"https://www.sun.com/",    "text/html"},
	{"ftp://ftp.mozilla.org/",  "image/gif"},
	{"gopher://gopher.tc.umn.edu/", "application/vnd.mozilla.xul+xml"},
	{"http://www.mozilla.org/projects/embedding", "text/plain"},
	{"file://C|/Program Files",  "image/jpeg"},
	{"about:mozilla",   ""},
	{"javascript:document.write('hi')", ""},
	{"data:text/plain;charset=iso-8859-7,%be%fg%be", ""},
	{"jar:resource:///chrome/comm.jar!/content/communicator/plugins.html",  ""},
}; 

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
	RvTestResult(rv, "NS_NewChannel", 1);
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
	RvTestResult(rv, "NS_NewURI", 1);
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
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. SetOriginalURITest failed.", 2);
	   return;
	}
	theURI = GetURIObject(theSpec);
	rv = theChannel->SetOriginalURI(theURI);
	RvTestResult(rv, "SetOriginalURITest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetOriginalURITest", true);
	if (!theURI)
	   QAOutput("Didn't get URI object. SetOriginalURITest failed.", displayMode);
}

void CnsIChannelTests::GetOriginalURITest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. GetOriginalURITest failed.", 2);
	   return;
	}
	rv = theChannel->GetOriginalURI(getter_AddRefs(theURI));
	RvTestResult(rv, "GetOriginalURITest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetOriginalURITest");
	if (!theURI)
	{
	   QAOutput("Didn't get URI object. GetOriginalURITest failed.", displayMode);
	   return;
	}
	GetTheURI(theURI);
}

void CnsIChannelTests::GetURITest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. GetURITest failed.", 2);
	   return;
	}
	rv = theChannel->GetURI(getter_AddRefs(theURI));
	RvTestResult(rv, "GetURITest", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetURITest");
	if (!theURI)
	{
	   QAOutput("Didn't get URI object. GetURITest failed.", displayMode);
	   return;
	}
	GetTheURI(theURI);
}

void CnsIChannelTests::SetOwnerTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. SetOwnerTest failed.", 2);
	   return;
	}
	theSupports = do_QueryInterface(qaWebBrowser);
	rv = theChannel->SetOwner(theChannel);
	RvTestResult(rv, "SetOwner", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetOwner");
	if (!theSupports)
	   QAOutput("Didn't get nsISupports object. SetOwnerTest failed.", displayMode);
}

void CnsIChannelTests::GetOwnerTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. GetOwnerTest failed.", 2);
	   return;
	}
	rv = theChannel->GetOwner(getter_AddRefs(theSupports));
	RvTestResult(rv, "GetOwner", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetOwner");
	if (!theSupports)
	   QAOutput("Didn't get nsISupports object. GetOwnerTest failed.", displayMode);
}

void CnsIChannelTests::SetNotificationsTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. SetNotificationsTest failed.", 2);
	   return;
	}
	if (!qaWebBrowser)
	{
	   QAOutput("Didn't get nsIWebBrowser object. SetNotificationsTest failed.", displayMode);
	   return;
	}
	theIRequestor = do_QueryInterface(qaWebBrowser);
	rv = theChannel->SetNotificationCallbacks(theIRequestor);
	RvTestResult(rv, "SetNotificationCallbacks", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "SetNotificationCallbacks");
	if (!theIRequestor)
	   QAOutput("Didn't get nsIInterfaceRequestor object. SetNotificationsTest failed.", displayMode);
}

void CnsIChannelTests::GetNotificationsTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get Channel object. GetNotificationsTest failed.", 2);
	   return;
	}
	rv = theChannel->GetNotificationCallbacks(getter_AddRefs(theIRequestor));
	RvTestResult(rv, "GetNotificationCallbacks", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "GetNotificationCallbacks");
	if(!theIRequestor)
	   QAOutput("Didn't get nsIInterfaceRequestor object. GetNotificationsTest failed.", displayMode);
}

void CnsIChannelTests::GetSecurityInfoTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. GetSecurityInfoTest failed.", displayMode);
	   return;
	}
	theSupports = do_QueryInterface(qaWebBrowser);
	rv = theChannel->GetSecurityInfo(getter_AddRefs(theSupports));	
	RvTestResult(rv, "GetSecurityInfo", displayMode);
	if (!theSupports)
	   QAOutput("Didn't get nsISupports object for GetSecurityInfoTest.", displayMode);
}

void CnsIChannelTests::SetContentTypeTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. SetContentTypeTest failed.", displayMode);
	   return;
	}
	rv = theChannel->SetContentType(NS_LITERAL_CSTRING("text/plain"));
	RvTestResult(rv, "SetContentType", displayMode);	
}

void CnsIChannelTests::GetContentTypeTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCAutoString contentType;

	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. GetContentTypeTest failed.", displayMode);
	   return;
	}
	rv = theChannel->GetContentType(contentType);
	RvTestResult(rv, "GetContentType", displayMode);
	FormatAndPrintOutput("the content type = ", contentType, displayMode);
}

void CnsIChannelTests::SetContentCharsetTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCAutoString charsetType;

	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. SetContentCharsetTest failed.", displayMode);
	   return;
	}
	rv = theChannel->SetContentCharset(NS_LITERAL_CSTRING("ISO-8859-1"));
	RvTestResult(rv, "SetContentCharset", displayMode);
}

void CnsIChannelTests::GetContentCharsetTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCAutoString charsetType;

	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. GetContentCharsetTest failed.", displayMode);
	   return;
	}
	rv = theChannel->GetContentCharset(charsetType);
	RvTestResult(rv, "GetContentCharset", displayMode);
	FormatAndPrintOutput("the charset type = ", charsetType, displayMode);
}

void CnsIChannelTests::SetContentLengthTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	PRInt32 contentLength;

	contentLength = 10000;

	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. SetContentLengthTest failed.", displayMode);
	   return;
	}
	rv = theChannel->SetContentLength(contentLength);
	RvTestResult(rv, "SetContentLength", displayMode);
}

void CnsIChannelTests::GetContentLengthTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	PRInt32 contentLength;

	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. GetContentLengthTest failed.", displayMode);
	   return;
	}
	rv = theChannel->GetContentLength(&contentLength);
	RvTestResult(rv, "GetContentLength", displayMode);
	FormatAndPrintOutput("the content length = ", contentLength, displayMode);
}

void CnsIChannelTests::OpenTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. OpenTest failed.", displayMode);
	   return;
	}
	rv =  theChannel->Open(getter_AddRefs(theInputStream));
	RvTestResult(rv, "OpenTest", displayMode);
	if (!theInputStream)
	   QAOutput("Didn't get theInputStream object. OpenTest failed.", displayMode);
}

void CnsIChannelTests::AsyncOpenTest(nsIChannel *theChannel, PRInt16 displayMode)
{
	nsCOMPtr<nsIStreamListener> listener(NS_STATIC_CAST(nsIStreamListener*, qaBrowserImpl));
	nsCOMPtr<nsIWeakReference> thisListener(do_GetWeakReference(listener));
	qaWebBrowser->AddWebBrowserListener(thisListener, NS_GET_IID(nsIStreamListener));
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. AsyncOpenTest failed.", displayMode);
	   return;
	}
	if (!listener) {
	   QAOutput("Didn't get the stream listener object. AsyncOpenTest failed.", displayMode);
	   return;
	}
	// this calls nsIStreamListener::OnDataAvailable()
	theSupports = do_QueryInterface(theChannel);
	if (!theSupports)
	   QAOutput("Didn't get the nsISupports object. AsyncOpen() failed.", displayMode);

	SaveObject(theSupports);

	rv = theChannel->AsyncOpen(listener, theSupports);
	RvTestResult(rv, "AsyncOpen()", displayMode);
	if (displayMode == 1)
		RvTestResultDlg(rv, "AsyncOpen()");
}

void CnsIChannelTests::PostAsyncOpenTests(nsIChannel *theChannel, PRInt16 displayMode)
{
	// These tests are run after the channel is opened (using AsyncOpen())
	// To run them in TestEmbed, select Tests > asyncOpen menu and enter complete URL with protocol
		GetOriginalURITest(theChannel, displayMode);
		GetURITest(theChannel, displayMode);
		SetOwnerTest(theChannel, displayMode);
		GetOwnerTest(theChannel, displayMode);
		SetNotificationsTest(theChannel, displayMode);
		GetNotificationsTest(theChannel, displayMode);
		GetSecurityInfoTest(theChannel, displayMode);
		SetContentTypeTest(theChannel, displayMode);
		GetContentTypeTest(theChannel, displayMode);
		SetContentCharsetTest(theChannel, displayMode);
		GetContentCharsetTest(theChannel, displayMode);
		SetContentLengthTest(theChannel, displayMode);
		GetContentLengthTest(theChannel, displayMode);
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
			AsyncOpenTest(theChannel, 1);
			GetSecurityInfoTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETCONTENTTYPE :
			AsyncOpenTest(theChannel, 1);
			SetContentTypeTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETCONTENTTYPE :
			AsyncOpenTest(theChannel, 1);
			GetContentTypeTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETCONTENTCHARSET  :
			AsyncOpenTest(theChannel, 1);
			SetContentCharsetTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETCONTENTCHARSET :
			AsyncOpenTest(theChannel, 1);
			GetContentCharsetTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_SETCONTENTLENGTH :
			AsyncOpenTest(theChannel, 1);
			SetContentLengthTest(theChannel, 2);
			break ;
		case ID_INTERFACES_NSICHANNEL_GETCONTENTLENGTH :
			AsyncOpenTest(theChannel, 1);
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
	int i;

	for (i=0; i<10; i++)
	{
		theSpec = ChannelTable[i].theURL;
		theChannel = GetChannelObject(theSpec);
		if (!theChannel)
		{
		   QAOutput("Didn't get nsIChannel object. RunAllTests not run.", 2);
		   return;
		}
		QAOutput("\nStart nsIChannel Tests: ");
		SetOriginalURITest(theChannel, theSpec, 1);
		GetOriginalURITest(theChannel, 1);
		GetURITest(theChannel, 1);
		SetOwnerTest(theChannel, 1);
		GetOwnerTest(theChannel, 1);
		SetNotificationsTest(theChannel, 1);
		GetNotificationsTest(theChannel, 1);
		AsyncOpenTest(theChannel, 1);
	 // PostAsyncOpenTests() called from nsIRequestObserver::OnStartRequest (in BrowserImpl.cpp)
		QAOutput("\n");
	}
}