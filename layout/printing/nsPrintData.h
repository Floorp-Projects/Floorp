/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintData_h___
#define nsPrintData_h___

#include "mozilla/Attributes.h"
#include "mozilla/UniquePtr.h"

// Interfaces
#include "nsDeviceContext.h"
#include "nsIPrintProgressParams.h"
#include "nsIPrintSettings.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsCOMArray.h"

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

  explicit nsPrintData(ePrintDataType aType);

  NS_INLINE_DECL_REFCOUNTING(nsPrintData)

  // Listener Helper Methods
  void OnEndPrinting();
  void OnStartPrinting();
  void DoOnProgressChange(int32_t      aProgress,
                          int32_t      aMaxProgress,
                          bool         aDoStartStop,
                          int32_t      aFlag);

  void DoOnStatusChange(nsresult aStatus);


  ePrintDataType               mType;            // the type of data this is (Printing or Print Preview)
  RefPtr<nsDeviceContext>   mPrintDC;

  mozilla::UniquePtr<nsPrintObject> mPrintObject;

  nsCOMArray<nsIWebProgressListener> mPrintProgressListeners;
  nsCOMPtr<nsIPrintProgressParams> mPrintProgressParams;

  nsCOMPtr<nsPIDOMWindowOuter> mCurrentFocusWin; // cache a pointer to the currently focused window

  // Array of non-owning pointers to all the nsPrintObjects owned by this
  // nsPrintData. This includes this->mPrintObject, as well as all of its
  // mKids (and their mKids, etc.)
  nsTArray<nsPrintObject*>    mPrintDocList;

  bool                        mIsIFrameSelected;
  bool                        mIsParentAFrameSet;
  bool                        mOnStartSent;
  bool                        mIsAborted;           // tells us the document is being aborted
  bool                        mPreparingForPrint;   // see comments above
  bool                        mDocWasToBeDestroyed; // see comments above
  bool                        mShrinkToFit;
  int16_t                     mPrintFrameType;
  int32_t                     mNumPrintablePages;
  int32_t                     mNumPagesPrinted;
  float                       mShrinkRatio;

  nsCOMPtr<nsIPrintSettings>  mPrintSettings;
  nsPrintPreviewListener*     mPPEventListeners;

  nsString                    mBrandName; //  needed as a substitute name for a document

private:
  nsPrintData() = delete;
  nsPrintData& operator=(const nsPrintData& aOther) = delete;

  ~nsPrintData(); // non-virtual
};

#endif /* nsPrintData_h___ */

