#if !defined(AFX_URLDIALOG_H__260C4EE1_2F8E_11D5_99F0_00C04FA02BE6__INCLUDED_)
#define AFX_URLDIALOG_H__260C4EE1_2F8E_11D5_99F0_00C04FA02BE6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// UrlDialog.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CUrlDialog dialog

class CUrlDialog : public CDialog
{
// Construction
public:
	CUrlDialog(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CUrlDialog)
	enum { IDD = IDD_URLDIALOG };
	CString	m_urlfield;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CUrlDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CUrlDialog)
	afx_msg void OnChangeUrlfield();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_URLDIALOG_H__260C4EE1_2F8E_11D5_99F0_00C04FA02BE6__INCLUDED_)
