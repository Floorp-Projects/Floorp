#if !defined(AFX_PROFILESDLG_H__48358887_EBFA_11D4_9905_00B0D0235410__INCLUDED_)
#define AFX_PROFILESDLG_H__48358887_EBFA_11D4_9905_00B0D0235410__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// ProfilesDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CNewProfileDlg dialog

class CNewProfileDlg : public CDialog
{
// Construction
public:
	CNewProfileDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CNewProfileDlg)
	enum { IDD = IDD_PROFILE_NEW };
	int		m_LocaleIndex;
	CString	m_Name;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CNewProfileDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CNewProfileDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CRenameProfileDlg dialog

class CRenameProfileDlg : public CDialog
{
// Construction
public:
	CRenameProfileDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CRenameProfileDlg)
	enum { IDD = IDD_PROFILE_RENAME };
	CString	m_NewName;
	//}}AFX_DATA

    CString     m_CurrentName;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CRenameProfileDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CRenameProfileDlg)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////
// CProfilesDlg dialog

class CProfilesDlg : public CDialog
{
// Construction
public:
	CProfilesDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CProfilesDlg)
	enum { IDD = IDD_PROFILES };
	CListBox	m_ProfileList;
    BOOL        m_bAtStartUp;
	BOOL	    m_bAskAtStartUp;
	//}}AFX_DATA

    nsAutoString m_SelectedProfile;

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CProfilesDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CProfilesDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnNewProfile();
	afx_msg void OnRenameProfile();
	afx_msg void OnDeleteProfile();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PROFILESDLG_H__48358887_EBFA_11D4_9905_00B0D0235410__INCLUDED_)
