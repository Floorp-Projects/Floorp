/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintingPromptService.h"

#include "nsArray.h"
#include "nsIComponentManager.h"
#include "nsIDialogParamBlock.h"
#include "nsIDOMWindow.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h"
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
    nsIDialogParamBlock * operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN { return mBlock; }
    operator nsIDialogParamBlock * () const { return mBlock; }

private:
    nsIDialogParamBlock *mBlock;
};

/****************************************************************
 ***************** nsPrintingPromptService **********************
 ****************************************************************/

NS_IMPL_ISUPPORTS(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

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

NS_IMETHODIMP 
nsPrintingPromptService::ShowPrintDialog(mozIDOMWindowProxy *parent,
                                         nsIWebBrowserPrint *webBrowserPrint,
                                         nsIPrintSettings *printSettings)
{
    NS_ENSURE_ARG(webBrowserPrint);
    NS_ENSURE_ARG(printSettings);

    // Try to access a component dialog
    nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                             NS_PRINTDIALOGSERVICE_CONTRACTID));
    if (dlgPrint)
      return dlgPrint->Show(nsPIDOMWindowOuter::From(parent),
                            printSettings, webBrowserPrint);

    // Show the built-in dialog instead
    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    return DoDialog(parent, block, webBrowserPrint, printSettings, kPrintDialogURL);
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowProgress(mozIDOMWindowProxy*      parent, 
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

    nsCOMPtr<mozIDOMWindowProxy> parentWindow = parent;

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

NS_IMETHODIMP 
nsPrintingPromptService::ShowPageSetup(mozIDOMWindowProxy *parent,
                                       nsIPrintSettings *printSettings,
                                       nsIObserver *aObs)
{
    NS_ENSURE_ARG(printSettings);

    // Try to access a component dialog
    nsCOMPtr<nsIPrintDialogService> dlgPrint(do_GetService(
                                             NS_PRINTDIALOGSERVICE_CONTRACTID));
    if (dlgPrint)
      return dlgPrint->ShowPageSetup(nsPIDOMWindowOuter::From(parent),
                                     printSettings);

    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    return DoDialog(parent, block, nullptr, printSettings, kPageSetupDialogURL);
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowPrinterProperties(mozIDOMWindowProxy *parent,
                                               const char16_t *printerName,
                                               nsIPrintSettings *printSettings)
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
nsPrintingPromptService::DoDialog(mozIDOMWindowProxy *aParent,
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

    // get a parent, if at all possible
    // (though we'd rather this didn't fail, it's OK if it does. so there's
    // no failure or null check.)
    nsCOMPtr<mozIDOMWindowProxy> activeParent;
    if (!aParent) 
    {
        mWatcher->GetActiveWindow(getter_AddRefs(activeParent));
        aParent = activeParent;
    }

    // create a nsIMutableArray of the parameters 
    // being passed to the window
    nsCOMPtr<nsIMutableArray> array = nsArray::Create();

    nsCOMPtr<nsISupports> psSupports(do_QueryInterface(aPS));
    NS_ASSERTION(psSupports, "PrintSettings must be a supports");
    array->AppendElement(psSupports, /*weak =*/ false);

    if (aWebBrowserPrint) {
      nsCOMPtr<nsISupports> wbpSupports(do_QueryInterface(aWebBrowserPrint));
      NS_ASSERTION(wbpSupports, "nsIWebBrowserPrint must be a supports");
      array->AppendElement(wbpSupports, /*weak =*/ false);
    }

    nsCOMPtr<nsISupports> blkSupps(do_QueryInterface(aParamBlock));
    NS_ASSERTION(blkSupps, "IOBlk must be a supports");
    array->AppendElement(blkSupps, /*weak =*/ false);

    nsCOMPtr<mozIDOMWindowProxy> dialog;
    nsresult rv = mWatcher->OpenWindow(aParent, aChromeURL, "_blank",
                              "centerscreen,chrome,modal,titlebar", array,
                              getter_AddRefs(dialog));

    // if aWebBrowserPrint is not null then we are printing
    // so we want to pass back NS_ERROR_ABORT on cancel
    if (NS_SUCCEEDED(rv) && aWebBrowserPrint) 
    {
        int32_t status;
        aParamBlock->GetInt(0, &status);
        return status == 0?NS_ERROR_ABORT:NS_OK;
    }

    return rv;
}

//////////////////////////////////////////////////////////////////////
// nsIWebProgressListener
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP 
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t aStateFlags, nsresult aStatus)
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

NS_IMETHODIMP 
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, int32_t aCurSelfProgress, int32_t aMaxSelfProgress, int32_t aCurTotalProgress, int32_t aMaxTotalProgress)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location, uint32_t aFlags)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnLocationChange(aWebProgress, aRequest, location, aFlags);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const char16_t *aMessage)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, uint32_t state)
{
  if (mWebProgressListener) {
    return mWebProgressListener->OnSecurityChange(aWebProgress, aRequest, state);
  }
  return NS_OK;
}
