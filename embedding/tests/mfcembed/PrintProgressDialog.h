/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined(AFX_PRINTPROGRESSDIALOG_H__1BAF9B13_1875_11D5_9773_000064657374__INCLUDED_)
#define AFX_PRINTPROGRESSDIALOG_H__1BAF9B13_1875_11D5_9773_000064657374__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
// PrintProgressDialog.h : header file
//

#include "nsIWebProgressListener.h"
class nsIWebBrowser;

/////////////////////////////////////////////////////////////////////////////
// CPrintProgressDialog dialog

class CPrintProgressDialog : public CDialog
{
// Construction
public:
	CPrintProgressDialog(nsIWebBrowser* aWebBrowser, 
                       nsIPrintSettings* aPrintSettings,
                       CWnd* pParent = NULL);
	virtual ~CPrintProgressDialog();
  virtual int DoModal( );

  // Helper
  void SetURI(const char* aTitle);

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
  nsCOMPtr<nsIWebProgressListener> m_PrintListener;
  nsIPrintSettings*          m_PrintSettings;
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
