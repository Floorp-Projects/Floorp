// mailtest.h : main header file for the MAILTEST application
//

#if !defined(AFX_MAILTEST_H__00AF81D7_7405_11D2_B323_0020AF70F393__INCLUDED_)
#define AFX_MAILTEST_H__00AF81D7_7405_11D2_B323_0020AF70F393__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CMailtestApp:
// See mailtest.cpp for the implementation of this class
//

class CMailtestApp : public CWinApp
{
public:
	CMailtestApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMailtestApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CMailtestApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAILTEST_H__00AF81D7_7405_11D2_B323_0020AF70F393__INCLUDED_)
