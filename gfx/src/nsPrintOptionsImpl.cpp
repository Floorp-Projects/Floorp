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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jessica Blanco <jblanco@us.ibm.com>
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

#include "nsPrintOptionsImpl.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"
#include "nsReadableUtils.h"
#include "nsPrintSettingsImpl.h"

// For Prefs
#include "nsIPref.h"
#include "nsIServiceManager.h"

#include "nsISimpleEnumerator.h"
#include "nsISupportsPrimitives.h"
#include "nsGfxCIID.h"
 
static NS_DEFINE_IID(kCPrinterEnumerator, NS_PRINTER_ENUMERATOR_CID);

NS_IMPL_ISUPPORTS1(nsPrintOptions, nsIPrintOptions)

// Pref Constants
const char kMarginTop[]       = "print.print_margin_top";
const char kMarginLeft[]      = "print.print_margin_left";
const char kMarginBottom[]    = "print.print_margin_bottom";
const char kMarginRight[]     = "print.print_margin_right";

// Prefs for Print Options
const char kPrintEvenPages[]  = "print.print_evenpages";
const char kPrintOddPages[]   = "print.print_oddpages";
const char kPrintHeaderStr1[] = "print.print_headerleft";
const char kPrintHeaderStr2[] = "print.print_headercenter";
const char kPrintHeaderStr3[] = "print.print_headerright";
const char kPrintFooterStr1[] = "print.print_footerleft";
const char kPrintFooterStr2[] = "print.print_footercenter";
const char kPrintFooterStr3[] = "print.print_footerright";

// Additional Prefs
const char kPrintPaperSize[]  = "print.print_paper_size"; // this has been deprecated

const char kPrintReversed[]   = "print.print_reversed";
const char kPrintColor[]      = "print.print_color";
const char kPrintPaperSizeType[] = "print.print_paper_size_type";
const char kPrintPaperData[]     = "print.print_paper_data";
const char kPrintPaperSizeUnit[] = "print.print_paper_size_unit";
const char kPrintPaperWidth[] = "print.print_paper_width";
const char kPrintPaperHeight[]= "print.print_paper_height";
const char kPrintOrientation[]= "print.print_orientation";
const char kPrintCommand[]    = "print.print_command";
const char kPrinter[]         = "print.print_printer";
const char kPrintFile[]       = "print.print_file";
const char kPrintToFile[]     = "print.print_tofile";
const char kPrintPageDelay[]  = "print.print_pagedelay";

// There are currently NOT supported
//const char kPrintBevelLines[]    = "print.print_bevellines";
//const char kPrintBlackText[]     = "print.print_blacktext";
//const char kPrintBlackLines[]    = "print.print_blacklines";
//const char kPrintLastPageFirst[] = "print.print_lastpagefirst";
//const char kPrintBackgrounds[]   = "print.print_backgrounds";

const char kLeftJust[]   = "left";
const char kCenterJust[] = "center";
const char kRightJust[]  = "right";

nsFont* nsPrintOptions::sDefaultFont = nsnull;

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptions::nsPrintOptions() :
  mPrintOptions(0L),
  mPrintRange(kRangeAllPages),
  mStartPageNum(1),
  mEndPageNum(1),
  mScaling(1.0),
  mNumCopies(1),
  mPrintFrameTypeUsage(kUseInternalDefault),
  mPrintFrameType(kFramesAsIs),
  mHowToEnableFrameUI(kFrameEnableNone),
  mIsCancelled(PR_FALSE),
  mPrintSilent(PR_FALSE),
  mPrintPageDelay(500),
  mPrintSettingsObj(nsnull),
  mPaperSizeType(kPaperSizeDefined),
  mPaperData(0),
  mPaperWidth(8.5),
  mPaperHeight(11.0),
  mPaperSizeUnit(kPaperSizeInches),
  mPaperSize(kLetterPaperSize), /* this has been deprecated */
  mPrintReversed(PR_FALSE),
  mPrintInColor(PR_TRUE),
  mOrientation(kPortraitOrientation),
  mPrintToFile(PR_FALSE)
{
  NS_INIT_ISUPPORTS();

  /* member initializers and constructor code */
  nscoord halfInch = NS_INCHES_TO_TWIPS(0.5);
  mMargin.SizeTo(halfInch, halfInch, halfInch, halfInch);

  mPrintOptions = kOptPrintOddPages | kOptPrintEvenPages;

  if (sDefaultFont == nsnull) {
    sDefaultFont = new nsFont("Times", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,
                               NS_FONT_WEIGHT_NORMAL,0,NSIntPointsToTwips(10));
  }

  mHeaderStrs[0].Assign(NS_LITERAL_STRING("&T"));
  mHeaderStrs[2].Assign(NS_LITERAL_STRING("&U"));

  mFooterStrs[0].Assign(NS_LITERAL_STRING("&PT")); // Use &P (Page Num Only) or &PT (Page Num of Page Total)
  mFooterStrs[2].Assign(NS_LITERAL_STRING("&D"));

  ReadPrefs();

}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptions::~nsPrintOptions()
{
  if (sDefaultFont != nsnull) {
    delete sDefaultFont;
    sDefaultFont = nsnull;
  }
}


//**************************************************************
//** PageList Enumerator
//**************************************************************
class
nsPrinterListEnumerator : public nsISimpleEnumerator
{
   public:
     nsPrinterListEnumerator();
     virtual ~nsPrinterListEnumerator();

     //nsISupports interface
     NS_DECL_ISUPPORTS

     //nsISimpleEnumerator interface
     NS_DECL_NSISIMPLEENUMERATOR

     NS_IMETHOD Init();

   protected:
     PRUnichar **mPrinters;
     PRUint32 mCount;
     PRUint32 mIndex;
};

nsPrinterListEnumerator::nsPrinterListEnumerator() :
  mPrinters(nsnull), mCount(0), mIndex(0)
{
   NS_INIT_REFCNT();
}

nsPrinterListEnumerator::~nsPrinterListEnumerator()
{
   if (mPrinters) {
      PRUint32 i;
      for (i = 0; i < mCount; i++ ) {
         nsMemory::Free(mPrinters[i]);
      }
      nsMemory::Free(mPrinters);
   }
}

NS_IMPL_ISUPPORTS1(nsPrinterListEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP nsPrinterListEnumerator::Init()
{
   nsresult rv;
   nsCOMPtr<nsIPrinterEnumerator> printerEnumerator;

   printerEnumerator = do_CreateInstance(kCPrinterEnumerator, &rv);
   if (NS_FAILED(rv))
      return rv;

   return printerEnumerator->EnumeratePrinters(&mCount, &mPrinters);
}

NS_IMETHODIMP nsPrinterListEnumerator::HasMoreElements(PRBool *result)
{
   *result = (mIndex < mCount);
    return NS_OK;
}

NS_IMETHODIMP nsPrinterListEnumerator::GetNext(nsISupports **aPrinter)
{
   NS_ENSURE_ARG_POINTER(aPrinter);
   *aPrinter = nsnull;
   if (mIndex >= mCount) {
     return NS_ERROR_UNEXPECTED;
   }

   PRUnichar *printerName = mPrinters[mIndex++];
   nsCOMPtr<nsISupportsWString> printerNameWrapper;
   nsresult rv;

   rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID, nsnull,
                                           NS_GET_IID(nsISupportsWString), getter_AddRefs(printerNameWrapper));
   NS_ENSURE_SUCCESS(rv, rv);
   NS_ENSURE_TRUE(printerNameWrapper, NS_ERROR_OUT_OF_MEMORY);
   printerNameWrapper->SetData(NS_CONST_CAST(PRUnichar*, printerName));
   *aPrinter = NS_STATIC_CAST(nsISupports*, printerNameWrapper);
   NS_ADDREF(*aPrinter);
   return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetFontNamePointSize(nsString& aFontName, PRInt32 aPointSize)
{
  if (sDefaultFont != nsnull && aFontName.Length() > 0 && aPointSize > 0) {
    sDefaultFont->name = aFontName;
    sDefaultFont->size = NSIntPointsToTwips(aPointSize);
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetDefaultFont(nsFont &aFont)
{
  if (sDefaultFont != nsnull) {
    delete sDefaultFont;
  }
  sDefaultFont = new nsFont(aFont);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetDefaultFont(nsFont &aFont)
{
  aFont = *sDefaultFont;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetMarginInTwips(nsMargin& aMargin)
{
  mMargin = aMargin;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::GetMarginInTwips(nsMargin& aMargin)
{
  aMargin = mMargin;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::GetPageSizeInTwips(PRInt32 *aWidth, PRInt32 *aHeight)
{
  if (mPaperSizeUnit == kPaperSizeInches) {
    *aWidth  = NS_INCHES_TO_TWIPS(float(mPaperWidth));
    *aHeight = NS_INCHES_TO_TWIPS(float(mPaperHeight));
  } else {
    *aWidth  = NS_MILLIMETERS_TO_TWIPS(float(mPaperWidth));
    *aHeight = NS_MILLIMETERS_TO_TWIPS(float(mPaperHeight));
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::ShowNativeDialog()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetPrintOptions(PRInt32 aType, PRBool aTurnOnOff)
{
  if (aTurnOnOff) {
    mPrintOptions |=  aType;
  } else {
    mPrintOptions &= ~aType;
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetPrintOptions(PRInt32 aType, PRBool *aTurnOnOff)
{
  NS_ENSURE_ARG_POINTER(aTurnOnOff);
  *aTurnOnOff = mPrintOptions & aType;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetPrintOptionsBits(PRInt32 *aBits)
{
  NS_ENSURE_ARG_POINTER(aBits);
  *aBits = mPrintOptions;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::ReadPrefs()
{
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (prefs) {
    ReadInchesToTwipsPref(prefs, kMarginTop,    mMargin.top);
    ReadInchesToTwipsPref(prefs, kMarginLeft,   mMargin.left);
    ReadInchesToTwipsPref(prefs, kMarginBottom, mMargin.bottom);
    ReadInchesToTwipsPref(prefs, kMarginRight,  mMargin.right);

    ReadBitFieldPref(prefs, kPrintEvenPages,  kOptPrintEvenPages);
    ReadBitFieldPref(prefs, kPrintOddPages,   kOptPrintOddPages);

    ReadPrefString(prefs, kPrintHeaderStr1, mHeaderStrs[0]);
    ReadPrefString(prefs, kPrintHeaderStr2, mHeaderStrs[1]);
    ReadPrefString(prefs, kPrintHeaderStr3, mHeaderStrs[2]);
    ReadPrefString(prefs, kPrintFooterStr1, mFooterStrs[0]);
    ReadPrefString(prefs, kPrintFooterStr2, mFooterStrs[1]);
    ReadPrefString(prefs, kPrintFooterStr3, mFooterStrs[2]);

    // Read Additional XP Prefs
    prefs->GetIntPref(kPrintPaperSize,   &mPaperSize); // this has been deprecated

    prefs->GetBoolPref(kPrintReversed,   &mPrintReversed);
    prefs->GetBoolPref(kPrintColor,      &mPrintInColor);

    PRInt32 iVal = kPaperSizeInches;
    prefs->GetIntPref(kPrintPaperSizeUnit,  &iVal);
    mPaperSizeUnit = PRInt16(iVal);
    iVal = kPaperSizeDefined;
    prefs->GetIntPref(kPrintPaperSizeType,  &iVal);
    mPaperSizeType = PRInt16(iVal);
    iVal = 0;
    prefs->GetIntPref(kPrintPaperData,  &iVal);
    mPaperData = PRInt16(iVal);
    ReadPrefDouble(prefs, kPrintPaperWidth,  mPaperWidth);
    ReadPrefDouble(prefs, kPrintPaperHeight,  mPaperHeight);

    prefs->GetIntPref(kPrintOrientation, &mOrientation);
    ReadPrefString(prefs, kPrintCommand, mPrintCommand);
    ReadPrefString(prefs, kPrinter, mPrinter);
    prefs->GetBoolPref(kPrintFile,       &mPrintToFile);
    ReadPrefString(prefs, kPrintToFile,  mToFileName);
    prefs->GetIntPref(kPrintPageDelay,   &mPrintPageDelay);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::InitPrintSettingsFromPrefs(nsIPrintSettings* aPS)
{
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (prefs) {
    nscoord halfInch = NS_INCHES_TO_TWIPS(0.5);
    nsMargin margin;
    margin.SizeTo(halfInch, halfInch, halfInch, halfInch);
    ReadInchesToTwipsPref(prefs, kMarginTop,    margin.top);
    ReadInchesToTwipsPref(prefs, kMarginLeft,   margin.left);
    ReadInchesToTwipsPref(prefs, kMarginBottom, margin.bottom);
    ReadInchesToTwipsPref(prefs, kMarginRight,  margin.right);
    aPS->SetMarginInTwips(margin);

    PRBool b;
    prefs->GetBoolPref(kPrintEvenPages, &b);
    aPS->SetPrintOptions(kOptPrintEvenPages, b);

    prefs->GetBoolPref(kPrintOddPages, &b);
    aPS->SetPrintOptions(kOptPrintOddPages, b);

    nsString str;
    str.SetLength(0);
    ReadPrefString(prefs, kPrintHeaderStr1, str);
    aPS->SetHeaderStrLeft(str.get());
    str.SetLength(0);
    ReadPrefString(prefs, kPrintHeaderStr2, str);
    aPS->SetHeaderStrCenter(str.get());
    str.SetLength(0);
    ReadPrefString(prefs, kPrintHeaderStr3, str);
    aPS->SetHeaderStrRight(str.get());

    str.SetLength(0);
    ReadPrefString(prefs, kPrintFooterStr1, str);
    aPS->SetFooterStrRight(str.get());
    str.SetLength(0);
    ReadPrefString(prefs, kPrintFooterStr2, str);
    aPS->SetFooterStrCenter(str.get());
    str.SetLength(0);
    ReadPrefString(prefs, kPrintFooterStr3, str);
    aPS->SetFooterStrRight(str.get());

    // Read Additional XP Prefs
    PRInt32 iVal = kLetterPaperSize;
    prefs->GetIntPref(kPrintPaperSize, &iVal); // this has been deprecated
    aPS->SetPaperSize(iVal);

    b = PR_FALSE;
    prefs->GetBoolPref(kPrintReversed, &b);
    aPS->SetPrintReversed(b);

    b = PR_TRUE;
    prefs->GetBoolPref(kPrintColor, &b);
    aPS->SetPrintInColor(b);

    iVal = kPaperSizeInches;
    prefs->GetIntPref(kPrintPaperSizeUnit,  &iVal);
    aPS->SetPaperSizeUnit(iVal);

    iVal = kPaperSizeDefined;
    prefs->GetIntPref(kPrintPaperSizeType,  &iVal);
    aPS->SetPaperSizeType(iVal);

    iVal = 0;
    prefs->GetIntPref(kPrintPaperData,  &iVal);
    aPS->SetPaperData(iVal);

    double d = 8.5;
    ReadPrefDouble(prefs, kPrintPaperWidth, d);
    aPS->SetPaperWidth(d);

    d = 11.0;
    ReadPrefDouble(prefs, kPrintPaperHeight,  d);
    aPS->SetPaperHeight(d);

    iVal = kPortraitOrientation;
    prefs->GetIntPref(kPrintOrientation, &mOrientation);
    aPS->SetOrientation(iVal);

    str.SetLength(0);
    ReadPrefString(prefs, kPrintCommand, str);
    aPS->SetPrintCommand(str.get());

    str.SetLength(0);
    ReadPrefString(prefs, kPrinter, mPrinter);
    aPS->SetPrinterName(str.get());

    b = PR_FALSE;
    prefs->GetBoolPref(kPrintFile, &b);
    aPS->SetPrintToFile(b);

    ReadPrefString(prefs, kPrintToFile,  mToFileName);

    iVal = 500;
    prefs->GetIntPref(kPrintPageDelay,   &iVal);
    aPS->SetPrintPageDelay(iVal);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::WritePrefs()
{
  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (prefs) {
    WriteInchesFromTwipsPref(prefs, kMarginTop,    mMargin.top);
    WriteInchesFromTwipsPref(prefs, kMarginLeft,   mMargin.left);
    WriteInchesFromTwipsPref(prefs, kMarginBottom, mMargin.bottom);
    WriteInchesFromTwipsPref(prefs, kMarginRight,  mMargin.right);

    WriteBitFieldPref(prefs, kPrintEvenPages,  kOptPrintEvenPages);
    WriteBitFieldPref(prefs, kPrintOddPages,   kOptPrintOddPages);

    WritePrefString(prefs, kPrintHeaderStr1, mHeaderStrs[0]);
    WritePrefString(prefs, kPrintHeaderStr2, mHeaderStrs[1]);
    WritePrefString(prefs, kPrintHeaderStr3, mHeaderStrs[2]);
    WritePrefString(prefs, kPrintFooterStr1, mFooterStrs[0]);
    WritePrefString(prefs, kPrintFooterStr2, mFooterStrs[1]);
    WritePrefString(prefs, kPrintFooterStr3, mFooterStrs[2]);

    // Write Additional XP Prefs
    prefs->SetIntPref(kPrintPaperSize,    mPaperSize); // this has been deprecated

    prefs->SetBoolPref(kPrintReversed,    mPrintReversed);
    prefs->SetBoolPref(kPrintColor,       mPrintInColor);
    prefs->SetIntPref(kPrintPaperSizeUnit,  PRInt32(mPaperSizeUnit));
    prefs->SetIntPref(kPrintPaperSizeType,  PRInt32(mPaperSizeType));
    prefs->SetIntPref(kPrintPaperData,      PRInt32(mPaperData));
    WritePrefDouble(prefs, kPrintPaperWidth,  mPaperWidth);
    WritePrefDouble(prefs, kPrintPaperHeight,  mPaperHeight);
    prefs->SetIntPref(kPrintOrientation,  mOrientation);
    WritePrefString(prefs, kPrintCommand, mPrintCommand);
    WritePrefString(prefs, kPrinter, mPrinter);
    prefs->SetBoolPref(kPrintFile,        mPrintToFile);
    WritePrefString(prefs, kPrintToFile,  mToFileName);
    prefs->SetIntPref(kPrintPageDelay,    mPrintPageDelay);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/* attribute long startPageRange; */
NS_IMETHODIMP nsPrintOptions::GetStartPageRange(PRInt32 *aStartPageRange)
{
  //NS_ENSURE_ARG_POINTER(aStartPageRange);
  *aStartPageRange = mStartPageNum;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetStartPageRange(PRInt32 aStartPageRange)
{
  mStartPageNum = aStartPageRange;
  return NS_OK;
}

/* attribute long endPageRange; */
NS_IMETHODIMP nsPrintOptions::GetEndPageRange(PRInt32 *aEndPageRange)
{
  //NS_ENSURE_ARG_POINTER(aEndPageRange);
  *aEndPageRange = mEndPageNum;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetEndPageRange(PRInt32 aEndPageRange)
{
  mEndPageNum = aEndPageRange;
  return NS_OK;
}

/* attribute boolean printReversed; */
NS_IMETHODIMP nsPrintOptions::GetPrintReversed(PRBool *aPrintReversed)
{
  //NS_ENSURE_ARG_POINTER(aPrintReversed);
  *aPrintReversed = mPrintReversed;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintReversed(PRBool aPrintReversed)
{
  mPrintReversed = aPrintReversed;
  return NS_OK;
}

/* attribute boolean printInColor; */
NS_IMETHODIMP nsPrintOptions::GetPrintInColor(PRBool *aPrintInColor)
{
  //NS_ENSURE_ARG_POINTER(aPrintInColor);
  *aPrintInColor = mPrintInColor;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintInColor(PRBool aPrintInColor)
{
  mPrintInColor = aPrintInColor;
  return NS_OK;
}

/* attribute wstring printCommand; */
NS_IMETHODIMP nsPrintOptions::GetPrintCommand(PRUnichar * *aPrintCommand)
{
  //NS_ENSURE_ARG_POINTER(aPrintCommand);
  *aPrintCommand = ToNewUnicode(mPrintCommand);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintCommand(const PRUnichar * aPrintCommand)
{
  mPrintCommand = aPrintCommand;
  return NS_OK;
}

/* attribute short orientation; */
NS_IMETHODIMP nsPrintOptions::GetOrientation(PRInt32 *aOrientation)
{
  //NS_ENSURE_ARG_POINTER(aOrientation);
  *aOrientation = mOrientation;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetOrientation(PRInt32 aOrientation)
{
  mOrientation = aOrientation;
  return NS_OK;
}

/* attribute wstring printer; */
NS_IMETHODIMP nsPrintOptions::GetPrinterName(PRUnichar * *aPrinter)
{
   //NS_ENSURE_ARG_POINTER(aPrinter);
   *aPrinter = ToNewUnicode(mPrinter);
   return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrinterName(const PRUnichar * aPrinter)
{
   mPrinter = aPrinter;
   return NS_OK;
}

/* attribute long numCopies; */
NS_IMETHODIMP nsPrintOptions::GetNumCopies(PRInt32 *aNumCopies)
{
  //NS_ENSURE_ARG_POINTER(aNumCopies);
  *aNumCopies = mNumCopies;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetNumCopies(PRInt32 aNumCopies)
{
  mNumCopies = aNumCopies;
  return NS_OK;
}

/* create and return a new |nsPrinterListEnumerator| */
NS_IMETHODIMP nsPrintOptions::AvailablePrinters(nsISimpleEnumerator **aPrinterEnumerator)
{
   NS_ENSURE_ARG_POINTER(aPrinterEnumerator);
   *aPrinterEnumerator = nsnull;

   nsCOMPtr<nsPrinterListEnumerator> printerListEnum = new nsPrinterListEnumerator();
   NS_ENSURE_TRUE(printerListEnum.get(), NS_ERROR_OUT_OF_MEMORY);

   // if Init fails NS_OK is return (script needs NS_OK) but the enumerator will be null
   nsresult rv = printerListEnum->Init();
   if (NS_SUCCEEDED(rv)) {
     *aPrinterEnumerator = NS_STATIC_CAST(nsISimpleEnumerator*, printerListEnum);
     NS_ADDREF(*aPrinterEnumerator);
   }
   return NS_OK;
}

NS_IMETHODIMP nsPrintOptions::DisplayJobProperties( const PRUnichar *aPrinter, nsIPrintSettings* aPrintSettings, PRBool *aDisplayed)
{
   NS_ENSURE_ARG(aPrinter);
   *aDisplayed = PR_FALSE;

   nsresult  rv;
   nsCOMPtr<nsIPrinterEnumerator> propDlg;

   propDlg = do_CreateInstance(kCPrinterEnumerator, &rv);
   if (NS_FAILED(rv))
     return rv;

   if (NS_FAILED(propDlg->DisplayPropertiesDlg((PRUnichar*)aPrinter, aPrintSettings)))
     return rv;
   
   *aDisplayed = PR_TRUE;
      
   return NS_OK;
}   

/* attribute boolean printToFile; */
NS_IMETHODIMP nsPrintOptions::GetPrintToFile(PRBool *aPrintToFile)
{
  //NS_ENSURE_ARG_POINTER(aPrintToFile);
  *aPrintToFile = mPrintToFile;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintToFile(PRBool aPrintToFile)
{
  mPrintToFile = aPrintToFile;
  return NS_OK;
}

/* attribute wstring toFileName; */
NS_IMETHODIMP nsPrintOptions::GetToFileName(PRUnichar * *aToFileName)
{
  //NS_ENSURE_ARG_POINTER(aToFileName);
  *aToFileName = ToNewUnicode(mToFileName);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetToFileName(const PRUnichar * aToFileName)
{
  mToFileName = aToFileName;
  return NS_OK;
}

/* attribute long printPageDelay; */
NS_IMETHODIMP nsPrintOptions::GetPrintPageDelay(PRInt32 *aPrintPageDelay)
{
  *aPrintPageDelay = mPrintPageDelay;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintPageDelay(PRInt32 aPrintPageDelay)
{
  mPrintPageDelay = aPrintPageDelay;
  return NS_OK;
}

/* attribute double marginTop; */
NS_IMETHODIMP nsPrintOptions::GetMarginTop(double *aMarginTop)
{
  NS_ENSURE_ARG_POINTER(aMarginTop);
  *aMarginTop = NS_TWIPS_TO_INCHES(mMargin.top);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetMarginTop(double aMarginTop)
{
  mMargin.top = NS_INCHES_TO_TWIPS(float(aMarginTop));
  return NS_OK;
}

/* attribute double marginLeft; */
NS_IMETHODIMP nsPrintOptions::GetMarginLeft(double *aMarginLeft)
{
  NS_ENSURE_ARG_POINTER(aMarginLeft);
  *aMarginLeft = NS_TWIPS_TO_INCHES(mMargin.left);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetMarginLeft(double aMarginLeft)
{
  mMargin.left = NS_INCHES_TO_TWIPS(float(aMarginLeft));
  return NS_OK;
}

/* attribute double marginBottom; */
NS_IMETHODIMP nsPrintOptions::GetMarginBottom(double *aMarginBottom)
{
  NS_ENSURE_ARG_POINTER(aMarginBottom);
  *aMarginBottom = NS_TWIPS_TO_INCHES(mMargin.bottom);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetMarginBottom(double aMarginBottom)
{
  mMargin.bottom = NS_INCHES_TO_TWIPS(float(aMarginBottom));
  return NS_OK;
}

/* attribute double marginRight; */
NS_IMETHODIMP nsPrintOptions::GetMarginRight(double *aMarginRight)
{
  NS_ENSURE_ARG_POINTER(aMarginRight);
  *aMarginRight = NS_TWIPS_TO_INCHES(mMargin.right);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetMarginRight(double aMarginRight)
{
  mMargin.right = NS_INCHES_TO_TWIPS(float(aMarginRight));
  return NS_OK;
}

/* attribute double scaling; */
NS_IMETHODIMP nsPrintOptions::GetScaling(double *aScaling)
{
  NS_ENSURE_ARG_POINTER(aScaling);
  *aScaling = mScaling;
  return NS_OK;
}

NS_IMETHODIMP nsPrintOptions::SetScaling(double aScaling)
{
  mScaling = aScaling;
  return NS_OK;
}

/* attribute boolean printBGColors; */
NS_IMETHODIMP nsPrintOptions::GetPrintBGColors(PRBool *aPrintBGColors)
{
  NS_ENSURE_ARG_POINTER(aPrintBGColors);
  *aPrintBGColors = mPrintBGColors;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintBGColors(PRBool aPrintBGColors)
{
  mPrintBGColors = aPrintBGColors;
  return NS_OK;
}

/* attribute boolean printBGImages; */
NS_IMETHODIMP nsPrintOptions::GetPrintBGImages(PRBool *aPrintBGImages)
{
  NS_ENSURE_ARG_POINTER(aPrintBGImages);
  *aPrintBGImages = mPrintBGImages;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintBGImages(PRBool aPrintBGImages)
{
  mPrintBGImages = aPrintBGImages;
  return NS_OK;
}

/* attribute long printRange; */
NS_IMETHODIMP nsPrintOptions::GetPrintRange(PRInt16 *aPrintRange)
{
  NS_ENSURE_ARG_POINTER(aPrintRange);
  *aPrintRange = mPrintRange;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintRange(PRInt16 aPrintRange)
{
  mPrintRange = aPrintRange;
  return NS_OK;
}

/* attribute wstring docTitle; */
NS_IMETHODIMP nsPrintOptions::GetTitle(PRUnichar * *aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = ToNewUnicode(mTitle);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetTitle(const PRUnichar * aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  mTitle = aTitle;
  return NS_OK;
}

/* attribute wstring docURL; */
NS_IMETHODIMP nsPrintOptions::GetDocURL(PRUnichar * *aDocURL)
{
  NS_ENSURE_ARG_POINTER(aDocURL);
  *aDocURL = ToNewUnicode(mURL);
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetDocURL(const PRUnichar * aDocURL)
{
  NS_ENSURE_ARG_POINTER(aDocURL);
  mURL = aDocURL;
  return NS_OK;
}

/* attribute wstring docTitle; */
nsresult 
nsPrintOptions::GetMarginStrs(PRUnichar * *aTitle, 
                              nsHeaderFooterEnum aType, 
                              PRInt16 aJust)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = nsnull;
  if (aType == eHeader) {
    switch (aJust) {
      case kJustLeft:   *aTitle = ToNewUnicode(mHeaderStrs[0]);break;
      case kJustCenter: *aTitle = ToNewUnicode(mHeaderStrs[1]);break;
      case kJustRight:  *aTitle = ToNewUnicode(mHeaderStrs[2]);break;
    } //switch
  } else {
    switch (aJust) {
      case kJustLeft:   *aTitle = ToNewUnicode(mFooterStrs[0]);break;
      case kJustCenter: *aTitle = ToNewUnicode(mFooterStrs[1]);break;
      case kJustRight:  *aTitle = ToNewUnicode(mFooterStrs[2]);break;
    } //switch
  }
  return NS_OK;
}

nsresult
nsPrintOptions::SetMarginStrs(const PRUnichar * aTitle, 
                              nsHeaderFooterEnum aType, 
                              PRInt16 aJust)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  if (aType == eHeader) {
    switch (aJust) {
      case kJustLeft:   mHeaderStrs[0] = aTitle;break;
      case kJustCenter: mHeaderStrs[1] = aTitle;break;
      case kJustRight:  mHeaderStrs[2] = aTitle;break;
    } //switch
  } else {
    switch (aJust) {
      case kJustLeft:   mFooterStrs[0] = aTitle;break;
      case kJustCenter: mFooterStrs[1] = aTitle;break;
      case kJustRight:  mFooterStrs[2] = aTitle;break;
    } //switch
  }
  return NS_OK;
}

/* attribute wstring Header String Left */
NS_IMETHODIMP nsPrintOptions::GetHeaderStrLeft(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eHeader, kJustLeft);
}
NS_IMETHODIMP nsPrintOptions::SetHeaderStrLeft(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eHeader, kJustLeft);
}

/* attribute wstring Header String Center */
NS_IMETHODIMP nsPrintOptions::GetHeaderStrCenter(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eHeader, kJustCenter);
}
NS_IMETHODIMP nsPrintOptions::SetHeaderStrCenter(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eHeader, kJustCenter);
}

/* attribute wstring Header String Right */
NS_IMETHODIMP nsPrintOptions::GetHeaderStrRight(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eHeader, kJustRight);
}
NS_IMETHODIMP nsPrintOptions::SetHeaderStrRight(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eHeader, kJustRight);
}


/* attribute wstring Footer String Left */
NS_IMETHODIMP nsPrintOptions::GetFooterStrLeft(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eFooter, kJustLeft);
}
NS_IMETHODIMP nsPrintOptions::SetFooterStrLeft(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eFooter, kJustLeft);
}

/* attribute wstring Footer String Center */
NS_IMETHODIMP nsPrintOptions::GetFooterStrCenter(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eFooter, kJustCenter);
}
NS_IMETHODIMP nsPrintOptions::SetFooterStrCenter(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eFooter, kJustCenter);
}

/* attribute wstring Footer String Right */
NS_IMETHODIMP nsPrintOptions::GetFooterStrRight(PRUnichar * *aTitle)
{
  return GetMarginStrs(aTitle, eFooter, kJustRight);
}
NS_IMETHODIMP nsPrintOptions::SetFooterStrRight(const PRUnichar * aTitle)
{
  return SetMarginStrs(aTitle, eFooter, kJustRight);
}


/* attribute boolean isPrintFrame; */
NS_IMETHODIMP nsPrintOptions::GetHowToEnableFrameUI(PRInt16 *aHowToEnableFrameUI)
{
  NS_ENSURE_ARG_POINTER(aHowToEnableFrameUI);
  *aHowToEnableFrameUI = (PRInt32)mHowToEnableFrameUI;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetHowToEnableFrameUI(PRInt16 aHowToEnableFrameUI)
{
  mHowToEnableFrameUI = aHowToEnableFrameUI;
  return NS_OK;
}

/* attribute long printFrame; */
NS_IMETHODIMP nsPrintOptions::GetPrintFrameType(PRInt16 *aPrintFrameType)
{
  NS_ENSURE_ARG_POINTER(aPrintFrameType);
  *aPrintFrameType = mPrintFrameType;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintFrameType(PRInt16 aPrintFrameType)
{
  mPrintFrameType = aPrintFrameType;
  return NS_OK;
}

/* attribute short printFrameTypeUsage; */
NS_IMETHODIMP nsPrintOptions::GetPrintFrameTypeUsage(PRInt16 *aPrintFrameTypeUsage)
{
  NS_ENSURE_ARG_POINTER(aPrintFrameTypeUsage);
  *aPrintFrameTypeUsage = mPrintFrameTypeUsage;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintFrameTypeUsage(PRInt16 aPrintFrameTypeUsage)
{
  mPrintFrameTypeUsage = aPrintFrameTypeUsage;
  return NS_OK;
}

/* attribute long isCancelled; */
NS_IMETHODIMP nsPrintOptions::GetIsCancelled(PRBool *aIsCancelled)
{
  NS_ENSURE_ARG_POINTER(aIsCancelled);
  *aIsCancelled = mIsCancelled;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetIsCancelled(PRBool aIsCancelled)
{
  mIsCancelled = aIsCancelled;
  return NS_OK;
}


/* attribute long isCancelled; */
NS_IMETHODIMP nsPrintOptions::GetPrintSilent(PRBool *aPrintSilent)
{
  NS_ENSURE_ARG_POINTER(aPrintSilent);
  *aPrintSilent = mPrintSilent;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPrintSilent(PRBool aPrintSilent)
{
  mPrintSilent = aPrintSilent;
  return NS_OK;
}

/* [noscript] voidPtr GetNativeData (in short aDataType); */
NS_IMETHODIMP nsPrintOptions::GetNativeData(PRInt16 aDataType, void * *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nsnull;

  return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIPrintSettings printSettings; */
NS_IMETHODIMP nsPrintOptions::GetPrintSettings(nsIPrintSettings * *aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPrintSettings);
  *aPrintSettings = mPrintSettingsObj;
  return NS_OK;
}

NS_IMETHODIMP nsPrintOptions::SetPrintSettings(nsIPrintSettings * aPrintSettings)
{
  NS_ENSURE_ARG_POINTER(aPrintSettings);
  mPrintSettingsObj = aPrintSettings;
  return NS_OK;
}

/* attribute nsIPrintSettings printSettingsValues; */
NS_IMETHODIMP nsPrintOptions::GetPrintSettingsValues(nsIPrintSettings * *aPrintSettingsValues)
{
  NS_ENSURE_ARG_POINTER(aPrintSettingsValues);
  nsIPrintSettings* printSettingsValues = *aPrintSettingsValues;
  if (printSettingsValues == nsnull) {
    if (NS_SUCCEEDED(CreatePrintSettings(&printSettingsValues))) {
      *aPrintSettingsValues = printSettingsValues;
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  printSettingsValues->SetStartPageRange(mStartPageNum);
  printSettingsValues->SetEndPageRange(mEndPageNum);

  double  dblVal;
  GetMarginTop(&dblVal);
  printSettingsValues->SetMarginTop(dblVal);

  GetMarginLeft(&dblVal);
  printSettingsValues->SetMarginLeft(dblVal);

  GetMarginBottom(&dblVal);
  printSettingsValues->SetMarginBottom(dblVal);

  GetMarginRight(&dblVal);
  printSettingsValues->SetMarginRight(dblVal);

  printSettingsValues->SetScaling(mScaling);
  printSettingsValues->SetPrintBGColors(mPrintBGColors);
  printSettingsValues->SetPrintBGImages(mPrintBGImages);

  printSettingsValues->SetPrintRange(mPrintRange);

  PRUnichar* uniChar;
  GetTitle(&uniChar);
  printSettingsValues->SetTitle(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetDocURL(&uniChar);
  printSettingsValues->SetDocURL(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetHeaderStrLeft(&uniChar);
  printSettingsValues->SetHeaderStrLeft(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetHeaderStrCenter(&uniChar);
  printSettingsValues->SetHeaderStrCenter(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetHeaderStrRight(&uniChar);
  printSettingsValues->SetHeaderStrRight(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetFooterStrLeft(&uniChar);
  printSettingsValues->SetFooterStrLeft(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetFooterStrCenter(&uniChar);
  printSettingsValues->SetFooterStrCenter(uniChar);

  GetFooterStrRight(&uniChar);
  printSettingsValues->SetFooterStrRight(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  printSettingsValues->SetPrintFrameTypeUsage(mPrintFrameTypeUsage);
  printSettingsValues->SetPrintFrameType(mPrintFrameType);
  printSettingsValues->SetPrintSilent(mPrintSilent);
  printSettingsValues->SetPrintReversed(mPrintReversed);
  printSettingsValues->SetPrintInColor(mPrintInColor);
  printSettingsValues->SetPaperSizeUnit(mPaperSizeUnit);
  printSettingsValues->SetPaperHeight(mPaperHeight);
  printSettingsValues->SetPaperWidth(mPaperWidth);
  printSettingsValues->SetOrientation(mOrientation);
  printSettingsValues->SetNumCopies(mNumCopies);

  GetPrinterName(&uniChar);
  printSettingsValues->SetPrinterName(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  GetPrintCommand(&uniChar);
  printSettingsValues->SetPrintCommand(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  printSettingsValues->SetPrintToFile(mPrintToFile);

  GetToFileName(&uniChar);
  printSettingsValues->SetToFileName(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  printSettingsValues->SetPrintPageDelay(mPrintPageDelay);
  
  return NS_OK;
}

NS_IMETHODIMP nsPrintOptions::SetPrintSettingsValues(nsIPrintSettings * aPrintSettingsValues)
{

  NS_ENSURE_ARG_POINTER(aPrintSettingsValues);

  aPrintSettingsValues->GetStartPageRange(&mStartPageNum);
  aPrintSettingsValues->GetEndPageRange(&mEndPageNum);

  double  dblVal;
  aPrintSettingsValues->GetMarginTop(&dblVal);
  SetMarginTop(dblVal);

  aPrintSettingsValues->GetMarginLeft(&dblVal);
  SetMarginLeft(dblVal);

  aPrintSettingsValues->GetMarginBottom(&dblVal);
  SetMarginBottom(dblVal);

  aPrintSettingsValues->GetMarginRight(&dblVal);
  SetMarginRight(dblVal);

  aPrintSettingsValues->GetScaling(&mScaling);
  aPrintSettingsValues->GetPrintBGColors(&mPrintBGColors);
  aPrintSettingsValues->GetPrintBGImages(&mPrintBGImages);

  aPrintSettingsValues->GetPrintRange(&mPrintRange);

  PRUnichar* uniChar;
  aPrintSettingsValues->GetTitle(&uniChar);
  SetTitle(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetDocURL(&uniChar);
  SetDocURL(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetHeaderStrLeft(&uniChar);
  SetHeaderStrLeft(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetHeaderStrCenter(&uniChar);
  SetHeaderStrCenter(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetHeaderStrRight(&uniChar);
  SetHeaderStrRight(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetFooterStrLeft(&uniChar);
  SetFooterStrLeft(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetFooterStrCenter(&uniChar);
  SetFooterStrCenter(uniChar);

  aPrintSettingsValues->GetFooterStrRight(&uniChar);
  SetFooterStrRight(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetPrintFrameTypeUsage(&mPrintFrameTypeUsage);
  aPrintSettingsValues->GetPrintFrameType(&mPrintFrameType);
  aPrintSettingsValues->GetPrintSilent(&mPrintSilent);
  aPrintSettingsValues->GetPrintReversed(&mPrintReversed);
  aPrintSettingsValues->GetPrintInColor(&mPrintInColor);
  aPrintSettingsValues->GetPaperSizeUnit(&mPaperSizeUnit);
  aPrintSettingsValues->GetPaperHeight(&mPaperHeight);
  aPrintSettingsValues->GetPaperWidth(&mPaperWidth);
  aPrintSettingsValues->GetOrientation(&mOrientation);
  aPrintSettingsValues->GetNumCopies(&mNumCopies);

  aPrintSettingsValues->GetPrinterName(&uniChar);
  SetPrinterName(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetPrintCommand(&uniChar);
  SetPrintCommand(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetPrintToFile(&mPrintToFile);

  aPrintSettingsValues->GetToFileName(&uniChar);
  SetToFileName(uniChar);
  if (uniChar != nsnull) nsMemory::Free(uniChar);

  aPrintSettingsValues->GetPrintPageDelay(&mPrintPageDelay);
  SetPrintPageDelay(mPrintPageDelay);

  return NS_OK;
}


/* attribute double paperWidth; */
NS_IMETHODIMP nsPrintOptions::GetPaperWidth(double *aPaperWidth)
{
  NS_ENSURE_ARG_POINTER(aPaperWidth);
  *aPaperWidth = mPaperWidth;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPaperWidth(double aPaperWidth)
{
  mPaperWidth = aPaperWidth;
  return NS_OK;
}

/* attribute double paperHeight; */
NS_IMETHODIMP nsPrintOptions::GetPaperHeight(double *aPaperHeight)
{
  NS_ENSURE_ARG_POINTER(aPaperHeight);
  *aPaperHeight = mPaperHeight;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPaperHeight(double aPaperHeight)
{
  mPaperHeight = aPaperHeight;
  return NS_OK;
}

/* attribute short PaperSizeUnit; */
NS_IMETHODIMP nsPrintOptions::GetPaperSizeUnit(PRInt16 *aPaperSizeUnit)
{
  NS_ENSURE_ARG_POINTER(aPaperSizeUnit);
  *aPaperSizeUnit = mPaperSizeUnit;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPaperSizeUnit(PRInt16 aPaperSizeUnit)
{
  mPaperSizeUnit = aPaperSizeUnit;
  return NS_OK;
}

/* attribute short PaperSizeType; */
NS_IMETHODIMP nsPrintOptions::GetPaperSizeType(PRInt16 *aPaperSizeType)
{
  NS_ENSURE_ARG_POINTER(aPaperSizeType);
  *aPaperSizeType = mPaperSizeType;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPaperSizeType(PRInt16 aPaperSizeType)
{
  mPaperSizeType = aPaperSizeType;
  return NS_OK;
}

/* attribute short PaperData; */
NS_IMETHODIMP nsPrintOptions::GetPaperData(PRInt16 *aPaperData)
{
  NS_ENSURE_ARG_POINTER(aPaperData);
  *aPaperData = mPaperData;
  return NS_OK;
}
NS_IMETHODIMP nsPrintOptions::SetPaperData(PRInt16 aPaperData)
{
  mPaperData = aPaperData;
  return NS_OK;
}

/* This has been deprecated */
NS_IMETHODIMP nsPrintOptions::GetPaperSize(PRInt32 *aPaperSize)
{
  *aPaperSize = mPaperSize;
  return NS_OK;
}
/* This has been deprecated */
NS_IMETHODIMP nsPrintOptions::SetPaperSize(PRInt32 aPaperSize)
{
  mPaperSize = aPaperSize;
  return NS_OK;
}

/* nsIPrintSettings CreatePrintSettings (); */
NS_IMETHODIMP nsPrintOptions::CreatePrintSettings(nsIPrintSettings **_retval)
{
  nsresult rv = NS_OK;
  nsPrintSettings* printSettings = new nsPrintSettings(); // does not initially ref count
  NS_ASSERTION(printSettings, "Can't be NULL!");

  rv = printSettings->QueryInterface(NS_GET_IID(nsIPrintSettings), (void**)_retval); // ref counts
  if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

  InitPrintSettingsFromPrefs(*_retval); // ignore return value

  return rv;
}


//-----------------------------------------------------
//-- Protected Methods
//-----------------------------------------------------
//---------------------------------------------------
nsresult nsPrintOptions::ReadPrefString(nsIPref *    aPref, 
                                        const char * aPrefId, 
                                        nsString&    aString)
{
  char * str = nsnull;
  nsresult rv = aPref->CopyCharPref(aPrefId, &str);
  if (NS_SUCCEEDED(rv) && str) {
    aString.AssignWithConversion(str);
    nsMemory::Free(str);
  }
  return rv;
}

nsresult nsPrintOptions::WritePrefString(nsIPref *    aPref, 
                                         const char * aPrefId, 
                                         nsString&    aString)
{
  NS_ENSURE_ARG_POINTER(aPref);
  NS_ENSURE_ARG_POINTER(aPrefId);

  PRUnichar * str = ToNewUnicode(aString);
  nsresult rv = aPref->SetUnicharPref(aPrefId, str);
  nsMemory::Free(str);

  return rv;
}

nsresult nsPrintOptions::ReadPrefDouble(nsIPref *    aPref, 
                                        const char * aPrefId, 
                                        double&      aVal)
{
  char * str = nsnull;
  nsresult rv = aPref->CopyCharPref(aPrefId, &str);
  if (NS_SUCCEEDED(rv) && str) {
    sscanf(str, "%6.2", &aVal);
    nsMemory::Free(str);
  }
  return rv;
}

nsresult nsPrintOptions::WritePrefDouble(nsIPref *    aPref, 
                                         const char * aPrefId, 
                                         double       aVal)
{
  NS_ENSURE_ARG_POINTER(aPref);
  NS_ENSURE_ARG_POINTER(aPrefId);

  char str[64];
  sprintf(str, "%6.2f", aVal);
  return aPref->SetCharPref(aPrefId, str);
}

void nsPrintOptions::ReadBitFieldPref(nsIPref *    aPref, 
                                      const char * aPrefId, 
                                      PRInt32      anOption)
{
  PRBool b;
  if (NS_SUCCEEDED(aPref->GetBoolPref(aPrefId, &b))) {
    SetPrintOptions(anOption, b);
  }
}

//---------------------------------------------------
void nsPrintOptions::WriteBitFieldPref(nsIPref *    aPref, 
                                      const char * aPrefId, 
                                      PRInt32      anOption)
{
  PRBool b;
  GetPrintOptions(anOption, &b);
  aPref->SetBoolPref(aPrefId, b);
}

//---------------------------------------------------
void nsPrintOptions::ReadInchesToTwipsPref(nsIPref *    aPref, 
                                           const char * aPrefId, 
                                           nscoord&     aTwips)
{
  char * str = nsnull;
  nsresult rv = aPref->CopyCharPref(aPrefId, &str);
  if (NS_SUCCEEDED(rv) && str) {
    nsAutoString justStr;
    justStr.AssignWithConversion(str);
    PRInt32 errCode;
    float inches = justStr.ToFloat(&errCode);
    if (NS_SUCCEEDED(errCode)) {
      aTwips = NS_INCHES_TO_TWIPS(inches);
    } else {
      aTwips = 0;
    }
    nsMemory::Free(str);
  }
}

//---------------------------------------------------
void nsPrintOptions::WriteInchesFromTwipsPref(nsIPref *    aPref, 
                                              const char * aPrefId, 
                                              nscoord      aTwips)
{
  double inches = NS_TWIPS_TO_INCHES(aTwips);
  nsAutoString inchesStr;
  inchesStr.AppendFloat(inches);
  char * str = ToNewCString(inchesStr);
  if (str) {
    aPref->SetCharPref(aPrefId, str);
  } else {
    aPref->SetCharPref(aPrefId, "0.5");
  }
}

void nsPrintOptions::ReadJustification(nsIPref *    aPref, 
                                       const char * aPrefId, 
                                       PRInt16&     aJust,
                                       PRInt16      aInitValue)
{
  aJust = aInitValue;
  nsAutoString justStr;
  if (NS_SUCCEEDED(ReadPrefString(aPref, aPrefId, justStr))) {
    if (justStr.EqualsWithConversion(kRightJust)) {
      aJust = kJustRight;

    } else if (justStr.EqualsWithConversion(kCenterJust)) {
      aJust = kJustCenter;

    } else {
      aJust = kJustLeft;
    }
  }
}

//---------------------------------------------------
void nsPrintOptions::WriteJustification(nsIPref *    aPref, 
                                        const char * aPrefId, 
                                        PRInt16      aJust)
{
  switch (aJust) {
    case kJustLeft: 
      aPref->SetCharPref(aPrefId, kLeftJust);
      break;

    case kJustCenter: 
      aPref->SetCharPref(aPrefId, kCenterJust);
      break;

    case kJustRight: 
      aPref->SetCharPref(aPrefId, kRightJust);
      break;
  } //switch
}



