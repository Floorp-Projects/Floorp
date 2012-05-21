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

class nsPrintEngine;

//---------------------------------------------------
//-- Page Timer Class
//---------------------------------------------------
class nsPagePrintTimer : public nsITimerCallback
{
public:

  NS_DECL_ISUPPORTS

  nsPagePrintTimer();
  ~nsPagePrintTimer();

  NS_DECL_NSITIMERCALLBACK

  void Init(nsPrintEngine*          aPrintEngine,
            nsIDocumentViewerPrint* aDocViewerPrint,
            PRUint32                aDelay);

  nsresult Start(nsPrintObject* aPO);

  void Stop();

private:
  nsresult StartTimer(bool aUseDelay);

  nsPrintEngine*             mPrintEngine;
  nsCOMPtr<nsIDocumentViewerPrint> mDocViewerPrint;
  nsCOMPtr<nsITimer>         mTimer;
  PRUint32                   mDelay;
  PRUint32                   mFiringCount;
  nsPrintObject *            mPrintObj;
};


nsresult
NS_NewPagePrintTimer(nsPagePrintTimer **aResult);

#endif /* nsPagePrintTimer_h___ */
