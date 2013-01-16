/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

#define NOMINMAX 1

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
static bool gDialogWasExtended     = false;

#define PRINTDLG_PROPERTIES "chrome://global/locale/printdialog.properties"

static HWND gParentWnd = NULL;

//******************************************************
// Define native paper sizes
//******************************************************
typedef struct {
  short  mPaperSize; // native enum
  double mWidth;
  double mHeight;
  bool mIsInches;
} NativePaperSizes;

// There are around 40 default print sizes defined by Windows
const NativePaperSizes kPaperSizes[] = {
  {DMPAPER_LETTER,    8.5,   11.0,  true},
  {DMPAPER_LEGAL,     8.5,   14.0,  true},
  {DMPAPER_A4,        210.0, 297.0, false},
  {DMPAPER_TABLOID,   11.0,  17.0,  true},
  {DMPAPER_LEDGER,    17.0,  11.0,  true},
  {DMPAPER_STATEMENT, 5.5,   8.5,   true},
  {DMPAPER_EXECUTIVE, 7.25,  10.5,  true},
  {DMPAPER_A3,        297.0, 420.0, false},
  {DMPAPER_A5,        148.0, 210.0, false},
  {DMPAPER_CSHEET,    17.0,  22.0,  true},  
  {DMPAPER_DSHEET,    22.0,  34.0,  true},  
  {DMPAPER_ESHEET,    34.0,  44.0,  true},  
  {DMPAPER_LETTERSMALL, 8.5, 11.0,  true},  
  {DMPAPER_A4SMALL,   210.0, 297.0, false}, 
  {DMPAPER_B4,        250.0, 354.0, false}, 
  {DMPAPER_B5,        182.0, 257.0, false},
  {DMPAPER_FOLIO,     8.5,   13.0,  true},
  {DMPAPER_QUARTO,    215.0, 275.0, false},
  {DMPAPER_10X14,     10.0,  14.0,  true},
  {DMPAPER_11X17,     11.0,  17.0,  true},
  {DMPAPER_NOTE,      8.5,   11.0,  true},  
  {DMPAPER_ENV_9,     3.875, 8.875, true},  
  {DMPAPER_ENV_10,    40.125, 9.5,  true},  
  {DMPAPER_ENV_11,    4.5,   10.375, true},  
  {DMPAPER_ENV_12,    4.75,  11.0,  true},  
  {DMPAPER_ENV_14,    5.0,   11.5,  true},  
  {DMPAPER_ENV_DL,    110.0, 220.0, false}, 
  {DMPAPER_ENV_C5,    162.0, 229.0, false}, 
  {DMPAPER_ENV_C3,    324.0, 458.0, false}, 
  {DMPAPER_ENV_C4,    229.0, 324.0, false}, 
  {DMPAPER_ENV_C6,    114.0, 162.0, false}, 
  {DMPAPER_ENV_C65,   114.0, 229.0, false}, 
  {DMPAPER_ENV_B4,    250.0, 353.0, false}, 
  {DMPAPER_ENV_B5,    176.0, 250.0, false}, 
  {DMPAPER_ENV_B6,    176.0, 125.0, false}, 
  {DMPAPER_ENV_ITALY, 110.0, 230.0, false}, 
  {DMPAPER_ENV_MONARCH,  3.875,  7.5, true},  
  {DMPAPER_ENV_PERSONAL, 3.625,  6.5, true},  
  {DMPAPER_FANFOLD_US,   14.875, 11.0, true},  
  {DMPAPER_FANFOLD_STD_GERMAN, 8.5, 12.0, true},  
  {DMPAPER_FANFOLD_LGL_GERMAN, 8.5, 13.0, true},  
};
const int32_t kNumPaperSizes = 41;

//----------------------------------------------------------------------------------
// Map an incoming size to a Windows Native enum in the DevMode
static void 
MapPaperSizeToNativeEnum(LPDEVMODEW aDevMode,
                         int16_t   aType, 
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
  for (int32_t i=0;i<kNumPaperSizes;i++) {
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
    int16_t type;
    aPrintSettings->GetPaperSizeType(&type);
    if (type == nsIPrintSettings::kPaperSizeNativeData) {
      int16_t paperEnum;
      aPrintSettings->GetPaperData(&paperEnum);
      aDevMode->dmPaperSize = paperEnum;
      aDevMode->dmFields &= ~DM_PAPERLENGTH;
      aDevMode->dmFields &= ~DM_PAPERWIDTH;
      aDevMode->dmFields |= DM_PAPERSIZE;
    } else {
      int16_t unit;
      double width, height;
      aPrintSettings->GetPaperSizeUnit(&unit);
      aPrintSettings->GetPaperWidth(&width);
      aPrintSettings->GetPaperHeight(&height);
      MapPaperSizeToNativeEnum(aDevMode, unit, width, height);
    }

    // Setup Orientation
    int32_t orientation;
    aPrintSettings->GetOrientation(&orientation);
    aDevMode->dmOrientation = orientation == nsIPrintSettings::kPortraitOrientation?DMORIENT_PORTRAIT:DMORIENT_LANDSCAPE;
    aDevMode->dmFields |= DM_ORIENTATION;

    // Setup Number of Copies
    int32_t copies;
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
  if (aPrintSettings == nullptr) {
    return NS_ERROR_FAILURE;
  }

  aPrintSettings->SetIsInitializedFromPrinter(true);
  if (aDevMode->dmFields & DM_ORIENTATION) {
    int32_t orientation  = aDevMode->dmOrientation == DMORIENT_PORTRAIT?
                           nsIPrintSettings::kPortraitOrientation:nsIPrintSettings::kLandscapeOrientation;
    aPrintSettings->SetOrientation(orientation);
  }

  // Setup Number of Copies
  if (aDevMode->dmFields & DM_COPIES) {
    aPrintSettings->SetNumCopies(int32_t(aDevMode->dmCopies));
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
    //aPrintSettings->SetShrinkToFit(false);
  }

  if (aDevMode->dmFields & DM_PAPERSIZE) {
    aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeNativeData);
    aPrintSettings->SetPaperData(aDevMode->dmPaperSize);
    for (int32_t i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeMillimeters);
        break;
      }
    }

  } else if (aDevMode->dmFields & DM_PAPERLENGTH && aDevMode->dmFields & DM_PAPERWIDTH) {
    bool found = false;
    for (int32_t i=0;i<kNumPaperSizes;i++) {
      if (kPaperSizes[i].mPaperSize == aDevMode->dmPaperSize) {
        aPrintSettings->SetPaperSizeType(nsIPrintSettings::kPaperSizeDefined);
        aPrintSettings->SetPaperWidth(kPaperSizes[i].mWidth);
        aPrintSettings->SetPaperHeight(kPaperSizes[i].mHeight);
        aPrintSettings->SetPaperSizeUnit(kPaperSizes[i].mIsInches?nsIPrintSettings::kPaperSizeInches:nsIPrintSettings::kPaperSizeMillimeters);
        found = true;
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
  nsAutoCString text;
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
                     bool         aIsSet,
                     bool         isEnabled = true) 
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
    {NULL, 0}};

//--------------------------------------------------------
//--------------------------------------------------------
//--------------------------------------------------------
//--------------------------------------------------------
// Get the absolute coords of the child windows relative
// to its parent window
static void GetLocalRect(HWND aWnd, RECT& aRect, HWND aParent)
{
  ::GetWindowRect(aWnd, &aRect);

  // MapWindowPoints converts screen coordinates to client coordinates.
  // It works correctly in both left-to-right and right-to-left windows.
  ::MapWindowPoints(NULL, aParent, (LPPOINT)&aRect, 2);
}

//--------------------------------------------------------
// Show or Hide the control
static void Show(HWND aWnd, bool bState)
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
  nsAutoCString str;
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
static void InitializeExtendedDialog(HWND hdlg, int16_t aHowToEnableFrameUI) 
{
  NS_ABORT_IF_FALSE(aHowToEnableFrameUI != nsIPrintSettings::kFrameEnableNone,
                    "should not be called");

  // Localize the new controls in the print dialog
  nsCOMPtr<nsIStringBundle> strBundle;
  if (NS_SUCCEEDED(GetLocalizedBundle(PRINTDLG_PROPERTIES, getter_AddRefs(strBundle)))) {
    int32_t i = 0;
    while (gAllPropKeys[i].mKeyStr != NULL) {
      SetText(hdlg, gAllPropKeys[i].mKeyId, strBundle, gAllPropKeys[i].mKeyStr);
      i++;
    }
  }

  // Set up radio buttons
  if (aHowToEnableFrameUI == nsIPrintSettings::kFrameEnableAll) {
    SetRadio(hdlg, rad4, false);  
    SetRadio(hdlg, rad5, true); 
    SetRadio(hdlg, rad6, false);
    // set default so user doesn't have to actually press on it
    gFrameSelectedRadioBtn = rad5;

  } else { // nsIPrintSettings::kFrameEnableAsIsAndEach
    SetRadio(hdlg, rad4, false);  
    SetRadio(hdlg, rad5, false, false); 
    SetRadio(hdlg, rad6, true);
    // set default so user doesn't have to actually press on it
    gFrameSelectedRadioBtn = rad6;
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

    int16_t howToEnableFrameUI = (int16_t)printDlg->lCustData;
    // don't add frame options if they would be disabled anyway
    // because there are no frames
    if (howToEnableFrameUI == nsIPrintSettings::kFrameEnableNone)
      return TRUE;

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
    gDialogWasExtended = true;
    return TRUE;
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
static HGLOBAL CreateGlobalDevModeAndInit(const nsXPIDLString& aPrintName, nsIPrintSettings* aPS)
{
  HGLOBAL hGlobalDevMode = NULL;

  HANDLE hPrinter = NULL;
  // const cast kludge for silly Win32 api's
  LPWSTR printName = const_cast<wchar_t*>(aPrintName.get());
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
static void GetDefaultPrinterNameFromGlobalPrinters(nsXPIDLString &printerName)
{
  nsCOMPtr<nsIPrinterEnumerator> prtEnum = do_GetService("@mozilla.org/gfx/printerenumerator;1");
  if (prtEnum) {
    prtEnum->GetDefaultPrinterName(getter_Copies(printerName));
  }
}

// Determine whether we have a completely native dialog
// or whether we cshould extend it
static bool ShouldExtendPrintDialog()
{
  nsresult rv;
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, true);
  nsCOMPtr<nsIPrefBranch> prefBranch;
  rv = prefs->GetBranch(nullptr, getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, true);

  bool result;
  rv = prefBranch->GetBoolPref("print.extend_native_print_dialog", &result);
  NS_ENSURE_SUCCESS(rv, true);
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

  gDialogWasExtended  = false;

  HGLOBAL hGlobalDevMode = NULL;
  HGLOBAL hDevNames      = NULL;

  // Get the Print Name to be used
  nsXPIDLString printerName;
  aPrintSettings->GetPrinterName(getter_Copies(printerName));

  // If there is no name then use the default printer
  if (printerName.IsEmpty()) {
    GetDefaultPrinterNameFromGlobalPrinters(printerName);
  } else {
    HANDLE hPrinter = NULL;
    if(!::OpenPrinterW(const_cast<wchar_t*>(printerName.get()), &hPrinter, NULL)) {
      // If the last used printer is not found, we should use default printer.
      GetDefaultPrinterNameFromGlobalPrinters(printerName);
    } else {
      ::ClosePrinter(hPrinter);
    }
  }

  // Now create a DEVNAMES struct so the the dialog is initialized correctly.

  uint32_t len = printerName.Length();
  hDevNames = (HGLOBAL)::GlobalAlloc(GHND, sizeof(wchar_t) * (len + 1) + 
                                     sizeof(DEVNAMES));
  if (!hDevNames) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  DEVNAMES* pDevNames = (DEVNAMES*)::GlobalLock(hDevNames);
  if (!pDevNames) {
    ::GlobalFree(hDevNames);
    return NS_ERROR_FAILURE;
  }
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
  prntdlg.Flags       = PD_ALLPAGES | PD_RETURNIC | 
                        PD_USEDEVMODECOPIESANDCOLLATE | PD_COLLATE;

  // if there is a current selection then enable the "Selection" radio button
  int16_t howToEnableFrameUI = nsIPrintSettings::kFrameEnableNone;
  bool isOn;
  aPrintSettings->GetPrintOptions(nsIPrintSettings::kEnableSelectionRB, &isOn);
  if (!isOn) {
    prntdlg.Flags |= PD_NOSELECTION;
  }
  aPrintSettings->GetHowToEnableFrameUI(&howToEnableFrameUI);

  int32_t pg = 1;
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
    prntdlg.lCustData         = 0;
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
      aPrintSettings->SetPrintToFile(true);
    } else {
      // clear "print to file" info
      aPrintSettings->SetPrintToFile(false);
      aPrintSettings->SetToFileName(nullptr);
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
    bool    printSelection = prntdlg.Flags & PD_SELECTION;
    bool    printAllPages  = prntdlg.Flags & PD_ALLPAGES;
    bool    printNumPages  = prntdlg.Flags & PD_PAGENUMS;
    int32_t fromPageNum    = 0;
    int32_t toPageNum      = 0;

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
    ::SetFocus(aHWnd);
    aPrintSettings->SetIsCancelled(true);
    if (hGlobalDevMode) ::GlobalFree(hGlobalDevMode);
    return NS_ERROR_ABORT;
  }

  return NS_OK;
}

//------------------------------------------------------------------
static void 
PrepareForPrintDialog(nsIWebBrowserPrint* aWebBrowserPrint, nsIPrintSettings* aPS)
{
  NS_ASSERTION(aWebBrowserPrint, "Can't be null");
  NS_ASSERTION(aPS, "Can't be null");

  bool isFramesetDocument;
  bool isFramesetFrameSelected;
  bool isIFrameSelected;
  bool isRangeSelection;

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
  PrepareForPrintDialog(aWebBrowserPrint, aPrintSettings);

  nsresult rv = ShowNativePrintDialog(aHWnd, aPrintSettings);
  if (aHWnd) {
    ::DestroyWindow(aHWnd);
  }

  return rv;
}

