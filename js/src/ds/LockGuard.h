/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_LockGuard_h
#define js_LockGuard_h

#include "mozilla/GuardObjects.h"

namespace js {

// An implementation of C++11's std::lock_guard, enhanced with a guard object
// to help with correct usage.
template <typename LockType>
class LockGuard
{
    LockType& lockRef_;
    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER;

  public:
    explicit LockGuard(LockType& lock
                       MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
      : lockRef_(lock)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        lockRef_.lock();
    }

    ~LockGuard() {
        lockRef_.unlock();
    }
};

} // namespace js

#endif // js_LockGuard_h
