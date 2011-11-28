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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
#ifndef nsPrintData_h___
#define nsPrintData_h___

// Interfaces
#include "nsIDOMWindow.h"
#include "nsDeviceContext.h"
#include "nsIPrintProgressParams.h"
#include "nsIPrintOptions.h"
#include "nsTArray.h"
#include "nsCOMArray.h"
#include "nsAutoPtr.h"

// Classes
class nsPrintObject;
class nsPrintPreviewListener;
class nsIWebProgressListener;

//------------------------------------------------------------------------
// nsPrintData Class
//
// mPreparingForPrint - indicates that we have started Printing but 
//   have not gone to the timer to start printing the pages. It gets turned 
//   off right before we go to the timer.
//
// mDocWasToBeDestroyed - Gets set when "someone" tries to unload the document
//   while we were prparing to Print. This typically happens if a user starts 
//   to print while a page is still loading. If they start printing and pause 
//   at the print dialog and then the page comes in, we then abort printing 
//   because the document is no longer stable.
// 
//------------------------------------------------------------------------
class nsPrintData {
public:

  typedef enum {eIsPrinting, eIsPrintPreview } ePrintDataType;

  // This enum tells indicates what the default should be for the title
  // if the title from the document is null
  enum eDocTitleDefault {
    eDocTitleDefNone,
    eDocTitleDefBlank,
    eDocTitleDefURLDoc
  };


  nsPrintData(ePrintDataType aType);
  ~nsPrintData(); // non-virtual

  // Listener Helper Methods
  void OnEndPrinting();
  void OnStartPrinting();
  void DoOnProgressChange(PRInt32      aProgess,
                          PRInt32      aMaxProgress,
                          bool         aDoStartStop,
                          PRInt32      aFlag);


  ePrintDataType               mType;            // the type of data this is (Printing or Print Preview)
  nsRefPtr<nsDeviceContext>   mPrintDC;
  FILE                        *mDebugFilePtr;    // a file where information can go to when printing

  nsPrintObject *                mPrintObject;
  nsPrintObject *                mSelectedPO;

  nsCOMArray<nsIWebProgressListener> mPrintProgressListeners;
  nsCOMPtr<nsIPrintProgressParams> mPrintProgressParams;

  nsCOMPtr<nsIDOMWindow> mCurrentFocusWin; // cache a pointer to the currently focused window

  nsTArray<nsPrintObject*>    mPrintDocList;
  bool                        mIsIFrameSelected;
  bool                        mIsParentAFrameSet;
  bool                        mOnStartSent;
  bool                        mIsAborted;           // tells us the document is being aborted
  bool                        mPreparingForPrint;   // see comments above
  bool                        mDocWasToBeDestroyed; // see comments above
  bool                        mShrinkToFit;
  PRInt16                     mPrintFrameType;
  PRInt32                     mNumPrintablePages;
  PRInt32                     mNumPagesPrinted;
  float                       mShrinkRatio;
  float                       mOrigDCScale;

  nsCOMPtr<nsIPrintSettings>  mPrintSettings;
  nsPrintPreviewListener*     mPPEventListeners;

  PRUnichar*            mBrandName; //  needed as a substitute name for a document

private:
  nsPrintData(); //not implemented
  nsPrintData& operator=(const nsPrintData& aOther); // not implemented

};

#endif /* nsPrintData_h___ */

