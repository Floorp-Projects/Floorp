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

//--------------------------------------------------------
//--------------------------------------------------------
static UINT gFrameSelectedRadioBtn = NULL;

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

    if (howToEnableFrameUI == nsIPrintOptions::kFrameEnableAll) {
      SetRadio(hdlg, rad4, PR_FALSE, PR_FALSE);  // XXX this is just temporary (should be enabled)
      SetRadio(hdlg, rad5, PR_TRUE); 
      SetRadio(hdlg, rad6, PR_FALSE);
      // set default so user doesn't have to actually press on it
      gFrameSelectedRadioBtn = rad5;

    } else if (howToEnableFrameUI == nsIPrintOptions::kFrameEnableAsIsAndEach) {
      SetRadio(hdlg, rad4, PR_FALSE, PR_FALSE);  // XXX this is just temporary (should be enabled)
      SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
      SetRadio(hdlg, rad6, PR_TRUE);
      // set default so user doesn't have to actually press on it
      gFrameSelectedRadioBtn = rad6;


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

  prntdlg.nFromPage           = 1;
  prntdlg.nToPage             = 1;
  prntdlg.nMinPage            = 0;
  prntdlg.nMaxPage            = 1000;
  prntdlg.nCopies             = 1;
  prntdlg.hInstance           = hInstance;
  prntdlg.lCustData           = (DWORD)howToEnableFrameUI;
  prntdlg.lpfnPrintHook       = PrintHookProc;
  prntdlg.lpfnSetupHook       = NULL;
  prntdlg.lpPrintTemplateName = (LPCTSTR)"PRINTDLGNEW";
  prntdlg.lpSetupTemplateName = NULL;
  prntdlg.hPrintTemplate      = NULL;
  prntdlg.hSetupTemplate      = NULL;


  if(PR_TRUE == aQuiet){
    prntdlg.Flags = PD_RETURNDEFAULT;
  }

  BOOL res = ::PrintDlg(&prntdlg);

  if (TRUE == res)
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
