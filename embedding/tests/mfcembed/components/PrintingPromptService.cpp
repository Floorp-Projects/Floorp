/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

/* PrintingPromptService is intended to override the default Mozilla PrintingPromptService,
   giving nsIPrompt implementations of our own design, rather than using
   Mozilla's. Do this by building this into a component and registering the
   factory with the same CID/ContractID as Mozilla's (see MfcEmbed.cpp).
*/

#include "stdafx.h"
#include "Dialogs.h"
#include "PrintingPromptService.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMWindow.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIFactory.h"
#include "nsIPrintingPromptService.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"
#include "nsIWebProgressListener.h"
#include "nsPrintProgressParams.h"
#include "nsPrintDialogUtil.h"
#include "PrintProgressDialog.h"

// For PLEvent
#include "nsIEventQueueService.h"
#include "plevent.h"

static HINSTANCE gInstance;

//*****************************************************************************
// ResourceState
//***************************************************************************** 

class ResourceState {
public:
  ResourceState() {
    mPreviousInstance = ::AfxGetResourceHandle();
    ::AfxSetResourceHandle(gInstance);
  }
  ~ResourceState() {
    ::AfxSetResourceHandle(mPreviousInstance);
  }
private:
  HINSTANCE mPreviousInstance;
};


//*****************************************************************************
// CPrintingPromptService
//*****************************************************************************

class CPrintingPromptService: public nsIPrintingPromptService,
                              public nsIWebProgressListener {
public:
                 CPrintingPromptService();
  virtual       ~CPrintingPromptService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTINGPROMPTSERVICE
  NS_DECL_NSIWEBPROGRESSLISTENER

  void NotifyObserver();

private:
  PRBool FirePauseEvent();
  CWnd *CWndForDOMWindow(nsIDOMWindow *aWindow);

  nsCOMPtr<nsIWindowWatcher>       mWWatch;
  nsCOMPtr<nsIWebProgressListener> mWebProgressListener;
  nsCOMPtr<nsIObserver>            mObserver;
  CPrintProgressDialog* m_PPDlg;
};

// Define PL Callback Functions
static void PR_CALLBACK HandlePLEvent(PLEvent* aEvent);
static void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent);


//*****************************************************************************

NS_IMPL_ISUPPORTS2(CPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

CPrintingPromptService::CPrintingPromptService() :
  mWWatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID)),
  m_PPDlg(NULL)
{
}

CPrintingPromptService::~CPrintingPromptService() {
}

CWnd *
CPrintingPromptService::CWndForDOMWindow(nsIDOMWindow *aWindow)
{
  nsCOMPtr<nsIWebBrowserChrome> chrome;
  CWnd *val = 0;

  if (mWWatch) {
    nsCOMPtr<nsIDOMWindow> fosterParent;
    if (!aWindow) { // it will be a dependent window. try to find a foster parent.
      mWWatch->GetActiveWindow(getter_AddRefs(fosterParent));
      aWindow = fosterParent;
    }
    mWWatch->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
  }

  if (chrome) {
    nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
    if (site) {
      HWND w;
      site->GetSiteWindow(reinterpret_cast<void **>(&w));
      val = CWnd::FromHandle(w);
    }
  }

  return val;
}

///////////////////////////////////////////////////////////////////////////////
// nsIPrintingPrompt

//-----------------------------------------------------------
NS_IMETHODIMP 
CPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
    //NS_ENSURE_ARG(parent);

    CWnd* wnd = CWndForDOMWindow(parent);

    NS_ASSERTION(wnd && wnd->m_hWnd, "Couldn't get native window for PRint Dialog!");
    if (wnd && wnd->m_hWnd) {
      return NativeShowPrintDialog(wnd->m_hWnd, webBrowserPrint, printSettings);
    } else {
      return NS_ERROR_FAILURE;
    }
}


//-----------------------------------------------------------
PRBool
CPrintingPromptService::FirePauseEvent()
{
  static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

  nsCOMPtr<nsIEventQueueService> event_service = do_GetService(kEventQueueServiceCID);

  if (!event_service) 
  {
    NS_WARNING("Failed to get event queue service");
    return PR_FALSE;
  }

  nsCOMPtr<nsIEventQueue> event_queue;

  event_service->GetThreadEventQueue(NS_CURRENT_THREAD,
                                     getter_AddRefs(event_queue));

  if (!event_queue) 
  {
    NS_WARNING("Failed to get event queue from service");
    return PR_FALSE;
  }

  PLEvent *event = new PLEvent;

  if (!event) 
  {
    NS_WARNING("Out of memory?");
    return PR_FALSE;
  }

  PL_InitEvent(event, this, (PLHandleEventProc)::HandlePLEvent, (PLDestroyEventProc)::DestroyPLEvent);

  // The event owns the content pointer now.
  NS_ADDREF_THIS();

  event_queue->PostEvent(event);
  return PR_TRUE;
}

/* void showProgress (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, in nsIObserver openDialogObserver, in boolean isForPrinting, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams, out boolean notifyOnOpen); */
NS_IMETHODIMP 
CPrintingPromptService::ShowProgress(nsIDOMWindow*            parent, 
                                      nsIWebBrowserPrint*      webBrowserPrint,    // ok to be null
                                      nsIPrintSettings*        printSettings,      // ok to be null
                                      nsIObserver*             openDialogObserver, // ok to be null
                                      PRBool                   isForPrinting,
                                      nsIWebProgressListener** webProgressListener,
                                      nsIPrintProgressParams** printProgressParams,
                                      PRBool*                  notifyOnOpen)
{
    NS_ENSURE_ARG(webProgressListener);
    NS_ENSURE_ARG(printProgressParams);
    NS_ENSURE_ARG(notifyOnOpen);

    ResourceState setState;
    nsresult rv;

    nsPrintProgressParams* prtProgressParams = new nsPrintProgressParams();
    rv = prtProgressParams->QueryInterface(NS_GET_IID(nsIPrintProgressParams), (void**)printProgressParams);
    NS_ENSURE_SUCCESS(rv, rv);

    mObserver = openDialogObserver;

    *notifyOnOpen = PR_FALSE;
    if (printProgressParams) 
    {
      CWnd *wnd = CWndForDOMWindow(parent);
      m_PPDlg = new CPrintProgressDialog(wnd, isForPrinting, *printProgressParams, webBrowserPrint, printSettings);
      m_PPDlg->Create(IDD_PRINT_PROGRESS_DIALOG);
      m_PPDlg->ShowWindow(SW_SHOW);
      m_PPDlg->UpdateWindow();

      *notifyOnOpen = FirePauseEvent();
    }

    *webProgressListener = NS_STATIC_CAST(nsIWebProgressListener*, this);
    NS_ADDREF(*webProgressListener);

    return rv;
}

/* void showPageSetup (in nsIDOMWindow parent, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
CPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings, nsIObserver *aObs)
{
    NS_ENSURE_ARG(printSettings);

    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void showPrinterProperties (in nsIDOMWindow parent, in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
CPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//------------------------------------------------------------------------
void CPrintingPromptService::NotifyObserver()
{
  if (mObserver) {
    mObserver->Observe(nsnull, nsnull, nsnull);
  }
}

//------------------------------------------------------------------------
void PR_CALLBACK HandlePLEvent(PLEvent* aEvent)
{
  CPrintingPromptService *printingPromptService = (CPrintingPromptService*)PL_GetEventOwner(aEvent);

  NS_ASSERTION(printingPromptService, "The event owner is null.");
  if (printingPromptService) {
    printingPromptService->NotifyObserver();
  }
}

//------------------------------------------------------------------------
void PR_CALLBACK DestroyPLEvent(PLEvent* aEvent)
{
  CPrintingPromptService *printingPromptService = (CPrintingPromptService*)PL_GetEventOwner(aEvent);
  NS_IF_RELEASE(printingPromptService);

  delete aEvent;
}

//////////////////////////////////////////////////////////////////////
// nsIWebProgressListener
//////////////////////////////////////////////////////////////////////

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP 
CPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    if (aStateFlags & STATE_START) 
    {
      if (m_PPDlg)
      {
        m_PPDlg->OnStartPrinting();
      }
    }

    if (aStateFlags & STATE_STOP) 
    {
      if (m_PPDlg)
      {
        m_PPDlg->OnProgressPrinting(100, 100);
        m_PPDlg->DestroyWindow();
        m_PPDlg = NULL;
      }
    }
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
CPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    if (m_PPDlg)
    {
      m_PPDlg->OnProgressPrinting(aCurTotalProgress, aMaxTotalProgress);
    }
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
CPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
CPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
CPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
    return NS_OK;
}


 
//*****************************************************************************
// CPrintingPromptServiceFactory
//*****************************************************************************   

class CPrintingPromptServiceFactory : public nsIFactory {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFACTORY

  CPrintingPromptServiceFactory();
  virtual ~CPrintingPromptServiceFactory();
};

//*****************************************************************************

NS_IMPL_ISUPPORTS1(CPrintingPromptServiceFactory, nsIFactory)

CPrintingPromptServiceFactory::CPrintingPromptServiceFactory() {
}

CPrintingPromptServiceFactory::~CPrintingPromptServiceFactory() {
}

NS_IMETHODIMP CPrintingPromptServiceFactory::CreateInstance(nsISupports *aOuter, const nsIID & aIID, void **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  
  *aResult = NULL;  
  CPrintingPromptService *inst = new CPrintingPromptService;    
  if (!inst)
    return NS_ERROR_OUT_OF_MEMORY;
    
  nsresult rv = inst->QueryInterface(aIID, aResult);
  if (rv != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  
    
  return rv;
}

NS_IMETHODIMP CPrintingPromptServiceFactory::LockFactory(PRBool lock)
{
  return NS_OK;
}

//*****************************************************************************

void InitPrintingPromptService(HINSTANCE instance) {

  gInstance = instance;
}

nsresult NS_NewPrintingPromptServiceFactory(nsIFactory** aFactory)
{
  NS_ENSURE_ARG_POINTER(aFactory);
  *aFactory = nsnull;
  
  CPrintingPromptServiceFactory *result = new CPrintingPromptServiceFactory;
  if (!result)
    return NS_ERROR_OUT_OF_MEMORY;
    
  NS_ADDREF(result);
  *aFactory = result;
  
  return NS_OK;
}
