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

#include "nsDeviceContextSpecFactoryW.h"
#include "nsDeviceContextSpecWin.h"
#include <windows.h>
#include <commdlg.h>
#include "nsGfxCIID.h"
#include "plstr.h"
#include "nsIServiceManager.h"
#include "nsIStringBundle.h"

#include "nsIPrintOptions.h"

// For Localization
#include "nsIStringBundle.h"
#include "nsDeviceContext.h"
#include "nsDeviceContextWin.h"

// This is for extending the dialog
#include <dlgs.h>

static NS_DEFINE_CID(kStringBundleServiceCID,  NS_STRINGBUNDLESERVICE_CID);

//----------------------------------------------------------------------------------
// Return localized bundle for resource strings
static nsresult
GetLocalizedBundle(const char * aPropFileName, nsIStringBundle** aStrBundle)
{
  NS_ENSURE_ARG_POINTER(aPropFileName);
  NS_ENSURE_ARG_POINTER(aStrBundle);

  nsresult rv;
  nsCOMPtr<nsIStringBundle> bundle;
  

  // Create bundle
  nsCOMPtr<nsIStringBundleService> stringService = 
    do_GetService(kStringBundleServiceCID, &rv);
  if (NS_SUCCEEDED(rv) && stringService) {
    rv = stringService->CreateBundle(aPropFileName, aStrBundle);
  }
  
  return rv;
}

//--------------------------------------------------------
// Return localized string 
static nsresult
GetLocalizedString(nsIStringBundle* aStrBundle, const char* aKey, nsString& oVal)
{
  NS_ENSURE_ARG_POINTER(aStrBundle);
  NS_ENSURE_ARG_POINTER(aKey);

  // Determine default label from string bundle
  nsXPIDLString valUni;
  nsAutoString key; 
  key.AssignWithConversion(aKey);
  nsresult rv = aStrBundle->GetStringFromName(key.get(), getter_Copies(valUni));
  if (NS_SUCCEEDED(rv) && valUni) {
    oVal.Assign(valUni);
  } else {
    oVal.Truncate();
  }
  return rv;
}

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
// Identifies whcih new radio btn was lciked on
static UINT         gFrameSelectedRadioBtn = NULL;

// Indicates whether the native print dialog was successfully extended
static PRPackedBool gDialogWasExtended     = PR_FALSE;

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
  nsresult rv = GetLocalizedString(aStrBundle, aKey, str);
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
  HWND wnd = ::GetDlgItem (aParent, aId);
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
static void SetRadioOfGroup(HWND aDlg, int aRadId)
{
  int radioIds[] = {rad4, rad5, rad6};
  int numRads = 3;

  for (int i=0;i<numRads;i++) {
    HWND radWnd = ::GetDlgItem(aDlg, radioIds[i]);
    if (radWnd != NULL) {
      ::SendMessage(radWnd, BM_SETCHECK, (WPARAM)(radioIds[i] == aRadId), (LPARAM)0);
    }
  }
}

//--------------------------------------------------------
typedef struct {
  char * mKeyStr;
  long   mKeyId;
} PropKeyInfo;

// These are the control ids used in the dialog and 
// defined by MS-Windows in commdlg.h
static PropKeyInfo gAllPropKeys[] = {
    {"PrintFrames", grp3},
    {"Aslaid", rad4},
    {"selectedframe", rad5},
    {"Eachframe", rad6},
    {NULL, NULL}};

    // This is a list of other controls Ids in the native dialog
    // I am leaving these here for documentation purposes
    //
    //{"Printer", grp4},
    //{"Name", stc6},
    //{"Properties", psh2},
    //{"Status", stc8},
    //{"Type", stc7},
    //{"Where", stc10},
    //{"Comment", stc9},
    //{"Printtofile", chx1},
    //{"Printrange", grp1},
    //{"All", rad1},
    //{"Pages", rad3},
    //{"Selection", rad2},
    //{"from", stc2},
    //{"to", stc3},
    //{"Copies", grp2},
    //{"Numberofcopies", stc5},
    //{"Collate", chx2},
    //{"OK", IDOK},
    //{"Cancel", IDCANCEL},
    //{"stc11", stc11},
    //{"stc12", stc12},
    //{"stc14", stc14},
    //{"stc13", stc13},

//--------------------------------------------------------
//--------------------------------------------------------
//--------------------------------------------------------
//--------------------------------------------------------
// Get the absolute coords of the child windows relative
// to its parent window
static void GetLocalRect(HWND aWnd, RECT& aRect, HWND aParent)
{
  RECT wr;
  ::GetWindowRect(aParent, &wr);

  RECT cr;
  ::GetClientRect(aParent, &cr);

  ::GetWindowRect(aWnd, &aRect);

  int borderH = (wr.bottom-wr.top+1) - (cr.bottom-cr.top+1);
  int borderW = ((wr.right-wr.left+1) - (cr.right-cr.left+1))/2;
  aRect.top    -= wr.top+borderH-borderW;
  aRect.left   -= wr.left+borderW;
  aRect.right  -= wr.left+borderW;
  aRect.bottom -= wr.top+borderH-borderW;
}

//--------------------------------------------------------
// Show or Hide the control
void Show(HWND aWnd, PRBool bState)
{
  if (aWnd) {
    ::ShowWindow(aWnd, bState?SW_SHOW:SW_HIDE);
  }
}

//--------------------------------------------------------
// Create a child window "control"
static HWND CreateControl(LPCTSTR          aType,
                          DWORD            aStyle,
                          HINSTANCE        aHInst, 
                          HWND             aHdlg, 
                          int              aId, 
                          const nsAString& aStr, 
                          const nsRect&    aRect)
{
  char* pStr = nsDeviceContextWin::GetACPString(aStr);
  if (pStr == NULL) return NULL;

  HWND hWnd = ::CreateWindow (aType, pStr,
                              WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | aStyle,
                              aRect.x, aRect.y, aRect.width, aRect.height,
                              (HWND)aHdlg, (HMENU)aId,
                              aHInst, NULL);
  if (hWnd == NULL) return NULL;

  delete [] pStr;

  // get the native font for the dialog and 
  // set it into the new control
  HFONT hFont = (HFONT)::SendMessage(aHdlg, WM_GETFONT, (WPARAM)0, (LPARAM)0);
  if (hFont != NULL) {
    ::SendMessage(hWnd, WM_SETFONT, (WPARAM) hFont, (LPARAM)0);
  }
  return hWnd;
}

//--------------------------------------------------------
// Create a Radio Button
static HWND CreateRadioBtn(HINSTANCE        aHInst, 
                           HWND             aHdlg, 
                           int              aId, 
                           const nsAString& aStr, 
                           const nsRect&    aRect)
{
  return CreateControl("BUTTON", BS_RADIOBUTTON, aHInst, aHdlg, aId, aStr, aRect);
}

//--------------------------------------------------------
// Create a Group Box
static HWND CreateGroupBox(HINSTANCE        aHInst, 
                           HWND             aHdlg, 
                           int              aId, 
                           const nsAString& aStr, 
                           const nsRect&    aRect)
{
  return CreateControl("BUTTON", BS_GROUPBOX, aHInst, aHdlg, aId, aStr, aRect);
}

//--------------------------------------------------------
// Special Hook Procedure for handling the print dialog messages
UINT CALLBACK PrintHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) 
{

  if (uiMsg == WM_COMMAND) {
    UINT id = LOWORD(wParam);
    if (id == rad4 || id == rad5 || id == rad6) {
      gFrameSelectedRadioBtn = id;
      SetRadioOfGroup(hdlg, id);
    }

  } else if (uiMsg == WM_INITDIALOG) {
    PRINTDLG * printDlg = (PRINTDLG *)lParam;
    if (printDlg == NULL) return 0L;

    PRInt16 howToEnableFrameUI = (PRBool)printDlg->lCustData;

    HINSTANCE hInst = (HINSTANCE)::GetWindowLong(hdlg, GWL_HINSTANCE);
    if (hInst == NULL) return 0L;

    // Start by getting the local rects of several of the controls
    // so we can calculate where the new controls are
    HWND wnd = ::GetDlgItem(hdlg, grp1);
    if (wnd == NULL) return 0L;
    RECT dlgRect;
    GetLocalRect(wnd, dlgRect, hdlg);

    wnd = ::GetDlgItem(hdlg, rad1); // this is the top control "All"
    if (wnd == NULL) return 0L;
    RECT rad1Rect;
    GetLocalRect(wnd, rad1Rect, hdlg);

    wnd = ::GetDlgItem(hdlg, rad2); // this is the bottom control "Selection"
    if (wnd == NULL) return 0L;
    RECT rad2Rect;
    GetLocalRect(wnd, rad2Rect, hdlg);

    wnd = ::GetDlgItem(hdlg, rad3); // this is the middle control "Pages"
    if (wnd == NULL) return 0L;
    RECT rad3Rect;
    GetLocalRect(wnd, rad3Rect, hdlg);

    HWND okWnd = ::GetDlgItem(hdlg, IDOK);
    if (okWnd == NULL) return 0L;
    RECT okRect;
    GetLocalRect(okWnd, okRect, hdlg);

    wnd = ::GetDlgItem(hdlg, grp4); // this is the "Print range" groupbox
    if (wnd == NULL) return 0L;
    RECT prtRect;
    GetLocalRect(wnd, prtRect, hdlg);


    // calculate various different "gaps" for layout purposes

    int rbGap     = rad3Rect.top - rad1Rect.bottom;     // gap between radiobtns
    int grpBotGap = dlgRect.bottom - rad2Rect.bottom;   // gap from bottom rb to bottom of grpbox
    int grpGap    = dlgRect.top - prtRect.bottom ;      // gap between group boxes
    int top       = dlgRect.bottom + grpGap;            
    int radHgt    = rad1Rect.bottom - rad1Rect.top + 1; // top of new group box
    int y         = top+(rad1Rect.top-dlgRect.top);     // starting pos of first radio
    int rbWidth   = dlgRect.right - rad1Rect.left - 5;  // measure from rb left to the edge of the groupbox
                                                        // (5 is arbitrary)
    nsRect rect;

    // Create and position the radio buttons
    //
    // If any one control cannot be created then 
    // hide the others and bail out
    //
    rect.SetRect(rad1Rect.left, y, rbWidth,radHgt);
    HWND rad4Wnd = CreateRadioBtn(hInst, hdlg, rad4, NS_LITERAL_STRING("As &laid out on the screen"), rect);
    if (rad4Wnd == NULL) return 0L;
    y += radHgt + rbGap;

    rect.SetRect(rad1Rect.left, y, rbWidth, radHgt);
    HWND rad5Wnd = CreateRadioBtn(hInst, hdlg, rad5, NS_LITERAL_STRING("The selected &frame"), rect);
    if (rad5Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + rbGap;

    rect.SetRect(rad1Rect.left, y, rbWidth, radHgt);
    HWND rad6Wnd = CreateRadioBtn(hInst, hdlg, rad6, NS_LITERAL_STRING("&Each frame separately"), rect);
    if (rad6Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + grpBotGap;

    // Create and position the group box
    rect.SetRect (dlgRect.left, top, dlgRect.right-dlgRect.left+1, y-top+1);
    HWND grpBoxWnd = CreateGroupBox(hInst, hdlg, grp3, NS_LITERAL_STRING("Print Frame"), rect);
    if (grpBoxWnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      Show(rad6Wnd, FALSE); // hide
      return 0L;
    }

    // Here we figure out the old height of the dlg
    // then figure it's gap from the old grpbx to the bottom
    // then size the dlg
    RECT pr, cr; 
    ::GetWindowRect(hdlg, &pr);
    ::GetClientRect(hdlg, &cr);

    int dlgHgt = (cr.bottom - cr.top) + 1;
    int bottomGap = dlgHgt - okRect.bottom;
    pr.bottom += (dlgRect.bottom-dlgRect.top) + grpGap + 1 - (dlgHgt-dlgRect.bottom) + bottomGap;

    ::SetWindowPos(hdlg, NULL, pr.left, pr.top, pr.right-pr.left+1, pr.bottom-pr.top+1, 
                   SWP_NOMOVE|SWP_NOREDRAW|SWP_NOZORDER);

    // figure out the new height of the dialog
    ::GetClientRect(hdlg, &cr);
    dlgHgt = (cr.bottom - cr.top) + 1;
 
    // Reposition the OK and Cancel btns
    int okHgt = okRect.bottom - okRect.top + 1;
    ::SetWindowPos(okWnd, NULL, okRect.left, dlgHgt-bottomGap-okHgt, 0, 0, 
                   SWP_NOSIZE|SWP_NOREDRAW|SWP_NOZORDER);

    HWND cancelWnd = ::GetDlgItem(hdlg, IDCANCEL);
    if (cancelWnd == NULL) return 0L;

    RECT cancelRect;
    GetLocalRect(cancelWnd, cancelRect, hdlg);
    int cancelHgt = cancelRect.bottom - cancelRect.top + 1;
    ::SetWindowPos(cancelWnd, NULL, cancelRect.left, dlgHgt-bottomGap-cancelHgt, 0, 0, 
                   SWP_NOSIZE|SWP_NOREDRAW|SWP_NOZORDER);

    // Localize the new controls in the print dialog
    nsCOMPtr<nsIStringBundle> strBundle;
    if (NS_SUCCEEDED(GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
      PRInt32 i = 0;
      while (gAllPropKeys[i].mKeyStr != NULL) {
        SetText(hdlg, gAllPropKeys[i].mKeyId, strBundle, gAllPropKeys[i].mKeyStr);
        i++;
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

    // Looks like we were able to extend the dialog
    gDialogWasExtended = PR_TRUE;
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
  nsCOMPtr<nsIPrintOptions> printService = 
           do_GetService(kPrintOptionsCID, &rv);

  gDialogWasExtended  = PR_FALSE;

  PRINTDLG  prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLG));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner   = (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW);
  prntdlg.hDevMode    = NULL;
  prntdlg.hDevNames   = NULL;
  prntdlg.hDC         = NULL;
  prntdlg.Flags       = PD_ALLPAGES | PD_RETURNIC | PD_HIDEPRINTTOFILE | PD_USEDEVMODECOPIESANDCOLLATE;

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

  // Determine whether we have a completely native dialog
  // or whether we cshould extend it
  // true  - do only the native
  // false - extend the dialog
  PRPackedBool doExtend = PR_FALSE;
  nsCOMPtr<nsIStringBundle> strBundle;
  if (NS_SUCCEEDED(GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
    nsAutoString doExtendStr;
    if (NS_SUCCEEDED(GetLocalizedString(strBundle, "extend", doExtendStr))) {
      doExtend = doExtendStr.EqualsIgnoreCase("true");
    }
  }

  prntdlg.nFromPage           = 0xFFFF;
  prntdlg.nToPage             = 0xFFFF;
  prntdlg.nMinPage            = 1;
  prntdlg.nMaxPage            = 0xFFFF;
  prntdlg.nCopies             = 1;
  prntdlg.lpfnSetupHook       = NULL;
  prntdlg.lpSetupTemplateName = NULL;
  prntdlg.hPrintTemplate      = NULL;
  prntdlg.hSetupTemplate      = NULL;

  prntdlg.hInstance           = NULL;
  prntdlg.lpPrintTemplateName = NULL;

  if (!doExtend || aQuiet) {
    prntdlg.lCustData         = NULL;
    prntdlg.lpfnPrintHook     = NULL;
  } else {
    // Set up print dialog "hook" procedure for extending the dialog
    prntdlg.lCustData         = (DWORD)howToEnableFrameUI;
    prntdlg.lpfnPrintHook     = PrintHookProc;
    prntdlg.Flags            |= PD_ENABLEPRINTHOOK;
  }

  if(PR_TRUE == aQuiet){
    prntdlg.Flags = PD_ALLPAGES | PD_RETURNDEFAULT | PD_RETURNIC | PD_USEDEVMODECOPIESANDCOLLATE;
  }

  rv = NS_ERROR_FAILURE; // reset

  BOOL result = ::PrintDlg(&prntdlg);

  if (TRUE == result){
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);
    if ( NULL != devnames ) {

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
          // make sure the dialog got extended
          if (gDialogWasExtended) {
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
            // if it didn't get extended then have it default to printing
            // each frame separately
            printService->SetPrintFrameType(nsIPrintOptions::kEachFrameSep);
          }
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

      if (nsnull != devspec){
        //XXX need to QI rather than cast... MMP
        if (NS_OK == ((nsDeviceContextSpecWin *)devspec)->Init(driver, device, prntdlg.hDevMode)){
          aNewSpec = devspec;
          rv = NS_OK;
        }
      }

      //don't free the DEVMODE because the device context spec now owns it...
      ::GlobalUnlock(prntdlg.hDevNames);
      ::GlobalFree(prntdlg.hDevNames);
    }
  } else {
    // print dialog aborted
    rv = NS_ERROR_ABORT;
  }

  return rv;
}
