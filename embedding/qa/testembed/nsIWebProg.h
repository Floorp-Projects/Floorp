#if !defined(AFX_NSIWEBPROG_H__8024AB8F_6DB3_11D6_9BA5_00C04FA02BE6__INCLUDED_)
#define AFX_NSIWEBPROG_H__8024AB8F_6DB3_11D6_9BA5_00C04FA02BE6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// nsiWebProg.h : header file
//

#include "BrowserView.h"
#include "BrowserImpl.h"
#include "StdAfx.h"
#include "Tests.h"

/////////////////////////////////////////////////////////////////////////////
// CnsiWebProg window

class CBrowserImpl;

class CnsiWebProg
{
public:
	nsCOMPtr<nsIWebBrowser> qaWebBrowser;
	CBrowserImpl	*qaBrowserImpl;
	// Construction
	CnsiWebProg(nsIWebBrowser *mWebBrowser,
				   CBrowserImpl *mpBrowserImpl);
	nsIWebProgress * GetWebProgObject();
	void AddWebProgLstnr(PRUint32);
	void RemoveWebProgLstnr(void);
	void GetTheDOMWindow(void);

	void ConvertWPFlagToString(PRUint32, nsCAutoString&);
	void StoreWebProgFlag(PRUint32);
	void RetrieveWebProgFlag();

	void OnStartTests(UINT nMenuID);
	void RunAllTests(void);

	PRUint32 theStoredFlag;

public:
// Attributes

// Operations
public:


// Implementation
public:
	virtual ~CnsiWebProg();

	// Generated message map functions
protected:

};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_NSIWEBPROG_H__8024AB8F_6DB3_11D6_9BA5_00C04FA02BE6__INCLUDED_)
