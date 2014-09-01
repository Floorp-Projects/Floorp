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

extern PRLogModuleInfo *gLoadManagerLog;

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
    virtual void LoadChanged(float aSystemLoad, float aProcessLoad) MOZ_OVERRIDE;
    // CpuOveruseObserver interface
    // Called as soon as an overuse is detected.
    virtual void OveruseDetected() MOZ_OVERRIDE;
    // Called periodically when the system is not overused any longer.
    virtual void NormalUsage() MOZ_OVERRIDE;
    // CPULoadStateCallbackInvoker interface
    virtual void AddObserver(webrtc::CPULoadStateObserver * aObserver) MOZ_OVERRIDE;
    virtual void RemoveObserver(webrtc::CPULoadStateObserver * aObserver) MOZ_OVERRIDE;

private:
    LoadManagerSingleton(int aLoadMeasurementInterval,
                         int aAveragingMeasurements,
                         float aHighLoadThreshold,
                         float aLowLoadThreshold);
    ~LoadManagerSingleton();

    void LoadHasChanged();

    nsRefPtr<LoadMonitor> mLoadMonitor;

    // This protects access to the mObservers list, the current state, and
    // pretty much all the other members (below).
    Mutex mLock;
    nsTArray<webrtc::CPULoadStateObserver*> mObservers;
    webrtc::CPULoadState mCurrentState;
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

class LoadManager MOZ_FINAL : public webrtc::CPULoadStateCallbackInvoker,
                              public webrtc::CpuOveruseObserver
{
public:
    explicit LoadManager(LoadManagerSingleton* aManager)
        : mManager(aManager)
    {}
    ~LoadManager() {}

    void AddObserver(webrtc::CPULoadStateObserver * aObserver) MOZ_OVERRIDE
    {
        mManager->AddObserver(aObserver);
    }
    void RemoveObserver(webrtc::CPULoadStateObserver * aObserver) MOZ_OVERRIDE
    {
        mManager->RemoveObserver(aObserver);
    }
    void OveruseDetected() MOZ_OVERRIDE
    {
        mManager->OveruseDetected();
    }
    void NormalUsage() MOZ_OVERRIDE
    {
        mManager->NormalUsage();
    }

private:
    LoadManagerSingleton* mManager;
};

} //namespace

#endif /* _LOADMANAGER_H_ */
