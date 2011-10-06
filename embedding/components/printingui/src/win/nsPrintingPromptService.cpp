/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"

#include "nsPrintingPromptService.h"
#include "nsIPrintingPromptService.h"
#include "nsIFactory.h"
#include "nsPIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"
#include "nsPrintDialogUtil.h"

// Printing Progress Includes
#include "nsPrintProgress.h"
#include "nsPrintProgressParams.h"
#include "nsIWebProgressListener.h"

// XP Dialog includes
#include "nsIDialogParamBlock.h"
#include "nsISupportsUtils.h"
#include "nsISupportsArray.h"

// Includes need to locate the native Window
#include "nsIWidget.h"
#include "nsIBaseWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"


static const char *kPrintProgressDialogURL  = "chrome://global/content/printProgress.xul";
static const char *kPrtPrvProgressDialogURL = "chrome://global/content/printPreviewProgress.xul";
static const char *kPageSetupDialogURL      = "chrome://global/content/printPageSetup.xul";

// Static Data 
static HINSTANCE gInstance;

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

//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService() 
{
}

//-----------------------------------------------------------
nsPrintingPromptService::~nsPrintingPromptService()
{
}

//-----------------------------------------------------------
nsresult
nsPrintingPromptService::Init()
{
    nsresult rv;
    mWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    return rv;
}

//-----------------------------------------------------------
HWND
nsPrintingPromptService::GetHWNDForDOMWindow(nsIDOMWindow *aWindow)
{
    nsCOMPtr<nsIWebBrowserChrome> chrome;
    HWND hWnd = NULL;

    // We might be embedded so check this path first
    if (mWatcher) {
        nsCOMPtr<nsIDOMWindow> fosterParent;
        if (!aWindow) 
        {   // it will be a dependent window. try to find a foster parent.
            mWatcher->GetActiveWindow(getter_AddRefs(fosterParent));
            aWindow = fosterParent;
        }
        mWatcher->GetChromeForWindow(aWindow, getter_AddRefs(chrome));
    }

    if (chrome) {
        nsCOMPtr<nsIEmbeddingSiteWindow> site(do_QueryInterface(chrome));
        if (site) 
        {
            HWND w;
            site->GetSiteWindow(reinterpret_cast<void **>(&w));
            return w;
        }
    }

    // Now we might be the Browser so check this path
    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));

    nsCOMPtr<nsIDocShellTreeItem> treeItem =
        do_QueryInterface(window->GetDocShell());
    if (!treeItem) return nsnull;

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    treeItem->GetTreeOwner(getter_AddRefs(treeOwner));
    if (!treeOwner) return nsnull;

    nsCOMPtr<nsIWebBrowserChrome> webBrowserChrome(do_GetInterface(treeOwner));
    if (!webBrowserChrome) return nsnull;

    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(webBrowserChrome));
    if (!baseWin) return nsnull;

    nsCOMPtr<nsIWidget> widget;
    baseWin->GetMainWidget(getter_AddRefs(widget));
    if (!widget) return nsnull;

    return (HWND)widget->GetNativeData(NS_NATIVE_TMP_WINDOW);

}


///////////////////////////////////////////////////////////////////////////////
// nsIPrintingPrompt

//-----------------------------------------------------------
NS_IMETHODIMP 
nsPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
    NS_ENSURE_ARG(parent);

    HWND hWnd = GetHWNDForDOMWindow(parent);
    NS_ASSERTION(hWnd, "Couldn't get native window for PRint Dialog!");

    return NativeShowPrintDialog(hWnd, webBrowserPrint, printSettings);
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

    *notifyOnOpen = PR_FALSE;
    if (mPrintProgress) {
        *webProgressListener = nsnull;
        *printProgressParams = nsnull;
        return NS_ERROR_FAILURE;
    }

    nsPrintProgress* prtProgress = new nsPrintProgress();
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

    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    rv = DoDialog(parent, block, printSettings, kPageSetupDialogURL);

    // if aWebBrowserPrint is not null then we are printing
    // so we want to pass back NS_ERROR_ABORT on cancel
    if (NS_SUCCEEDED(rv)) 
    {
      PRInt32 status;
      block->GetInt(0, &status);
      return status == 0?NS_ERROR_ABORT:NS_OK;
    }

    return rv;
}

/* void showPrinterProperties (in nsIDOMWindow parent, in wstring printerName, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------
// Helper to Fly XP Dialog
nsresult
nsPrintingPromptService::DoDialog(nsIDOMWindow *aParent,
                                  nsIDialogParamBlock *aParamBlock, 
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

    nsCOMPtr<nsISupports> blkSupps(do_QueryInterface(aParamBlock));
    NS_ASSERTION(blkSupps, "IOBlk must be a supports");
    array->AppendElement(blkSupps);

    nsCOMPtr<nsISupports> arguments(do_QueryInterface(array));
    NS_ASSERTION(array, "array must be a supports");


    nsCOMPtr<nsIDOMWindow> dialog;
    rv = mWatcher->OpenWindow(aParent, aChromeURL, "_blank",
                              "centerscreen,chrome,modal,titlebar", arguments,
                              getter_AddRefs(dialog));

    return rv;
}

//////////////////////////////////////////////////////////////////////
// nsIWebProgressListener
//////////////////////////////////////////////////////////////////////

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in nsresult aStatus); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 aStateFlags, nsresult aStatus)
{
    if ((aStateFlags & STATE_STOP) && mWebProgressListener) 
    {
        mWebProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
        if (mPrintProgress) 
        {
            mPrintProgress->CloseProgressDialog(PR_TRUE);
        }
        mPrintProgress       = nsnull;
        mWebProgressListener = nsnull;
    }
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
  if (mWebProgressListener) 
  {
      return mWebProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
  }
  return NS_ERROR_FAILURE;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
  if (mWebProgressListener) 
  {
      return mWebProgressListener->OnLocationChange(aWebProgress, aRequest, location);
  }
  return NS_ERROR_FAILURE;
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
  if (mWebProgressListener) 
  {
      return mWebProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
  }
  return NS_ERROR_FAILURE;
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in unsigned long state); */
NS_IMETHODIMP 
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRUint32 state)
{
  if (mWebProgressListener) 
  {
      return mWebProgressListener->OnSecurityChange(aWebProgress, aRequest, state);
  }
  return NS_ERROR_FAILURE;
}


