/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CRITICALSECTION_H_
#define MOZILLA_GFX_CRITICALSECTION_H_

#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#include "mozilla/DebugOnly.h"
#endif

namespace mozilla {
namespace gfx {

#ifdef WIN32

class CriticalSection {
public:
  CriticalSection() { ::InitializeCriticalSection(&mCriticalSection); }

  ~CriticalSection() { ::DeleteCriticalSection(&mCriticalSection); }

  void Enter() { ::EnterCriticalSection(&mCriticalSection); }

  void Leave() { ::LeaveCriticalSection(&mCriticalSection); }

protected:
  CRITICAL_SECTION mCriticalSection;
};

#else
// posix

class PosixCondvar;
class CriticalSection {
public:
  CriticalSection() {
    DebugOnly<int> err = pthread_mutex_init(&mMutex, nullptr);
    MOZ_ASSERT(!err);
  }

  ~CriticalSection() {
    DebugOnly<int> err = pthread_mutex_destroy(&mMutex);
    MOZ_ASSERT(!err);
  }

  void Enter() {
    DebugOnly<int> err = pthread_mutex_lock(&mMutex);
    MOZ_ASSERT(!err);
  }

  void Leave() {
    DebugOnly<int> err = pthread_mutex_unlock(&mMutex);
    MOZ_ASSERT(!err);
  }

protected:
  pthread_mutex_t mMutex;
  friend class PosixCondVar;
};

#endif

/// RAII helper.
struct CriticalSectionAutoEnter {
    explicit CriticalSectionAutoEnter(CriticalSection* aSection) : mSection(aSection) { mSection->Enter(); }
    ~CriticalSectionAutoEnter() { mSection->Leave(); }
protected:
    CriticalSection* mSection;
};


} // namespace
} // namespace

#endif
