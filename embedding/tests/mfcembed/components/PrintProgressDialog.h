#if !defined(AFX_PRINTPROGRESSDIALOG_H__1E3AA1B5_B8BB_4B25_86A5_A90E663D137F__INCLUDED_)
#define AFX_PRINTPROGRESSDIALOG_H__1E3AA1B5_B8BB_4B25_86A5_A90E663D137F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PrintProgressDialog.h : header file
//

#include "resource.h"
#include "nsIWebProgressListener.h"
#include "nsIWebBrowserPrint.h"
#include "nsIPrintSettings.h"
#include "nsIPrintProgressParams.h"
#include "nsCOMPtr.h"

/////////////////////////////////////////////////////////////////////////////
// CPrintProgressDialog dialog

class CPrintProgressDialog : public CDialog
{
// Construction
public:
	CPrintProgressDialog(CWnd*                   pParent,
                       BOOL                    aIsForPrinting,
                       nsIPrintProgressParams* aPPParams,
                       nsIWebBrowserPrint*     aWebBrowserPrint, 
                       nsIPrintSettings*       aPrintSettings);
	virtual ~CPrintProgressDialog();

  // Helper
  void SetDocAndURL();

  NS_IMETHOD OnStartPrinting(void);
  NS_IMETHOD OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnEndPrinting(PRUint32 aStatus);

// Dialog Data
	//{{AFX_DATA(CPrintProgressDialog)
	enum { IDD = IDD_PRINT_PROGRESS_DIALOG };
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPrintProgressDialog)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	CProgressCtrl                    m_wndProgress;
  CString                          m_URL;
  nsCOMPtr<nsIWebBrowserPrint>     m_WebBrowserPrint;
  nsCOMPtr<nsIPrintProgressParams> m_PPParams;
  nsCOMPtr<nsIWebProgressListener> m_PrintListener;
  nsCOMPtr<nsIPrintSettings>       m_PrintSettings;
  BOOL                             m_HasStarted;
  BOOL                             m_IsForPrinting;

	// Generated message map functions
	//{{AFX_MSG(CPrintProgressDialog)
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PRINTPROGRESSDIALOG_H__1E3AA1B5_B8BB_4B25_86A5_A90E663D137F__INCLUDED_)
