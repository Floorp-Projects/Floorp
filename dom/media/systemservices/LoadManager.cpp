/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LoadManager.h"
#include "LoadMonitor.h"
#include "nsString.h"
#include "mozilla/Logging.h"
#include "prtime.h"
#include "prinrval.h"
#include "prsystem.h"

#include "nsString.h"
#include "nsThreadUtils.h"
#include "nsReadableUtils.h"
#include "nsIObserverService.h"
#include "mozilla/Telemetry.h"
#include "mozilla/ArrayUtils.h"

// MOZ_LOG=LoadManager:5
mozilla::LazyLogModule gLoadManagerLog("LoadManager");
#undef LOG
#undef LOG_ENABLED
#define LOG(args) MOZ_LOG(gLoadManagerLog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gLoadManagerLog, mozilla::LogLevel::Verbose)

namespace mozilla {

/* static */ StaticRefPtr<LoadManagerSingleton> LoadManagerSingleton::sSingleton;

NS_IMPL_ISUPPORTS(LoadManagerSingleton, nsIObserver)


LoadManagerSingleton::LoadManagerSingleton(int aLoadMeasurementInterval,
                                           int aAveragingMeasurements,
                                           float aHighLoadThreshold,
                                           float aLowLoadThreshold)
  : mLock("LoadManager"),
    mCurrentState(webrtc::kLoadNormal),
    mOveruseActive(false),
    mLoadSum(0.0f),
    mLoadSumMeasurements(0),
    mLoadMeasurementInterval(aLoadMeasurementInterval),
    mAveragingMeasurements(aAveragingMeasurements),
    mHighLoadThreshold(aHighLoadThreshold),
    mLowLoadThreshold(aLowLoadThreshold)
{
  LOG(("LoadManager - Initializing (%dms x %d, %f, %f)",
       mLoadMeasurementInterval, mAveragingMeasurements,
       mHighLoadThreshold, mLowLoadThreshold));
  MOZ_ASSERT(mHighLoadThreshold > mLowLoadThreshold);
  mLoadMonitor = new LoadMonitor(mLoadMeasurementInterval);
  mLoadMonitor->Init(mLoadMonitor);
  mLoadMonitor->SetLoadChangeCallback(this);

  mLastStateChange = TimeStamp::Now();
  for (auto &in_state : mTimeInState) {
    in_state = 0;
  }
}

LoadManagerSingleton::~LoadManagerSingleton()
{
  LOG(("LoadManager: shutting down LoadMonitor"));
  MOZ_ASSERT(!mLoadMonitor, "why wasn't the LoadMonitor shut down in xpcom-shutdown?");
  if (mLoadMonitor) {
    mLoadMonitor->Shutdown();
  }
}

nsresult
LoadManagerSingleton::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Observer invoked off the main thread");
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (!strcmp(aTopic, "xpcom-shutdown")) {
    obs->RemoveObserver(this, "xpcom-shutdown");
    {
      MutexAutoLock lock(mLock);
      mObservers.Clear();
    }
    if (mLoadMonitor) {
      mLoadMonitor->Shutdown();
      mLoadMonitor = nullptr;
    }

    LOG(("Releasing LoadManager singleton and thread"));
    // Note: won't be released immediately as the Observer has a ref to us
    sSingleton = nullptr;
  }
  return NS_OK;
}

void
LoadManagerSingleton::LoadChanged(float aSystemLoad, float aProcesLoad)
{
  MutexAutoLock lock(mLock);
  // Update total load, and total amount of measured seconds.
  mLoadSum += aSystemLoad;
  mLoadSumMeasurements++;

  if (mLoadSumMeasurements >= mAveragingMeasurements) {
    double averagedLoad = mLoadSum / (float)mLoadSumMeasurements;

    webrtc::CPULoadState newState = mCurrentState;

    if (mOveruseActive || averagedLoad > mHighLoadThreshold) {
      LOG(("LoadManager - LoadStressed"));
      newState = webrtc::kLoadStressed;
    } else if (averagedLoad < mLowLoadThreshold) {
      LOG(("LoadManager - LoadRelaxed"));
      newState = webrtc::kLoadRelaxed;
    } else {
      LOG(("LoadManager - LoadNormal"));
      newState = webrtc::kLoadNormal;
    }

    if (newState != mCurrentState) {
      LoadHasChanged(newState);
    }

    mLoadSum = 0;
    mLoadSumMeasurements = 0;
  }
}

void
LoadManagerSingleton::OveruseDetected()
{
  LOG(("LoadManager - Overuse Detected"));
  MutexAutoLock lock(mLock);
  mOveruseActive = true;
  if (mCurrentState != webrtc::kLoadStressed) {
    LoadHasChanged(webrtc::kLoadStressed);
  }
}

void
LoadManagerSingleton::NormalUsage()
{
  LOG(("LoadManager - Overuse finished"));
  MutexAutoLock lock(mLock);
  mOveruseActive = false;
}

void
LoadManagerSingleton::LoadHasChanged(webrtc::CPULoadState aNewState)
{
  mLock.AssertCurrentThreadOwns();
  LOG(("LoadManager - Signaling LoadHasChanged from %d to %d to %d listeners",
       mCurrentState, aNewState, mObservers.Length()));

  // Record how long we spent in this state for later Telemetry or display
  TimeStamp now = TimeStamp::Now();
  mTimeInState[mCurrentState] += (now - mLastStateChange).ToMilliseconds();
  mLastStateChange = now;

  mCurrentState = aNewState;
  for (size_t i = 0; i < mObservers.Length(); i++) {
    mObservers.ElementAt(i)->onLoadStateChanged(mCurrentState);
  }
}

void
LoadManagerSingleton::AddObserver(webrtc::CPULoadStateObserver * aObserver)
{
  LOG(("LoadManager - Adding Observer"));
  MutexAutoLock lock(mLock);
  mObservers.AppendElement(aObserver);
}

void
LoadManagerSingleton::RemoveObserver(webrtc::CPULoadStateObserver * aObserver)
{
  LOG(("LoadManager - Removing Observer"));
  MutexAutoLock lock(mLock);
  if (!mObservers.RemoveElement(aObserver)) {
    LOG(("LoadManager - Element to remove not found"));
  }
  if (mObservers.Length() == 0) {
    if (mLoadMonitor) {
      // Record how long we spent in the final state for later Telemetry or display
      TimeStamp now = TimeStamp::Now();
      mTimeInState[mCurrentState] += (now - mLastStateChange).ToMilliseconds();

      float total = 0;
      for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mTimeInState); i++) {
        total += mTimeInState[i];
      }
      // Don't include short calls; we don't have reasonable load data, and
      // such short calls rarely reach a stable state.  Keep relatively
      // short calls separate from longer ones
      bool log = total > 5*PR_MSEC_PER_SEC;
      bool small = log && total < 30*PR_MSEC_PER_SEC;
      if (log) {
        // Note: We don't care about rounding here; thus total may be < 100
        Telemetry::Accumulate(small ? Telemetry::WEBRTC_LOAD_STATE_RELAXED_SHORT :
                                      Telemetry::WEBRTC_LOAD_STATE_RELAXED,
                              (uint32_t) (mTimeInState[webrtc::CPULoadState::kLoadRelaxed]/total * 100));
        Telemetry::Accumulate(small ? Telemetry::WEBRTC_LOAD_STATE_NORMAL_SHORT :
                                      Telemetry::WEBRTC_LOAD_STATE_NORMAL,
                              (uint32_t) (mTimeInState[webrtc::CPULoadState::kLoadNormal]/total * 100));
        Telemetry::Accumulate(small ? Telemetry::WEBRTC_LOAD_STATE_STRESSED_SHORT :
                                      Telemetry::WEBRTC_LOAD_STATE_STRESSED,
                              (uint32_t) (mTimeInState[webrtc::CPULoadState::kLoadStressed]/total * 100));
      }
      for (auto &in_state : mTimeInState) {
        in_state = 0;
      }

      // Dance to avoid deadlock on mLock!
      RefPtr<LoadMonitor> loadMonitor = mLoadMonitor.forget();
      MutexAutoUnlock unlock(mLock);

      loadMonitor->Shutdown();
    }
  }
}


}
