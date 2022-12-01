/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CALLBACKTHREADREGISTRY_H
#define CALLBACKTHREADREGISTRY_H

#include <cstdint>
#include <mozilla/DataMutex.h>
#include <nsTArray.h>
#include <thread>
#include <GeckoProfiler.h>

namespace mozilla {

// This class is a singleton that tracks various callback threads and makes
// sure they are registered or unregistered to the profiler safely and
// consistently.
//
// Register and Unregister are fairly expensive and shouldn't be used in a hot
// path.
class CallbackThreadRegistry final {
 public:
  CallbackThreadRegistry();

  ~CallbackThreadRegistry() {
    // It would be nice to be able to assert that all threads have been
    // unregistered, but we can't: it's legal to suspend an audio stream, so
    // that the callback isn't called, and then immediately destroy it.
  }

  CallbackThreadRegistry(const CallbackThreadRegistry&) = delete;
  CallbackThreadRegistry& operator=(const CallbackThreadRegistry&) = delete;
  CallbackThreadRegistry(CallbackThreadRegistry&&) = delete;
  CallbackThreadRegistry& operator=(CallbackThreadRegistry&&) = delete;

  // Returns the global instance of CallbackThreadRegistry. Safe from all
  // threads.
  static CallbackThreadRegistry* Get();

  // This is intended to be called in the first callback of a callback
  // thread.
  void Register(ProfilerThreadId aThreadId, const char* aName);

  // This is intended to be called when an object stops an audio callback thread
  void Unregister(ProfilerThreadId aThreadId);

 private:
  struct ThreadUserCount {
    ProfilerThreadId mId;  // from profiler_current_thread_id
    int mUserCount = 0;
  };
  DataMutex<nsTArray<ThreadUserCount>> mThreadIds;
};

}  // namespace mozilla

#endif  // CALLBACKTHREADREGISTRY_H
