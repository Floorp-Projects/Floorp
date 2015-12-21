/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPagePrintTimer.h"
#include "nsIContentViewer.h"
#include "nsIServiceManager.h"
#include "nsPrintEngine.h"

NS_IMPL_ISUPPORTS_INHERITED(nsPagePrintTimer, nsRunnable, nsITimerCallback)

nsPagePrintTimer::~nsPagePrintTimer()
{
  // "Destroy" the document viewer; this normally doesn't actually
  // destroy it because of the IncrementDestroyRefCount call below
  // XXX This is messy; the document viewer should use a single approach
  // to keep itself alive during printing
  nsCOMPtr<nsIContentViewer> cv(do_QueryInterface(mDocViewerPrint));
  if (cv) {
    cv->Destroy();
  }
}

nsresult 
nsPagePrintTimer::StartTimer(bool aUseDelay)
{
  nsresult result;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &result);
  if (NS_FAILED(result)) {
    NS_WARNING("unable to start the timer");
  } else {
    uint32_t delay = 0;
    if (aUseDelay) {
      if (mFiringCount < 10) {
        // Longer delay for the few first pages.
        delay = mDelay + ((10 - mFiringCount) * 100);
      } else {
        delay = mDelay;
      }
    }
    mTimer->InitWithCallback(this, delay, nsITimer::TYPE_ONE_SHOT);
  }
  return result;
}

nsresult
nsPagePrintTimer::StartWatchDogTimer()
{
  nsresult result;
  if (mWatchDogTimer) {
    mWatchDogTimer->Cancel();
  }
  mWatchDogTimer = do_CreateInstance("@mozilla.org/timer;1", &result);
  if (NS_FAILED(result)) {
    NS_WARNING("unable to start the timer");
  } else {
    // Instead of just doing one timer for a long period do multiple so we
    // can check if the user cancelled the printing.
    mWatchDogTimer->InitWithCallback(this, WATCH_DOG_INTERVAL,
                                     nsITimer::TYPE_ONE_SHOT);
  }
  return result;
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
  donePrinting = mPrintEngine->PrintPage(mPrintObj, inRange);
  if (donePrinting) {
    // now clean up print or print the next webshell
    if (mPrintEngine->DonePrintingPages(mPrintObj, NS_OK)) {
      initNewTimer = false;
      mDone = true;
    }
  }

  // Note that the Stop() destroys this after the print job finishes
  // (The PrintEngine stops holding a reference when DonePrintingPages
  // returns true.)
  Stop(); 
  if (initNewTimer) {
    ++mFiringCount;
    nsresult result = StartTimer(inRange);
    if (NS_FAILED(result)) {
      mDone = true;     // had a failure.. we are finished..
      mPrintEngine->SetIsPrinting(false);
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

  // There are three things that call Notify with different values for timer:
  // 1) the delay between pages (timer == mTimer)
  // 2) canvasPrintState done (timer == null)
  // 3) the watch dog timer (timer == mWatchDogTimer)
  if (timer && timer == mWatchDogTimer) {
    mWatchDogCount++;
    if (mWatchDogCount > WATCH_DOG_MAX_COUNT) {
      Fail();
      return NS_OK;
    }
  } else if(!timer) {
    // Reset the counter since a mozPrintCallback has finished.
    mWatchDogCount = 0;
  }

  if (mDocViewerPrint) {
    bool donePrePrint = mPrintEngine->PrePrintPage();

    if (donePrePrint) {
      StopWatchDogTimer();
      NS_DispatchToMainThread(this);
    } else {
      // Start the watch dog if we're waiting for preprint to ensure that if any
      // mozPrintCallbacks take to long we error out.
      StartWatchDogTimer();
    }

  }
  return NS_OK;
}

nsresult 
nsPagePrintTimer::Start(nsPrintObject* aPO)
{
  mPrintObj = aPO;
  mWatchDogCount = 0;
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
  mDone = true;
  Stop();
  if (mPrintEngine) {
    mPrintEngine->CleanupOnFailure(NS_OK, false);
  }
}
