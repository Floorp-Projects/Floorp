#if !defined(AFX_WEBPROGDLG_H__79FFB433_9E6A_11D6_9BD9_00C04FA02BE6__INCLUDED_)
#define AFX_WEBPROGDLG_H__79FFB433_9E6A_11D6_9BD9_00C04FA02BE6__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// WebProgDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CWebProgDlg dialog

class CWebProgDlg : public CDialog
{
// Construction
public:
	CWebProgDlg(CWnd* pParent = NULL);   // standard constructor
	unsigned long m_wpFlagValue;
// Dialog Data
	//{{AFX_DATA(CWebProgDlg)
	enum { IDD = IDD_WEBPROGDLG };
	CComboBox	m_webProgFlags;
	int			m_wpFlagIndex;
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWebProgDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CWebProgDlg)
	virtual BOOL OnInitWPDialog();
	afx_msg	void OnSelectWPCombo();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WEBPROGDLG_H__79FFB433_9E6A_11D6_9BD9_00C04FA02BE6__INCLUDED_)
