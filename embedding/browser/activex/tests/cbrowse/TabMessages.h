#if !defined(AFX_TABMESSAGES_H__33D0A350_12E1_11D3_9407_000000000000__INCLUDED_)
#define AFX_TABMESSAGES_H__33D0A350_12E1_11D3_9407_000000000000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// TabMessages.h : header file
//

class CBrowseDlg;

/////////////////////////////////////////////////////////////////////////////
// CTabMessages dialog

class CTabMessages : public CPropertyPage
{
	DECLARE_DYNCREATE(CTabMessages)

// Construction
public:
	CTabMessages();
	~CTabMessages();

	CBrowseDlg *m_pBrowseDlg;

// Dialog Data
	//{{AFX_DATA(CTabMessages)
	enum { IDD = IDD_TAB_MESSAGES };
	CProgressCtrl	m_pcProgress;
	CListBox	m_lbMessages;
	CString	m_szStatus;
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CTabMessages)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CTabMessages)
		// NOTE: the ClassWizard will add member functions here
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_TABMESSAGES_H__33D0A350_12E1_11D3_9407_000000000000__INCLUDED_)
