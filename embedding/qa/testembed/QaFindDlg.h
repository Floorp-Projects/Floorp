#if !defined(AFX_QAFINDDLG_H__D27E3DAF_8479_11D6_9BC7_00C04FA02BE6__INCLUDED_)
#define AFX_QAFINDDLG_H__D27E3DAF_8479_11D6_9BC7_00C04FA02BE6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// QaFindDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// QaFindDlg dialog

class CQaFindDlg : public CDialog
{
// Construction
public:
	CQaFindDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CQaFindDlg)
	enum { IDD = IDD_QAFINDDLG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA
	CString	m_textfield;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CQaFindDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CQaFindDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_QAFINDDLG_H__D27E3DAF_8479_11D6_9BC7_00C04FA02BE6__INCLUDED_)
