/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Mutex.h"

namespace js {

/* static */ mozilla::Maybe<detail::MutexBase>
detail::MutexBase::Create()
{
    auto lock = PR_NewLock();
    if (!lock)
        return mozilla::Nothing();

    return mozilla::Some(detail::MutexBase(lock));
}

detail::MutexBase::~MutexBase()
{
    if (lock_)
        PR_DestroyLock(lock_);
}

void
detail::MutexBase::acquire() const
{
    PR_Lock(lock_);
}

void
detail::MutexBase::release() const
{
    MOZ_RELEASE_ASSERT(PR_Unlock(lock_) == PR_SUCCESS);
}

} // namespace js
