// cbrowse.h : main header file for the CBROWSE application
//

#if !defined(AFX_CBROWSE_H__5121F5E4_5324_11D2_93E1_000000000000__INCLUDED_)
#define AFX_CBROWSE_H__5121F5E4_5324_11D2_93E1_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"		// main symbols
#include "Cbrowse_i.h"
#include "CBrowseDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CBrowseApp:
// See cbrowse.cpp for the implementation of this class
//

class CBrowseApp : public CWinApp
{
public:
	CBrowseApp();

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowseApp)
	public:
	virtual BOOL InitInstance();
		virtual int ExitInstance();
	virtual int Run();
	//}}AFX_VIRTUAL

// Implementation

	//{{AFX_MSG(CBrowseApp)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:

	CBrowseDlg *m_pDlg;
	BOOL m_bATLInited;
private:
	BOOL InitATL();

};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CBROWSE_H__5121F5E4_5324_11D2_93E1_000000000000__INCLUDED_)
