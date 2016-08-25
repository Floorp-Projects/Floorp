/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LOADMANAGER_H_
#define _LOADMANAGER_H_

#include "LoadMonitor.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Services.h"
#include "nsTArray.h"
#include "nsIObserver.h"

#include "webrtc/common_types.h"
#include "webrtc/video_engine/include/vie_base.h"

extern mozilla::LazyLogModule gLoadManagerLog;

namespace mozilla {

class LoadManagerSingleton : public LoadNotificationCallback,
                             public webrtc::CPULoadStateCallbackInvoker,
                             public webrtc::CpuOveruseObserver,
                             public nsIObserver

{
public:
    static LoadManagerSingleton* Get();

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER

    // LoadNotificationCallback interface
    void LoadChanged(float aSystemLoad, float aProcessLoad) override;
    // CpuOveruseObserver interface
    // Called as soon as an overuse is detected.
    void OveruseDetected() override;
    // Called periodically when the system is not overused any longer.
    void NormalUsage() override;
    // CPULoadStateCallbackInvoker interface
    void AddObserver(webrtc::CPULoadStateObserver * aObserver) override;
    void RemoveObserver(webrtc::CPULoadStateObserver * aObserver) override;

private:
    LoadManagerSingleton(int aLoadMeasurementInterval,
                         int aAveragingMeasurements,
                         float aHighLoadThreshold,
                         float aLowLoadThreshold);
    ~LoadManagerSingleton();

    void LoadHasChanged(webrtc::CPULoadState aNewState);

    RefPtr<LoadMonitor> mLoadMonitor;

    // This protects access to the mObservers list, the current state, and
    // pretty much all the other members (below).
    Mutex mLock;
    nsTArray<webrtc::CPULoadStateObserver*> mObservers;
    webrtc::CPULoadState mCurrentState;
    TimeStamp mLastStateChange;
    float mTimeInState[static_cast<int>(webrtc::kLoadLast)];

    // Set when overuse was signaled to us, and hasn't been un-signaled yet.
    bool  mOveruseActive;
    float mLoadSum;
    int   mLoadSumMeasurements;
    // Load measurement settings
    int mLoadMeasurementInterval;
    int mAveragingMeasurements;
    float mHighLoadThreshold;
    float mLowLoadThreshold;

    static StaticRefPtr<LoadManagerSingleton> sSingleton;
};

class LoadManager final : public webrtc::CPULoadStateCallbackInvoker,
                          public webrtc::CpuOveruseObserver
{
public:
    explicit LoadManager(LoadManagerSingleton* aManager)
        : mManager(aManager)
    {}
    ~LoadManager() {}

    void AddObserver(webrtc::CPULoadStateObserver * aObserver) override
    {
        mManager->AddObserver(aObserver);
    }
    void RemoveObserver(webrtc::CPULoadStateObserver * aObserver) override
    {
        mManager->RemoveObserver(aObserver);
    }
    void OveruseDetected() override
    {
        mManager->OveruseDetected();
    }
    void NormalUsage() override
    {
        mManager->NormalUsage();
    }

private:
    RefPtr<LoadManagerSingleton> mManager;
};

} //namespace

#endif /* _LOADMANAGER_H_ */
