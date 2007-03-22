#if !defined(AFX_SCANNERTHREAD_H__2CAAFC04_6C47_11D2_A1F5_000000000000__INCLUDED_)
#define AFX_SCANNERTHREAD_H__2CAAFC04_6C47_11D2_A1F5_000000000000__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// ScannerThread.h : header file
//

#include "ScannerWnd.h"


/////////////////////////////////////////////////////////////////////////////
// CScannerThread thread

class CScannerThread : public CWinThread
{
	DECLARE_DYNCREATE(CScannerThread)
protected:
	CScannerThread();           // protected constructor used by dynamic creation

// Attributes
public:
	CScannerWnd m_cScannerWnd;

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CScannerThread)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	//}}AFX_VIRTUAL

// Implementation
protected:
	virtual ~CScannerThread();

	// Generated message map functions
	//{{AFX_MSG(CScannerThread)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SCANNERTHREAD_H__2CAAFC04_6C47_11D2_A1F5_000000000000__INCLUDED_)
