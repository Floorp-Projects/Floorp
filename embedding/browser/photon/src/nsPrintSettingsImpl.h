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
 *   Brian Edmond <briane@qnx.com>
 */

#ifndef nsPrintSettingsImpl_h__
#define nsPrintSettingsImpl_h__

#include "nsIPrintSettings.h"  
#include "nsMargin.h"  
#include "nsString.h"  

#include <Pt.h>

//*****************************************************************************
//***    nsPrintSettings
//*****************************************************************************
class nsPrintSettings : public nsIPrintSettings
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTSETTINGS

  nsPrintSettings();
  virtual ~nsPrintSettings();

protected:
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
  double        mScaling;
  PRBool        mPrintBGColors;  // print background colors
  PRBool        mPrintBGImages;  // print background images

  PRInt16       mPrintFrameType;
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
};



#endif /* nsPrintSettings_h__ */
