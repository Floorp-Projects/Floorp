/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_SpinLock_h
#define js_SpinLock_h

#include "mozilla/Atomics.h"

#include "ds/LockGuard.h"

namespace js {

// A trivial spin-lock implementation. Extremely fast when rarely-contended.
class SpinLock
{
    mozilla::Atomic<bool, mozilla::ReleaseAcquire> locked_;

  public:
    SpinLock() : locked_(false) {}

    void lock() {
        do {
            while (locked_)
                ; // Spin until the lock seems free.
        } while (!locked_.compareExchange(false, true)); // Atomically take the lock.
    }

    void unlock() {
        locked_ = false;
    }
};

using AutoSpinLock = LockGuard<SpinLock>;

} // namespace js

#endif // js_SpinLock_h
