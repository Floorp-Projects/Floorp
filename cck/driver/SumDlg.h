#if !defined(AFX_SUMDLG_H__A7E38442_03F0_11D3_B1F5_006008A6BBCE__INCLUDED_)
#define AFX_SUMDLG_H__A7E38442_03F0_11D3_B1F5_006008A6BBCE__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// SumDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSumDlg dialog

class CSumDlg : public CDialog
{
// Construction
public:
	CSumDlg(CWnd* pParent = NULL);   // standard constructor

// Dialog Data
	//{{AFX_DATA(CSumDlg)
	enum { IDD = IDD_SUMMARY };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSumDlg)
	public:
	virtual int DoModal();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CSumDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SUMDLG_H__A7E38442_03F0_11D3_B1F5_006008A6BBCE__INCLUDED_)
