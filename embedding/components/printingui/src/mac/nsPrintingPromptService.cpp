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
 *   Don Cone <dcone@netscape.com>
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
#include "nsIPrintSettingsMac.h"
#include "nsComponentResContext.h"
#include "nsWatchTask.h"

// Printing Progress Includes
#include "nsPrintProgress.h"
#include "nsPrintProgressParams.h"
#include "nsIWebProgressListener.h"

// OS Includes
#include <Printing.h>
#include <Dialogs.h>
#include <Appearance.h>
#include <Resources.h>

// Constants
static const char *kPrintProgressDialogURL = "chrome://global/content/printProgress.xul";

//-----------------------------------------------------------------------------
// Dialog Extension Code
//-----------------------------------------------------------------------------

// Types and Defines

// items to support the additional items for the dialog
#define DITL_ADDITIONS  128

enum {
  ePrintSelectionCheckboxID = 1,
  ePrintFrameAsIsCheckboxID,
  ePrintSelectedFrameCheckboxID,
  ePrintAllFramesCheckboxID,
  eDrawFrameID
};

typedef struct dialog_item_struct {
  Handle  handle;       // handle or procedure pointer for this item */
  Rect    bounds;         // display rectangle for this item */
  char    type;           // item type - 1 */
  char    data[1];        // length byte of data */
} DialogItem, *DialogItemPtr, **DialogItemHandle;
 
typedef struct append_item_list_struct {
  short max_index;      // number of items - 1 
  DialogItem  items[1]; // first item in the array
} ItemList, *ItemListPtr, **ItemListHandle;

// Static Variables - used by the dialog callbacks - no choice but globals

//static pascal TPPrDlg   MyJobDlgInit(THPrint);        // Our extention to PrJobInit
static TPPrDlg          gPrtJobDialog;                 // pointer to job dialog 
static long             prFirstItem;                  // our first item in the extended dialog
static PItemUPP         prPItemProc;                  // store the old item handler here
static PRBool           gPrintSelection;
static PItemUPP         gPrtJobDialogItemProc;
static UserItemUPP      gDrawListUPP = nsnull;
static nsIPrintSettings	*gPrintSettings = nsnull;

// Routines

/** -------------------------------------------------------
 *  this is a drawing procedure for the user item.. this draws a box around the frameset radio buttons
 *  @update   dc 12/02/98
 */
static pascal void MyBBoxDraw(WindowPtr theWindow, short aItemNo)
{
  short   itemType;
  Rect    itemBox;
  Handle  itemH;

  ::GetDialogItem((DialogPtr)gPrtJobDialog, prFirstItem + eDrawFrameID-1, &itemType, &itemH, &itemBox);
  
  // use appearance if possible
  if ((long)DrawThemeSecondaryGroup != kUnresolvedCFragSymbolAddress)
    ::DrawThemeSecondaryGroup(&itemBox, kThemeStateActive);
  else
    ::FrameRect(&itemBox);
}


/** -------------------------------------------------------
 *  this is the dialog hook, takes care of setting the dialog items
 *  @update   dc 12/02/98
 */
static pascal void MyJobItems(DialogPtr aDialog, short aItemNo)
{
short   myItem, firstItem, i, itemType;
short   value;
Rect    itemBox;
Handle  itemH;

  firstItem = prFirstItem;
  
  myItem = aItemNo-firstItem+1;
  if (myItem>0) {
    switch (myItem) {
      case ePrintSelectionCheckboxID:
        ::GetDialogItem(aDialog, firstItem, &itemType, &itemH, &itemBox);
        gPrintSelection = !gPrintSelection;
        ::SetControlValue((ControlHandle)itemH, gPrintSelection);
        break;

      case ePrintFrameAsIsCheckboxID:
      case ePrintSelectedFrameCheckboxID:
      case ePrintAllFramesCheckboxID:
        for (i=ePrintFrameAsIsCheckboxID; i<=ePrintAllFramesCheckboxID; i++){
          ::GetDialogItem(aDialog, firstItem+i-1, &itemType, &itemH, &itemBox);
          ::SetControlValue((ControlHandle)itemH, i==myItem);
        }
        break;
        
      default:
        break;
    }
  } else {
    // chain to standard Item handler
    CallPItemProc(prPItemProc, aDialog, aItemNo);
    
    if (((TPPrDlg)aDialog)->fDone)
    {
      // cleanup and set the print options to what we want
      if (gPrintSettings)
      {
        // print selection
        ::GetDialogItem(aDialog, firstItem+ePrintSelectionCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          gPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);
        } else {
          gPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
        }
        
        // print frames as is
        ::GetDialogItem(aDialog, firstItem+ePrintFrameAsIsCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          gPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
        }
        
        // selected frame
        ::GetDialogItem(aDialog, firstItem+ePrintSelectedFrameCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          gPrintSettings->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
        }
        
        // print all frames
        ::GetDialogItem(aDialog, firstItem+ePrintAllFramesCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          gPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
        }        
      }
    }
  }
}

/** -------------------------------------------------------
 *  Append DITL items to the dialog
 *  @update   dc 05/04/2001
 */
static PRInt32  AppendToDialog(TPPrDlg  aDialog, PRInt32  aDITLID)
{
  short           firstItem;
  ItemListHandle  myAppendDITLH;
  ItemListHandle  dlg_Item_List;

  dlg_Item_List = (ItemListHandle)((DialogPeek)aDialog)->items;
  firstItem = (**dlg_Item_List).max_index+2;

  nsComponentResourceContext resContext;
  if (resContext.BecomeCurrent()) { // destructor restores
    myAppendDITLH = (ItemListHandle)::GetResource('DITL', aDITLID);
    NS_ASSERTION(myAppendDITLH, "Failed to get DITL items");
    if (myAppendDITLH) {
      ::AppendDITL((DialogPtr)aDialog, (Handle)myAppendDITLH, appendDITLBottom);
      ::ReleaseResource((Handle) myAppendDITLH);
    }
  }

  return firstItem;
}


/** -------------------------------------------------------
 *  Initialize the print dialogs additional items
 *  @update   dc 05/04/2001
 */
static pascal TPPrDlg MyJobDlgInit(THPrint aHPrint)
{
  PRInt32 i;
  short   itemType;
  Handle  itemH;
  Rect    itemBox;
  PRBool  isOn;
  PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;

  prFirstItem = AppendToDialog(gPrtJobDialog, DITL_ADDITIONS);

  if (gPrintSettings) {
    gPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    gPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
  }

  ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+ePrintSelectionCheckboxID-1, &itemType, &itemH, &itemBox);
  if ( isOn ) {
    ::HiliteControl((ControlHandle)itemH, 0);
  } else {
    ::HiliteControl((ControlHandle)itemH, 255); 
  }
  
  gPrintSelection = PR_FALSE;
  ::SetControlValue((ControlHandle) itemH, gPrintSelection);

  if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableAll) {
    for (i = ePrintFrameAsIsCheckboxID; i <= ePrintAllFramesCheckboxID; i++){
      ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+i-1, &itemType, &itemH, &itemBox);
      ::SetControlValue((ControlHandle) itemH, (i==4));
      ::HiliteControl((ControlHandle)itemH, 0);
    }
  }
  else if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableAsIsAndEach) {
    for (i = ePrintFrameAsIsCheckboxID; i <= ePrintAllFramesCheckboxID; i++){
      ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+i-1, &itemType, &itemH, &itemBox);
      ::SetControlValue((ControlHandle) itemH, (i==4));
      if ( i == 3){
        ::HiliteControl((ControlHandle)itemH, 255);
      }
    }
  }
  else {
    for (i = ePrintFrameAsIsCheckboxID; i <= ePrintAllFramesCheckboxID; i++){
      ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+i-1, &itemType, &itemH, &itemBox);
      ::SetControlValue((ControlHandle) itemH, FALSE);
      ::HiliteControl((ControlHandle)itemH, 255); 
    }
  }
  
  // attach our handler
  prPItemProc = gPrtJobDialog->pItemProc;
  gPrtJobDialog->pItemProc = gPrtJobDialogItemProc = NewPItemUPP(MyJobItems);


  // attach a draw routine
  gDrawListUPP = NewUserItemProc(MyBBoxDraw);
  ::GetDialogItem((DialogPtr)gPrtJobDialog, prFirstItem+eDrawFrameID-1, &itemType, &itemH, &itemBox);
  ::SetDialogItem((DialogPtr)gPrtJobDialog, prFirstItem+eDrawFrameID-1, itemType, (Handle)gDrawListUPP, &itemBox);

  return gPrtJobDialog;
}


//*****************************************************************************
// nsPrintingPromptService
//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsPrintingPromptService, nsIPrintingPromptService, nsIWebProgressListener)

nsPrintingPromptService::nsPrintingPromptService() :
    mWatcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID))
{
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
  THPrint     printRecH = nsnull;    // local copy of nsIPrintSettingsMac's data
  GrafPtr     oldport;
  PDlgInitUPP theInitProcPtr;

	gPrintSettings = printSettings;

  ::GetPort(&oldport);
  
  nsresult rv;
  nsCOMPtr<nsIPrintSettingsMac> printSettingsMac(do_QueryInterface(printSettings));
  if (!printSettingsMac)
    return NS_ERROR_NO_INTERFACE;

  theInitProcPtr = NewPDlgInitProc(MyJobDlgInit);
  if (!theInitProcPtr)
    return NS_ERROR_FAILURE;
      
  // Get the print record from the settings
  rv = printSettingsMac->GetTHPrint(&printRecH);
  if (NS_FAILED(rv))
    return rv;

  // open the printing manager
  ::PrOpen();
  if (::PrError() != noErr) {
    ::DisposeHandle((Handle)printRecH);
    return NS_ERROR_FAILURE;
  }
  
  // make sure the print record is valid
  ::PrValidate(printRecH);
  if (::PrError() != noErr) {
    ::DisposeHandle((Handle)printRecH);
    ::PrClose();
    return NS_ERROR_FAILURE;
  }
  
  // get pointer to invisible job dialog box
  gPrtJobDialog = ::PrJobInit(printRecH);
  if (::PrError() != noErr) {
    ::DisposeHandle((Handle)printRecH);
    ::PrClose();
    return NS_ERROR_FAILURE;
  }

  // create a UUP  for the dialog init procedure
  theInitProcPtr = NewPDlgInitProc(MyJobDlgInit);
  if (!theInitProcPtr)
    return NS_ERROR_FAILURE;      

  nsWatchTask::GetTask().Suspend();
  ::InitCursor();
	
  // put up the print dialog
  if (::PrDlgMain(printRecH, theInitProcPtr))
  {
    // have the print record
    rv = NS_OK;
    printSettingsMac->SetTHPrint(printRecH);
  }
  else
  {
    // don't print
    ::SetPort(oldport); 
    rv = NS_ERROR_ABORT;
  }
  
  ::DisposeHandle((Handle)printRecH);
  
  // clean up our dialog routines
  DisposePItemUPP(gPrtJobDialogItemProc);
  gPrtJobDialogItemProc = nsnull;
  
  DisposePItemUPP(theInitProcPtr);
  DisposePItemUPP(gDrawListUPP);
  gDrawListUPP = nsnull;
      
  nsWatchTask::GetTask().Resume();
  return rv;
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
  nsCOMPtr<nsIPrintSettingsMac> printSettingsMac(do_QueryInterface(printSettings));
  if (!printSettingsMac)
    return NS_ERROR_NO_INTERFACE;

  // open the printing manager
  ::PrOpen();
  if(::PrError() != noErr)
    return NS_ERROR_FAILURE;
  
  THPrint printRecH;
  nsresult rv;
  
  rv = printSettingsMac->GetTHPrint(&printRecH);
  if (NS_FAILED(rv))
    return rv;
    
  ::PrValidate(printRecH);
  NS_ASSERTION(::PrError() == noErr, "PrValidate error");

  nsWatchTask::GetTask().Suspend();
  ::InitCursor();
  Boolean   dialogOK = ::PrStlDialog(printRecH);		// open up and process the style record
  nsWatchTask::GetTask().Resume();
  
  OSErr err = ::PrError();
  ::PrClose();

  if (dialogOK)
    rv = printSettingsMac->SetTHPrint(printRecH);
      
  if (err != noErr)
    return NS_ERROR_FAILURE;
  if (!dialogOK)
    return NS_ERROR_ABORT;
    
  return rv;
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
