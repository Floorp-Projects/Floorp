#if !defined(AFX_DLGFIND_H__1A85D3AB_C7B1_4F91_A9A9_6A1DEECD13E2__INCLUDED_)
#define AFX_DLGFIND_H__1A85D3AB_C7B1_4F91_A9A9_6A1DEECD13E2__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DlgFind.h : header file
//

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CDlgFind dialog

class CDlgFind : public CDialog
{
// Construction
public:
	CDlgFind(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDlgFind)
	enum { IDD = IDD_FIND };
	CString	m_strFind;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgFind)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDlgFind)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DLGFIND_H__1A85D3AB_C7B1_4F91_A9A9_6A1DEECD13E2__INCLUDED_)
