// nsIHttpChannelTests.cpp : implementation file
//

#include "stdafx.h"
#include "testembed.h"
#include "nsIHttpChannelTests.h"
#include "BrowserImpl.h"
#include "BrowserFrm.h"
#include "UrlDialog.h"
#include "ProfileMgr.h"
#include "ProfilesDlg.h"
#include "QaUtils.h"
#include "Tests.h"
#include "nsichanneltests.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// nsIHttpChannelTests

CnsIHttpChannelTests::CnsIHttpChannelTests(nsIWebBrowser *mWebBrowser,
								   CBrowserImpl *mpBrowserImpl)
{
	qaWebBrowser = mWebBrowser;
	qaBrowserImpl = mpBrowserImpl;
}

CnsIHttpChannelTests::~CnsIHttpChannelTests()
{
}


/////////////////////////////////////////////////////////////////////////////
// CnsIHttpChannelTests message handlers

nsIHttpChannel * CnsIHttpChannelTests::GetHttpChannelObject(nsCAutoString theSpec)
{
	NS_NewURI(getter_AddRefs(theURI), theSpec);
	if (!theURI) 	
	   QAOutput("Didn't get URI object. Test failed.", 2);
	NS_NewChannel(getter_AddRefs(theChannel), theURI, nsnull, nsnull);
	if (!theChannel)
	   QAOutput("Didn't get Channel object. GetChannelObject Test failed.", 2);
	theHttpChannel = do_QueryInterface(theChannel);
	if (!theHttpChannel)
	   QAOutput("Didn't get httpChannel object. Test failed.", 2);

	return theHttpChannel;
}

void CnsIHttpChannelTests::OnStartTests(UINT nMenuID)
{
	theSpec = "http://www.netscape.com";
	theHttpChannel = GetHttpChannelObject(theSpec);
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. Test not run.", 1);
	   return;
	}
	switch(nMenuID)
	{
		case ID_INTERFACES_NSIHTTPCHANNEL_RUNALLTESTS :
			RunAllTests();
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETREQUESTMETHOD :
			SetRequestMethodTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREQUESTMETHOD :
			GetRequestMethodTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETREFERRER :
			SetReferrerTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREFERRER :
			GetReferrerTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETREQUESTHEADER :
			SetRequestHeaderTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREQUESTHEADER :
			GetRequestHeaderTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_VISITREQUESTHEADERS :
			VisitRequestHeadersTest(theHttpChannel, 2);
			break;
	}
}

void CnsIHttpChannelTests::RunAllTests()
{
	theSpec = "http://www.netscape.com";
	theHttpChannel = GetHttpChannelObject(theSpec);
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. RunAllTests not run.", 1);
	   return;
	}

	SetRequestMethodTest(theHttpChannel, 1);
	GetRequestMethodTest(theHttpChannel, 1);
	SetReferrerTest(theHttpChannel, 1);
	GetReferrerTest(theHttpChannel, 1);
	SetRequestHeaderTest(theHttpChannel, 1);
	GetRequestHeaderTest(theHttpChannel, 1);
	VisitRequestHeadersTest(theHttpChannel, 1);

	QAOutput("\n");
}

void  CnsIHttpChannelTests::SetRequestMethodTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{ 
	// SetRequestMethod
	// try "GET", "PUT", "HEAD", or "POST"
	rv = theHttpChannel->SetRequestMethod(NS_LITERAL_CSTRING("POST"));
	RvTestResult(rv, "SetRequestMethod()", displayMode);
}

void  CnsIHttpChannelTests::GetRequestMethodTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// GetRequestMethod
	nsCAutoString method;
	rv = theHttpChannel->GetRequestMethod(method);
	RvTestResult(rv, "GetRequestMethod()", displayMode);
	// if (method.Equals("POST")
	FormatAndPrintOutput("GetRequestMethod method = ", method, displayMode);
}

void CnsIHttpChannelTests::SetReferrerTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// SetReferrer
	nsCAutoString theSpec;
	theSpec = "http://www.cisco.com";

	NS_NewURI(getter_AddRefs(theURI), theSpec);
	if (!theURI) 	
	   QAOutput("Didn't get URI object. Test failed.", 2);
	rv = theHttpChannel->SetReferrer(theURI);
	RvTestResult(rv, "SetReferrer()", displayMode);
}

void CnsIHttpChannelTests::GetReferrerTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// GetReferrer
	nsCAutoString theSpec;
	rv = theHttpChannel->GetReferrer(getter_AddRefs(theURI));
	RvTestResult(rv, "GetReferrer()", displayMode);
	if (!theURI)
	   QAOutput("Didn't get nsIURI object. Test failed.", displayMode);
	else {
	   theURI->GetSpec(theSpec);
	   FormatAndPrintOutput("GetReferrerTest uri = ", theSpec, displayMode);	   
	}
}

void  CnsIHttpChannelTests::SetRequestHeaderTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// SetRequestHeader
	// try {"Content-Type","text/xml"}, {"Content-Length", 10000}, and
	//     {"Accept"), NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
	const char *headerVal = "TheCookie";
    rv = theHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Cookie"),
                                         nsDependentCString(headerVal),
                                         PR_FALSE);
	RvTestResult(rv, "SetRequestHeader()", displayMode);
}

void  CnsIHttpChannelTests::GetRequestHeaderTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// GetRequestHeader
	nsCAutoString header;
	// could be set to "referrer"
	rv = theHttpChannel->GetRequestHeader(NS_LITERAL_CSTRING("Cookie"), header);
	RvTestResult(rv, "GetRequestHeader()", displayMode);
	FormatAndPrintOutput("GetRequestHeader type = ", header, displayMode);
}

void  CnsIHttpChannelTests::VisitRequestHeadersTest(nsIHttpChannel *theHttpChannel,
												    PRInt16 displayMode)
{
	// visitRequestHeaders()
	nsCOMPtr<nsIHttpHeaderVisitor> theVisitor;
	if (!theVisitor)
	   QAOutput("Didn't get nsIHttpHeaderVisitor object. Test failed.", displayMode);

	rv = theHttpChannel->VisitRequestHeaders(theVisitor);
	RvTestResult(rv, "VisitRequestHeaders()", displayMode);
}