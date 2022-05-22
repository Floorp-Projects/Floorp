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
#include "nsIPrintSettings.h"
#include "nsISupportsImpl.h"
#include "nsCOMArray.h"

class nsPrintObject;
class nsIWebProgressListener;

class nsPrintData {
 public:
  typedef enum { eIsPrinting, eIsPrintPreview } ePrintDataType;

  explicit nsPrintData(ePrintDataType aType);

  NS_INLINE_DECL_REFCOUNTING(nsPrintData)

  // Listener Helper Methods
  void OnEndPrinting();
  void OnStartPrinting();
  void DoOnProgressChange(int32_t aProgress, int32_t aMaxProgress,
                          bool aDoStartStop, int32_t aFlag);

  void DoOnStatusChange(nsresult aStatus);

  ePrintDataType mType;  // the type of data this is (Printing or Print Preview)
  RefPtr<nsDeviceContext> mPrintDC;

  nsCOMArray<nsIWebProgressListener> mPrintProgressListeners;

  bool mOnStartSent;
  bool mIsAborted;  // tells us the document is being aborted

 private:
  nsPrintData() = delete;
  nsPrintData& operator=(const nsPrintData& aOther) = delete;

  ~nsPrintData();  // non-virtual
};

#endif /* nsPrintData_h___ */
