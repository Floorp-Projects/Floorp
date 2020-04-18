/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintData.h"

#include "nsIPrintProgressParams.h"
#include "nsIStringBundle.h"
#include "nsIWidget.h"
#include "nsPrintObject.h"
#include "nsIWebProgressListener.h"
#include "mozilla/Services.h"
#include "PrintPreviewUserEventSuppressor.h"

//-----------------------------------------------------
// PR LOGGING
#include "mozilla/Logging.h"

static mozilla::LazyLogModule gPrintingLog("printing");

#define PR_PL(_p1) MOZ_LOG(gPrintingLog, mozilla::LogLevel::Debug, _p1);

//---------------------------------------------------
//-- nsPrintData Class Impl
//---------------------------------------------------
nsPrintData::nsPrintData(ePrintDataType aType)
    : mType(aType),
      mPrintDocList(0),
      mIsIFrameSelected(false),
      mIsParentAFrameSet(false),
      mOnStartSent(false),
      mIsAborted(false),
      mPreparingForPrint(false),
      mShrinkToFit(false),
      mNumPrintablePages(0),
      mNumPagesPrinted(0),
      mShrinkRatio(1.0) {}

nsPrintData::~nsPrintData() {
  if (mPPEventSuppressor) {
    mPPEventSuppressor->StopSuppressing();
    mPPEventSuppressor = nullptr;
  }

  // Only Send an OnEndPrinting if we have started printing
  if (mOnStartSent && mType != eIsPrintPreview) {
    OnEndPrinting();
  }

  if (mPrintDC) {
    PR_PL(("****************** End Document ************************\n"));
    PR_PL(("\n"));
    bool isCancelled = false;
    mPrintSettings->GetIsCancelled(&isCancelled);

    nsresult rv = NS_OK;
    if (mType == eIsPrinting && mPrintDC->IsCurrentlyPrintingDocument()) {
      if (!isCancelled && !mIsAborted) {
        rv = mPrintDC->EndDocument();
      } else {
        rv = mPrintDC->AbortDocument();
      }
      if (NS_FAILED(rv)) {
        // XXX nsPrintData::ShowPrintErrorDialog(rv);
      }
    }
  }
}

void nsPrintData::OnStartPrinting() {
  if (!mOnStartSent) {
    DoOnProgressChange(0, 0, true,
                       nsIWebProgressListener::STATE_START |
                           nsIWebProgressListener::STATE_IS_DOCUMENT |
                           nsIWebProgressListener::STATE_IS_NETWORK);
    mOnStartSent = true;
  }
}

void nsPrintData::OnEndPrinting() {
  DoOnProgressChange(100, 100, true,
                     nsIWebProgressListener::STATE_STOP |
                         nsIWebProgressListener::STATE_IS_DOCUMENT);
  DoOnProgressChange(100, 100, true,
                     nsIWebProgressListener::STATE_STOP |
                         nsIWebProgressListener::STATE_IS_NETWORK);
}

void nsPrintData::DoOnProgressChange(int32_t aProgress, int32_t aMaxProgress,
                                     bool aDoStartStop, int32_t aFlag) {
  size_t numberOfListeners = mPrintProgressListeners.Length();
  for (size_t i = 0; i < numberOfListeners; ++i) {
    nsCOMPtr<nsIWebProgressListener> listener =
        mPrintProgressListeners.SafeElementAt(i);
    if (NS_WARN_IF(!listener)) {
      continue;
    }
    listener->OnProgressChange(nullptr, nullptr, aProgress, aMaxProgress,
                               aProgress, aMaxProgress);
    if (aDoStartStop) {
      listener->OnStateChange(nullptr, nullptr, aFlag, NS_OK);
    }
  }
}

void nsPrintData::DoOnStatusChange(nsresult aStatus) {
  size_t numberOfListeners = mPrintProgressListeners.Length();
  for (size_t i = 0; i < numberOfListeners; ++i) {
    nsCOMPtr<nsIWebProgressListener> listener =
        mPrintProgressListeners.SafeElementAt(i);
    if (NS_WARN_IF(!listener)) {
      continue;
    }
    listener->OnStatusChange(nullptr, nullptr, aStatus, nullptr);
  }
}
