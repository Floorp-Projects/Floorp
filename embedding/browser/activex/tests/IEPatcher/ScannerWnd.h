#if !defined(AFX_SCANNERWND_H__2CAAFC05_6C47_11D2_A1F5_000000000000__INCLUDED_)
#define AFX_SCANNERWND_H__2CAAFC05_6C47_11D2_A1F5_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ScannerWnd.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CScannerWnd window

class CScannerWnd : public CWnd
{
// Construction
public:
	CScannerWnd();

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScannerWnd)
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CScannerWnd();

	// Generated message map functions
protected:
	//{{AFX_MSG(CScannerWnd)
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCANNERWND_H__2CAAFC05_6C47_11D2_A1F5_000000000000__INCLUDED_)
