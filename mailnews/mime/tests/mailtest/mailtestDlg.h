// mailtestDlg.h : header file
//

#if !defined(AFX_MAILTESTDLG_H__00AF81D9_7405_11D2_B323_0020AF70F393__INCLUDED_)
#define AFX_MAILTESTDLG_H__00AF81D9_7405_11D2_B323_0020AF70F393__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

/////////////////////////////////////////////////////////////////////////////
// CMailtestDlg dialog

class CMailtestDlg : public CDialog
{
// Construction
public:
	CWnd			*m_pWndBrowser;
	int				m_tempFileCount;
	CString			m_tmpDirectory;
	CFile			m_tmpFilePointer;

	// Standard methods
	CMailtestDlg(CWnd* pParent = NULL);	// standard constructor

	// My custom Various methods
	void LoadURL(CString urlString);
	void CleanupMailbox();
	void ProcessMailbox(CString mBoxName);

// Dialog Data
	//{{AFX_DATA(CMailtestDlg)
	enum { IDD = IDD_MAILTEST_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMailtestDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	HICON m_hIcon;

	afx_msg void OnAbout();
  afx_msg void OnHelp();
	afx_msg void OnExit();
	afx_msg void OnOpen();

	// Generated message map functions
	//{{AFX_MSG(CMailtestDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnDblclkHeaders();
	afx_msg void OnClose();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLoadURL();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_MAILTESTDLG_H__00AF81D9_7405_11D2_B323_0020AF70F393__INCLUDED_)
