// RegMozCtlDlg.h : header file
//

#if !defined(AFX_REGMOZCTLDLG_H__C7C0A788_F424_11D2_A27B_000000000000__INCLUDED_)
#define AFX_REGMOZCTLDLG_H__C7C0A788_F424_11D2_A27B_000000000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "RegTaskManager.h"

/////////////////////////////////////////////////////////////////////////////
// CRegMozCtlDlg dialog

class CRegMozCtlDlg : public CDialog
{
// Construction
public:
	CRegMozCtlDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	//{{AFX_DATA(CRegMozCtlDlg)
	enum { IDD = IDD_REGMOZCTL_DIALOG };
	CListCtrl	m_cTaskList;
	CString	m_szMozillaDir;
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRegMozCtlDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;
    CRegTaskManager m_cTaskMgr;


	// Generated message map functions
	//{{AFX_MSG(CRegMozCtlDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnRegister();
	afx_msg void OnUnregister();
	afx_msg void OnPickDir();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

	void RegisterMozillaControl(BOOL bRegister);
	BOOL SetSystemPath(const CString &szNewPath);
	CString GetSystemPath();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_REGMOZCTLDLG_H__C7C0A788_F424_11D2_A27B_000000000000__INCLUDED_)
