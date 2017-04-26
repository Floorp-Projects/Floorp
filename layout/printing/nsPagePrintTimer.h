/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPagePrintTimer_h___
#define nsPagePrintTimer_h___

// Timer Includes
#include "nsITimer.h"

#include "nsIDocumentViewerPrint.h"
#include "nsPrintObject.h"
#include "mozilla/Attributes.h"
#include "nsThreadUtils.h"

class nsPrintEngine;
class nsIDocument;

//---------------------------------------------------
//-- Page Timer Class
//---------------------------------------------------
class nsPagePrintTimer final : public mozilla::Runnable,
                               public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS_INHERITED

  nsPagePrintTimer(nsPrintEngine* aPrintEngine,
                   nsIDocumentViewerPrint* aDocViewerPrint,
                   nsIDocument* aDocument,
                   uint32_t aDelay)
    : Runnable("nsPagePrintTimer")
    , mPrintEngine(aPrintEngine)
    , mDocViewerPrint(aDocViewerPrint)
    , mDocument(aDocument)
    , mDelay(aDelay)
    , mFiringCount(0)
    , mPrintObj(nullptr)
    , mWatchDogCount(0)
    , mDone(false)
  {
    MOZ_ASSERT(aDocument);
    mDocViewerPrint->IncrementDestroyRefCount();
  }

  NS_DECL_NSITIMERCALLBACK

  nsresult Start(nsPrintObject* aPO);

  NS_IMETHOD Run() override;

  void Stop();

  void WaitForRemotePrint();
  void RemotePrintFinished();

  void Disconnect() { mPrintEngine = nullptr; }

private:
  ~nsPagePrintTimer();

  nsresult StartTimer(bool aUseDelay);
  nsresult StartWatchDogTimer();
  void     StopWatchDogTimer();
  void     Fail();

  nsPrintEngine*             mPrintEngine;
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
  nsCOMPtr<nsIDocument>      mDocument;
  nsCOMPtr<nsITimer>         mTimer;
  nsCOMPtr<nsITimer>         mWatchDogTimer;
  nsCOMPtr<nsITimer>         mWaitingForRemotePrint;
  uint32_t                   mDelay;
  uint32_t                   mFiringCount;
  nsPrintObject *            mPrintObj;
  uint32_t                   mWatchDogCount;
  bool                       mDone;

  static const uint32_t WATCH_DOG_INTERVAL  = 1000;
  static const uint32_t WATCH_DOG_MAX_COUNT =
#ifdef DEBUG
    // Debug builds are very slow (on Mac at least) and can need extra time
                                              30
#else
                                              10
#endif
  ;
};

#endif /* nsPagePrintTimer_h___ */
