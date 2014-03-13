/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadManager.h"
#include "LoadMonitor.h"
#include "nsString.h"
#include "prlog.h"
#include "prtime.h"
#include "prinrval.h"
#include "prsystem.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"

// NSPR_LOG_MODULES=LoadManager:5
PRLogModuleInfo *gLoadManagerLog = nullptr;
#undef LOG
#undef LOG_ENABLED
#if defined(PR_LOGGING)
#define LOG(args) PR_LOG(gLoadManagerLog, PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(gLoadManagerLog, 5)
#else
#define LOG(args)
#define LOG_ENABLED() (false)
#endif

namespace mozilla {

LoadManager::LoadManager(int aLoadMeasurementInterval,
                         int aAveragingMeasurements,
                         float aHighLoadThreshold,
                         float aLowLoadThreshold)
  : mLastSystemLoad(0),
    mLoadSum(0.0f),
    mLoadSumMeasurements(0),
    mOveruseActive(false),
    mLoadMeasurementInterval(aLoadMeasurementInterval),
    mAveragingMeasurements(aAveragingMeasurements),
    mHighLoadThreshold(aHighLoadThreshold),
    mLowLoadThreshold(aLowLoadThreshold),
    mCurrentState(webrtc::kLoadNormal)
{
#if defined(PR_LOGGING)
  if (!gLoadManagerLog)
    gLoadManagerLog = PR_NewLogModule("LoadManager");
#endif
  LOG(("LoadManager - Initializing (%dms x %d, %f, %f)",
       mLoadMeasurementInterval, mAveragingMeasurements,
       mHighLoadThreshold, mLowLoadThreshold));
  MOZ_ASSERT(mHighLoadThreshold > mLowLoadThreshold);
  mLoadMonitor = new LoadMonitor(mLoadMeasurementInterval);
  mLoadMonitor->Init(mLoadMonitor);
  mLoadMonitor->SetLoadChangeCallback(this);
}

LoadManager::~LoadManager()
{
  mLoadMonitor->Shutdown();
}

void
LoadManager::LoadChanged(float aSystemLoad, float aProcesLoad)
{
  // Update total load, and total amount of measured seconds.
  mLoadSum += aSystemLoad;
  mLoadSumMeasurements++;

  if (mLoadSumMeasurements >= mAveragingMeasurements) {
    double averagedLoad = mLoadSum / (float)mLoadSumMeasurements;

    webrtc::CPULoadState oldState = mCurrentState;

    if (mOveruseActive || averagedLoad > mHighLoadThreshold) {
      LOG(("LoadManager - LoadStressed"));
      mCurrentState = webrtc::kLoadStressed;
    } else if (averagedLoad < mLowLoadThreshold) {
      LOG(("LoadManager - LoadRelaxed"));
      mCurrentState = webrtc::kLoadRelaxed;
    } else {
      LOG(("LoadManager - LoadNormal"));
      mCurrentState = webrtc::kLoadNormal;
    }

    if (oldState != mCurrentState)
      LoadHasChanged();

    mLoadSum = 0;
    mLoadSumMeasurements = 0;
  }
}

void
LoadManager::OveruseDetected()
{
  LOG(("LoadManager - Overuse Detected"));
  mOveruseActive = true;
  if (mCurrentState != webrtc::kLoadStressed) {
    mCurrentState = webrtc::kLoadStressed;
    LoadHasChanged();
  }
}

void
LoadManager::NormalUsage()
{
  LOG(("LoadManager - Overuse finished"));
  mOveruseActive = false;
}

void
LoadManager::LoadHasChanged()
{
  LOG(("LoadManager - Signaling LoadHasChanged to %d listeners", mObservers.Length()));
  for (size_t i = 0; i < mObservers.Length(); i++) {
    mObservers.ElementAt(i)->onLoadStateChanged(mCurrentState);
  }
}

void
LoadManager::AddObserver(webrtc::CPULoadStateObserver * aObserver)
{
  LOG(("LoadManager - Adding Observer"));
  mObservers.AppendElement(aObserver);
}

void
LoadManager::RemoveObserver(webrtc::CPULoadStateObserver * aObserver)
{
  LOG(("LoadManager - Removing Observer"));
  if (!mObservers.RemoveElement(aObserver)) {
    LOG(("LOadManager - Element to remove not found"));
  }
}


}
