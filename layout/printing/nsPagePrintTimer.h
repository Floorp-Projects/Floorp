/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/OwningNonNull.h"
#include "nsThreadUtils.h"

class nsPrintJob;

//---------------------------------------------------
//-- Page Timer Class
//---------------------------------------------------
// Strictly speaking, this actually manages the timing of printing *sheets*
// (instances of "PrintedSheetFrame"), each of which may encompass multiple
// pages (nsPageFrames) of the document. The use of "Page" in the class name
// here is for historical / colloquial purposes.
class nsPagePrintTimer final : public mozilla::Runnable,
                               public nsITimerCallback {
 public:
  NS_DECL_ISUPPORTS_INHERITED

  nsPagePrintTimer(nsPrintJob* aPrintJob,
                   nsIDocumentViewerPrint* aDocViewerPrint,
                   mozilla::dom::Document* aDocument, uint32_t aDelay)
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

  NS_DECL_NSITIMERCALLBACK

  nsresult Start(nsPrintObject* aPO);

  NS_IMETHOD Run() override;

  void Stop();

  void WaitForRemotePrint();
  void RemotePrintFinished();

  void Disconnect();

 private:
  ~nsPagePrintTimer();

  nsresult StartTimer(bool aUseDelay);
  nsresult StartWatchDogTimer();
  void StopWatchDogTimer();
  void Fail();

  nsPrintJob* mPrintJob;
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
  RefPtr<mozilla::dom::Document> mDocument;
  nsCOMPtr<nsITimer> mTimer;
  nsCOMPtr<nsITimer> mWatchDogTimer;
  nsCOMPtr<nsITimer> mWaitingForRemotePrint;
  uint32_t mDelay;
  uint32_t mFiringCount;
  nsPrintObject* mPrintObj;
  uint32_t mWatchDogCount;
  bool mDone;

  static const uint32_t WATCH_DOG_INTERVAL = 1000;
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
