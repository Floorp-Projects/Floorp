/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPagePrintTimer.h"

#include "mozilla/Unused.h"
#include "nsIContentViewer.h"
#include "nsIServiceManager.h"
#include "nsPrintJob.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS_INHERITED(nsPagePrintTimer, mozilla::Runnable, nsITimerCallback)

nsPagePrintTimer::~nsPagePrintTimer()
{
  // This matches the IncrementDestroyBlockedCount call in the constructor.
  mDocViewerPrint->DecrementDestroyBlockedCount();
}

nsresult
nsPagePrintTimer::StartTimer(bool aUseDelay)
{
  uint32_t delay = 0;
  if (aUseDelay) {
    if (mFiringCount < 10) {
      // Longer delay for the few first pages.
      delay = mDelay + ((10 - mFiringCount) * 100);
    } else {
      delay = mDelay;
    }
  }
  return NS_NewTimerWithCallback(getter_AddRefs(mTimer),
                                 this, delay, nsITimer::TYPE_ONE_SHOT,
                                 mDocument->EventTargetFor(TaskCategory::Other));
}

nsresult
nsPagePrintTimer::StartWatchDogTimer()
{
  if (mWatchDogTimer) {
    mWatchDogTimer->Cancel();
  }
  // Instead of just doing one timer for a long period do multiple so we
  // can check if the user cancelled the printing.
  return NS_NewTimerWithCallback(getter_AddRefs(mWatchDogTimer),
                                 this, WATCH_DOG_INTERVAL,
                                 nsITimer::TYPE_ONE_SHOT,
                                 mDocument->EventTargetFor(TaskCategory::Other));
}

void
nsPagePrintTimer::StopWatchDogTimer()
{
  if (mWatchDogTimer) {
    mWatchDogTimer->Cancel();
    mWatchDogTimer = nullptr;
  }
}

//nsRunnable
NS_IMETHODIMP
nsPagePrintTimer::Run()
{
  bool initNewTimer = true;
  // Check to see if we are done
  // inRange will be true if a page is actually printed
  bool inRange;
  bool donePrinting;

  // donePrinting will be true if it completed successfully or
  // if the printing was cancelled
  donePrinting = !mPrintJob || mPrintJob->PrintPage(mPrintObj, inRange);
  if (donePrinting) {

    if (mWaitingForRemotePrint ||
        // If we are not waiting for the remote printing, it is the time to
        // end printing task by calling DonePrintingPages.
        (!mPrintJob || mPrintJob->DonePrintingPages(mPrintObj, NS_OK))) {
      initNewTimer = false;
      mDone = true;
    }
  }

  // Note that the Stop() destroys this after the print job finishes
  // (The nsPrintJob stops holding a reference when DonePrintingPages
  // returns true.)
  Stop();
  if (initNewTimer) {
    ++mFiringCount;
    nsresult result = StartTimer(inRange);
    if (NS_FAILED(result)) {
      mDone = true;     // had a failure.. we are finished..
      if (mPrintJob) {
        mPrintJob->SetIsPrinting(false);
      }
    }
  }
  return NS_OK;
}

// nsITimerCallback
NS_IMETHODIMP
nsPagePrintTimer::Notify(nsITimer *timer)
{
  // When finished there may be still pending notifications, which we can just
  // ignore.
  if (mDone) {
    return NS_OK;
  }

  // There are four things that call Notify with different values for timer:
  // 1) the delay between pages (timer == mTimer)
  // 2) canvasPrintState done (timer == null)
  // 3) the watch dog timer (timer == mWatchDogTimer)
  // 4) the waiting for remote print "timer" (timer == mWaitingForRemotePrint)
  if (!timer) {
    // Reset the counter since a mozPrintCallback has finished.
    mWatchDogCount = 0;
  } else if (timer == mTimer) {
    // Reset the watchdog timer before the start of every page.
    mWatchDogCount = 0;
    mTimer = nullptr;
  } else if (timer == mWaitingForRemotePrint) {
    mWaitingForRemotePrint = nullptr;

    // If we are still waiting for the page delay timer, don't let the
    // notification from the remote print job trigger the next page.
    if (mTimer) {
      return NS_OK;
    }
  } else if (timer == mWatchDogTimer) {
    mWatchDogCount++;
    if (mWatchDogCount > WATCH_DOG_MAX_COUNT) {
      Fail();
      return NS_OK;
    }
  }

  bool donePrePrint = true;
  // Don't start to pre-print if we're waiting on the parent still.
  if (mPrintJob && !mWaitingForRemotePrint) {
    donePrePrint = mPrintJob->PrePrintPage();
  }

  if (donePrePrint && !mWaitingForRemotePrint) {
    StopWatchDogTimer();
    // Pass nullptr here since name already was set in constructor.
    mDocument->Dispatch(TaskCategory::Other, do_AddRef(this));
  } else {
    // Start the watch dog if we're waiting for preprint to ensure that if any
    // mozPrintCallbacks take to long we error out.
    StartWatchDogTimer();
  }

  return NS_OK;
}


void
nsPagePrintTimer::WaitForRemotePrint()
{
  mWaitingForRemotePrint = NS_NewTimer();
  if (!mWaitingForRemotePrint) {
    NS_WARNING("Failed to wait for remote print, we might time-out.");
  }
}

void
nsPagePrintTimer::RemotePrintFinished()
{
  if (!mWaitingForRemotePrint) {
    return;
  }

  // now clean up print or print the next webshell
  if (mDone && mPrintJob) {
    mDone = mPrintJob->DonePrintingPages(mPrintObj, NS_OK);
  }

  mWaitingForRemotePrint->SetTarget(
    mDocument->EventTargetFor(mozilla::TaskCategory::Other));
  mozilla::Unused <<
    mWaitingForRemotePrint->InitWithCallback(this, 0, nsITimer::TYPE_ONE_SHOT);
}

nsresult
nsPagePrintTimer::Start(nsPrintObject* aPO)
{
  mPrintObj = aPO;
  mDone = false;
  return StartTimer(false);
}


void
nsPagePrintTimer::Stop()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  StopWatchDogTimer();
}

void
nsPagePrintTimer::Fail()
{
  NS_WARNING("nsPagePrintTimer::Fail called");

  mDone = true;
  Stop();
  if (mPrintJob) {
    mPrintJob->CleanupOnFailure(NS_OK, false);
  }
}
