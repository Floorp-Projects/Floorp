/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *
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

#include "nsDeviceContextSpecMac.h"
#include "prmem.h"
#include "plstr.h"
#include "nsWatchTask.h"
#include "nsIServiceManager.h"
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"

#include "nsGfxUtils.h"

#if !TARGET_CARBON
#include "nsMacResources.h"
#include <Resources.h>
#include <Dialogs.h>
#endif

#if !TARGET_CARBON

enum {
  ePrintSelectionCheckboxID = 1,
  ePrintFrameAsIsCheckboxID,
  ePrintSelectedFrameCheckboxID,
  ePrintAllFramesCheckboxID,
  eDrawFrameID
};



// items to support the additional items for the dialog
#define DITL_ADDITIONS  128

static pascal TPPrDlg   MyJobDlgInit(THPrint);        // Our extention to PrJobInit
static TPPrDlg          gPrtJobDialog;                 // pointer to job dialog 
static long             prFirstItem;                  // our first item in the extended dialog
static PItemUPP         prPItemProc;                  // store the old item handler here
static PRBool           gPrintSelection;
static UserItemUPP      gDrawListUPP = nsnull;


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

#endif

/** -------------------------------------------------------
 *  Construct the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecMac::nsDeviceContextSpecMac()
: mPrtRec(nsnull)
, mPrintManagerOpen(PR_FALSE)
{
  NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destroy the nsDeviceContextSpecMac
 *  @update   dc 12/02/98
 */
nsDeviceContextSpecMac::~nsDeviceContextSpecMac()
{
  ClosePrintManager();

  if (mPrtRec) {
    ::DisposeHandle((Handle)mPrtRec);
    mPrtRec = nsnull;
  }

}

NS_IMPL_ISUPPORTS2(nsDeviceContextSpecMac, nsIDeviceContextSpec, nsIPrintingContext)

#if !TARGET_CARBON

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
static pascal void MyJobItems(TPPrDlg aDialog, short aItemNo)
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
        ::GetDialogItem((DialogPtr)aDialog, firstItem, &itemType, &itemH, &itemBox);
        gPrintSelection = !gPrintSelection;
        ::SetControlValue((ControlHandle)itemH, gPrintSelection);
        break;

      case ePrintFrameAsIsCheckboxID:
      case ePrintSelectedFrameCheckboxID:
      case ePrintAllFramesCheckboxID:
        for (i=ePrintFrameAsIsCheckboxID; i<=ePrintAllFramesCheckboxID; i++){
          ::GetDialogItem((DialogPtr)aDialog, firstItem+i-1, &itemType, &itemH, &itemBox);
          ::SetControlValue((ControlHandle)itemH, i==myItem);
        }
        break;
        
      default:
        break;
    }
  } else {
    // chain to standard Item handler
    CallPItemProc(prPItemProc, (DialogPtr)aDialog, aItemNo);
    
    if (aDialog->fDone)
    {
      nsCOMPtr<nsIPrintOptions> printOptionsService = do_GetService("@mozilla.org/gfx/printoptions;1");
      // cleanup and set the print options to what we want
      if (printOptionsService)
      {
        // print selection
        ::GetDialogItem((DialogPtr)aDialog, firstItem+ePrintSelectionCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          printOptionsService->SetPrintRange(nsIPrintOptions::kRangeSelection);
        } else {
          printOptionsService->SetPrintRange(nsIPrintOptions::kRangeAllPages);
        }
        
        // print frames as is
        ::GetDialogItem((DialogPtr)aDialog, firstItem+ePrintFrameAsIsCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          printOptionsService->SetPrintFrameType(nsIPrintOptions::kFramesAsIs);
        }
        
        // selected frame
        ::GetDialogItem((DialogPtr)aDialog, firstItem+ePrintSelectedFrameCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          printOptionsService->SetPrintFrameType(nsIPrintOptions::kSelectedFrame);
        }
        
        // print all frames
        ::GetDialogItem((DialogPtr)aDialog, firstItem+ePrintAllFramesCheckboxID-1, &itemType, &itemH, &itemBox);
        value = ::GetControlValue((ControlHandle)itemH);
        if (1==value){
          printOptionsService->SetPrintFrameType(nsIPrintOptions::kEachFrameSep);
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
nsresult        theResult = NS_ERROR_FAILURE;
short           firstItem;
ItemListHandle  myAppendDITLH;
ItemListHandle  dlg_Item_List;

  dlg_Item_List = (ItemListHandle)((DialogPeek)aDialog)->items;
  firstItem = (**dlg_Item_List).max_index+2;

  theResult = nsMacResources::OpenLocalResourceFile();
  if (theResult == NS_OK) {
    myAppendDITLH = (ItemListHandle)::GetResource('DITL', aDITLID);
    if (nsnull == myAppendDITLH) {
      // some sort of error
      theResult = NS_ERROR_FAILURE;
    } else {
      ::AppendDITL((DialogPtr)aDialog, (Handle)myAppendDITLH, appendDITLBottom);
      ::ReleaseResource((Handle) myAppendDITLH);
    }
  theResult = nsMacResources::CloseLocalResourceFile();
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
  PRInt16 howToEnableFrameUI = nsIPrintOptions::kFrameEnableNone;

  prFirstItem = AppendToDialog(gPrtJobDialog, DITL_ADDITIONS);

  nsCOMPtr<nsIPrintOptions> printOptionsService = do_GetService("@mozilla.org/gfx/printoptions;1");

  if (printOptionsService) {
    printOptionsService->GetPrintOptions(nsIPrintOptions::kPrintOptionsEnableSelectionRB, &isOn);
    printOptionsService->GetHowToEnableFrameUI(&howToEnableFrameUI);
  }

  ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+ePrintSelectionCheckboxID-1, &itemType, &itemH, &itemBox);
  if ( isOn ) {
    ::HiliteControl((ControlHandle)itemH, 0);
  } else {
    ::HiliteControl((ControlHandle)itemH, 255); 
  }
  
  gPrintSelection = PR_FALSE;
  ::SetControlValue((ControlHandle) itemH, gPrintSelection);

  if (howToEnableFrameUI == nsIPrintOptions::kFrameEnableAll) {
    for (i = ePrintFrameAsIsCheckboxID; i <= ePrintAllFramesCheckboxID; i++){
      ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+i-1, &itemType, &itemH, &itemBox);
      ::SetControlValue((ControlHandle) itemH, (i==2));
      ::HiliteControl((ControlHandle)itemH, 0);
    }
  }
  else if (howToEnableFrameUI == nsIPrintOptions::kFrameEnableAsIsAndEach) {
    for (i = ePrintFrameAsIsCheckboxID; i <= ePrintAllFramesCheckboxID; i++){
      ::GetDialogItem((DialogPtr) gPrtJobDialog, prFirstItem+i-1, &itemType, &itemH, &itemBox);
      ::SetControlValue((ControlHandle) itemH, (i==2));
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
  gPrtJobDialog->pItemProc = NewPItemUPP(MyJobItems);


  // attach a draw routine
  gDrawListUPP = NewUserItemProc(MyBBoxDraw);
  ::GetDialogItem((DialogPtr)gPrtJobDialog, prFirstItem+eDrawFrameID-1, &itemType, &itemH, &itemBox);
  ::SetDialogItem((DialogPtr)gPrtJobDialog, prFirstItem+eDrawFrameID-1, itemType, (Handle)gDrawListUPP, &itemBox);

  return gPrtJobDialog;
}

#endif


/** -------------------------------------------------------
 *  Initialize the nsDeviceContextSpecMac
 *  @update   dc 05/04/2001
 */
NS_IMETHODIMP nsDeviceContextSpecMac::Init(PRBool aQuiet)
{
#if !TARGET_CARBON

  THPrint     hPrintRec;    // handle to print record
  GrafPtr     oldport;
  PDlgInitUPP theInitProcPtr;

  ::GetPort(&oldport);
  
  nsresult rv;
  nsCOMPtr<nsIPrintOptions> printOptionsService = do_GetService("@mozilla.org/gfx/printoptions;1", &rv);
  if (NS_FAILED(rv)) return rv;

  // open the printing manager
  ::PrOpen();
  if (::PrError() != noErr)
    return NS_ERROR_FAILURE;

  mPrintManagerOpen = PR_TRUE;
  
  // Allocate a print record
  hPrintRec = (THPrint)::NewHandleClear(sizeof(TPrint));
  if (!hPrintRec) return NS_ERROR_OUT_OF_MEMORY;
  
  StHandleOwner printRecOwner((Handle)hPrintRec);
  
  // see if we have a print record
  void* printRecordData = nsnull;
  rv = printOptionsService->GetNativeData(nsIPrintOptions::kNativeDataPrintRecord, &printRecordData);
  if (NS_SUCCEEDED(rv) && printRecordData)
  {
    ::BlockMoveData(printRecordData, *hPrintRec, sizeof(TPrint));
  }
  else
  {
    // fill in default values
    ::PrintDefault(hPrintRec);
  }

  if (printRecordData)
  {
    nsMemory::Free(printRecordData);
    printRecordData = nsnull;
  }
  
  // make sure the print record is valid
  ::PrValidate(hPrintRec);
  if (::PrError() != noErr)
    return NS_ERROR_FAILURE;
  
  // get pointer to invisible job dialog box
  gPrtJobDialog = ::PrJobInit(hPrintRec);
  if (::PrError() != noErr)
    return NS_ERROR_FAILURE;      

  // create a UUP  for the dialog init procedure
  theInitProcPtr = NewPDlgInitProc(MyJobDlgInit);
  if (!theInitProcPtr)
    return NS_ERROR_FAILURE;      

  // standard print dialog, if true print
  nsWatchTask::GetTask().Suspend();
	::InitCursor();
	
  // put up the print dialog
  if (::PrDlgMain(hPrintRec, theInitProcPtr))
  {
    // have the print record
    rv = NS_OK;
    printRecOwner.ClearHandle(false);
    mPrtRec = hPrintRec;
  }
  else
  {
    // don't print
    ::SetPort(oldport); 
    rv = NS_ERROR_ABORT;
  }
  
  // clean up our dialog routines
  DisposePItemUPP(gPrtJobDialog->pItemProc);
  gPrtJobDialog->pItemProc = prPItemProc;        // put back the old just in case
  
  DisposePItemUPP(theInitProcPtr);
  DisposePItemUPP(gDrawListUPP);
  gDrawListUPP = nsnull;
      
  nsWatchTask::GetTask().Resume();
  return rv;

#endif    /* TARGET_CARBON */

  return NS_ERROR_FAILURE;
}

/** -------------------------------------------------------
 * Closes the printmanager if it is open.
 * @update   dc 12/03/98
 */
NS_IMETHODIMP nsDeviceContextSpecMac::ClosePrintManager()
{
  PRBool  isPMOpen;
  PrintManagerOpen(&isPMOpen);
  if (isPMOpen) {
#if !TARGET_CARBON
    ::PrClose();
#endif
  }
  
  return NS_OK;
}  

NS_IMETHODIMP nsDeviceContextSpecMac::BeginDocument()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::EndDocument()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::BeginPage()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::EndPage()
{
    nsresult rv = NS_OK;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::GetPrinterResolution(double* aResolution)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    return rv;
}

NS_IMETHODIMP nsDeviceContextSpecMac::GetPageRect(double* aTop, double* aLeft, double* aBottom, double* aRight)
{
    nsresult rv = NS_ERROR_NOT_IMPLEMENTED;
    return rv;
}
