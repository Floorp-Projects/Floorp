/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPagePrintTimer.h"
#include "nsIContentViewer.h"
#include "nsIServiceManager.h"
#include "nsPrintEngine.h"

NS_IMPL_ISUPPORTS1(nsPagePrintTimer, nsITimerCallback)

nsPagePrintTimer::nsPagePrintTimer() :
  mPrintEngine(nsnull),
  mDelay(0),
  mFiringCount(0),
  mPrintObj(nsnull)
{
}

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
    PRUint32 delay = 0;
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



// nsITimerCallback
NS_IMETHODIMP
nsPagePrintTimer::Notify(nsITimer *timer)
{
  if (mDocViewerPrint) {
    bool initNewTimer = true;
    // Check to see if we are done
    // inRange will be true if a page is actually printed
    bool inRange;
    // donePrinting will be true if it completed successfully or
    // if the printing was cancelled
    bool donePrinting = mPrintEngine->PrintPage(mPrintObj, inRange);
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
  }
  return NS_OK;
}

void 
nsPagePrintTimer::Init(nsPrintEngine*          aPrintEngine,
                       nsIDocumentViewerPrint* aDocViewerPrint,
                       PRUint32                aDelay)
{
  mPrintEngine     = aPrintEngine;
  mDocViewerPrint  = aDocViewerPrint;
  mDelay           = aDelay;

  mDocViewerPrint->IncrementDestroyRefCount();
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
    mTimer = nsnull;
  }
}

nsresult NS_NewPagePrintTimer(nsPagePrintTimer **aResult)
{

  NS_PRECONDITION(aResult, "null param");

  nsPagePrintTimer* result = new nsPagePrintTimer;

  if (!result) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(result);
  *aResult = result;

  return NS_OK;
}

