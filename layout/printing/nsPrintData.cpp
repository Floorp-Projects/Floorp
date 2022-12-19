/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintData.h"

#include "mozilla/gfx/PrintPromise.h"
#include "nsIStringBundle.h"
#include "nsIWidget.h"
#include "nsPrintObject.h"
#include "nsIWebProgressListener.h"
#include "mozilla/Services.h"

//-----------------------------------------------------
// PR LOGGING
#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gPrintingLog;

#define PR_PL(_p1) MOZ_LOG(gPrintingLog, mozilla::LogLevel::Debug, _p1);

static void InformListenersOfProgressChange(
    const nsCOMArray<nsIWebProgressListener>& aListeners, int32_t aProgress,
    int32_t aMaxProgress, bool aDoStartStop, int32_t aFlag) {
  size_t numberOfListeners = aListeners.Length();
  for (size_t i = 0; i < numberOfListeners; ++i) {
    nsCOMPtr<nsIWebProgressListener> listener = aListeners.SafeElementAt(i);
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

static void InformListenersOfEndPrinting(
    const nsCOMArray<nsIWebProgressListener>& aListeners) {
  InformListenersOfProgressChange(
      aListeners, 100, 100, true,
      nsIWebProgressListener::STATE_STOP |
          nsIWebProgressListener::STATE_IS_DOCUMENT);
  InformListenersOfProgressChange(aListeners, 100, 100, true,
                                  nsIWebProgressListener::STATE_STOP |
                                      nsIWebProgressListener::STATE_IS_NETWORK);
}

//---------------------------------------------------
//-- nsPrintData Class Impl
//---------------------------------------------------
nsPrintData::nsPrintData(ePrintDataType aType)
    : mType(aType), mOnStartSent(false), mIsAborted(false) {}

nsPrintData::~nsPrintData() {
  // Two things need to be done:
  // - Inform the listeners
  // - End/Abort document
  // Preview requires neither, so return early.
  if (mType == eIsPrintPreview) {
    return;
  }

  if (mPrintDC) {
    PR_PL(("****************** End Document ************************\n"));
    PR_PL(("\n"));
    if (mPrintDC->IsCurrentlyPrintingDocument()) {
      if (!mIsAborted) {
        auto promise = mPrintDC->EndDocument();
        if (mOnStartSent) {
          promise->Then(mozilla::GetMainThreadSerialEventTarget(), __func__,
                        [listeners = std::move(mPrintProgressListeners)](
                            // We're in dtor, so capture listeners by move.
                            const mozilla::gfx::PrintEndDocumentPromise::
                                ResolveOrRejectValue&) {
                          InformListenersOfEndPrinting(listeners);
                        });
        }
        // Informing listeners asynchronously, or don't need to inform them, so
        // return early.
        return;
      }
      mPrintDC->AbortDocument();
    }
  }
  if (mOnStartSent) {
    // Synchronously notify the listeners.
    OnEndPrinting();
  }
}

void nsPrintData::OnStartPrinting() {
  if (!mOnStartSent) {
    InformListenersOfProgressChange(
        mPrintProgressListeners, 0, 0, true,
        nsIWebProgressListener::STATE_START |
            nsIWebProgressListener::STATE_IS_DOCUMENT |
            nsIWebProgressListener::STATE_IS_NETWORK);
    mOnStartSent = true;
  }
}

void nsPrintData::OnEndPrinting() {
  InformListenersOfEndPrinting(mPrintProgressListeners);
}

void nsPrintData::DoOnProgressChange(int32_t aProgress, int32_t aMaxProgress,
                                     bool aDoStartStop, int32_t aFlag) {
  InformListenersOfProgressChange(mPrintProgressListeners, aProgress,
                                  aMaxProgress, aDoStartStop, aFlag);
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
