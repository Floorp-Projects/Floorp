#if !defined(AFX_NSIHTTPCHANNELTESTS_H__A7985BC6_9A57_453F_BEE4_17A083131427__INCLUDED_)
#define AFX_NSIHTTPCHANNELTESTS_H__A7985BC6_9A57_453F_BEE4_17A083131427__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// nsIHttpChannelTests.h : header file
//

#include "Tests.h"

/////////////////////////////////////////////////////////////////////////////
// nsIHttpChannelTests window

class CnsIHttpChannelTests
{
// Construction
public:
	CnsIHttpChannelTests(nsIWebBrowser* mWebBrowser, CBrowserImpl *mpBrowserImpl);

// Attributes

	nsIHttpChannel * GetHttpChannelObject(nsCAutoString);
	void OnStartTests(UINT nMenuID);
	void RunAllTests();
	void SetRequestMethodTest(nsIHttpChannel *, PRInt16);
	void GetRequestMethodTest(nsIHttpChannel *, PRInt16);
	void SetReferrerTest(nsIHttpChannel *, PRInt16);
	void GetReferrerTest(nsIHttpChannel *, PRInt16);
	void SetRequestHeaderTest(nsIHttpChannel *, PRInt16);
	void GetRequestHeaderTest(nsIHttpChannel *, PRInt16);
	void VisitRequestHeadersTest(nsIHttpChannel *, PRInt16);
	void SetAllowPipeliningTest(nsIHttpChannel *, PRBool, PRInt16);
	void GetAllowPipeliningTest(nsIHttpChannel *, PRInt16);
	void SetRedirectionLimitTest(nsIHttpChannel *, PRUint32, PRInt16);
	void GetRedirectionLimitTest(nsIHttpChannel *, PRInt16);

	// response methods
	void CallResponseTests(nsIHttpChannel *, PRInt16);
	void GetResponseStatusTest(nsIHttpChannel *, PRInt16);
	void GetResponseStatusTextTest(nsIHttpChannel *, PRInt16);
	void GetRequestSucceededTest(nsIHttpChannel *, PRInt16);
	void GetResponseHeaderTest(nsIHttpChannel *, PRInt16);
	void SetResponseHeaderTest(nsIHttpChannel *, PRInt16);
	void IsNoStoreResponseTest(nsIHttpChannel *, PRInt16);
	void IsNoCacheResponseTest(nsIHttpChannel *, PRInt16);
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(nsIHttpChannelTests)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CnsIHttpChannelTests();

	// Generated message map functions
protected:

private:
	CBrowserImpl *qaBrowserImpl;
	nsCOMPtr<nsIWebBrowser> qaWebBrowser;
	nsCOMPtr<nsIChannel> theChannel;
	nsCOMPtr<nsIURI> theURI;
	nsCOMPtr<nsIHttpChannel> theHttpChannel;
	nsCAutoString theSpec;
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NSIHTTPCHANNELTESTS_H__A7985BC6_9A57_453F_BEE4_17A083131427__INCLUDED_)
