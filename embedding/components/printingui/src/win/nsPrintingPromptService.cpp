/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"

#include "nsPrintingPromptService.h"
#include "nsIPrintingPromptService.h"
#include "nsIFactory.h"
#include "nsIDOMWindow.h"
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
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"


#define NS_DIALOGPARAMBLOCK_CONTRACTID "@mozilla.org/embedcomp/dialogparam;1"

static const char *kPrintProgressDialogURL = "chrome://global/content/printProgress.xul";
static const char *kPageSetupDialogURL     = "chrome://communicator/content/printPageSetup.xul";

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
      return nsComponentManager::CreateInstance(
                                   NS_DIALOGPARAMBLOCK_CONTRACTID,
                                   0, NS_GET_IID(nsIDialogParamBlock),
                                   (void**) &mBlock);
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
    NS_INIT_REFCNT();
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
    mWatcher = do_GetService("@mozilla.org/embedcomp/window-watcher;1", &rv);
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
    nsCOMPtr<nsIScriptGlobalObject> scriptGlobal(do_QueryInterface(aWindow));
    nsCOMPtr<nsIDocShell> docShell;
    scriptGlobal->GetDocShell(getter_AddRefs(docShell));
    if (!docShell) return nsnull;

    nsCOMPtr<nsIDocShellTreeItem> treeItem;
    treeItem = do_QueryInterface(docShell);
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

    return (HWND)widget->GetNativeData(NS_NATIVE_WINDOW);

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


//-----------------------------------------------------------
NS_IMETHODIMP 
nsPrintingPromptService::ShowProgress(nsIDOMWindow*            parent, 
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

    nsPrintProgress* prtProgress = new nsPrintProgress();
    nsresult rv = prtProgress->QueryInterface(NS_GET_IID(nsIPrintProgress), (void**)getter_AddRefs(mPrintProgress));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = prtProgress->QueryInterface(NS_GET_IID(nsIWebProgressListener), (void**)getter_AddRefs(mWebProgressListener));
    NS_ENSURE_SUCCESS(rv, rv);

    nsPrintProgressParams* prtProgressParams = new nsPrintProgressParams();
    rv = prtProgressParams->QueryInterface(NS_GET_IID(nsIPrintProgressParams), (void**)printProgressParams);
    NS_ENSURE_SUCCESS(rv, rv);

    *notifyOnOpen = PR_FALSE;
    if (printProgressParams) 
    {
        nsCOMPtr<nsIDOMWindowInternal> parentDOMIntl(do_QueryInterface(parent));

        if (mWatcher && !parentDOMIntl) 
        {
            nsCOMPtr<nsIDOMWindow> active;
            mWatcher->GetActiveWindow(getter_AddRefs(active));
            parentDOMIntl = do_QueryInterface(active);
        }

        if (parentDOMIntl) 
        {
            mPrintProgress->OpenProgressDialog(parentDOMIntl, kPrintProgressDialogURL, *printProgressParams, openDialogObserver, notifyOnOpen);
        }
    }

    *webProgressListener = NS_STATIC_CAST(nsIWebProgressListener*, this);
    NS_ADDREF(*webProgressListener);

    return rv;
}

/* void showPageSetup (in nsIDOMWindow parent, in nsIPrintSettings printSettings); */
NS_IMETHODIMP 
nsPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings)
{
    NS_ENSURE_ARG(printSettings);

    ParamBlock block;
    nsresult rv = block.Init();
    if (NS_FAILED(rv))
      return rv;

    block->SetInt(0, 0);
    rv = DoDialog(parent, block, printSettings, kPageSetupDialogURL);

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

/* void onStateChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aStateFlags, in unsigned long aStatus); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStateChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aStateFlags, PRUint32 aStatus)
{
    if (aStateFlags & STATE_STOP) 
    {
        mWebProgressListener->OnStateChange(aWebProgress, aRequest, aStateFlags, aStatus);
        mPrintProgress->CloseProgressDialog(PR_TRUE);
        mPrintProgress = nsnull;
    }
    return NS_OK;
}

/* void onProgressChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long aCurSelfProgress, in long aMaxSelfProgress, in long aCurTotalProgress, in long aMaxTotalProgress); */
NS_IMETHODIMP 
nsPrintingPromptService::OnProgressChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 aCurSelfProgress, PRInt32 aMaxSelfProgress, PRInt32 aCurTotalProgress, PRInt32 aMaxTotalProgress)
{
    return mWebProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    return mWebProgressListener->OnLocationChange(aWebProgress, aRequest, location);
}

/* void onStatusChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsresult aStatus, in wstring aMessage); */
NS_IMETHODIMP 
nsPrintingPromptService::OnStatusChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsresult aStatus, const PRUnichar *aMessage)
{
    return mWebProgressListener->OnStatusChange(aWebProgress, aRequest, aStatus, aMessage);
}

/* void onSecurityChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in long state); */
NS_IMETHODIMP 
nsPrintingPromptService::OnSecurityChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, PRInt32 state)
{
    return mWebProgressListener->OnSecurityChange(aWebProgress, aRequest, state);
}


