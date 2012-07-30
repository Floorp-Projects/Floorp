/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingPromptService.h"

#include "nsIComponentManager.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"
#include "nsISupportsArray.h"
#include "nsString.h"
#include "nsIPrintDialogService.h"

// Printing Progress Includes
#include "nsPrintProgress.h"
#include "nsPrintProgressParams.h"

static const char *kPrintDialogURL         = "chrome://global/content/printdialog.xul";
static const char *kPrintProgressDialogURL = "chrome://global/content/printProgress.xul";
static const char *kPrtPrvProgressDialogURL = "chrome://global/content/printPreviewProgress.xul";
static const char *kPageSetupDialogURL     = "chrome://global/content/printPageSetup.xul";
static const char *kPrinterPropertiesURL   = "chrome://global/content/printjoboptions.xul";
 
/****************************************************************
 ************************* ParamBlock ***************************
 ****************************************************************/

class ParamBlock {

public:
    ParamBlock() 
    {
        mBlock = 0;
    }
    ~ParamBlock() 
    {
        NS_IF_RELEASE(mBlock);
    }
    nsresult Init() {
      return CallCreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID, &mBlock);
    }
    nsIDialogParamBlock * operator->() const { return mBlock; }
    operator nsIDialogParamBlock * const ()  { return mBlock; }

private:
    nsIDialogParamBlock *mBlock;
};

/****************************************************************
 ***************** nsPrintingPromptService **********************
 ****************************************************************/

NS_IMPL_ISUPPORTS2(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService() 
{
}

nsPrintingPromptService::~nsPrintingPromptService() 
{
}

nsresult
nsPrintingPromptService::Init()
{
    nsresult rv;
    mWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    return rv;
}

/* void showPrintDialog (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
    NS_ENSURE_ARG(webBrowserPrint);
    NS_ENSURE_ARG(printSettings);

    // Try to access a component dialog
    nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                             NS_PRINTDIALOGSERVICE_CONTRACTID));
    if (dlgPrint)
      return dlgPrint->Show(parent, printSettings, webBrowserPrint);

    // Show the built-in dialog instead
    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    return DoDialog(parent, block, webBrowserPrint, printSettings, kPrintDialogURL);
}

/* void showProgress (in nsIDOMWindow parent, in nsIWebBrowserPrint webBrowserPrint, in nsIPrintSettings printSettings, in nsIObserver openDialogObserver, in boolean isForPrinting, out nsIWebProgressListener webProgressListener, out nsIPrintProgressParams printProgressParams, out boolean notifyOnOpen); */
NS_IMETHODIMP 
nsPrintingPromptService::ShowProgress(nsIDOMWindow*            parent, 
                                      nsIWebBrowserPrint*      webBrowserPrint,    // ok to be null
                                      nsIPrintSettings*        printSettings,      // ok to be null
                                      nsIObserver*             openDialogObserver, // ok to be null
                                      bool                     isForPrinting,
                                      nsIWebProgressListener** webProgressListener,
                                      nsIPrintProgressParams** printProgressParams, 
                                      bool*                  notifyOnOpen)
{
    NS_ENSURE_ARG(webProgressListener);
    NS_ENSURE_ARG(printProgressParams);
    NS_ENSURE_ARG(notifyOnOpen);

    *notifyOnOpen = false;

    nsPrintProgress* prtProgress = new nsPrintProgress(printSettings);
    mPrintProgress = prtProgress;
    mWebProgressListener = prtProgress;

    nsCOMPtr<nsIPrintProgressParams> prtProgressParams = new nsPrintProgressParams();

    nsCOMPtr<nsIDOMWindow> parentWindow = parent;

    if (mWatcher && !parentWindow) {
        mWatcher->GetActiveWindow(getter_AddRefs(parentWindow));
    }

    if (parentWindow) {
        mPrintProgress->OpenProgressDialog(parentWindow,
                                           isForPrinting ? kPrintProgressDialogURL : kPrtPrvProgressDialogURL,
                                           prtProgressParams, openDialogObserver, notifyOnOpen);
    }

    prtProgressParams.forget(printProgressParams);
    NS_ADDREF(*webProgressListener = this);

    return NS_OK;
}

/* void showPageSetup (in nsIDOMWindow parent, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings, nsIObserver *aObs)
{
    NS_ENSURE_ARG(printSettings);

    // Try to access a component dialog
    nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                             NS_PRINTDIALOGSERVICE_CONTRACTID));
    if (dlgPrint)
      return dlgPrint->ShowPageSetup(parent, printSettings);

    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    return DoDialog(parent, block, nullptr, printSettings, kPageSetupDialogURL);
}

/* void showPrinterProperties (in nsIDOMWindow parent, in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    /* fixme: We simply ignore the |aPrinter| argument here
     * We should get the supported printer attributes from the printer and 
     * populate the print job options dialog with these data instead of using 
     * the "default set" here.
     * However, this requires changes on all platforms and is another big chunk
     * of patches ... ;-(
     */
    NS_ENSURE_ARG(printerName);
    NS_ENSURE_ARG(printSettings);

    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    return DoDialog(parent, block, nullptr, printSettings, kPrinterPropertiesURL);
   
}

nsresult
nsPrintingPromptService::DoDialog(nsIDOMWindow *aParent,
                                  nsIDialogParamBlock *aParamBlock, 
                                  nsIWebBrowserPrint *aWebBrowserPrint, 
                                  nsIPrintSettings* aPS,
                                  const char *aChromeURL)
{
    NS_ENSURE_ARG(aParamBlock);
    NS_ENSURE_ARG(aPS);
    NS_ENSURE_ARG(aChromeURL);

    if (!mWatcher)
        return NS_ERROR_FAILURE;

    nsresult rv = NS_OK;

    // get a parent, if at all possible
    // (though we'd rather this didn't fail, it's OK if it does. so there's
    // no failure or null check.)
    nsCOMPtr<nsIDOMWindow> activeParent; // retain ownership for method lifetime
    if (!aParent) 
    {
        mWatcher->GetActiveWindow(getter_AddRefs(activeParent));
        aParent = activeParent;
    }

    // create a nsISupportsArray of the parameters 
    // being passed to the window
    nsCOMPtr<nsISupportsArray> array;
    NS_NewISupportsArray(getter_AddRefs(array));
    if (!array) return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupports> psSupports(do_QueryInterface(aPS));
    NS_ASSERTION(psSupports, "PrintSettings must be a supports");
    array->AppendElement(psSupports);

    if (aWebBrowserPrint) {
      nsCOMPtr<nsISupports> wbpSupports(do_QueryInterface(aWebBrowserPrint));
      NS_ASSERTION(wbpSupports, "nsIWebBrowserPrint must be a supports");
      array->AppendElement(wbpSupports);
    }

    nsCOMPtr<nsISupports> blkSupps(do_QueryInterface(aParamBlock));
    NS_ASSERTION(blkSupps, "IOBlk must be a supports");
    array->AppendElement(blkSupps);

    nsCOMPtr<nsISupports> arguments(do_QueryInterface(array));
    NS_ASSERTION(array, "array must be a supports");


    nsCOMPtr<nsIDOMWindow> dialog;
    rv = mWatcher->OpenWindow(aParent, aChromeURL, "_blank",
                              "centerscreen,chrome,modal,titlebar", arguments,
                              getter_AddRefs(dialog));

    // if aWebBrowserPrint is not null then we are printing
    // so we want to pass back NS_ERROR_ABORT on cancel
    if (NS_SUCCEEDED(rv) && aWebBrowserPrint) 
    {
        PRInt32 status;
        aParamBlock->GetInt(0, &status);
        return status == 0?NS_ERROR_ABORT:NS_OK;
    }

    return rv;
}

//////////////////////////////////////////////////////////////////////
// nsIWebProgressListener
//////////////////////////////////////////////////////////////////////

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
  if ((aStateFlags & STATE_STOP) && mWebProgressListener) {
    mWebProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
    if (mPrintProgress) {
      mPrintProgress->CloseProgressDialog(true);
    }
    mPrintProgress       = nullptr;
    mWebProgressListener = nullptr;
  }
  return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
  }
  return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location, in unsigned long aFlags); */
NS_IMETHODIMP 
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, PRUint32 aFlags)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnLocationChange(aWebProgress, aRequest, location, aFlags);
  }
  return NS_OK;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  return NS_OK;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnSecurityChange(aWebProgress, aRequest, state);
  }
  return NS_OK;
}
