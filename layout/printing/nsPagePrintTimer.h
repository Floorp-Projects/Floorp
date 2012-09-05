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

//---------------------------------------------------
//-- Page Timer Class
//---------------------------------------------------
class nsPagePrintTimer MOZ_FINAL : public nsITimerCallback,
                                   public nsRunnable
{
public:

  NS_DECL_ISUPPORTS

  nsPagePrintTimer();
  ~nsPagePrintTimer();

  NS_DECL_NSITIMERCALLBACK

  void Init(nsPrintEngine*          aPrintEngine,
            nsIDocumentViewerPrint* aDocViewerPrint,
            uint32_t                aDelay);

  nsresult Start(nsPrintObject* aPO);

  NS_IMETHOD Run();

  void Stop();

private:
  nsresult StartTimer(bool aUseDelay);

  nsPrintEngine*             mPrintEngine;
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
  nsCOMPtr<nsITimer>         mTimer;
  uint32_t                   mDelay;
  uint32_t                   mFiringCount;
  nsPrintObject *            mPrintObj;
};


nsresult
NS_NewPagePrintTimer(nsPagePrintTimer **aResult);

#endif /* nsPagePrintTimer_h___ */
