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
class NS_GFX nsPrintOptions : public nsIPrintOptions
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTOPTIONS

  nsPrintOptions();
  virtual ~nsPrintOptions();

protected:
  void ReadBitFieldPref(nsIPref * aPref, const char * aPrefId, PRInt32 anOption);
  void WriteBitFieldPref(nsIPref * aPref, const char * aPrefId, PRInt32 anOption);
  void ReadJustification(nsIPref *  aPref, const char * aPrefId, PRInt16& aJust, PRInt16 aInitValue);
  void WriteJustification(nsIPref * aPref, const char * aPrefId, PRInt16 aJust);
  void ReadInchesToTwipsPref(nsIPref * aPref, const char * aPrefId, nscoord&  aTwips);
  void WriteInchesFromTwipsPref(nsIPref * aPref, const char * aPrefId, nscoord aTwips);

  nsresult ReadPrefString(nsIPref * aPref, const char * aPrefId, nsString& aString);
  nsresult WritePrefString(nsIPref * aPref, const char * aPrefId, nsString& aString);

  typedef enum {
    eHeader,
    eFooter
  } nsHeaderFooterEnum;

  nsresult GetMarginStrs(PRUnichar * *aTitle, nsHeaderFooterEnum aType, PRInt16 aJust);
  nsresult SetMarginStrs(const PRUnichar * aTitle, nsHeaderFooterEnum aType, PRInt16 aJust);

  // Members 
  nsMargin      mMargin;
  PRInt32       mPrintOptions;

  // scriptable data members
  PRInt16       mPrintRange;
  PRInt32       mStartPageNum; // only used for ePrintRange_SpecifiedRange
  PRInt32       mEndPageNum;

  PRInt16       mPrintFrameType;
  PRBool        mHowToEnableFrameUI;
  PRBool        mIsCancelled;
  PRBool        mPrintSilent;
  PRInt32       mPrintPageDelay;

  nsString      mTitle;
  nsString      mURL;
  nsString      mPageNumberFormat;
  nsString      mHeaderStrs[3];
  nsString      mFooterStrs[3];

  PRBool        mPrintReversed;
  PRBool        mPrintInColor; // a false means grayscale
  PRInt32       mPaperSize;    // see page size consts
  PRInt32       mOrientation;  // see orientation consts
  nsString      mPrintCommand;
  PRBool        mPrintToFile;
  nsString      mToFileName;

  static nsFont* sDefaultFont;
};



#endif /* nsPrintOptions_h__ */
