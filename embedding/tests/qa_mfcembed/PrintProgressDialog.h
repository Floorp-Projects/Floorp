#if !defined(AFX_PRINTPROGRESSDIALOG_H__1BAF9B13_1875_11D5_9773_000064657374__INCLUDED_)
#define AFX_PRINTPROGRESSDIALOG_H__1BAF9B13_1875_11D5_9773_000064657374__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PrintProgressDialog.h : header file
//

#include "nsIPrintListener.h"
class nsIWebBrowser;
class nsIDOMWindow;
class nsIPrintListener;

/////////////////////////////////////////////////////////////////////////////
// CPrintProgressDialog dialog

class CPrintProgressDialog : public CDialog
{
// Construction
public:
	CPrintProgressDialog(nsIWebBrowser* aWebBrowser, 
                       nsIDOMWindow* aDOMWin,
                       CWnd* pParent = NULL);
	virtual ~CPrintProgressDialog();
  virtual int DoModal( );

  // Helper
  void SetURI(const char* aTitle);

  // same as nsIPrintListener
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
	CProgressCtrl              m_wndProgress;
  CString                    m_URL;
  nsIWebBrowser*             m_WebBrowser;
  nsIDOMWindow*              m_DOMWin;
  nsCOMPtr<nsIPrintListener> m_PrintListener;
  BOOL                       m_InModalMode;

	// Generated message map functions
	//{{AFX_MSG(CPrintProgressDialog)
	virtual BOOL OnInitDialog();
	virtual void OnCancel();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PRINTPROGRESSDIALOG_H__1BAF9B13_1875_11D5_9773_000064657374__INCLUDED_)
