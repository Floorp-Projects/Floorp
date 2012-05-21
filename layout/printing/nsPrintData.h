/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintData_h___
#define nsPrintData_h___

#include "mozilla/Attributes.h"

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
  void DoOnProgressChange(PRInt32      aProgress,
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
  nsPrintData() MOZ_DELETE;
  nsPrintData& operator=(const nsPrintData& aOther) MOZ_DELETE;

};

#endif /* nsPrintData_h___ */

