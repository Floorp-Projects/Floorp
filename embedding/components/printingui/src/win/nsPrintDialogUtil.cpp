/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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

#ifdef MOZ_REQUIRE_CURRENT_SDK
#undef WINVER
#define WINVER 0x0500
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x500
#endif

/* -------------------------------------------------------------------
To Build This:

  You need to add this to the the makefile.win in mozilla/content/base/src:

	.\$(OBJDIR)\nsFlyOwnPrintDialog.obj	\


  And this to the makefile.win in mozilla/content/build:

WIN_LIBS=                                       \
        winspool.lib                           \
        comctl32.lib                           \
        comdlg32.lib

---------------------------------------------------------------------- */

#include "prmem.h"
#include "plstr.h"
#include <windows.h>
#include <tchar.h>

#include <unknwn.h>
#include <commdlg.h>

#include "nsIWebBrowserPrint.h"
#include "nsString.h"
#include "nsIServiceManager.h"
#include "nsReadableUtils.h"
#include "nsIPrintSettings.h"
#include "nsIPrintSettingsWin.h"
#include "nsIPrintOptions.h"

#include "nsRect.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch.h"

#include "nsCRT.h"
#include "prenv.h" /* for PR_GetEnv */

#include <windows.h>
#include <winspool.h> 

// For Localization
#include "nsIStringBundle.h"

// For NS_CopyUnicodeToNative
#include "nsNativeCharsetUtils.h"

// This is for extending the dialog
#include <dlgs.h>

// For PrintDlgEx
// needed because there are unicode/ansi versions of this routine
// and we need to make sure we get the correct one.
#define GetPrintDlgExQuoted "PrintDlgExA"

// Default labels for the radio buttons
static const char* kAsLaidOutOnScreenStr = "As &laid out on the screen";
static const char* kTheSelectedFrameStr  = "The selected &frame";
static const char* kEachFrameSeparately  = "&Each frame separately";


//-----------------------------------------------
// Global Data
//-----------------------------------------------
// Identifies which new radio btn was cliked on
static UINT gFrameSelectedRadioBtn = 0;

// Indicates whether the native print dialog was successfully extended
static PRPackedBool gDialogWasExtended     = PR_FALSE;

#define PRINTDLG_PROPERTIES "chrome://global/locale/printdialog.properties"

static HWND gParentWnd = NULL;

//******************************************************
// Define native paper sizes
//******************************************************
typedef struct {
  short  mPaperSize; // native enum
  double mWidth;
  double mHeight;
  PRBool mIsInches;
} NativePaperSizes;

// There are around 40 default print sizes defined by Windows
const NativePaperSizes kPaperSizes[] = {
  {DMPAPER_LETTER,    8.5,   11.0,  PR_TRUE},
  {DMPAPER_LEGAL,     8.5,   14.0,  PR_TRUE},
  {DMPAPER_A4,        210.0, 297.0, PR_FALSE},
  {DMPAPER_TABLOID,   11.0,  17.0,  PR_TRUE},
  {DMPAPER_LEDGER,    17.0,  11.0,  PR_TRUE},
  {DMPAPER_STATEMENT, 5.5,   8.5,   PR_TRUE},
  {DMPAPER_EXECUTIVE, 7.25,  10.5,  PR_TRUE},
  {DMPAPER_A3,        297.0, 420.0, PR_FALSE},
  {DMPAPER_A5,        148.0, 210.0, PR_FALSE},
  {DMPAPER_CSHEET,    17.0,  22.0,  PR_TRUE},  
  {DMPAPER_DSHEET,    22.0,  34.0,  PR_TRUE},  
  {DMPAPER_ESHEET,    34.0,  44.0,  PR_TRUE},  
  {DMPAPER_LETTERSMALL, 8.5, 11.0,  PR_TRUE},  
  {DMPAPER_A4SMALL,   210.0, 297.0, PR_FALSE}, 
  {DMPAPER_B4,        250.0, 354.0, PR_FALSE}, 
  {DMPAPER_B5,        182.0, 257.0, PR_FALSE},
  {DMPAPER_FOLIO,     8.5,   13.0,  PR_TRUE},
  {DMPAPER_QUARTO,    215.0, 275.0, PR_FALSE},
  {DMPAPER_10X14,     10.0,  14.0,  PR_TRUE},
  {DMPAPER_11X17,     11.0,  17.0,  PR_TRUE},
  {DMPAPER_NOTE,      8.5,   11.0,  PR_TRUE},  
  {DMPAPER_ENV_9,     3.875, 8.875, PR_TRUE},  
  {DMPAPER_ENV_10,    40.125, 9.5,  PR_TRUE},  
  {DMPAPER_ENV_11,    4.5,   10.375, PR_TRUE},  
  {DMPAPER_ENV_12,    4.75,  11.0,  PR_TRUE},  
  {DMPAPER_ENV_14,    5.0,   11.5,  PR_TRUE},  
  {DMPAPER_ENV_DL,    110.0, 220.0, PR_FALSE}, 
  {DMPAPER_ENV_C5,    162.0, 229.0, PR_FALSE}, 
  {DMPAPER_ENV_C3,    324.0, 458.0, PR_FALSE}, 
  {DMPAPER_ENV_C4,    229.0, 324.0, PR_FALSE}, 
  {DMPAPER_ENV_C6,    114.0, 162.0, PR_FALSE}, 
  {DMPAPER_ENV_C65,   114.0, 229.0, PR_FALSE}, 
  {DMPAPER_ENV_B4,    250.0, 353.0, PR_FALSE}, 
  {DMPAPER_ENV_B5,    176.0, 250.0, PR_FALSE}, 
  {DMPAPER_ENV_B6,    176.0, 125.0, PR_FALSE}, 
  {DMPAPER_ENV_ITALY, 110.0, 230.0, PR_FALSE}, 
  {DMPAPER_ENV_MONARCH,  3.875,  7.5, PR_TRUE},  
  {DMPAPER_ENV_PERSONAL, 3.625,  6.5, PR_TRUE},  
  {DMPAPER_FANFOLD_US,   14.875, 11.0, PR_TRUE},  
  {DMPAPER_FANFOLD_STD_GERMAN, 8.5, 12.0, PR_TRUE},  
  {DMPAPER_FANFOLD_LGL_GERMAN, 8.5, 13.0, PR_TRUE},  
};
const PRInt32 kNumPaperSizes = 41;

//----------------------------------------------------------------------------------
static PRBool 
CheckForExtendedDialog()
{
#ifdef MOZ_REQUIRE_CURRENT_SDK
  HMODULE lib = GetModuleHandle("comdlg32.dll");
  if ( lib ) {
    return GetProcAddress(lib, GetPrintDlgExQuoted);
  }
#endif
  return PR_FALSE;
}

//----------------------------------------------------------------------------------
// Map an incoming size to a Windows Native enum in the DevMode
static void 
MapPaperSizeToNativeEnum(LPDEVMODEW aDevMode,
                         PRInt16   aType, 
                         double    aW, 
                         double    aH)
{

#ifdef DEBUG_rods
  BOOL doingOrientation = aDevMode->dmFields & DM_ORIENTATION;
  BOOL doingPaperSize   = aDevMode->dmFields & DM_PAPERSIZE;
  BOOL doingPaperLength = aDevMode->dmFields & DM_PAPERLENGTH;
  BOOL doingPaperWidth  = aDevMode->dmFields & DM_PAPERWIDTH;
#endif

  const double kThreshold = 0.05;
  for (PRInt32 i=0;i<kNumPaperSizes;i++) {
    double width  = kPaperSizes[i].mWidth;
    double height = kPaperSizes[i].mHeight;
    if (aW < width+kThreshold && aW > width-kThreshold && 
        aH < height+kThreshold && aH > height-kThreshold) {
      aDevMode->dmPaperSize = kPaperSizes[i].mPaperSize;
      aDevMode->dmFields &= ~DM_PAPERLENGTH;
      aDevMode->dmFields &= ~DM_PAPERWIDTH;
      aDevMode->dmFields |= DM_PAPERSIZE;
      return;
    }
  }

  short width  = 0;
  short height = 0;
  if (aType == nsIPrintSettings::kPaperSizeInches) {
    width  = short(NS_TWIPS_TO_MILLIMETERS(NS_INCHES_TO_TWIPS(float(aW))) / 10);
    height = short(NS_TWIPS_TO_MILLIMETERS(NS_INCHES_TO_TWIPS(float(aH))) / 10);

  } else if (aType == nsIPrintSettings::kPaperSizeMillimeters) {
    width  = short(aW / 10.0);
    height = short(aH / 10.0);
  } else {
    return; // don't set anything
  }

  // width and height is in 
  aDevMode->dmPaperSize   = 0;
  aDevMode->dmPaperWidth  = width;
  aDevMode->dmPaperLength = height;

  aDevMode->dmFields |= DM_PAPERSIZE;
  aDevMode->dmFields |= DM_PAPERLENGTH;
  aDevMode->dmFields |= DM_PAPERWIDTH;
}

//----------------------------------------------------------------------------------
// Setup Paper Size & Orientation options into the DevMode
// 
static void 
SetupDevModeFromSettings(LPDEVMODEW aDevMode, nsIPrintSettings* aPrintSettings)
{
  // Setup paper size
  if (aPrintSettings) {
    PRInt16 type;
    aPrintSettings->GetPaperSizeType(&type);
    if (type == nsIPrintSettings::kPaperSizeNativeData) {
      PRInt16 paperEnum;
      aPrintSettings->GetPaperData(&paperEnum);
      aDevMode->dmPaperSize = paperEnum;
      aDevMode->dmFields &= ~DM_PAPERLENGTH;
      aDevMode->dmFields &= ~DM_PAPERWIDTH;
      aDevMode->dmFields |= DM_PAPERSIZE;
    } else {
      PRInt16 unit;
      double width, height;
      aPrintSettings->GetPaperSizeUnit(&unit);
      aPrintSettings->GetPaperWidth(&width);
      aPrintSettings->GetPaperHeight(&height);
      MapPaperSizeToNativeEnum(aDevMode, unit, width, height);
    }

    // Setup Orientation
    PRInt32 orientation;
    aPrintSettings->GetOrientation(&orientation);
    aDevMode->dmOrientation = orientation == nsIPrintSettings::kPortraitOrientation?DMORIENT_PORTRAIT:DMORIENT_LANDSCAPE;
    aDevMode->dmFields |= DM_ORIENTATION;

    // Setup Number of Copies
    PRInt32 copies;
    aPrintSettings->GetNumCopies(&copies);
    aDevMode->dmCopies = copies;
    aDevMode->dmFields |= DM_COPIES;

  }

}

//----------------------------------------------------------------------------------
// Helper Function - Free and reallocate the string
static nsresult 
SetPrintSettingsFromDevMode(nsIPrintSettings* aPrintSettings, 
                            LPDEVMODEW         aDevMode)
{
  if (aPrintSettings == nsnull) {
    return NS_ERROR_FAILURE;
  }

  aPrintSettings->SetIsInitializedFromPrinter(PR_TRUE);
  if (aDevMode->dmFields & DM_ORIENTATION) {
    PRInt32 orientation  = aDevMode->dmOrientation == DMORIENT_PORTRAIT?
                           nsIPrintSettings::kPortraitOrientation:nsIPrintSettings::kLandscapeOrientation;
    aPrintSettings->SetOrientation(orientation);
  }

  // Setup Number of Copies
  if (aDevMode->dmFields & DM_COPIES) {
    aPrintSettings->SetNumCopies(PRInt32(aDevMode->dmCopies));
  }

  // Scaling
  // Since we do the scaling, grab their value and reset back to 100
  if (aDevMode->dmFields & DM_SCALE) {
    double origScale = 1.0;
    aPrintSettings->GetScaling(&origScale);
    double scale = double(aDevMode->dmScale) / 100.0f;
    if (origScale == 1.0 || scale != 1.0) {
      aPrintSettings->SetScaling(scale);
    }
    aDevMode->dmScale = 100;
    // To turn this on you must change where the mPrt->mShrinkToFit is being set in the DocumentViewer
    //aPrintSettings->SetShrinkToFit(PR_FALSE);
  }

  if (aDevMode->dmFields & DM_PAPERSIZE) {
    aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeNativeData);
    aPrintSettings->SetPaperData(aDevMode->dmPaperSize);
    for (PRInt32 i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeMillimeters);
        break;
      }
    }

  } else if (aDevMode->dmFields & DM_PAPERLENGTH && aDevMode->dmFields & DM_PAPERWIDTH) {
    PRBool found = PR_FALSE;
    for (PRInt32 i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
        aPrintSettings->SetPaperWidth(kPaperSizes[i].mWidth);
        aPrintSettings->SetPaperHeight(kPaperSizes[i].mHeight);
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeMillimeters);
        found = PR_TRUE;
        break;
      }
    }
    if (!found) {
      return NS_ERROR_FAILURE;
    }
  } else {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

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
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
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

//--------------------------------------------------------
// Set a multi-byte string in the control
static void SetTextOnWnd(HWND aControl, const nsString& aStr)
{
  nsCAutoString text;
  if (NS_SUCCEEDED(NS_CopyUnicodeToNative(aStr, text))) {
    ::SetWindowText(aControl, text.get());
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
  const char * mKeyStr;
  long   mKeyId;
} PropKeyInfo;

// These are the control ids used in the dialog and 
// defined by MS-Windows in commdlg.h
static PropKeyInfo gAllPropKeys[] = {
    {"printFramesTitleWindows", grp3},
    {"asLaidOutWindows", rad4},
    {"selectedFrameWindows", rad5},
    {"separateFramesWindows", rad6},
    {NULL, NULL}};

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
static void Show(HWND aWnd, PRBool bState)
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
                          const nsIntRect& aRect)
{
  nsCAutoString str;
  if (NS_FAILED(NS_CopyUnicodeToNative(aStr, str)))
    return NULL;

  HWND hWnd = ::CreateWindow (aType, str.get(),
                              WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | aStyle,
                              aRect.x, aRect.y, aRect.width, aRect.height,
                              (HWND)aHdlg, (HMENU)aId,
                              aHInst, NULL);
  if (hWnd == NULL) return NULL;

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
                           const char*      aStr, 
                           const nsIntRect& aRect)
{
  nsString cStr;
  cStr.AssignWithConversion(aStr);
  return CreateControl("BUTTON", BS_RADIOBUTTON, aHInst, aHdlg, aId, cStr, aRect);
}

//--------------------------------------------------------
// Create a Group Box
static HWND CreateGroupBox(HINSTANCE        aHInst, 
                           HWND             aHdlg, 
                           int              aId, 
                           const nsAString& aStr, 
                           const nsIntRect& aRect)
{
  return CreateControl("BUTTON", BS_GROUPBOX, aHInst, aHdlg, aId, aStr, aRect);
}

//--------------------------------------------------------
// Localizes and initializes the radio buttons and group
static void InitializeExtendedDialog(HWND hdlg, PRInt16 aHowToEnableFrameUI) 
{
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
  if (aHowToEnableFrameUI == nsIPrintSettings::kFrameEnableAll) {
    SetRadio(hdlg, rad4, PR_FALSE);  
    SetRadio(hdlg, rad5, PR_TRUE); 
    SetRadio(hdlg, rad6, PR_FALSE);
    // set default so user doesn't have to actually press on it
    gFrameSelectedRadioBtn = rad5;

  } else if (aHowToEnableFrameUI == nsIPrintSettings::kFrameEnableAsIsAndEach) {
    SetRadio(hdlg, rad4, PR_FALSE);  
    SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
    SetRadio(hdlg, rad6, PR_TRUE);
    // set default so user doesn't have to actually press on it
    gFrameSelectedRadioBtn = rad6;


  } else {  // nsIPrintSettings::kFrameEnableNone
    // we are using this function to disabe the group box
    SetRadio(hdlg, grp3, PR_FALSE, PR_FALSE); 
    // now disable radiobuttons
    SetRadio(hdlg, rad4, PR_FALSE, PR_FALSE); 
    SetRadio(hdlg, rad5, PR_FALSE, PR_FALSE); 
    SetRadio(hdlg, rad6, PR_FALSE, PR_FALSE); 
  }

}


//--------------------------------------------------------
// Special Hook Procedure for handling the print dialog messages
static UINT CALLBACK PrintHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) 
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

    PRInt16 howToEnableFrameUI = (PRInt16)printDlg->lCustData;

    HINSTANCE hInst = (HINSTANCE)::GetWindowLongPtr(hdlg, GWLP_HINSTANCE);
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
    nsIntRect rect;

    // Create and position the radio buttons
    //
    // If any one control cannot be created then 
    // hide the others and bail out
    //
    rect.SetRect(rad1Rect.left, y, rbWidth,radHgt);
    HWND rad4Wnd = CreateRadioBtn(hInst, hdlg, rad4, kAsLaidOutOnScreenStr, rect);
    if (rad4Wnd == NULL) return 0L;
    y += radHgt + rbGap;

    rect.SetRect(rad1Rect.left, y, rbWidth, radHgt);
    HWND rad5Wnd = CreateRadioBtn(hInst, hdlg, rad5, kTheSelectedFrameStr, rect);
    if (rad5Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + rbGap;

    rect.SetRect(rad1Rect.left, y, rbWidth, radHgt);
    HWND rad6Wnd = CreateRadioBtn(hInst, hdlg, rad6, kEachFrameSeparately, rect);
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
    // then figure its gap from the old grpbx to the bottom
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

    // localize and initialize the groupbox and radiobuttons
    InitializeExtendedDialog(hdlg, howToEnableFrameUI);

    // Looks like we were able to extend the dialog
    gDialogWasExtended = PR_TRUE;
  }
  return 0L;
}

//----------------------------------------------------------------------------------
// Returns a Global Moveable Memory Handle to a DevMode
// from the Printer by the name of aPrintName
//
// NOTE:
//   This function assumes that aPrintName has already been converted from 
//   unicode
//
static HGLOBAL CreateGlobalDevModeAndInit(LPCWSTR aPrintName, nsIPrintSettings* aPS)
{
  HGLOBAL hGlobalDevMode = NULL;

  HANDLE hPrinter = NULL;
  // const cast kludge for silly Win32 api's
  LPWSTR printName = const_cast<wchar_t*>(aPrintName);
  BOOL status = ::OpenPrinterW(printName, &hPrinter, NULL);
  if (status) {

    LPDEVMODEW  pNewDevMode;
    DWORD       dwNeeded, dwRet;

    // Get the buffer size
    dwNeeded = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, NULL, NULL, 0);
    if (dwNeeded == 0) {
      return NULL;
    }

    // Allocate a buffer of the correct size.
    pNewDevMode = (LPDEVMODEW)::HeapAlloc (::GetProcessHeap(), HEAP_ZERO_MEMORY, dwNeeded);
    if (!pNewDevMode) return NULL;

    hGlobalDevMode = (HGLOBAL)::GlobalAlloc(GHND, dwNeeded);
    if (!hGlobalDevMode) {
      ::HeapFree(::GetProcessHeap(), 0, pNewDevMode);
      return NULL;
    }

    dwRet = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, pNewDevMode, NULL, DM_OUT_BUFFER);

    if (dwRet != IDOK) {
      ::HeapFree(::GetProcessHeap(), 0, pNewDevMode);
      ::GlobalFree(hGlobalDevMode);
      ::ClosePrinter(hPrinter);
      return NULL;
    }

    // Lock memory and copy contents from DEVMODE (current printer)
    // to Global Memory DEVMODE
    LPDEVMODEW devMode = (DEVMODEW *)::GlobalLock(hGlobalDevMode);
    if (devMode) {
      memcpy(devMode, pNewDevMode, dwNeeded);
      // Initialize values from the PrintSettings
      SetupDevModeFromSettings(devMode, aPS);

      // Sets back the changes we made to the DevMode into the Printer Driver
      dwRet = ::DocumentPropertiesW(gParentWnd, hPrinter, printName, devMode, devMode, DM_IN_BUFFER | DM_OUT_BUFFER);
      if (dwRet != IDOK) {
        ::GlobalUnlock(hGlobalDevMode);
        ::GlobalFree(hGlobalDevMode);
        ::HeapFree(::GetProcessHeap(), 0, pNewDevMode);
        ::ClosePrinter(hPrinter);
         return NULL;
      }

      ::GlobalUnlock(hGlobalDevMode);
    } else {
      ::GlobalFree(hGlobalDevMode);
      hGlobalDevMode = NULL;
    }

    ::HeapFree(::GetProcessHeap(), 0, pNewDevMode);

    ::ClosePrinter(hPrinter);

  } else {
    return NULL;
  }

  return hGlobalDevMode;
}

//------------------------------------------------------------------
// helper
static PRUnichar * GetDefaultPrinterNameFromGlobalPrinters()
{
  PRUnichar * printerName = nsnull;
  nsCOMPtr<nsIPrinterEnumerator> prtEnum = do_GetService("@mozilla.org/gfx/printerenumerator;1");
  if (prtEnum) {
    prtEnum->GetDefaultPrinterName(&printerName);
  }
  return printerName;
}

// Determine whether we have a completely native dialog
// or whether we cshould extend it
static PRBool ShouldExtendPrintDialog()
{
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefs->GetBranch(nsnull, getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, PR_TRUE);

  PRBool result;
  rv = prefBranch->GetBoolPref("print.extend_native_print_dialog", &result);
  NS_ENSURE_SUCCESS(rv, PR_TRUE);
  return result;
}

//------------------------------------------------------------------
// Displays the native Print Dialog
static nsresult 
ShowNativePrintDialog(HWND              aHWnd,
                      nsIPrintSettings* aPrintSettings)
{
  //NS_ENSURE_ARG_POINTER(aHWnd);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  gDialogWasExtended  = PR_FALSE;

  HGLOBAL hGlobalDevMode = NULL;
  HGLOBAL hDevNames      = NULL;

  // Get the Print Name to be used
  PRUnichar * printerName;
  aPrintSettings->GetPrinterName(&printerName);

  // If there is no name then use the default printer
  if (!printerName || (printerName && !*printerName)) {
    printerName = GetDefaultPrinterNameFromGlobalPrinters();
  } else {
    HANDLE hPrinter = NULL;
    if(!::OpenPrinterW(const_cast<wchar_t*>(printerName), &hPrinter, NULL)) {
      // If the last used printer is not found, we should use default printer.
      printerName = GetDefaultPrinterNameFromGlobalPrinters();
    } else {
      ::ClosePrinter(hPrinter);
    }
  }

  NS_ASSERTION(printerName, "We have to have a printer name");
  if (!printerName) return NS_ERROR_FAILURE;

  // Now create a DEVNAMES struct so the the dialog is initialized correctly.

  PRUint32 len = wcslen(printerName);
  hDevNames = (HGLOBAL)::GlobalAlloc(GHND, sizeof(wchar_t) * (len + 1) + 
                                     sizeof(DEVNAMES));
  DEVNAMES* pDevNames = (DEVNAMES*)::GlobalLock(hDevNames);
  pDevNames->wDriverOffset = sizeof(DEVNAMES)/sizeof(wchar_t);
  pDevNames->wDeviceOffset = sizeof(DEVNAMES)/sizeof(wchar_t);
  pDevNames->wOutputOffset = sizeof(DEVNAMES)/sizeof(wchar_t)+len;
  pDevNames->wDefault      = 0;

  memcpy(pDevNames+1, printerName, (len + 1) * sizeof(wchar_t));
  ::GlobalUnlock(hDevNames);

  // Create a Moveable Memory Object that holds a new DevMode
  // from the Printer Name
  // The PRINTDLG.hDevMode requires that it be a moveable memory object
  // NOTE: We only need to free hGlobalDevMode when the dialog is cancelled
  // When the user prints, it comes back in the printdlg struct and 
  // is used and cleaned up later
  hGlobalDevMode = CreateGlobalDevModeAndInit(printerName, aPrintSettings);

  // Prepare to Display the Print Dialog
  PRINTDLGW  prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLGW));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner   = aHWnd;
  prntdlg.hDevMode    = hGlobalDevMode;
  prntdlg.hDevNames   = hDevNames;
  prntdlg.hDC         = NULL;
  prntdlg.Flags       = PD_ALLPAGES | PD_RETURNIC | PD_USEDEVMODECOPIESANDCOLLATE;

  // if there is a current selection then enable the "Selection" radio button
  PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
  PRBool isOn;
  aPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
  if (!isOn) {
    prntdlg.Flags |= PD_NOSELECTION;
  }
  aPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);

  PRInt32 pg = 1;
  aPrintSettings->GetStartPageRange(&pg);
  prntdlg.nFromPage           = pg;
  
  aPrintSettings->GetEndPageRange(&pg);
  prntdlg.nToPage             = pg;

  prntdlg.nMinPage            = 1;
  prntdlg.nMaxPage            = 0xFFFF;
  prntdlg.nCopies             = 1;
  prntdlg.lpfnSetupHook       = NULL;
  prntdlg.lpSetupTemplateName = NULL;
  prntdlg.hPrintTemplate      = NULL;
  prntdlg.hSetupTemplate      = NULL;

  prntdlg.hInstance           = NULL;
  prntdlg.lpPrintTemplateName = NULL;

  if (!ShouldExtendPrintDialog()) {
    prntdlg.lCustData         = NULL;
    prntdlg.lpfnPrintHook     = NULL;
  } else {
    // Set up print dialog "hook" procedure for extending the dialog
    prntdlg.lCustData         = (DWORD)howToEnableFrameUI;
    prntdlg.lpfnPrintHook     = (LPPRINTHOOKPROC)PrintHookProc;
    prntdlg.Flags            |= PD_ENABLEPRINTHOOK;
  }

  BOOL result = ::PrintDlgW(&prntdlg);

  if (TRUE == result) {
    // check to make sure we don't have any NULL pointers
    NS_ENSURE_TRUE(aPrintSettings && prntdlg.hDevMode, NS_ERROR_FAILURE);

    if (prntdlg.hDevNames == NULL) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }
    // Lock the deviceNames and check for NULL
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);
    if (devnames == NULL) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }

    wchar_t* device = &(((wchar_t *)devnames)[devnames->wDeviceOffset]);
    wchar_t* driver = &(((wchar_t *)devnames)[devnames->wDriverOffset]);

    // Check to see if the "Print To File" control is checked
    // then take the name from devNames and set it in the PrintSettings
    //
    // NOTE:
    // As per Microsoft SDK documentation the returned value offset from
    // devnames->wOutputOffset is either "FILE:" or NULL
    // if the "Print To File" checkbox is checked it MUST be "FILE:"
    // We assert as an extra safety check.
    if (prntdlg.Flags & PD_PRINTTOFILE) {
      wchar_t* fileName = &(((wchar_t *)devnames)[devnames->wOutputOffset]);
      NS_ASSERTION(wcscmp(fileName, L"FILE:") == 0, "FileName must be `FILE:`");
      aPrintSettings->SetToFileName(fileName);
      aPrintSettings->SetPrintToFile(PR_TRUE);
    } else {
      // clear "print to file" info
      aPrintSettings->SetPrintToFile(PR_FALSE);
      aPrintSettings->SetToFileName(nsnull);
    }

    nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(aPrintSettings));
    if (!psWin) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }

    // Setup local Data members
    psWin->SetDeviceName(device);
    psWin->SetDriverName(driver);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    wprintf(L"printer: driver %s, device %s  flags: %d\n", driver, device, prntdlg.Flags);
#endif
    // fill the print options with the info from the dialog

    aPrintSettings->SetPrinterName(device);

    if (prntdlg.Flags & PD_SELECTION) {
      aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);

    } else if (prntdlg.Flags & PD_PAGENUMS) {
      aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
      aPrintSettings->SetStartPageRange(prntdlg.nFromPage);
      aPrintSettings->SetEndPageRange(prntdlg.nToPage);

    } else { // (prntdlg.Flags & PD_ALLPAGES)
      aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
    }

    if (howToEnableFrameUI != nsIPrintSettings::kFrameEnableNone) {
      // make sure the dialog got extended
      if (gDialogWasExtended) {
        // check to see about the frame radio buttons
        switch (gFrameSelectedRadioBtn) {
          case rad4: 
            aPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
            break;
          case rad5: 
            aPrintSettings->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
            break;
          case rad6: 
            aPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
            break;
        } // switch
      } else {
        // if it didn't get extended then have it default to printing
        // each frame separately
        aPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
      }
    } else {
      aPrintSettings->SetPrintFrameType(nsIPrintSettings::kNoFrames);
    }
    // Unlock DeviceNames
    ::GlobalUnlock(prntdlg.hDevNames);

    // Transfer the settings from the native data to the PrintSettings
    LPDEVMODEW devMode = (LPDEVMODEW)::GlobalLock(prntdlg.hDevMode);
    if (devMode == NULL) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }
    psWin->SetDevMode(devMode); // copies DevMode
    SetPrintSettingsFromDevMode(aPrintSettings, devMode);
    ::GlobalUnlock(prntdlg.hDevMode);

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
    
  } else {
    aPrintSettings->SetIsCancelled(PR_TRUE);
    if (hGlobalDevMode) ::GlobalFree(hGlobalDevMode);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}


#ifdef MOZ_REQUIRE_CURRENT_SDK
//------------------------------------------------------------------
// Callback for Property Sheet
static BOOL APIENTRY PropSheetCallBack(HWND hdlg, UINT uiMsg, UINT wParam, LONG lParam)
{
  if (uiMsg == WM_COMMAND) {
    UINT id = LOWORD(wParam);
    if (id == rad4 || id == rad5 || id == rad6) {
      gFrameSelectedRadioBtn = id;
      SetRadioOfGroup(hdlg, id);
    }

  } else if (uiMsg == WM_INITDIALOG) {
    // Create the groupbox and Radiobuttons on the "Options" Property Sheet

    // We temporarily borrowed the global value for initialization
    // now clear it before the dialog appears
    PRInt16 howToEnableFrameUI = gFrameSelectedRadioBtn;
    gFrameSelectedRadioBtn     = 0;

    HINSTANCE hInst = (HINSTANCE)::GetWindowLongPtr(hdlg, GWLP_HINSTANCE);
    if (hInst == NULL) return 0L;

    // Get default font for the dialog & then its font metrics
    // we need the text height to determine the height of the radio buttons
    TEXTMETRIC metrics;
    HFONT hFont = (HFONT)::SendMessage(hdlg, WM_GETFONT, (WPARAM)0, (LPARAM)0);
    HDC localDC = ::GetDC(hdlg);
    ::SelectObject(localDC, (HGDIOBJ)hFont);
    ::GetTextMetrics(localDC, &metrics);
    ::ReleaseDC(hdlg, localDC);

    // calculate various different "gaps" for layout purposes
     RECT dlgr; 
    ::GetWindowRect(hdlg, &dlgr);

    int horzGap    = 5;                                 // generic horz gap
    int vertGap    = 5;                                 // generic vert gap
    int rbGap      = metrics.tmHeight / 2;               // gap between radiobtns
    int top        = vertGap*2;                            // start at the top
    int radHgt     = metrics.tmHeight;                   // top of new group box
    int y          = top;                                // starting pos of first radio
    int x          = horzGap*2;
    int rbWidth    = dlgr.right - dlgr.left - (5*horzGap);  
    int grpWidth   = dlgr.right - dlgr.left - (2*horzGap);  

    nsRect rect;

    // Create and position the radio buttons
    //
    // If any one control cannot be created then 
    // hide the others and bail out
    //
    x += horzGap*2;
    y += vertGap + metrics.tmHeight;
    rect.SetRect(x, y, rbWidth,radHgt);
    HWND rad4Wnd = CreateRadioBtn(hInst, hdlg, rad4, kAsLaidOutOnScreenStr, rect);
    if (rad4Wnd == NULL) return 0L;
    y += radHgt + rbGap;

    rect.SetRect(x, y, rbWidth, radHgt);
    HWND rad5Wnd = CreateRadioBtn(hInst, hdlg, rad5, kTheSelectedFrameStr, rect);
    if (rad5Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + rbGap;

    rect.SetRect(x, y, rbWidth, radHgt);
    HWND rad6Wnd = CreateRadioBtn(hInst, hdlg, rad6, kEachFrameSeparately, rect);
    if (rad6Wnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      return 0L;
    }
    y += radHgt + (vertGap*2);

    x -= horzGap*2;
    // Create and position the group box
    rect.SetRect (x, top, grpWidth, y-top+1);
    HWND grpBoxWnd = CreateGroupBox(hInst, hdlg, grp3, NS_LITERAL_STRING("Print Frame"), rect);
    if (grpBoxWnd == NULL) {
      Show(rad4Wnd, FALSE); // hide
      Show(rad5Wnd, FALSE); // hide
      Show(rad6Wnd, FALSE); // hide
      return 0L;
    }

    // localize and initialize the groupbox and radiobuttons
    InitializeExtendedDialog(hdlg, howToEnableFrameUI);

    // Looks like we were able to extend the dialog
    gDialogWasExtended = PR_TRUE;
  }
  return 0L;
}

//------------------------------------------------------------------
// Creates the "Options" Property Sheet
static HPROPSHEETPAGE ExtendPrintDialog(HWND aHWnd, char* aTitle)
{
  // The resource "OPTPROPSHEET" comes out of the widget/build/widget.rc file
  HINSTANCE hInst = (HINSTANCE)::GetWindowLongPtr(aHWnd, GWLP_HINSTANCE);
  PROPSHEETPAGE psp;
  memset(&psp, 0, sizeof(PROPSHEETPAGE));
  psp.dwSize      = sizeof(PROPSHEETPAGE);
  psp.dwFlags     = PSP_USETITLE | PSP_PREMATURE;
  psp.hInstance   = hInst;
  psp.pszTemplate = "OPTPROPSHEET";
  psp.pfnDlgProc  = PropSheetCallBack;
  psp.pszTitle    = aTitle?aTitle:"Options";

  HPROPSHEETPAGE newPropSheet = ::CreatePropertySheetPage(&psp);
  return newPropSheet;

}

//------------------------------------------------------------------
// Displays the native Print Dialog
static nsresult 
ShowNativePrintDialogEx(HWND              aHWnd,
                        nsIPrintSettings* aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aHWnd);
  NS_ENSURE_ARG_POINTER(aPrintSettings);

  nsresult  rv = NS_ERROR_FAILURE;
  gDialogWasExtended  = PR_FALSE;

  // Create a Moveable Memory Object that holds a new DevMode
  // from the Printer Name
  // The PRINTDLG.hDevMode requires that it be a moveable memory object
  // NOTE: We only need to free hGlobalDevMode when the dialog is cancelled
  // When the user prints, it comes back in the printdlg struct and 
  // is used and cleaned up later
  PRUnichar * printerName;
  aPrintSettings->GetPrinterName(&printerName);
  HGLOBAL hGlobalDevMode = NULL;
  if (printerName) {
    NS_ENSURE_SUCCESS(rv, rv);
    hGlobalDevMode = CreateGlobalDevModeAndInit(printerName, aPrintSettings);
  }

  // Prepare to Display the Print Dialog
  PRINTDLGEX  prntdlg;
  memset(&prntdlg, 0, sizeof(PRINTDLGEX));

  prntdlg.lStructSize = sizeof(prntdlg);
  prntdlg.hwndOwner   = aHWnd;
  prntdlg.hDevMode    = hGlobalDevMode;
  prntdlg.Flags       = PD_ALLPAGES | PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE |
                        PD_NOCURRENTPAGE;
  prntdlg.nStartPage  = START_PAGE_GENERAL;

  // if there is a current selection then enable the "Selection" radio button
  PRInt16 howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
  if (aPrintSettings != nsnull) {
    PRBool isOn;
    aPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
    if (!isOn) {
      prntdlg.Flags |= PD_NOSELECTION;
    }
    aPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);
  }

  // At the moment we can only support one page range
  // from all the documentation I can find, it appears that this 
  // will get cleanup automatically when the struct goes away
  const int kNumPageRanges     = 1;
  LPPRINTPAGERANGE pPageRanges = (LPPRINTPAGERANGE) GlobalAlloc(GPTR, kNumPageRanges * sizeof(PRINTPAGERANGE));
  if (!pPageRanges)
      return E_OUTOFMEMORY;

  prntdlg.nPageRanges    = 0;
  prntdlg.nMaxPageRanges = kNumPageRanges;
  prntdlg.lpPageRanges   = pPageRanges;
  prntdlg.nMinPage       = 1;
  prntdlg.nMaxPage       = 0xFFFF;
  prntdlg.nCopies        = 1;

  if (ShouldExtendPrintDialog()) {
    // lLcalize the Property Sheet (Tab) title
    nsCAutoString title;
    nsString optionsStr;
    if (NS_SUCCEEDED(GetLocalizedString(strBundle, "optionsTitleWindows", optionsStr))) {
      // Failure here just means a blank string
      NS_CopyUnicodeToNative(optionsStr, title);
    }

    // Temporarily borrow this variable for setting up the radiobuttons
    // if we don't use this, we will need to define a new global var
    gFrameSelectedRadioBtn = howToEnableFrameUI;
    HPROPSHEETPAGE psp[1];
    psp[0] = ExtendPrintDialog(aHWnd, title.get());
    prntdlg.nPropertyPages      = 1;
    prntdlg.lphPropertyPages    = psp;
  }

  HRESULT result = ::PrintDlgEx(&prntdlg);

  if (S_OK == result && (prntdlg.dwResultAction == PD_RESULT_PRINT)) {

    // check to make sure we don't have any NULL pointers
    NS_ENSURE_TRUE(aPrintSettings && prntdlg.hDevMode, NS_ERROR_FAILURE);

    if (prntdlg.hDevNames == NULL) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }
    // Lock the deviceNames and check for NULL
    DEVNAMES *devnames = (DEVNAMES *)::GlobalLock(prntdlg.hDevNames);
    if (devnames == NULL) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }

    char* device = &(((char *)devnames)[devnames->wDeviceOffset]);
    char* driver = &(((char *)devnames)[devnames->wDriverOffset]);

    // Check to see if the "Print To File" control is checked
    // then take the name from devNames and set it in the PrintSettings
    //
    // NOTE:
    // As per Microsoft SDK documentation the returned value offset from
    // devnames->wOutputOffset is either "FILE:" or NULL
    // if the "Print To File" checkbox is checked it MUST be "FILE:"
    // We assert as an extra safety check.
    if (prntdlg.Flags & PD_PRINTTOFILE) {
      char* fileName = &(((char *)devnames)[devnames->wOutputOffset]);
      NS_ASSERTION(strcmp(fileName, "FILE:") == 0, "FileName must be `FILE:`");
      aPrintSettings->SetToFileName(NS_ConvertASCIItoUTF16(fileName).get());
      aPrintSettings->SetPrintToFile(PR_TRUE);
    } else {
      // clear "print to file" info
      aPrintSettings->SetPrintToFile(PR_FALSE);
      aPrintSettings->SetToFileName(nsnull);
    }

    nsCOMPtr<nsIPrintSettingsWin> psWin(do_QueryInterface(aPrintSettings));
    if (!psWin) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }

    // Setup local Data members
    psWin->SetDeviceName(device);
    psWin->SetDriverName(driver);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    printf("printer: driver %s, device %s  flags: %d\n", driver, device, prntdlg.Flags);
#endif
    ::GlobalUnlock(prntdlg.hDevNames);

    // fill the print options with the info from the dialog
    if (aPrintSettings != nsnull) {

      if (prntdlg.Flags & PD_SELECTION) {
        aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSelection);

      } else if (prntdlg.Flags & PD_PAGENUMS) {
        aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeSpecifiedPageRange);
        aPrintSettings->SetStartPageRange(pPageRanges->nFromPage);
        aPrintSettings->SetEndPageRange(pPageRanges->nToPage);

      } else { // (prntdlg.Flags & PD_ALLPAGES)
        aPrintSettings->SetPrintRange(nsIPrintSettings::kRangeAllPages);
      }

      if (howToEnableFrameUI != nsIPrintSettings::kFrameEnableNone) {
        // make sure the dialog got extended
        if (gDialogWasExtended) {
          // check to see about the frame radio buttons
          switch (gFrameSelectedRadioBtn) {
            case rad4: 
              aPrintSettings->SetPrintFrameType(nsIPrintSettings::kFramesAsIs);
              break;
            case rad5: 
              aPrintSettings->SetPrintFrameType(nsIPrintSettings::kSelectedFrame);
              break;
            case rad6: 
              aPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
              break;
          } // switch
        } else {
          // if it didn't get extended then have it default to printing
          // each frame separately
          aPrintSettings->SetPrintFrameType(nsIPrintSettings::kEachFrameSep);
        }
      } else {
        aPrintSettings->SetPrintFrameType(nsIPrintSettings::kNoFrames);
      }
    }

    // Unlock DeviceNames
    ::GlobalUnlock(prntdlg.hDevNames);

    // Transfer the settings from the native data to the PrintSettings
    LPDEVMODEW devMode = (LPDEVMODEW)::GlobalLock(prntdlg.hDevMode);
    if (devMode == NULL) {
      ::GlobalFree(hGlobalDevMode);
      return NS_ERROR_FAILURE;
    }
    psWin->SetDevMode(devMode); // copies DevMode
    SetPrintSettingsFromDevMode(aPrintSettings, devMode);
    ::GlobalUnlock(prntdlg.hDevMode);

#if defined(DEBUG_rods) || defined(DEBUG_dcone)
    PRBool  printSelection = prntdlg.Flags & PD_SELECTION;
    PRBool  printAllPages  = prntdlg.Flags & PD_ALLPAGES;
    PRBool  printNumPages  = prntdlg.Flags & PD_PAGENUMS;
    PRInt32 fromPageNum    = 0;
    PRInt32 toPageNum      = 0;

    if (printNumPages) {
      fromPageNum = pPageRanges->nFromPage;
      toPageNum   = pPageRanges->nToPage;
    } 
    if (printSelection) {
      printf("Printing the selection\n");

    } else if (printAllPages) {
      printf("Printing all the pages\n");

    } else {
      printf("Printing from page no. %d to %d\n", fromPageNum, toPageNum);
    }
#endif
    
  } else {
    if (hGlobalDevMode) ::GlobalFree(hGlobalDevMode);
    return NS_ERROR_ABORT;
  }

  ::GlobalFree(pPageRanges);

  return NS_OK;
}
#endif // MOZ_REQUIRE_CURRENT_SDK

//------------------------------------------------------------------
static void 
PrepareForPrintDialog(nsIWebBrowserPrint* aWebBrowserPrint, nsIPrintSettings* aPS)
{
  NS_ASSERTION(aWebBrowserPrint, "Can't be null");
  NS_ASSERTION(aPS, "Can't be null");

  PRBool isFramesetDocument;
  PRBool isFramesetFrameSelected;
  PRBool isIFrameSelected;
  PRBool isRangeSelection;

  aWebBrowserPrint->GetIsFramesetDocument(&isFramesetDocument);
  aWebBrowserPrint->GetIsFramesetFrameSelected(&isFramesetFrameSelected);
  aWebBrowserPrint->GetIsIFrameSelected(&isIFrameSelected);
  aWebBrowserPrint->GetIsRangeSelection(&isRangeSelection);

  // Setup print options for UI
  if (isFramesetDocument) {
    if (isFramesetFrameSelected) {
      aPS->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableAll);
    } else {
      aPS->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableAsIsAndEach);
    }
  } else {
    aPS->SetHowToEnableFrameUI(nsIPrintSettings::kFrameEnableNone);
  }

  // Now determine how to set up the Frame print UI
  aPS->SetPrintOptions(nsIPrintSettings::kEnableSelectionRB, isRangeSelection || isIFrameSelected);

}

//----------------------------------------------------------------------------------
//-- Show Print Dialog
//----------------------------------------------------------------------------------
nsresult NativeShowPrintDialog(HWND                aHWnd,
                               nsIWebBrowserPrint* aWebBrowserPrint,
                               nsIPrintSettings*   aPrintSettings)
{
  nsresult rv = NS_ERROR_FAILURE;

  PrepareForPrintDialog(aWebBrowserPrint, aPrintSettings);

#ifdef MOZ_REQUIRE_CURRENT_SDK
  if (CheckForExtendedDialog()) {
    rv = ShowNativePrintDialogEx(aHWnd, aPrintSettings);
  } else {
    rv = ShowNativePrintDialog(aHWnd, aPrintSettings);
  }
#else
  rv = ShowNativePrintDialog(aHWnd, aPrintSettings);
#endif
  if (aHWnd) {
    ::DestroyWindow(aHWnd);
  }

  return rv;
}

