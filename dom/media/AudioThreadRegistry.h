/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AUDIOTHREADREGISTRY_H
#define AUDIOTHREADREGISTRY_H

#include <cstdint>
#include <mozilla/DataMutex.h>
#include <nsTArray.h>
#include <thread>
#include <GeckoProfiler.h>

namespace mozilla {

// This class is a singleton that tracks all audio callback threads and makes
// sure they are registered or unregistered to the profiler safely and
// consistently.
//
// Register and Unregister are fairly expensive and shouldn't be used in a hot
// path.
class AudioThreadRegistry final {
 public:
  AudioThreadRegistry()
#ifdef MOZ_GECKO_PROFILER
      : mThreadIds("AudioThreadId")
#endif  // MOZ_GECKO_PROFILER
  {
  }

#ifdef MOZ_GECKO_PROFILER
  ~AudioThreadRegistry() {
    // It would be nice to be able to assert that all threads have be
    // unregistered, but we can't: it's legal to suspend an audio stream, so
    // that the callback isn't called, and then immediately destroy it.
  }
#endif  // MOZ_GECKO_PROFILER

  // This is intended to be called when an object starts an audio callback
  // thread.
  void Register(int aThreadId) {
#ifdef MOZ_GECKO_PROFILER
    auto threadIds = mThreadIds.Lock();
    for (uint32_t i = 0; i < threadIds->Length(); i++) {
      if ((*threadIds)[i].mId == aThreadId) {
        (*threadIds)[i].mUserCount++;
        return;
      }
    }
    ThreadUserCount tuc;
    tuc.mId = aThreadId;
    tuc.mUserCount = 1;
    threadIds->AppendElement(tuc);
    PROFILER_REGISTER_THREAD("NativeAudioCallback");
#endif  // MOZ_GECKO_PROFILER
  }

  // This is intended to be called when an object stops an audio callback thread
  void Unregister(int aThreadId) {
#ifdef MOZ_GECKO_PROFILER
    auto threadIds = mThreadIds.Lock();
    for (uint32_t i = 0; i < threadIds->Length(); i++) {
      if ((*threadIds)[i].mId == aThreadId) {
        MOZ_ASSERT((*threadIds)[i].mUserCount > 0);
        (*threadIds)[i].mUserCount--;

        if ((*threadIds)[i].mUserCount == 0) {
          PROFILER_UNREGISTER_THREAD();
          threadIds->RemoveElementAt(i);
        }
        return;
      }
    }
    MOZ_ASSERT(false);
#endif  // MOZ_GECKO_PROFILER
  }

 private:
  AudioThreadRegistry(const AudioThreadRegistry&) = delete;
  AudioThreadRegistry& operator=(const AudioThreadRegistry&) = delete;
  AudioThreadRegistry(AudioThreadRegistry&&) = delete;
  AudioThreadRegistry& operator=(AudioThreadRegistry&&) = delete;

#ifdef MOZ_GECKO_PROFILER
  struct ThreadUserCount {
    int mId;  // from profiler_current_thread_id
    int mUserCount;
  };
  DataMutex<nsTArray<ThreadUserCount>> mThreadIds;
#endif  // MOZ_GECKO_PROFILER
};

}  // namespace mozilla

#endif  // AUDIOTHREADREGISTRY_H
