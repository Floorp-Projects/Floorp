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

#include "nsPrintOptionsImpl.h"
#include "nsCoord.h"
#include "nsUnitConversion.h"

// For Prefs
#include "nsIPref.h"
#include "nsIServiceManager.h"

NS_IMPL_ISUPPORTS1(nsPrintOptions, nsIPrintOptions)

// Pref Constants
const char * kMarginTop       = "print.print_margin_top";
const char * kMarginLeft      = "print.print_margin_left";
const char * kMarginBottom    = "print.print_margin_bottom";
const char * kMarginRight     = "print.print_margin_right";

// Prefs for Print Option
const char * kPrintEvenPages  = "print.print_evenpages";
const char * kPrintOddPages   = "print.print_oddpages";
const char * kPrintDocTitle   = "print.print_doctitle";
const char * kPrintDocLoc     = "print.print_doclocation";
const char * kPageNums        = "print.print_pagenumbers";
const char * kPageNumsJust    = "print.print_pagenumjust";
const char * kPrintPageTotals = "print.print_pagetotals";
const char * kPrintDate       = "print.print_date";

// There are currently NOT supported
//const char * kPrintBevelLines    = "print.print_bevellines";
//const char * kPrintBlackText     = "print.print_blacktext";
//const char * kPrintBlackLines    = "print.print_blacklines";
//const char * kPrintLastPageFirst = "print.print_lastpagefirst";
//const char * kPrintBackgrounds   = "print.print_backgrounds";

const char * kLeftJust   = "left";
const char * kCenterJust = "center";
const char * kRightJust  = "right";

#define form_properties "chrome://communicator/locale/printing.properties"

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptions::nsPrintOptions() :
  mPrintRange(ePrintRange_AllPages),
  mStartPageNum(0),
  mEndPageNum(0),
  mPrintOptions(0L)
{
  NS_INIT_ISUPPORTS();

  /* member initializers and constructor code */
  nscoord halfInch = NS_INCHES_TO_TWIPS(0.5);
  SetMargins(halfInch, halfInch, halfInch, halfInch);

  mPrintOptions = NS_PRINT_OPTIONS_PRINT_EVEN_PAGES   |
                  NS_PRINT_OPTIONS_PRINT_ODD_PAGES    |
                  NS_PRINT_OPTIONS_PRINT_DOC_LOCATION |
                  NS_PRINT_OPTIONS_PRINT_DOC_TITLE    |
                  NS_PRINT_OPTIONS_PRINT_PAGE_NUMS    |
                  NS_PRINT_OPTIONS_PRINT_PAGE_TOTAL   |
                  NS_PRINT_OPTIONS_PRINT_DATE_PRINTED;

  mDefaultFont = new nsFont("Times", NS_FONT_STYLE_NORMAL,NS_FONT_VARIANT_NORMAL,
                             NS_FONT_WEIGHT_NORMAL,0,NSIntPointsToTwips(10));

  ReadPrefs();
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
nsPrintOptions::~nsPrintOptions()
{
  if (mDefaultFont != nsnull) {
    delete mDefaultFont;
  }
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetDefaultFont(const nsFont &aFont)
{
  if (mDefaultFont != nsnull) {
    delete mDefaultFont;
  }
  mDefaultFont = new nsFont(aFont);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetFontNamePointSize(const nsString& aFontName, nscoord aPointSize)
{
  if (mDefaultFont != nsnull && aFontName.Length() > 0 && aPointSize > 0) {
    mDefaultFont->name = aFontName;
    mDefaultFont->size = NSIntPointsToTwips(aPointSize);
  }
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetDefaultFont(nsFont &aFont)
{
  aFont = *mDefaultFont;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetMargins(PRInt32 aTop, PRInt32 aLeft, PRInt32 aRight, PRInt32 aBottom)
{
  mMargin.SizeTo(aLeft, aTop, aRight, aBottom);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetMargins(PRInt32 *aTop, PRInt32 *aLeft, PRInt32 *aRight, PRInt32 *aBottom)
{
  NS_ENSURE_ARG_POINTER(aTop);
  NS_ENSURE_ARG_POINTER(aLeft);
  NS_ENSURE_ARG_POINTER(aRight);
  NS_ENSURE_ARG_POINTER(aBottom);

  *aTop    = mMargin.top;
  *aLeft   = mMargin.left;
  *aRight  = mMargin.right;
  *aBottom = mMargin.bottom;

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::GetMargin(nsMargin& aMargin)
{
  aMargin = mMargin;
  //aMargin.SizeTo(mLeftMargin, mTopMargin, mRightMargin, mBottomMargin);
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP 
nsPrintOptions::ShowNativeDialog()
{

  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetPrintRange(PRInt32 aPrintRange)
{
  mPrintRange = (nsPrintRange)aPrintRange;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetPrintRange(PRInt32 *aPrintRange)
{
  NS_ENSURE_ARG_POINTER(aPrintRange);

  *aPrintRange = (PRInt32)mPrintRange;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetPageRange(PRInt32 aStartPage, PRInt32 aEndPage)
{
  mStartPageNum = aStartPage;
  mEndPageNum   = aEndPage;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetPageRange(PRInt32 *aStartPage, PRInt32 *aEndPage)
{
  NS_ENSURE_ARG_POINTER(aStartPage);
  NS_ENSURE_ARG_POINTER(aEndPage);
  *aStartPage = mStartPageNum;
  *aEndPage   = mEndPageNum;
  return NS_OK;
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
nsPrintOptions::SetTitle(const PRUnichar *aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  mTitle = aTitle;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetTitle(PRUnichar **aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);
  *aTitle = mTitle.ToNewUnicode();
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetURL(const PRUnichar *aURL)
{
  NS_ENSURE_ARG_POINTER(aURL);
  mURL = aURL;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetURL(PRUnichar **aURL)
{
  NS_ENSURE_ARG_POINTER(aURL);
  *aURL = mURL.ToNewUnicode();
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::SetPageNumJust(PRInt32 aJust)
{
  mPageNumJust = aJust;
  return NS_OK;
}

/** ---------------------------------------------------
 *  See documentation in nsPrintOptionsImpl.h
 *	@update 1/12/01 rods
 */
NS_IMETHODIMP 
nsPrintOptions::GetPageNumJust(PRInt32 *aJust)
{
  NS_ENSURE_ARG_POINTER(aJust);

  *aJust = (PRInt32)mPageNumJust;
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

    ReadBitFieldPref(prefs, kPrintEvenPages,  NS_PRINT_OPTIONS_PRINT_EVEN_PAGES);
    ReadBitFieldPref(prefs, kPrintOddPages,   NS_PRINT_OPTIONS_PRINT_ODD_PAGES);
    ReadBitFieldPref(prefs, kPrintDocTitle,   NS_PRINT_OPTIONS_PRINT_DOC_TITLE);
    ReadBitFieldPref(prefs, kPrintDocLoc,     NS_PRINT_OPTIONS_PRINT_DOC_LOCATION);
    ReadBitFieldPref(prefs, kPageNums,        NS_PRINT_OPTIONS_PRINT_PAGE_NUMS);
    ReadBitFieldPref(prefs, kPrintPageTotals, NS_PRINT_OPTIONS_PRINT_PAGE_TOTAL);
    ReadBitFieldPref(prefs, kPrintDate,       NS_PRINT_OPTIONS_PRINT_DATE_PRINTED);

    ReadJustification(prefs, kPageNumsJust, mPageNumJust, NS_PRINT_JUSTIFY_LEFT);

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

    WriteBitFieldPref(prefs, kPrintEvenPages,  NS_PRINT_OPTIONS_PRINT_EVEN_PAGES);
    WriteBitFieldPref(prefs, kPrintOddPages,   NS_PRINT_OPTIONS_PRINT_ODD_PAGES);
    WriteBitFieldPref(prefs, kPrintDocTitle,   NS_PRINT_OPTIONS_PRINT_DOC_TITLE);
    WriteBitFieldPref(prefs, kPrintDocLoc,     NS_PRINT_OPTIONS_PRINT_DOC_LOCATION);
    WriteBitFieldPref(prefs, kPageNums,        NS_PRINT_OPTIONS_PRINT_PAGE_NUMS);
    WriteBitFieldPref(prefs, kPrintPageTotals, NS_PRINT_OPTIONS_PRINT_PAGE_TOTAL);
    WriteBitFieldPref(prefs, kPrintDate,       NS_PRINT_OPTIONS_PRINT_DATE_PRINTED);

    WriteJustification(prefs, kPageNumsJust, mPageNumJust);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------
//-- Protected Methods
//-----------------------------------------------------
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
  char * str = inchesStr.ToNewCString();
  if (str) {
    aPref->SetCharPref(aPrefId, str);
  } else {
    aPref->SetCharPref(aPrefId, "0.5");
  }
}

//---------------------------------------------------
void nsPrintOptions::ReadJustification(nsIPref *    aPref, 
                                       const char * aPrefId, 
                                       PRInt32&     aJust,
                                       PRInt32      aInitValue)
{
  aJust = aInitValue;
  char * str = nsnull;
  nsresult rv = aPref->CopyCharPref(aPrefId, &str);
  if (NS_SUCCEEDED(rv) && str) {
    nsAutoString justStr;
    justStr.AssignWithConversion(str);

    if (justStr.EqualsWithConversion(kRightJust)) {
      aJust = NS_PRINT_JUSTIFY_RIGHT;

    } else if (justStr.EqualsWithConversion(kCenterJust)) {
      aJust = NS_PRINT_JUSTIFY_CENTER;

    } else {
      aJust = NS_PRINT_JUSTIFY_LEFT;
    }
    nsMemory::Free(str);
  }
}

//---------------------------------------------------
void nsPrintOptions::WriteJustification(nsIPref *    aPref, 
                                        const char * aPrefId, 
                                        PRInt32      aJust)
{
  switch (aJust) {
    case NS_PRINT_JUSTIFY_LEFT: 
      aPref->SetCharPref(aPrefId, kLeftJust);
      break;

    case NS_PRINT_JUSTIFY_CENTER: 
      aPref->SetCharPref(aPrefId, kCenterJust);
      break;

    case NS_PRINT_JUSTIFY_RIGHT: 
      aPref->SetCharPref(aPrefId, kRightJust);
      break;
  } //switch
}

