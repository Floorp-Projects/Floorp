/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsDeviceContextSpecFactoryW.h"
#include "nsDeviceContextSpecWin.h"
#include <windows.h>
#include <commdlg.h>
#include "nsGfxCIID.h"
#include "plstr.h"
#include "nsIServiceManager.h"

#include "nsIPrintOptions.h"

// For Localization
#include "nsIStringBundle.h"
#include "nsDeviceContext.h"
#include "nsDeviceContextWin.h"

// This is for extending the dialog
#include <dlgs.h>

nsDeviceContextSpecFactoryWin :: nsDeviceContextSpecFactoryWin()
{
  NS_INIT_REFCNT();
}

nsDeviceContextSpecFactoryWin :: ~nsDeviceContextSpecFactoryWin()
{
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecFactoryWin, nsIDeviceContextSpecFactory)


NS_IMETHODIMP nsDeviceContextSpecFactoryWin :: Init(void)
{
  return NS_OK;
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
static UINT gFrameSelectedRadioBtn = NULL;

#define PRINTDLG_PROPERTIES "chrome://communicator/locale/gfx/printdialog.properties"
#define TEMPLATE_NAME "PRINTDLGNEW"

//--------------------------------------------------------
// Set a multi-byte string in the control
static void SetTextOnWnd(HWND aControl, const nsString& aStr)
{
  char* pStr = nsDeviceContextWin::GetACPString(aStr);
  if (pStr) {
    ::SetWindowText(aControl, pStr);
    delete [] pStr;
  }
}

//--------------------------------------------------------
// Will get the control and localized string by "key"
static void SetText(HWND             aParent, 
                    UINT             aId, 
                    nsIStringBundle* aStrBundle,
                    const char*      aKey) 
{
  HWND wnd = GetDlgItem (aParent, aId);
  if (!wnd) {
    return;
  }
  nsAutoString str;
  nsresult rv = DeviceContextImpl::GetLocalizedString(aStrBundle, aKey, str);
  if (NS_SUCCEEDED(rv)) {
    SetTextOnWnd(wnd, str);
  }
}

//--------------------------------------------------------
static void SetRadio(HWND         aParent, 
                     UINT         aId, 
                     PRBool       aIsSet,
                     PRBool       isEnabled = PR_TRUE) 
{
  HWND wnd = GetDlgItem (aParent, aId);
  if (!wnd) {
    return;
  }
  if (!isEnabled) {
    ::EnableWindow(wnd, FALSE);
    return;
  }
  ::EnableWindow(wnd, TRUE);
  ::SendMessage(wnd, BM_SETCHECK, (WPARAM)aIsSet, (LPARAM)0);
}

//--------------------------------------------------------
typedef struct {
  char * mKeyStr;
  long   mKeyId;
} PropKeyInfo;

// These are the control ids used in the dialog and 
// defined by MS-Windows in commdlg.h
static PropKeyInfo gAllPropKeys[] = {
    {"Printer", grp4},
    {"Name", stc6},
    {"Properties", psh2},
    {"Status", stc8},
    {"Type", stc7},
    {"Where", stc10},
    {"Comment", stc9},
    {"Printtofile", chx1},
    {"Printrange", grp1},
    {"All", rad1},
    {"Pages", rad3},
    {"Selection", rad2},
    {"from", stc2},
    {"to", stc3},
    {"Copies", grp2},
    {"Numberofcopies", stc5},
    {"Collate", chx2},
    {"PrintFrames", grp3},
    {"Aslaid", rad4},
    {"selectedframe", rad5},
    {"Eachframe", rad6},
    {"OK", IDOK},
    {"Cancel", IDCANCEL},
    {NULL, NULL}};
    //{"stc11", stc11},
    //{"stc12", stc12},
    //{"stc14", stc14},
    //{"stc13", stc13},

//--------------------------------------------------------
UINT CALLBACK PrintHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) 
{
  if (uiMsg == WM_COMMAND) {
    UINT id = LOWORD(wParam);
    if (id == rad4 || id == rad5 || id == rad6) {
      gFrameSelectedRadioBtn = id;
    }

  } else if (uiMsg == WM_INITDIALOG) {
    PRINTDLG *   printDlg           = (PRINTDLG *)lParam;
    PRInt16      howToEnableFrameUI = (PRBool)printDlg->lCustData;
    HWND         collateChk         = GetDlgItem (hdlg, chx2);
    if (collateChk) {
      ::ShowWindow(collateChk, SW_SHOW);
    }

    // Localize the print dialog
    nsCOMPtr<nsIStringBundle> strBundle;
    if (NS_SUCCEEDED(DeviceContextImpl::GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
      PRInt32 i = 0;
      while (gAllPropKeys[i].mKeyStr != NULL) {
        SetText(hdlg, gAllPropKeys[i].mKeyId, strBundle, gAllPropKeys[i].mKeyStr);
        i++;
      }
      nsAutoString titleStr;
      if (NS_SUCCEEDED(DeviceContextImpl::GetLocalizedString(strBundle, "title", titleStr))) {
        SetTextOnWnd(hdlg, titleStr);
      }
    }

    // Set up radio buttons
    if (howToEnableFrameUI == nsIPrintOptions::kFrameEnableAll) {
      SetRadio(hdlg, rad4, PR_TRUE);  
      SetRadio(hdlg, rad5, PR_FALSE); 
      SetRadio(hdlg, rad6, PR_FALSE);
      // set default so user doesn't have to actually press on it
      gFrameSelectedRadioBtn = rad4;

    } else if (howToEnableFrameUI == nsIPrintOptions::kFrameEnableAsIsAndEach) {
      SetRadio(hdlg, rad4, PR_TRUE);  
      SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
      SetRadio(hdlg, rad6, PR_FALSE);
      // set default so user doesn't have to actually press on it
      gFrameSelectedRadioBtn = rad4;


    } else {  // nsIPrintOptions::kFrameEnableNone
      // we are using this function to disabe the group box
      SetRadio(hdlg, grp3, PR_FALSE, PR_FALSE); 
      // now disable radiobuttons
      SetRadio(hdlg, rad4, PR_FALSE, PR_FALSE); 
      SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
      SetRadio(hdlg, rad6, PR_FALSE, PR_FALSE); 
    }
  }
  return 0L;
}

//XXX this method needs to do what the API says...

NS_IMETHODIMP nsDeviceContextSpecFactoryWin :: CreateDeviceContextSpec(nsIWidget *aWidget,
                                                                       nsIDeviceContextSpec *&aNewSpec,
                                                                       PRBool aQuiet)
{
  NS_ENSURE_ARG_POINTER(aWidget);

  nsresult  rv = NS_ERROR_FAILURE;
  NS_WITH_SERVICE(nsIPrintOptions, printService, kPrintOptionsCID, &rv);

  PRINTDLG  prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLG));

  HWND      hWnd      = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  HINSTANCE hInstance = (HINSTANCE)::GetWindowLong(hWnd, GWL_HINSTANCE);

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner = NULL;               //XXX need to find a window here. MMP
  prntdlg.hDevMode = NULL;
  prntdlg.hDevNames = NULL;
  prntdlg.hDC = NULL;
  prntdlg.Flags = PD_ALLPAGES | PD_RETURNIC | PD_HIDEPRINTTOFILE | 
                  PD_ENABLEPRINTTEMPLATE | PD_ENABLEPRINTHOOK | PD_USEDEVMODECOPIESANDCOLLATE;

  // if there is a current selection then enable the "Selection" radio button
  PRInt16 howToEnableFrameUI = nsIPrintOptions::kFrameEnableNone;
  if (printService) {
    PRBool isOn;
    printService->GetPrintOptions(nsIPrintOptions::kPrintOptionsEnableSelectionRB, &isOn);
    if (!isOn) {
      prntdlg.Flags |= PD_NOSELECTION;
    }
    printService->GetHowToEnableFrameUI(&howToEnableFrameUI);
  }

  // Get the template name from the string bundle
  nsAutoString templateName;
  nsCOMPtr<nsIStringBundle> strBundle;
  nsresult res = DeviceContextImpl::GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle));
  if (NS_SUCCEEDED(res)) {
    res = DeviceContextImpl::GetLocalizedString(strBundle, "templateName", templateName);
  }
  if (NS_FAILED(res)) {
    templateName.AssignWithConversion(TEMPLATE_NAME);
  }

  char * tmpStr = templateName.ToNewCString();

  prntdlg.nFromPage           = 0xFFFF;
  prntdlg.nToPage             = 0xFFFF;
  prntdlg.nMinPage            = 1;
  prntdlg.nMaxPage            = 0xFFFF;
  prntdlg.nCopies             = 1;
  prntdlg.hInstance           = hInstance;
  prntdlg.lCustData           = (DWORD)howToEnableFrameUI;
  prntdlg.lpfnPrintHook       = PrintHookProc;
  prntdlg.lpfnSetupHook       = NULL;
  prntdlg.lpPrintTemplateName = (LPCTSTR)tmpStr?tmpStr:TEMPLATE_NAME;
  prntdlg.lpSetupTemplateName = NULL;
  prntdlg.hPrintTemplate      = NULL;
  prntdlg.hSetupTemplate      = NULL;

  if(PR_TRUE == aQuiet){
    prntdlg.Flags = PD_ALLPAGES | PD_RETURNDEFAULT | PD_RETURNIC | PD_USEDEVMODECOPIESANDCOLLATE;
  }

  BOOL result = ::PrintDlg(&prntdlg);

  nsMemory::Free(tmpStr); // free template name

  if (TRUE == result)
  {
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);

    char device[200], driver[200];

    //print something...

    PL_strcpy(device, &(((char *)devnames)[devnames->wDeviceOffset]));
    PL_strcpy(driver, &(((char *)devnames)[devnames->wDriverOffset]));

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    printf("printer: driver %s, device %s  flags: %d\n", driver, device, prntdlg.Flags);
#endif

    // fill the print options with the info from the dialog
    if (printService) {

      if (prntdlg.Flags & PD_SELECTION) {
        printService->SetPrintRange(nsIPrintOptions::kRangeSelection);

      } else if (prntdlg.Flags & PD_PAGENUMS) {
        printService->SetPrintRange(nsIPrintOptions::kRangeSpecifiedPageRange);
        printService->SetStartPageRange(prntdlg.nFromPage);
        printService->SetEndPageRange( prntdlg.nToPage);

      } else { // (prntdlg.Flags & PD_ALLPAGES)
        printService->SetPrintRange(nsIPrintOptions::kRangeAllPages);
      }

      if (howToEnableFrameUI != nsIPrintOptions::kFrameEnableNone) {
        // check to see about the frame radio buttons
        switch (gFrameSelectedRadioBtn) {
          case rad4: 
            printService->SetPrintFrameType(nsIPrintOptions::kFramesAsIs);
            break;
          case rad5: 
            printService->SetPrintFrameType(nsIPrintOptions::kSelectedFrame);
            break;
          case rad6: 
            printService->SetPrintFrameType(nsIPrintOptions::kEachFrameSep);
            break;
        } // switch 
      } else {
        printService->SetPrintFrameType(nsIPrintOptions::kNoFrames);
      }
    }


#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    PRBool  printSelection = prntdlg.Flags & PD_SELECTION;
    PRBool  printAllPages  = prntdlg.Flags & PD_ALLPAGES;
    PRBool  printNumPages  = prntdlg.Flags & PD_PAGENUMS;
    PRInt32 fromPageNum    = 0;
    PRInt32 toPageNum      = 0;

    if (printNumPages) {
      fromPageNum = prntdlg.nFromPage;
      toPageNum   = prntdlg.nToPage;
    } 
    if (printSelection) {
      printf("Printing the selection\n");

    } else if (printAllPages) {
      printf("Printing all the pages\n");

    } else {
      printf("Printing from page no. %d to %d\n", fromPageNum, toPageNum);
    }
#endif
    

    nsIDeviceContextSpec  *devspec = nsnull;

    nsComponentManager::CreateInstance(kDeviceContextSpecCID, nsnull, kIDeviceContextSpecIID, (void **)&devspec);

    if (nsnull != devspec)
    {
      //XXX need to QI rather than cast... MMP
      if (NS_OK == ((nsDeviceContextSpecWin *)devspec)->Init(driver, device, prntdlg.hDevMode))
      {
        aNewSpec = devspec;
        rv = NS_OK;
      }
    }

    //don't free the DEVMODE because the device context spec now owns it...
    ::GlobalUnlock(prntdlg.hDevNames);
    ::GlobalFree(prntdlg.hDevNames);
  }

  return rv;
}
