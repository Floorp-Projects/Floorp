#if !defined(AFX_DLGADD_H__55B7BFE0_C8B7_43FA_AFD2_BCB1AA18E820__INCLUDED_)
#define AFX_DLGADD_H__55B7BFE0_C8B7_43FA_AFD2_BCB1AA18E820__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DlgAdd.h : header file
//

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CDlgAdd dialog

class CDlgAdd : public CDialog
{
// Construction
public:
	CDlgAdd(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CDlgAdd)
	enum { IDD = IDD_ADDPREF };
	CString	m_strPrefDesc;
	CString	m_strPrefName;
	int		m_intPrefType;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgAdd)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDlgAdd)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DLGADD_H__55B7BFE0_C8B7_43FA_AFD2_BCB1AA18E820__INCLUDED_)
