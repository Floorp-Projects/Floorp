/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPagePrintTimer.h"

#include "mozilla/dom/Document.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/Unused.h"
#include "nsPrintJob.h"
#include "nsPrintObject.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED(nsPagePrintTimer, mozilla::Runnable,
                            nsITimerCallback)

nsPagePrintTimer::nsPagePrintTimer(nsPrintJob* aPrintJob,
                                   nsIDocumentViewerPrint* aDocViewerPrint,
                                   mozilla::dom::Document* aDocument,
                                   uint32_t aDelay)
    : Runnable("nsPagePrintTimer"),
      mPrintJob(aPrintJob),
      mDocViewerPrint(aDocViewerPrint),
      mDocument(aDocument),
      mDelay(aDelay),
      mFiringCount(0),
      mPrintObj(nullptr),
      mWatchDogCount(0),
      mDone(false) {
  MOZ_ASSERT(aDocViewerPrint && aDocument);
  mDocViewerPrint->IncrementDestroyBlockedCount();
}

nsPagePrintTimer::~nsPagePrintTimer() { Disconnect(); }

void nsPagePrintTimer::Disconnect() {
  mPrintJob = nullptr;
  mPrintObj = nullptr;
  if (mDocViewerPrint) {
    // This matches the IncrementDestroyBlockedCount call in the constructor.
    mDocViewerPrint->DecrementDestroyBlockedCount();
    mDocViewerPrint = nullptr;
  }
}

nsresult nsPagePrintTimer::StartTimer(bool aUseDelay) {
  uint32_t delay = 0;
  if (aUseDelay) {
    if (mFiringCount < 10) {
      // Longer delay for the few first pages.
      delay = mDelay + ((10 - mFiringCount) * 100);
    } else {
      delay = mDelay;
    }
  }
  return NS_NewTimerWithCallback(getter_AddRefs(mTimer), this, delay,
                                 nsITimer::TYPE_ONE_SHOT,
                                 GetMainThreadSerialEventTarget());
}

nsresult nsPagePrintTimer::StartWatchDogTimer() {
  if (mWatchDogTimer) {
    mWatchDogTimer->Cancel();
  }
  // Instead of just doing one timer for a long period do multiple so we
  // can check if the user cancelled the printing.
  return NS_NewTimerWithCallback(getter_AddRefs(mWatchDogTimer), this,
                                 WATCH_DOG_INTERVAL, nsITimer::TYPE_ONE_SHOT,
                                 GetMainThreadSerialEventTarget());
}

void nsPagePrintTimer::StopWatchDogTimer() {
  if (mWatchDogTimer) {
    mWatchDogTimer->Cancel();
    mWatchDogTimer = nullptr;
  }
}

// nsRunnable
NS_IMETHODIMP
nsPagePrintTimer::Run() {
  bool initNewTimer = true;
  bool donePrinting;

  // donePrinting will be true if it completed successfully or
  // if the printing was cancelled
  donePrinting = !mPrintJob || mPrintJob->PrintSheet(mPrintObj);
  if (donePrinting) {
    if (mWaitingForRemotePrint ||
        // If we are not waiting for the remote printing, it is the time to
        // end printing task by calling DonePrintingSheets.
        (!mPrintJob || mPrintJob->DonePrintingSheets(mPrintObj, NS_OK))) {
      initNewTimer = false;
      mDone = true;
    }
  }

  // Note that the Stop() destroys this after the print job finishes
  // (The nsPrintJob stops holding a reference when DonePrintingSheets
  // returns true.)
  Stop();
  if (initNewTimer) {
    ++mFiringCount;
    nsresult result = StartTimer(/*aUseDelay*/ true);
    if (NS_FAILED(result)) {
      mDone = true;  // had a failure.. we are finished..
      if (mPrintJob) {
        mPrintJob->SetIsPrinting(false);
      }
    }
  }
  return NS_OK;
}

// nsITimerCallback
NS_IMETHODIMP
nsPagePrintTimer::Notify(nsITimer* timer) {
  // When finished there may be still pending notifications, which we can just
  // ignore.
  if (mDone) {
    return NS_OK;
  }

  // There are four things that call Notify with different values for timer:
  // 1) the delay between sheets (timer == mTimer)
  // 2) canvasPrintState done (timer == null)
  // 3) the watch dog timer (timer == mWatchDogTimer)
  // 4) the waiting for remote print "timer" (timer == mWaitingForRemotePrint)
  if (!timer) {
    // Reset the counter since a mozPrintCallback has finished.
    mWatchDogCount = 0;
  } else if (timer == mTimer) {
    // Reset the watchdog timer before the start of every sheet.
    mWatchDogCount = 0;
    mTimer = nullptr;
  } else if (timer == mWaitingForRemotePrint) {
    mWaitingForRemotePrint = nullptr;

    // If we are still waiting for the sheet delay timer, don't let the
    // notification from the remote print job trigger the next sheet.
    if (mTimer) {
      return NS_OK;
    }
  } else if (timer == mWatchDogTimer) {
    mWatchDogCount++;
    PROFILER_MARKER_TEXT(
        "nsPagePrintTimer::Notify", LAYOUT_Printing, {},
        nsPrintfCString("Watchdog Timer Count %d", mWatchDogCount));

    if (mWatchDogCount > WATCH_DOG_MAX_COUNT) {
      Fail();
      return NS_OK;
    }
  }

  bool donePrePrint = true;
  // Don't start to pre-print if we're waiting on the parent still.
  if (mPrintJob && !mWaitingForRemotePrint) {
    donePrePrint = mPrintJob->PrePrintSheet();
  }

  if (donePrePrint && !mWaitingForRemotePrint) {
    StopWatchDogTimer();
    // Pass nullptr here since name already was set in constructor.
    mDocument->Dispatch(do_AddRef(this));
  } else {
    // Start the watch dog if we're waiting for preprint to ensure that if any
    // mozPrintCallbacks take to long we error out.
    StartWatchDogTimer();
  }

  return NS_OK;
}

void nsPagePrintTimer::WaitForRemotePrint() {
  mWaitingForRemotePrint = NS_NewTimer();
  if (!mWaitingForRemotePrint) {
    NS_WARNING("Failed to wait for remote print, we might time-out.");
  }
}

void nsPagePrintTimer::RemotePrintFinished() {
  if (!mWaitingForRemotePrint) {
    return;
  }

  // now clean up print or print the next webshell
  if (mDone && mPrintJob) {
    mDone = mPrintJob->DonePrintingSheets(mPrintObj, NS_OK);
  }

  mWaitingForRemotePrint->SetTarget(GetMainThreadSerialEventTarget());
  mozilla::Unused << mWaitingForRemotePrint->InitWithCallback(
      this, 0, nsITimer::TYPE_ONE_SHOT);
}

nsresult nsPagePrintTimer::Start(nsPrintObject* aPO) {
  mPrintObj = aPO;
  mDone = false;
  return StartTimer(false);
}

void nsPagePrintTimer::Stop() {
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  StopWatchDogTimer();
}

void nsPagePrintTimer::Fail() {
  NS_WARNING("nsPagePrintTimer::Fail called");
  PROFILER_MARKER_TEXT("nsPagePrintTimer", LAYOUT_Printing, {},
                       "nsPagePrintTimer::Fail aborting print operation"_ns);

  mDone = true;
  Stop();
  if (mPrintJob) {
    mPrintJob->CleanupOnFailure(NS_OK, false);
  }
}
