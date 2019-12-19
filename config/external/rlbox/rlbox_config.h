/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RLBOX_CONFIG
#define RLBOX_CONFIG

#ifdef XP_MACOSX

// RLBox uses c++17's shared_locks by default, even for the noop_sandbox
// However c++17 shared_lock is not supported on macOS 10.9 to 10.11
// Thus we use Firefox's shared lock implementation
// This can be removed if macOS 10.9 to 10.11 support is dropped
#  include "mozilla/RWLock.h"
namespace rlbox {
struct rlbox_shared_lock {
  mozilla::RWLock rwlock;
  rlbox_shared_lock() : rwlock("rlbox") {}
};
}  // namespace rlbox
#  define RLBOX_USE_CUSTOM_SHARED_LOCK
#  define RLBOX_SHARED_LOCK(name) rlbox::rlbox_shared_lock name
#  define RLBOX_ACQUIRE_SHARED_GUARD(name, ...) \
    mozilla::AutoReadLock name((__VA_ARGS__).rwlock)
#  define RLBOX_ACQUIRE_UNIQUE_GUARD(name, ...) \
    mozilla::AutoWriteLock name((__VA_ARGS__).rwlock)

#endif

// All uses are on the main thread right now, disable rlbox thread checks for
// performance
#define RLBOX_SINGLE_THREADED_INVOCATIONS

// The MingW compiler does not correctly handle static thread_local inline
// members. This toggles a workaround that allows the host application (firefox)
// to provide TLS storage via functions. This can be removed if the MingW bug is
// fixed.
#define RLBOX_EMBEDDER_PROVIDES_TLS_STATIC_VARIABLES

#endif
