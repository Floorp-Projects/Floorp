// RegMozCtl.h : main header file for the REGMOZCTL application
//

#if !defined(AFX_REGMOZCTL_H__C7C0A786_F424_11D2_A27B_000000000000__INCLUDED_)
#define AFX_REGMOZCTL_H__C7C0A786_F424_11D2_A27B_000000000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

/////////////////////////////////////////////////////////////////////////////
// CRegMozCtlApp:
// See RegMozCtl.cpp for the implementation of this class
//

class CRegMozCtlApp : public CWinApp
{
public:
	CRegMozCtlApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRegMozCtlApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CRegMozCtlApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REGMOZCTL_H__C7C0A786_F424_11D2_A27B_000000000000__INCLUDED_)
