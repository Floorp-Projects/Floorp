/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPrintData.h"

#include "nsIStringBundle.h"
#include "nsIServiceManager.h"
#include "nsPrintObject.h"
#include "nsPrintPreviewListener.h"
#include "nsIWebProgressListener.h"
#include "mozilla/Services.h"

//-----------------------------------------------------
// PR LOGGING
#include "mozilla/Logging.h"

#define DUMP_LAYOUT_LEVEL 9 // this turns on the dumping of each doucment's layout info
static mozilla::LazyLogModule gPrintingLog("printing");

#define PR_PL(_p1)  MOZ_LOG(gPrintingLog, mozilla::LogLevel::Debug, _p1);

//---------------------------------------------------
//-- nsPrintData Class Impl
//---------------------------------------------------
nsPrintData::nsPrintData(ePrintDataType aType) :
  mType(aType), mDebugFilePtr(nullptr), mPrintObject(nullptr), mSelectedPO(nullptr),
  mPrintDocList(0), mIsIFrameSelected(false),
  mIsParentAFrameSet(false), mOnStartSent(false),
  mIsAborted(false), mPreparingForPrint(false), mDocWasToBeDestroyed(false),
  mShrinkToFit(false), mPrintFrameType(nsIPrintSettings::kFramesAsIs), 
  mNumPrintablePages(0), mNumPagesPrinted(0),
  mShrinkRatio(1.0), mOrigDCScale(1.0), mPPEventListeners(nullptr), 
  mBrandName(nullptr)
{
  MOZ_COUNT_CTOR(nsPrintData);
  nsCOMPtr<nsIStringBundle> brandBundle;
  nsCOMPtr<nsIStringBundleService> svc =
    mozilla::services::GetStringBundleService();
  if (svc) {
    svc->CreateBundle( "chrome://branding/locale/brand.properties", getter_AddRefs( brandBundle ) );
    if (brandBundle) {
      brandBundle->GetStringFromName(u"brandShortName", &mBrandName );
    }
  }

  if (!mBrandName) {
    mBrandName = ToNewUnicode(NS_LITERAL_STRING("Mozilla Document"));
  }

}

nsPrintData::~nsPrintData()
{
  MOZ_COUNT_DTOR(nsPrintData);
  // remove the event listeners
  if (mPPEventListeners) {
    mPPEventListeners->RemoveListeners();
    NS_RELEASE(mPPEventListeners);
  }

  // Only Send an OnEndPrinting if we have started printing
  if (mOnStartSent && mType != eIsPrintPreview) {
    OnEndPrinting();
  }

  if (mPrintDC && !mDebugFilePtr) {
    PR_PL(("****************** End Document ************************\n"));
    PR_PL(("\n"));
    bool isCancelled = false;
    mPrintSettings->GetIsCancelled(&isCancelled);

    nsresult rv = NS_OK;
    if (mType == eIsPrinting) {
      if (!isCancelled && !mIsAborted) {
        rv = mPrintDC->EndDocument();
      } else {
        rv = mPrintDC->AbortDocument();  
      }
      if (NS_FAILED(rv)) {
        // XXX nsPrintData::ShowPrintErrorDialog(rv);
      }
    }
  }

  delete mPrintObject;

  if (mBrandName) {
    free(mBrandName);
  }
}

void nsPrintData::OnStartPrinting()
{
  if (!mOnStartSent) {
    DoOnProgressChange(0, 0, true, nsIWebProgressListener::STATE_START|nsIWebProgressListener::STATE_IS_DOCUMENT|nsIWebProgressListener::STATE_IS_NETWORK);
    mOnStartSent = true;
  }
}

void nsPrintData::OnEndPrinting()
{
  DoOnProgressChange(100, 100, true, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_DOCUMENT);
  DoOnProgressChange(100, 100, true, nsIWebProgressListener::STATE_STOP|nsIWebProgressListener::STATE_IS_NETWORK);
}

void
nsPrintData::DoOnProgressChange(int32_t      aProgress,
                                int32_t      aMaxProgress,
                                bool         aDoStartStop,
                                int32_t      aFlag)
{
  for (int32_t i=0;i<mPrintProgressListeners.Count();i++) {
    nsIWebProgressListener* wpl = mPrintProgressListeners.ObjectAt(i);
    wpl->OnProgressChange(nullptr, nullptr, aProgress, aMaxProgress, aProgress, aMaxProgress);
    if (aDoStartStop) {
      wpl->OnStateChange(nullptr, nullptr, aFlag, NS_OK);
    }
  }
}

void
nsPrintData::DoOnStatusChange(nsresult aStatus)
{
  uint32_t numberOfListeners = mPrintProgressListeners.Length();
  for (uint32_t i = 0; i < numberOfListeners; ++i) {
    nsIWebProgressListener* listener = mPrintProgressListeners.SafeElementAt(i);
    if (listener) {
      listener->OnStatusChange(nullptr, nullptr, aStatus, nullptr);
    }
  }
}

