#if !defined(AFX_DLGEDITPREFSTR_H__EA62EAF1_9CF9_474F_AC2E_7AE66DCAF863__INCLUDED_)
#define AFX_DLGEDITPREFSTR_H__EA62EAF1_9CF9_474F_AC2E_7AE66DCAF863__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// DlgEditPrefStr.h : header file
//

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CDlgEditPrefStr dialog

class CDlgEditPrefStr : public CDialog
{
// Construction
public:
	CString m_strSelectedChoice;
	CString* m_pstrChoices;
	CString m_strType;
	CString m_strTitle;
	CDlgEditPrefStr(CWnd* pParent = NULL);   // standard constructor
  BOOL m_bChoose;

// Dialog Data
	//{{AFX_DATA(CDlgEditPrefStr)
	enum { IDD = IDD_EDITPREF };
	CComboBox	m_listValue;
	CButton	m_checkValue;
	CButton m_checkLocked;
	CButton m_checkManage;
	CEdit	m_editValue;
	CString	m_strDescription;
	CString	m_strPrefName;
	CString	m_strValue;
	BOOL	m_bLocked;
	BOOL	m_bValue;
	BOOL  m_bManage;
  BOOL  m_bLockable;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CDlgEditPrefStr)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CDlgEditPrefStr)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnManage();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	void EnableControls(BOOL bEnable);
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_DLGEDITPREFSTR_H__EA62EAF1_9CF9_474F_AC2E_7AE66DCAF863__INCLUDED_)
