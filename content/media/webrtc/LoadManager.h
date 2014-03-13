/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LOADMANAGER_H_
#define _LOADMANAGER_H_

#include "LoadMonitor.h"
#include "webrtc/common_types.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "mozilla/TimeStamp.h"
#include "nsTArray.h"

extern PRLogModuleInfo *gLoadManagerLog;

namespace mozilla {

class LoadManager : public LoadNotificationCallback,
                    public webrtc::CPULoadStateCallbackInvoker,
                    public webrtc::CpuOveruseObserver
{
public:
    LoadManager(int aLoadMeasurementInterval,
                int aAveragingMeasurements,
                float aHighLoadThreshold,
                float aLowLoadThreshold);
    ~LoadManager();

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
    void LoadHasChanged();

    nsRefPtr<LoadMonitor> mLoadMonitor;
    float mLastSystemLoad;
    float mLoadSum;
    int   mLoadSumMeasurements;
    // Set when overuse was signaled to us, and hasn't been un-signaled yet.
    bool  mOveruseActive;
    // Load measurement settings
    int mLoadMeasurementInterval;
    int mAveragingMeasurements;
    float mHighLoadThreshold;
    float mLowLoadThreshold;

    webrtc::CPULoadState mCurrentState;

    nsTArray<webrtc::CPULoadStateObserver*> mObservers;
};

} //namespace

#endif /* _LOADMANAGER_H_ */
