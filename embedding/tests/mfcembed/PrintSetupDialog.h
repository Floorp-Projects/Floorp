#if !defined(AFX_PRINTSETUPDIALOG_H__ABEFEDA3_8AFA_4D77_9E89_646E9E5DD93C__INCLUDED_)
#define AFX_PRINTSETUPDIALOG_H__ABEFEDA3_8AFA_4D77_9E89_646E9E5DD93C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PrintSetupDialog.h : header file
//
class nsIPrintSettings;

/////////////////////////////////////////////////////////////////////////////
// CPrintSetupDialog dialog

class CPrintSetupDialog : public CDialog
{
// Construction
public:
	CPrintSetupDialog(nsIPrintSettings* aPrintSettings, CWnd* pParent = NULL);   // standard constructor
  void SetPrintSettings(nsIPrintSettings* aPrintSettings);

// Dialog Data
	//{{AFX_DATA(CPrintSetupDialog)
	enum { IDD = IDD_PRINTSETUP_DIALOG };
	CString	m_BottomMargin;
	CString	m_LeftMargin;
	CString	m_RightMargin;
	CString	m_TopMargin;
	int		m_Scaling;
	BOOL	m_PrintBGImages;
	BOOL	m_PrintBGColors;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrintSetupDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
  nsIPrintSettings* m_PrintSettings;

	// Generated message map functions
	//{{AFX_MSG(CPrintSetupDialog)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnCustomdrawScale(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnKillfocusScaleTxt();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PRINTSETUPDIALOG_H__ABEFEDA3_8AFA_4D77_9E89_646E9E5DD93C__INCLUDED_)
