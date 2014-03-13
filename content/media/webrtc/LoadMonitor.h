/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LOADMONITOR_H_
#define _LOADMONITOR_H_

#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Atomics.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsIThread.h"
#include "nsIObserver.h"

namespace mozilla {
class LoadInfoUpdateRunner;
class LoadInfoCollectRunner;

class LoadNotificationCallback
{
public:
    virtual void LoadChanged(float aSystemLoad, float aProcessLoad) = 0;
};

class LoadMonitor MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER

    LoadMonitor(int aLoadUpdateInterval);
    ~LoadMonitor();

    nsresult Init(nsRefPtr<LoadMonitor> &self);
    void SetLoadChangeCallback(LoadNotificationCallback* aCallback);
    void Shutdown();
    float GetSystemLoad();
    float GetProcessLoad();

    friend class LoadInfoCollectRunner;

private:

    void SetProcessLoad(float load);
    void SetSystemLoad(float load);
    void FireCallbacks();

    int                  mLoadUpdateInterval;
    mozilla::Mutex       mLock;
    mozilla::CondVar     mCondVar;
    bool                 mShutdownPending;
    nsCOMPtr<nsIThread>  mLoadInfoThread;
    uint64_t             mTicksPerInterval;
    float                mSystemLoad;
    float                mProcessLoad;
    LoadNotificationCallback* mLoadNotificationCallback;
};

} //namespace

#endif /* _LOADMONITOR_H_ */
