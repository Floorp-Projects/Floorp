/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "threading/ProtectedData.h"

#include "jscntxt.h"

#include "gc/Heap.h"
#include "vm/HelperThreads.h"

namespace js {

#ifdef DEBUG

/* static */ mozilla::Atomic<size_t> AutoNoteSingleThreadedRegion::count(0);

template <AllowedBackgroundThread Background>
static inline bool
OnBackgroundThread()
{
    if (Background == AllowedBackgroundThread::IonCompile || Background == AllowedBackgroundThread::GCTaskOrIonCompile) {
        if (CurrentThreadIsIonCompiling())
            return true;
    }

    if (Background == AllowedBackgroundThread::GCTask || Background == AllowedBackgroundThread::GCTaskOrIonCompile) {
        if (TlsContext.get()->performingGC || TlsContext.get()->runtime()->gc.onBackgroundThread())
            return true;
    }

    return false;
}

template <AllowedBackgroundThread Background>
void
CheckActiveThread<Background>::check() const
{
    // When interrupting a thread on Windows, changes are made to the runtime
    // and active thread's state from another thread while the active thread is
    // suspended. We need a way to mark these accesses as being tantamount to
    // accesses by the active thread. See bug 1323066.
#ifndef XP_WIN
    if (OnBackgroundThread<Background>())
        return;

    JSContext* cx = TlsContext.get();
    MOZ_ASSERT(cx == cx->runtime()->activeContext);
#endif // XP_WIN
}

template class CheckActiveThread<AllowedBackgroundThread::None>;
template class CheckActiveThread<AllowedBackgroundThread::GCTask>;
template class CheckActiveThread<AllowedBackgroundThread::IonCompile>;

template <AllowedBackgroundThread Background>
void
CheckZoneGroup<Background>::check() const
{
    if (OnBackgroundThread<Background>())
        return;

    if (group) {
        // This check is disabled for now because helper thread parse tasks
        // access data in the same zone group that the single main thread is
        // using. This will be fixed soon (bug 1323066).
        //MOZ_ASSERT(group->context && group->context == TlsContext.get());
    } else {
        // |group| will be null for data in the atoms zone. This is protected
        // by the exclusive access lock.
        MOZ_ASSERT(TlsContext.get()->runtime()->currentThreadHasExclusiveAccess());
    }
}

template class CheckZoneGroup<AllowedBackgroundThread::None>;
template class CheckZoneGroup<AllowedBackgroundThread::GCTask>;
template class CheckZoneGroup<AllowedBackgroundThread::IonCompile>;
template class CheckZoneGroup<AllowedBackgroundThread::GCTaskOrIonCompile>;

template <GlobalLock Lock, AllowedBackgroundThread Background>
void
CheckGlobalLock<Lock, Background>::check() const
{
    if (OnBackgroundThread<Background>())
        return;

    switch (Lock) {
      case GlobalLock::GCLock:
        MOZ_ASSERT(TlsContext.get()->runtime()->gc.currentThreadHasLockedGC());
        break;
      case GlobalLock::ExclusiveAccessLock:
        MOZ_ASSERT(TlsContext.get()->runtime()->currentThreadHasExclusiveAccess());
        break;
      case GlobalLock::HelperThreadLock:
        MOZ_ASSERT(HelperThreadState().isLockedByCurrentThread());
        break;
    }
}

template class CheckGlobalLock<GlobalLock::GCLock, AllowedBackgroundThread::None>;
template class CheckGlobalLock<GlobalLock::ExclusiveAccessLock, AllowedBackgroundThread::None>;
template class CheckGlobalLock<GlobalLock::ExclusiveAccessLock, AllowedBackgroundThread::GCTask>;
template class CheckGlobalLock<GlobalLock::HelperThreadLock, AllowedBackgroundThread::None>;

#endif // DEBUG

} // namespace js
