// IEPatcher.h : main header file for the IEPATCHER application
//

#if !defined(AFX_IEPATCHER_H__A603167A_3B36_11D2_B44D_00600819607E__INCLUDED_)
#define AFX_IEPATCHER_H__A603167A_3B36_11D2_B44D_00600819607E__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols

#include "IEPatcherDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CIEPatcherApp:
// See IEPatcher.cpp for the implementation of this class
//

class CIEPatcherApp : public CWinApp
{
public:
	CIEPatcherApp();

	CIEPatcherDlg *m_pIEPatcherDlg;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CIEPatcherApp)
	public:
	virtual BOOL InitInstance();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CIEPatcherApp)
		// NOTE - the ClassWizard will add and remove member functions here.
		//    DO NOT EDIT what you see in these blocks of generated code !
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_IEPATCHER_H__A603167A_3B36_11D2_B44D_00600819607E__INCLUDED_)
