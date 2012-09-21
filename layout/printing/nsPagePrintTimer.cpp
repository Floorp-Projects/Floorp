/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPagePrintTimer.h"
#include "nsIContentViewer.h"
#include "nsIServiceManager.h"
#include "nsPrintEngine.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsPagePrintTimer, nsRunnable, nsITimerCallback)

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
      donePrinting = true;     // had a failure.. we are finished..
      mPrintEngine->SetIsPrinting(false);
    }
  }
  return NS_OK;
}

// nsITimerCallback
NS_IMETHODIMP
nsPagePrintTimer::Notify(nsITimer *timer)
{
  if (mDocViewerPrint) {
    bool donePrePrint = mPrintEngine->PrePrintPage();

    if (donePrePrint) {
      NS_DispatchToMainThread(this);
    }

  }
  return NS_OK;
}

nsresult 
nsPagePrintTimer::Start(nsPrintObject* aPO)
{
  mPrintObj = aPO;
  return StartTimer(false);
}


void  
nsPagePrintTimer::Stop()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}
