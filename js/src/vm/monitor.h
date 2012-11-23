/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsmonitor_h___
#define jsmonitor_h___

#include <stdlib.h>
#include "mozilla/Util.h"
#include "js/Utility.h"
#include "prlock.h"
#include "prcvar.h"

namespace js {

/*
 * A base class used for types intended to be used in a parallel
 * fashion, such as the workers in the |ThreadPool| class.  Combines a
 * lock and a condition variable.  You can acquire the lock or signal
 * the condition variable using the |AutoLockMonitor| type.
 */
class Monitor
{
protected:
    friend class AutoLockMonitor;
    friend class AutoUnlockMonitor;

    PRLock *lock_;
    PRCondVar *condVar_;

public:
    Monitor();
    ~Monitor();

    bool init();
};

class AutoLockMonitor
{
private:
    Monitor &monitor;

public:
    AutoLockMonitor(Monitor &monitor) : monitor(monitor) {
        PR_Lock(monitor.lock_);
    }

    ~AutoLockMonitor() {
        PR_Unlock(monitor.lock_);
    }

    void wait() {
        mozilla::DebugOnly<PRStatus> status =
          PR_WaitCondVar(monitor.condVar_, PR_INTERVAL_NO_TIMEOUT);
        JS_ASSERT(status == PR_SUCCESS);
    }

    void notify() {
        PR_NotifyCondVar(monitor.condVar_);
    }

    void notifyAll() {
        PR_NotifyAllCondVar(monitor.condVar_);
    }
};

class AutoUnlockMonitor
{
  private:
    Monitor &monitor;

  public:
    AutoUnlockMonitor(Monitor &monitor) : monitor(monitor) { PR_Unlock(monitor.lock_); }
    ~AutoUnlockMonitor() { PR_Lock(monitor.lock_); }
};

}

#endif /* ndef jsmonitor_h___ */
