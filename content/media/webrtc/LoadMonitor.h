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

class LoadInfoUpdateRunner;
class LoadInfoCollectRunner;

class LoadMonitor MOZ_FINAL : public nsIObserver
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIOBSERVER

    LoadMonitor();
    ~LoadMonitor();

    nsresult Init(nsRefPtr<LoadMonitor> &self);
    void Shutdown();
    float GetSystemLoad();
    float GetProcessLoad();

    friend class LoadInfoCollectRunner;

private:

    void SetProcessLoad(float load);
    void SetSystemLoad(float load);

    mozilla::Mutex       mLock;
    mozilla::CondVar     mCondVar;
    bool                 mShutdownPending;
    nsCOMPtr<nsIThread>  mLoadInfoThread;
    float                mSystemLoad;
    float                mProcessLoad;
};

#endif /* _LOADMONITOR_H_ */
