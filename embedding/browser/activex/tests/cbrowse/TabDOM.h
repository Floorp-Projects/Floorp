#if !defined(AFX_TABDOM_H__5216DE08_12D4_11D3_9407_000000000000__INCLUDED_)
#define AFX_TABDOM_H__5216DE08_12D4_11D3_9407_000000000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TabDOM.h : header file
//

class CBrowseDlg;

/////////////////////////////////////////////////////////////////////////////
// CTabDOM dialog

class CTabDOM : public CPropertyPage
{
	DECLARE_DYNCREATE(CTabDOM)

// Construction
public:
	CTabDOM();
	~CTabDOM();

	CBrowseDlg *m_pBrowseDlg;

// Dialog Data
	//{{AFX_DATA(CTabDOM)
	enum { IDD = IDD_TAB_DOM };
	CTreeCtrl	m_tcDOM;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CTabDOM)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CTabDOM)
	afx_msg void OnRefreshDOM();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TABDOM_H__5216DE08_12D4_11D3_9407_000000000000__INCLUDED_)
