/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Rod Spears <rods@netscape.com>
 *   Conrad Carlen <ccarlen@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsPrintingPromptService.h"

#include "nsCOMPtr.h"

#include "nsIPrintingPromptService.h"
#include "nsIFactory.h"
#include "nsIDOMWindow.h"
#include "nsReadableUtils.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIServiceManager.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"
#include "nsIPrintSettingsX.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsPDECommon.h"

#ifndef XP_MACOSX
#include "nsILocalFIleMac.h"
#endif

// Printing Progress Includes
#include "nsPrintProgress.h"
#include "nsPrintProgressParams.h"
#include "nsIWebProgressListener.h"

// OS includes
#include <PMApplication.h>
#include <CFPlugIn.h>
#include <Gestalt.h>

// Constants
static const char *kPrintProgressDialogURL = "chrome://global/content/printProgress.xul";

// Globals
static CFPlugInRef gPDEPlugIn = nsnull;

//-----------------------------------------------------------------------------
// Static Helpers
//-----------------------------------------------------------------------------

static CFPlugInRef LoadPDEPlugIn()
{
#ifndef XP_MACOSX
    if (!gPDEPlugIn) {
        
        nsresult rv;    
        nsCOMPtr<nsIProperties> dirService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
        if (!dirService)
            return nsnull;
        nsCOMPtr<nsILocalFileMac> pluginDir;
        rv = dirService->Get(NS_XPCOM_CURRENT_PROCESS_DIR, NS_GET_IID(nsILocalFileMac), getter_AddRefs(pluginDir));
        if (NS_FAILED(rv))
            return nsnull;
        pluginDir->AppendNative(NS_LITERAL_CSTRING("Essential Files"));
        pluginDir->AppendNative(NS_LITERAL_CSTRING("PrintDialogPDE.plugin"));
        FSSpec pluginSpec;
        rv = pluginDir->GetFSSpec(&pluginSpec);
        if (NS_FAILED(rv))
            return nsnull;
            
        OSErr err;
        FSRef pluginFSRef;
        char pathBuf[512];
        
        err = ::FSpMakeFSRef(&pluginSpec, &pluginFSRef);
        if (err)
            return nsnull;
        err = ::FSRefMakePath(&pluginFSRef, (UInt8 *)pathBuf, sizeof(pathBuf)-1);
        if (err)
            return nsnull;
            
        CFStringRef pathRef = ::CFStringCreateWithCString(NULL, pathBuf, kCFStringEncodingUTF8);
        if (!pathRef)
            return nsnull;
        CFURLRef url = ::CFURLCreateWithFileSystemPath(NULL, pathRef, kCFURLPOSIXPathStyle, TRUE);
        if (url)
            gPDEPlugIn = ::CFPlugInCreate(NULL, url);
        
        ::CFRelease(pathRef);    
        ::CFRelease(url);   
    }
#endif
    return gPDEPlugIn;
}

//*****************************************************************************
// nsPrintingPromptService
//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService() :
    mWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID))
{
    NS_INIT_ISUPPORTS();
}

nsPrintingPromptService::~nsPrintingPromptService()
{
}

nsresult nsPrintingPromptService::Init()
{
    return NS_OK;
}

//*****************************************************************************
// nsPrintingPromptService::nsIPrintingPromptService
//*****************************************************************************   

NS_IMETHODIMP 
nsPrintingPromptService::ShowPrintDialog(nsIDOMWindow *parent, nsIWebBrowserPrint *webBrowserPrint, nsIPrintSettings *printSettings)
{
    nsresult rv;
    OSStatus status;
      
    nsCOMPtr<nsIPrintSettingsX> printSettingsX(do_QueryInterface(printSettings));
    if (!printSettingsX)
        return NS_ERROR_NO_INTERFACE;
    
    PMPrintSession  printSession;
    rv = printSettingsX->GetNativePrintSession(&printSession);
    if (NS_FAILED(rv))
        return rv;
    
    PMPageFormat    pageFormat = kPMNoPageFormat;
    rv = printSettingsX->GetPMPageFormat(&pageFormat);
    if (NS_FAILED(rv))
        return rv;

    PMPrintSettings nativePrintSettings = kPMNoPrintSettings;
    rv = printSettingsX->GetPMPrintSettings(&nativePrintSettings);
    if (NS_FAILED(rv))
        return rv;
    
    status = ::PMSessionValidatePageFormat(printSession, pageFormat, kPMDontWantBoolean);
    if (status != noErr)
        return NS_ERROR_FAILURE;
        
    // Reset the print settings to their defaults each time. We don't want to remember
    // the last printed page range or whatever. This is expected Mac behavior.
    status = ::PMSessionDefaultPrintSettings(printSession, nativePrintSettings);
    if (status != noErr)
        return NS_ERROR_FAILURE;
        
    ::InitCursor();

    PRBool  isOn;
    PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
    nsPrintExtensions printData = {false,false,false,false,false,false,false,false};

    // set the values for the plugin here
    printSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    printData.mHaveSelection = isOn;
    printSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
    if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableAll) {
        printData.mHaveFrames = true;
        printData.mHaveFrameSelected = true;
    }
    if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableAsIsAndEach) {
        printData.mHaveFrames = true;
        printData.mHaveFrameSelected = false;
    }
    printSettings->GetShrinkToFit(&isOn);
    printData.mShrinkToFit = isOn;    

    CFPlugInRef pdePlugIn = ::LoadPDEPlugIn();
    status = ::PMSetPrintSettingsExtendedData(nativePrintSettings, kPDE_Creator, sizeof(printData),&printData);

    Boolean accepted;
    status = ::PMSessionPrintDialog(printSession, nativePrintSettings, pageFormat, &accepted);
    if (status == noErr && accepted) {

        UInt32 bytesNeeded;
        int pageRange = -1;
        status = ::PMGetPrintSettingsExtendedData(nativePrintSettings, kPDE_Creator, &bytesNeeded, NULL);
        if (status == noErr && bytesNeeded == sizeof(printData)) {
            status = ::PMGetPrintSettingsExtendedData(nativePrintSettings, kPDE_Creator,&bytesNeeded, &printData);        

            // set the correct data fields
            if (printData.mPrintSelection) {
                printSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);
                pageRange = nsIPrintSettings::kRangeSelection;
            }
            else {
                printSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
                pageRange = nsIPrintSettings::kRangeAllPages;
            }

            if (printData.mPrintFrameAsIs)
                printSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
            if (printData.mPrintSelectedFrame)
                printSettings->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
            if (printData.mPrintFramesSeperatly)
                printSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);

            printSettings->SetShrinkToFit(printData.mShrinkToFit);
        }

        if (pageRange == -1) {
            printSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
            pageRange = nsIPrintSettings::kRangeAllPages;
        }
        if (pageRange != nsIPrintSettings::kRangeSelection) {
            UInt32 firstPage, lastPage;
            status = ::PMGetFirstPage(nativePrintSettings, &firstPage);
            if (status == noErr) {
                status = ::PMGetLastPage(nativePrintSettings, &lastPage);
                if (status == noErr && lastPage != LONG_MAX) {
                    printSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
                    printSettings->SetStartPageRange(firstPage);
                    printSettings->SetEndPageRange(lastPage);
                }
            }
        }
    }
        
    if (!accepted)
        return NS_ERROR_ABORT;

    if (status != noErr)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

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

    *notifyOnOpen = PR_FALSE;

    // If running on OS X, the printing manager displays a nice progress dialog
    // so we don't need to do this. Keeping this code here in order to support
    // running TARGET_CARBON builds on OS 9.
    
    long version;
    if (::Gestalt(gestaltSystemVersion, &version) == noErr && version >= 0x00001000)
        return NS_ERROR_NOT_IMPLEMENTED;
        
    nsPrintProgress* prtProgress = new nsPrintProgress();
    nsresult rv = prtProgress->QueryInterface(NS_GET_IID(nsIPrintProgress), (void**)getter_AddRefs(mPrintProgress));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = prtProgress->QueryInterface(NS_GET_IID(nsIWebProgressListener), (void**)getter_AddRefs(mWebProgressListener));
    NS_ENSURE_SUCCESS(rv, rv);

    nsPrintProgressParams* prtProgressParams = new nsPrintProgressParams();
    rv = prtProgressParams->QueryInterface(NS_GET_IID(nsIPrintProgressParams), (void**)printProgressParams);
    NS_ENSURE_SUCCESS(rv, rv);

    if (printProgressParams) 
    {
        if (mWatcher) 
        {
            nsCOMPtr<nsIDOMWindow> active;
            mWatcher->GetActiveWindow(getter_AddRefs(active));
            nsCOMPtr<nsIDOMWindowInternal> parent(do_QueryInterface(active));
            mPrintProgress->OpenProgressDialog(parent, kPrintProgressDialogURL, *printProgressParams, openDialogObserver, notifyOnOpen);
        }
    }

    *webProgressListener = NS_STATIC_CAST(nsIWebProgressListener*, this);
    NS_ADDREF(*webProgressListener);

    return rv;
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowPageSetup(nsIDOMWindow *parent, nsIPrintSettings *printSettings, nsIObserver *aObs)
{
  nsCOMPtr<nsIPrintSettingsX> printSettingsX(do_QueryInterface(printSettings));
  if (!printSettingsX)
    return NS_ERROR_NO_INTERFACE;
  
  OSStatus status;
    
  PMPrintSession printSession;
  status = ::PMCreateSession(&printSession);
  if (status != noErr)
    return NS_ERROR_FAILURE;
    
  PMPageFormat pageFormat;
  printSettingsX->GetPMPageFormat(&pageFormat);
  if (pageFormat == kPMNoPageFormat) {
    ::PMRelease(printSession);
    return NS_ERROR_FAILURE;
  }
    
  Boolean validated;
  ::PMSessionValidatePageFormat(printSession, pageFormat, &validated);

  ::InitCursor();

  Boolean   accepted = false;
  status = ::PMSessionPageSetupDialog(printSession, pageFormat, &accepted);
  OSStatus tempStatus = ::PMRelease(printSession);
  if (status == noErr)
    status = tempStatus;
    
  if (status != noErr)
    return NS_ERROR_FAILURE;    
  if (!accepted)
    return NS_ERROR_ABORT;

  return NS_OK;
}

NS_IMETHODIMP 
nsPrintingPromptService::ShowPrinterProperties(nsIDOMWindow *parent, const PRUnichar *printerName, nsIPrintSettings *printSettings)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// nsPrintingPromptService::nsIWebProgressListener
//*****************************************************************************   

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
    if (mWebProgressListener) {
      return mWebProgressListener->OnProgressChange(aWebProgress, aRequest, aCurSelfProgress, aMaxSelfProgress, aCurTotalProgress, aMaxTotalProgress);
    }
    return NS_OK;
}

/* void onLocationChange (in nsIWebProgress aWebProgress, in nsIRequest aRequest, in nsIURI location); */
NS_IMETHODIMP 
nsPrintingPromptService::OnLocationChange(nsIWebProgress *aWebProgress, nsIRequest *aRequest, nsIURI *location)
{
    if (mWebProgressListener) {
        return mWebProgressListener->OnLocationChange(aWebProgress, aRequest, location);
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
