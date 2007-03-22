// PrintProgressDialog.cpp : implementation file
//

#include "stdafx.h"
//#include "mfcembed.h"
#include "PrintProgressDialog.h"
//#include "BrowserView.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserPrint.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsMemory.h"

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


CPrintProgressDialog::CPrintProgressDialog(CWnd*               pParent,
                                           BOOL                aIsForPrinting,
                                           nsIPrintProgressParams* aPPParams,
                                           nsIWebBrowserPrint* aWebBrowserPrint,
                                           nsIPrintSettings*   aPrintSettings)
	: CDialog(CPrintProgressDialog::IDD, pParent),
  m_WebBrowserPrint(aWebBrowserPrint),
  m_PPParams(aPPParams),
  m_PrintListener(nsnull),
  m_PrintSettings(aPrintSettings),
  m_HasStarted(FALSE),
  m_IsForPrinting(aIsForPrinting)
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

BOOL CPrintProgressDialog::OnInitDialog() 
{
	CDialog::OnInitDialog();

  CWnd* cancelBtn = (CWnd*)GetDlgItem(IDCANCEL);
  if (cancelBtn)
  {
    if (m_IsForPrinting)
    {
      cancelBtn->EnableWindow(FALSE);
    } else {
      cancelBtn->ShowWindow(SW_HIDE);
      SetWindowText(_T("Print Preview"));
    }
  }
	
  SetDocAndURL();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}


/* void OnStartPrinting (); */
NS_IMETHODIMP 
CPrintProgressDialog::OnStartPrinting()
{
	CProgressCtrl* progressCtrl = (CProgressCtrl *)GetDlgItem(IDC_PRINT_PROGRESS_PRG);
	CWnd* progressText = (CWnd*)GetDlgItem(IDC_PRINT_PROGRESS_TXT);
  if (!progressCtrl || !progressText) return NS_OK;

  m_HasStarted = TRUE;
	progressCtrl->ShowWindow(SW_HIDE);

  SetDocAndURL();

  return NS_OK;
}

/* void OnProgressPrinting (in PRUint32 aProgress, in PRUint32 aProgressMax); */
NS_IMETHODIMP 
CPrintProgressDialog::OnProgressPrinting(PRUint32 aProgress, PRUint32 aProgressMax)
{
	CProgressCtrl* progressCtrl = (CProgressCtrl *)GetDlgItem(IDC_PRINT_PROGRESS_PRG);
	CWnd* progressText = (CWnd*)GetDlgItem(IDC_PRINT_PROGRESS_TXT);
  if (!progressCtrl || !progressText) return NS_OK;

  if (m_HasStarted && aProgress != 100) 
  {
	  progressText->ShowWindow(SW_HIDE);
	  progressCtrl->ShowWindow(SW_SHOW);
	  progressCtrl->SetRange(0, aProgressMax);
    m_HasStarted = FALSE;
	  progressText->UpdateWindow();
  }

  SetDocAndURL();

  // Initialize the progress meter we we get the "zero" progress
  // which also tells us the max progress
  if (aProgress == 0) {
	  progressCtrl->SetRange(0, aProgressMax);
    progressCtrl->SetPos(0);

  }
  CWnd* cancelBtn = (CWnd*)GetDlgItem(IDCANCEL);
  if (cancelBtn && m_IsForPrinting)
  {
    cancelBtn->EnableWindow(TRUE);
  }
	progressCtrl->SetPos(aProgress);
  RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);

  return NS_OK;
}

/* void OnEndPrinting (in PRUint32 aStatus); */
NS_IMETHODIMP 
CPrintProgressDialog::OnEndPrinting(PRUint32 aStatus)
{
  return NS_OK;
}

void CPrintProgressDialog::OnCancel() 
{
  if (m_WebBrowserPrint) {
    m_WebBrowserPrint->Cancel();
  }

	CDialog::OnCancel();
}

void CPrintProgressDialog::SetDocAndURL()
{
	USES_CONVERSION;

  if (m_PPParams) 
  {
    PRUnichar* docTitle = nsnull;
    PRUnichar* urlStr   = nsnull;
    m_PPParams->GetDocTitle(&docTitle);
    m_PPParams->GetDocURL(&urlStr);

	  if (docTitle)
    {
      if (*docTitle && W2T(docTitle))
	    {
		    SetWindowText(W2T(docTitle));
	    }
      nsMemory::Free(docTitle);
    }

	  if (urlStr)
    {
      if (*urlStr && W2T(urlStr))
	    {
	      CWnd *pWnd = GetDlgItem(IDC_PRINT_PROGRESS_URL_TXT);
	      if (pWnd)
        {
		      pWnd->SetWindowText(W2T(urlStr));
	      }
      }
      nsMemory::Free(urlStr);
    }
  }
  Invalidate();
  UpdateWindow();
}
