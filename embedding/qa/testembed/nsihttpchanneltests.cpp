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
#include "nsIHttpHeaderVisitor.h"

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

class HeaderVisitor : public nsIHttpHeaderVisitor
{
public:
   NS_DECL_ISUPPORTS
   NS_DECL_NSIHTTPHEADERVISITOR
 
   HeaderVisitor() { }
   virtual ~HeaderVisitor() {}
 };

NS_IMPL_ISUPPORTS1(HeaderVisitor, nsIHttpHeaderVisitor)

NS_IMETHODIMP
HeaderVisitor::VisitHeader(const nsACString &header, const nsACString &value)
{
   FormatAndPrintOutput("VisitHeader header = ", PromiseFlatCString(header).get(), 1);
   FormatAndPrintOutput("VisitHeader value = ", PromiseFlatCString(value).get(), 1);
return NS_OK;
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
			SetRequestMethodTest(theHttpChannel, "PUT", 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREQUESTMETHOD :
			GetRequestMethodTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETREFERRER :
			SetReferrerTest(theHttpChannel, "https://www.sun.com", 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREFERRER :
			GetReferrerTest(theHttpChannel, 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETREQUESTHEADER :
			SetRequestHeaderTest(theHttpChannel, "Content-Type", "text/html", 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREQUESTHEADER :
			GetRequestHeaderTest(theHttpChannel, "Content-Type", 2);
			break ;
		case ID_INTERFACES_NSIHTTPCHANNEL_VISITREQUESTHEADERS :
			VisitRequestHeadersTest(theHttpChannel, 2);
			break;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETALLOWPIPELINING :
			SetAllowPipeliningTest(theHttpChannel, PR_FALSE, 2);
			break;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETALLOWPIPELINING :
			GetAllowPipeliningTest(theHttpChannel, 2);
			break;
		case ID_INTERFACES_NSIHTTPCHANNEL_SETREDIRECTIONLIMIT :
			SetRedirectionLimitTest(theHttpChannel, 100, 2);
			break;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETREDIRECTIONLIMIT :
			GetRedirectionLimitTest(theHttpChannel, 2);
			break;

			// response methods

		case ID_INTERFACES_NSIHTTPCHANNEL_GETRESPONSESTATUS :
			GetResponseStatusTest(theHttpChannel, 2);
			break;
		case ID_INTERFACES_NSIHTTPCHANNEL_GETRESPONSESTATUSTEXT :
			GetResponseStatusTextTest(theHttpChannel, 2);
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
	QAOutput("   nsIHttpChannel request tests:");
	SetRequestMethodTest(theHttpChannel, "get", 1);
	GetRequestMethodTest(theHttpChannel, 1);
	SetRequestMethodTest(theHttpChannel, "PUT", 1);
	GetRequestMethodTest(theHttpChannel, 1);
	SetRequestMethodTest(theHttpChannel, "POST", 1);
	GetRequestMethodTest(theHttpChannel, 1);
	SetRequestMethodTest(theHttpChannel, "HEAD", 1);
	GetRequestMethodTest(theHttpChannel, 1);
	SetReferrerTest(theHttpChannel, "http://www.cisco.com", 1);
	GetReferrerTest(theHttpChannel, 1);
	SetRequestHeaderTest(theHttpChannel, "Cookie", "TheCookie", 1);
	GetRequestHeaderTest(theHttpChannel, "Cookie", 1);
	SetRequestHeaderTest(theHttpChannel, "Content-Type", "text/xml", 1);
	GetRequestHeaderTest(theHttpChannel, "Content-Type", 1);
	SetRequestHeaderTest(theHttpChannel, "Content-Length", "10000", 1);
	GetRequestHeaderTest(theHttpChannel, "Content-Length", 1);
	VisitRequestHeadersTest(theHttpChannel, 1);
	SetAllowPipeliningTest(theHttpChannel, PR_FALSE, 1);
	SetAllowPipeliningTest(theHttpChannel, PR_TRUE, 1);
	GetAllowPipeliningTest(theHttpChannel, 1);
	SetRedirectionLimitTest(theHttpChannel, 0, 1);
	SetRedirectionLimitTest(theHttpChannel, 5, 1);
	GetRedirectionLimitTest(theHttpChannel, 1);

	// see nsIRequestObserver->OnStartRequest for response tests
	CnsIChannelTests *channelObj = new CnsIChannelTests(qaWebBrowser, qaBrowserImpl);
	if (!channelObj)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetChannelObject will not be run.", 1);
	   return;
	}
	theChannel = channelObj->GetChannelObject(theSpec);
	if (!theChannel)
	{
	   QAOutput("Didn't get nsIChannel object. AsyncOpenTest will not be run.", 1);
	   return;
	}
	// this will open the channel and run the httpChannel response tests
	channelObj->AsyncOpenTest(theChannel, 1);
	QAOutput("\n");
}

void  CnsIHttpChannelTests::SetRequestMethodTest(nsIHttpChannel *theHttpChannel,
												 const char * requestType,
												 PRInt16 displayMode)
{ 
	// SetRequestMethod
	// try "GET", "PUT", "HEAD", or "POST"
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. SetRequestMethodTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->SetRequestMethod(nsDependentCString(requestType));
	RvTestResult(rv, "SetRequestMethod()", displayMode);
	RvTestResultDlg(rv, "SetRequestMethod() test", true);
}

void  CnsIHttpChannelTests::GetRequestMethodTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// GetRequestMethod
	nsCAutoString method;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetRequestMethodTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetRequestMethod(method);
	RvTestResult(rv, "GetRequestMethod()", displayMode);
	RvTestResultDlg(rv, "GetRequestMethod() test");
	// if (method.Equals("POST")
	FormatAndPrintOutput("GetRequestMethod method = ", method, displayMode);
}

void CnsIHttpChannelTests::SetReferrerTest(nsIHttpChannel *theHttpChannel,
												 const char *theUrl,
												 PRInt16 displayMode)
{
	// SetReferrer
	nsCAutoString theSpec;
	theSpec = theUrl;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. SetReferrerTest failed.", displayMode);
	   return;
	}
	NS_NewURI(getter_AddRefs(theURI), theSpec);
	if (!theURI) 	
	   QAOutput("Didn't get URI object. Test failed.", 2);
	rv = theHttpChannel->SetReferrer(theURI);
	RvTestResult(rv, "SetReferrer()", displayMode);
	RvTestResultDlg(rv, "SetReferrer() test");
}

void CnsIHttpChannelTests::GetReferrerTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	// GetReferrer
	nsCAutoString theSpec;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetReferrerTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetReferrer(getter_AddRefs(theURI));
	RvTestResult(rv, "GetReferrer()", displayMode);
	RvTestResultDlg(rv, "GetReferrer() test");
	if (!theURI)
	   QAOutput("Didn't get nsIURI object. Test failed.", displayMode);
	else {
	   theURI->GetSpec(theSpec);
	   FormatAndPrintOutput("GetReferrerTest uri = ", theSpec, displayMode);	   
	}
}

void  CnsIHttpChannelTests::SetRequestHeaderTest(nsIHttpChannel *theHttpChannel,
												 const char *requestType,
												 const char *headerVal,
												 PRInt16 displayMode)
{
	// SetRequestHeader
	// try {"Content-Type","text/xml"}, {"Content-Length", 10000}, and
	//     {"Accept"), NS_LITERAL_CSTRING("text/xml,application/xml,application/xhtml+xml,*/*;q=0.1"),
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. SetRequestHeaderTest failed.", displayMode);
	   return;
	}
    rv = theHttpChannel->SetRequestHeader(nsDependentCString(requestType),
                                         nsDependentCString(headerVal),
                                         PR_FALSE);
	RvTestResult(rv, "SetRequestHeader()", displayMode);
	RvTestResultDlg(rv, "SetRequestHeader() test");
}

void  CnsIHttpChannelTests::GetRequestHeaderTest(nsIHttpChannel *theHttpChannel,
												 const char *requestType,
												 PRInt16 displayMode)
{
	// GetRequestHeader
	nsCAutoString header;
	// could be set to "referrer"

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetRequestHeaderTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetRequestHeader(nsDependentCString(requestType), header);
	RvTestResult(rv, "GetRequestHeader()", displayMode);
	RvTestResultDlg(rv, "GetRequestHeader() test");
	FormatAndPrintOutput("GetRequestHeader type = ", header, displayMode);
}

void  CnsIHttpChannelTests::VisitRequestHeadersTest(nsIHttpChannel *theHttpChannel,
												    PRInt16 displayMode)
{
	// visitRequestHeaders()
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. VisitRequestHeadersTest failed.", displayMode);
	   return;
	}
	HeaderVisitor *theVisitor = new HeaderVisitor();
	if (!theVisitor)
	   QAOutput("Didn't get nsIHttpHeaderVisitor object. Test failed.", displayMode);

	rv = theHttpChannel->VisitRequestHeaders(theVisitor);
	RvTestResult(rv, "VisitRequestHeaders()", displayMode);
	RvTestResultDlg(rv, "VisitRequestHeaders() test");
}

void  CnsIHttpChannelTests::SetAllowPipeliningTest(nsIHttpChannel *theHttpChannel,
											   PRBool mAllowPipelining,
											   PRInt16 displayMode)
{
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. SetAllowPipeliningTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->SetAllowPipelining(mAllowPipelining);
	RvTestResult(rv, "SetAllowPipelining()", displayMode);
	RvTestResultDlg(rv, "SetAllowPipelining() test");
	FormatAndPrintOutput("SetAllowPipelining value = ", mAllowPipelining, displayMode);

	rv = theHttpChannel->GetAllowPipelining(&mAllowPipelining);
	FormatAndPrintOutput("GetAllowPipelining value = ", mAllowPipelining, displayMode);
}

void  CnsIHttpChannelTests::GetAllowPipeliningTest(nsIHttpChannel *theHttpChannel,
												   PRInt16 displayMode)
{
	PRBool mAllowPipelining;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetAllowPipeliningTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetAllowPipelining(&mAllowPipelining);
	RvTestResult(rv, "GetAllowPipelining()", displayMode);
	RvTestResultDlg(rv, "GetAllowPipelining() test");
	FormatAndPrintOutput("GetAllowPipelining value = ", mAllowPipelining, displayMode);
}

void  CnsIHttpChannelTests::SetRedirectionLimitTest(nsIHttpChannel *theHttpChannel,
												   PRUint32 mRedirectionLimit,
												   PRInt16 displayMode)
{
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. SetRedirectionLimitTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->SetRedirectionLimit(mRedirectionLimit);
	RvTestResult(rv, "SetRedirectionLimit()", displayMode);
	RvTestResultDlg(rv, "SetRedirectionLimit() test");
}

void  CnsIHttpChannelTests::GetRedirectionLimitTest(nsIHttpChannel *theHttpChannel,
												    PRInt16 displayMode)
{
	PRUint32 mRedirectionLimit;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetRedirectionLimitTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetRedirectionLimit(&mRedirectionLimit);
	RvTestResult(rv, "GetRedirectionLimit()", displayMode);
	RvTestResultDlg(rv, "GetRedirectionLimit() test");
	FormatAndPrintOutput("GetRedirectionLimit value = ", mRedirectionLimit, displayMode);
}

// Response tests
// called from OnStartRequest

void CnsIHttpChannelTests::CallResponseTests(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	GetResponseStatusTest(theHttpChannel, displayMode);
	GetResponseStatusTextTest(theHttpChannel, displayMode);
	GetRequestSucceededTest(theHttpChannel, displayMode);
	SetResponseHeaderTest(theHttpChannel, "Refresh", "1001", PR_TRUE, displayMode);
	GetResponseHeaderTest(theHttpChannel, "Refresh", displayMode);
	SetResponseHeaderTest(theHttpChannel, "Set-Cookie", "MyCookie", PR_TRUE, displayMode);
	GetResponseHeaderTest(theHttpChannel, "Set-Cookie", displayMode);
	SetResponseHeaderTest(theHttpChannel, "Cache-control", "no-cache", PR_FALSE, displayMode);
	GetResponseHeaderTest(theHttpChannel, "Cache-control", displayMode);
	VisitResponseHeaderTest(theHttpChannel, displayMode);
	IsNoStoreResponseTest(theHttpChannel, displayMode);
	IsNoCacheResponseTest(theHttpChannel, displayMode);
}

void CnsIHttpChannelTests::GetResponseStatusTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	PRUint32 mResponseStatus;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetResponseStatusTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetResponseStatus(&mResponseStatus);
	RvTestResult(rv, "GetResponseStatus()", displayMode);
	FormatAndPrintOutput("GetResponseStatus value = ", mResponseStatus, displayMode);
}

void CnsIHttpChannelTests::GetResponseStatusTextTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	nsCAutoString mText;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetResponseStatusTextTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetResponseStatusText(mText);
	RvTestResult(rv, "GetResponseStatusText()", displayMode);
	FormatAndPrintOutput("GetResponseStatusText = ", mText, displayMode);
}


void CnsIHttpChannelTests::GetRequestSucceededTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	PRBool mRequest;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetRequestSucceededTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetRequestSucceeded(&mRequest);
	RvTestResult(rv, "GetRequestSucceeded()", displayMode);
	FormatAndPrintOutput("GetRequestSucceeded = ", mRequest, displayMode);
}

void CnsIHttpChannelTests::GetResponseHeaderTest(nsIHttpChannel *theHttpChannel,
												 const char * responseType,
												 PRInt16 displayMode)
{
	nsCAutoString mResponse;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. GetResponseHeaderTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->GetResponseHeader(nsDependentCString(responseType), mResponse);
	RvTestResult(rv, "GetResponseHeader()", displayMode);
	FormatAndPrintOutput("GetResponseHeader = ", mResponse, displayMode);
	FormatAndPrintOutput("GetResponseHeader response type = ", responseType, displayMode);
}
 
void CnsIHttpChannelTests::SetResponseHeaderTest(nsIHttpChannel *theHttpChannel,
												 const char * responseType,
												 const char * value,
												 PRBool merge, PRInt16 displayMode)
{
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. SetResponseHeaderTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->SetResponseHeader(nsDependentCString(responseType),
                                           nsDependentCString(value), merge);
	RvTestResult(rv, "SetResponseHeader()", displayMode);
	FormatAndPrintOutput("SetResponseHeader value = ", value, displayMode);
	FormatAndPrintOutput("SetResponseHeader response type = ", responseType, displayMode);
}

void CnsIHttpChannelTests::VisitResponseHeaderTest(nsIHttpChannel *theHttpChannel,
												   PRInt16 displayMode)
{
	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. VisitResponseHeaderTest failed.", displayMode);
	   return;
	}
	// visitResponseHeaders()
	HeaderVisitor *theVisitor = new HeaderVisitor();
	if (!theVisitor)
	   QAOutput("Didn't get nsIHttpHeaderVisitor object. Test failed.", displayMode);

	rv = theHttpChannel->VisitResponseHeaders(theVisitor);
	RvTestResult(rv, "VisitResponseHeaders()", displayMode);
}

void CnsIHttpChannelTests::IsNoStoreResponseTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	PRBool mNoResponse;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. IsNoStoreResponseTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->IsNoStoreResponse(&mNoResponse);
	RvTestResult(rv, "IsNoStoreResponse()", displayMode);
	FormatAndPrintOutput("IsNoStoreResponse = ", mNoResponse, displayMode);
}

void CnsIHttpChannelTests::IsNoCacheResponseTest(nsIHttpChannel *theHttpChannel,
												 PRInt16 displayMode)
{
	PRBool mNoResponse;

	if (!theHttpChannel)
	{
	   QAOutput("Didn't get nsIHttpChannel object. IsNoCacheResponseTest failed.", displayMode);
	   return;
	}
	rv = theHttpChannel->IsNoCacheResponse(&mNoResponse);
	RvTestResult(rv, "IsNoCacheResponse()", displayMode);
	FormatAndPrintOutput("IsNoCacheResponse = ", mNoResponse, displayMode);
}