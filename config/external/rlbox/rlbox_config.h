/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RLBOX_CONFIG
#define RLBOX_CONFIG

#include "mozilla/Assertions.h"

#ifdef XP_MACOSX

// RLBox uses c++17's shared_locks by default, even for the noop_sandbox
// However c++17 shared_lock is not supported on macOS 10.9 to 10.11
// Thus we use Firefox's shared lock implementation
// This can be removed if macOS 10.9 to 10.11 support is dropped
#  include "mozilla/RWLock.h"
namespace rlbox {
struct rlbox_shared_lock {
  mozilla::detail::StaticRWLock rwlock;
};
}  // namespace rlbox
#  define RLBOX_USE_CUSTOM_SHARED_LOCK
#  define RLBOX_SHARED_LOCK(name) rlbox::rlbox_shared_lock name
#  define RLBOX_ACQUIRE_SHARED_GUARD(name, ...) \
    mozilla::detail::StaticAutoReadLock name((__VA_ARGS__).rwlock)
#  define RLBOX_ACQUIRE_UNIQUE_GUARD(name, ...) \
    mozilla::detail::StaticAutoWriteLock name((__VA_ARGS__).rwlock)

#endif

// All uses of rlbox's function and callbacks invocations are on a single
// thread right now, so we disable rlbox thread checks for performance
// See (Bug 1739298) for more details
#define RLBOX_SINGLE_THREADED_INVOCATIONS

#define RLBOX_CUSTOM_ABORT(msg) MOZ_CRASH_UNSAFE_PRINTF("RLBox crash: %s", msg)

// The MingW compiler does not correctly handle static thread_local inline
// members. This toggles a workaround that allows the host application (firefox)
// to provide TLS storage via functions. This can be removed if the MingW bug is
// fixed.
#define RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES

#endif
