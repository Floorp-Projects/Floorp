/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#ifndef nsPrintOptionsImpl_h__
#define nsPrintOptionsImpl_h__

#include "nsIPrintOptions.h"  

class nsIPref;

//*****************************************************************************
//***    nsPrintOptions
//*****************************************************************************
class nsPrintOptions : public nsIPrintOptions
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTOPTIONS

  // C++ methods
  NS_IMETHOD SetDefaultFont(const nsFont &aFont);
  NS_IMETHOD SetFontNamePointSize(const nsString& aFontName, nscoord aPointSize);

  NS_IMETHOD GetDefaultFont(nsFont &aFont);

  // non-scriptable C++ helper method
  NS_IMETHOD GetMargin(nsMargin& aMargin);

  nsPrintOptions();
  virtual ~nsPrintOptions();

protected:
  void ReadBitFieldPref(nsIPref * aPref, const char * aPrefId, PRInt32 anOption);
  void WriteBitFieldPref(nsIPref * aPref, const char * aPrefId, PRInt32 anOption);
  void ReadJustification(nsIPref *  aPref, const char * aPrefId, PRInt32& aJust, PRInt32 aInitValue);
  void WriteJustification(nsIPref * aPref, const char * aPrefId, PRInt32 aJust);
  void ReadInchesToTwipsPref(nsIPref * aPref, const char * aPrefId, nscoord&  aTwips);
  void WriteInchesFromTwipsPref(nsIPref * aPref, const char * aPrefId, nscoord aTwips);

  // Members 
  nsMargin      mMargin;

  nsPrintRange  mPrintRange;
  PRInt32       mStartPageNum; // only used for ePrintRange_SpecifiedRange
  PRInt32       mEndPageNum;
  PRInt32       mPageNumJust;

  PRInt32       mPrintOptions;

  nsFont*       mDefaultFont;
  nsString      mTitle;
  nsString      mURL;

};



#endif /* nsPrintOptions_h__ */
