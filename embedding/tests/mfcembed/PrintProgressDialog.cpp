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

// PrintProgressDialog.cpp : implementation file
//

#include "stdafx.h"
#include "mfcembed.h"
#include "PrintProgressDialog.h"
#include "BrowserView.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserPrint.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPrintProgressDialog dialog

class CDlgPrintListener : public nsIWebProgressListener
{
// Construction
public:
	CDlgPrintListener(CPrintProgressDialog* aDlg); 

  NS_DECL_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER

  void ClearDlg() { m_PrintDlg = NULL; } // weak reference

// Implementation
protected:
	CPrintProgressDialog* m_PrintDlg;
};

NS_IMPL_ADDREF(CDlgPrintListener)
NS_IMPL_RELEASE(CDlgPrintListener)

NS_INTERFACE_MAP_BEGIN(CDlgPrintListener)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebProgressListener)
    NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END


CDlgPrintListener::CDlgPrintListener(CPrintProgressDialog* aDlg) :
  m_PrintDlg(aDlg)
{
  NS_INIT_ISUPPORTS();
  //NS_ADDREF_THIS();
}

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP 
CDlgPrintListener::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  if (m_PrintDlg) {
    if (aStateFlags == (nsIWebProgressListener::STATE_START|nsIWebProgressListener::STATE_IS_DOCUMENT)) {
      return m_PrintDlg->OnStartPrinting();

    } else if (aStateFlags == (nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT)) {
      return m_PrintDlg->OnEndPrinting(aStatus);
    }
  }
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
CDlgPrintListener::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  if (m_PrintDlg) {
    return m_PrintDlg->OnProgressPrinting(aCurSelfProgress, aMaxSelfProgress);
  }
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
CDlgPrintListener::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
CDlgPrintListener::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
CDlgPrintListener::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}


/////////////////////////////////////////////////////////////////////////////
// CPrintProgressDialog dialog


CPrintProgressDialog::CPrintProgressDialog(nsIWebBrowser* aWebBrowser,
                                           nsIPrintSettings* aPrintSettings,
                                           CWnd* pParent /*=NULL*/)
	: CDialog(CPrintProgressDialog::IDD, pParent),
  m_WebBrowser(aWebBrowser),
  m_PrintListener(nsnull),
  m_PrintSettings(aPrintSettings),
  m_InModalMode(PR_FALSE)
{
	//{{AFX_DATA_INIT(CPrintProgressDialog)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
}

CPrintProgressDialog::~CPrintProgressDialog()
{
  CDlgPrintListener * pl = (CDlgPrintListener*)m_PrintListener.get();
  if (pl) {
    pl->ClearDlg();
  }
}


void CPrintProgressDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CPrintProgressDialog)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CPrintProgressDialog, CDialog)
	//{{AFX_MSG_MAP(CPrintProgressDialog)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPrintProgressDialog message handlers
static void GetLocalRect(CWnd * aWnd, CRect& aRect, CWnd * aParent)
{
  CRect wr;
  aParent->GetWindowRect(wr);

  CRect cr;
  aParent->GetClientRect(cr);

  aWnd->GetWindowRect(aRect);

  int borderH = wr.Height() - cr.Height();
  int borderW = (wr.Width() - cr.Width())/2;
  aRect.top    -= wr.top+borderH-borderW;
  aRect.left   -= wr.left+borderW;
  aRect.right  -= wr.left+borderW;
  aRect.bottom -= wr.top+borderH-borderW;

}

BOOL CPrintProgressDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();
	
	CRect clientRect;
	GetClientRect(&clientRect);

	CRect titleRect;
  GetLocalRect(GetDlgItem(IDC_PPD_DOC_TITLE_STATIC), titleRect, this);

	CRect itemRect;
  GetLocalRect(GetDlgItem(IDC_PPD_DOC_TXT), itemRect, this);

  CRect progRect;
  progRect.left   = titleRect.left;
  progRect.top    = itemRect.top+itemRect.Height()+5;
  progRect.right  = clientRect.Width()-(2*titleRect.left);
  progRect.bottom = progRect.top+titleRect.Height();


	m_wndProgress.Create (WS_CHILD | WS_VISIBLE, progRect, this, -1);
	m_wndProgress.SetPos (0);
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


int CPrintProgressDialog::DoModal( )
{
  PRBool doModal = PR_FALSE;
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(m_WebBrowser));
  if(print) 
  {
    m_PrintListener = new CDlgPrintListener(this); // constructor addrefs
    if (m_PrintListener) {
      // doModal will be set to false if the print job was cancelled
      nsIWebProgressListener * wpl = NS_STATIC_CAST(nsIWebProgressListener*, m_PrintListener);
      doModal = NS_SUCCEEDED(print->Print(m_PrintSettings, wpl)) == PR_TRUE;
    }
  }

  if (doModal) {
    m_InModalMode = PR_TRUE;
    return CDialog::DoModal();
  }
  return 0;
}


/* void OnStartPrinting (); */
NS_IMETHODIMP 
CPrintProgressDialog::OnStartPrinting()
{
  return NS_OK;
}

/* void OnProgressPrinting (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP 
CPrintProgressDialog::OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax)
{
  if (m_wndProgress.m_hWnd == NULL) return NS_OK;

  // Initialize the progress meter we we get the "zero" progress
  // which also tells us the max progress
  if (aProgress == 0) {
	  CWnd *pWnd = GetDlgItem(IDC_PPD_DOC_TXT);
	  if(pWnd)
		  pWnd->SetWindowText(m_URL);

	  m_wndProgress.SetRange(0, aProgressMax);
    m_wndProgress.SetPos(0);
  }
	m_wndProgress.SetPos(aProgress);
  RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

  return NS_OK;
}

/* void OnEndPrinting (in PRUint32 aStatus); */
NS_IMETHODIMP 
CPrintProgressDialog::OnEndPrinting(PRUint32 aStatus)
{
  // Here we need to know whether we have gone "modal" 
  // because we could get notified here if the user cancels
  // before we ever get a chance to go into the modal loop
  if (m_InModalMode) {
    EndDialog(1);
  }
  return NS_OK;
}

void CPrintProgressDialog::OnCancel() 
{
  nsCOMPtr<nsIWebBrowserPrint> print(do_GetInterface(m_WebBrowser));
  if (print) {
    print->Cancel();
  }

	CDialog::OnCancel();
}

void CPrintProgressDialog::SetURI(const char* aTitle)
{
  m_URL = _T(aTitle);
}
