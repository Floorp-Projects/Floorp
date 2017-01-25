/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef threading_Mutex_h
#define threading_Mutex_h

#include "mozilla/Assertions.h"
#include "mozilla/Move.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Vector.h"

namespace js {

class ConditionVariable;

namespace detail {

class MutexImpl
{
public:
  struct PlatformData;

  MutexImpl();
  ~MutexImpl();

  bool operator==(const MutexImpl& rhs) {
    return platformData_ == rhs.platformData_;
  }

protected:
  void lock();
  void unlock();

private:
  MutexImpl(const MutexImpl&) = delete;
  void operator=(const MutexImpl&) = delete;
  MutexImpl(MutexImpl&&) = delete;
  void operator=(MutexImpl&&) = delete;

  PlatformData* platformData();

// Linux and maybe other platforms define the storage size of pthread_mutex_t in
// bytes. However, we must define it as an array of void pointers to ensure
// proper alignment.
#if defined(__APPLE__) && defined(__MACH__) && defined(__i386__)
  void* platformData_[11];
#elif defined(__APPLE__) && defined(__MACH__) && defined(__amd64__)
  void* platformData_[8];
#elif defined(__linux__)
  void* platformData_[40 / sizeof(void*)];
#elif defined(_WIN32)
  void* platformData_[6];
#else
  void* platformData_[64 / sizeof(void*)];
#endif

  friend class js::ConditionVariable;
};

} // namespace detail

// A MutexId secifies the name and mutex order for a mutex.
//
// The mutex order defines the allowed order of mutex acqusition on a single
// thread. Mutexes must be acquired in strictly increasing order. Mutexes with
// the same order may not be held at the same time by that thread.
struct MutexId
{
  const char* name;
  uint32_t order;
};

#ifndef DEBUG

class Mutex : public detail::MutexImpl
{
public:
  static bool Init() { return true; }
  static void ShutDown() {}

  explicit Mutex(const MutexId& id) {}

  using MutexImpl::lock;
  using MutexImpl::unlock;
};

#else

// In debug builds, js::Mutex is a wrapper over MutexImpl that checks correct
// locking order is observed.
//
// The class maintains a per-thread stack of currently-held mutexes to enable it
// to check this.
class Mutex : public detail::MutexImpl
{
public:
  static bool Init();
  static void ShutDown();

  explicit Mutex(const MutexId& id)
   : id_(id)
  {
    MOZ_ASSERT(id_.order != 0);
  }

  void lock();
  void unlock();

private:
  const MutexId id_;

  using MutexVector = mozilla::Vector<const Mutex*>;
  static MOZ_THREAD_LOCAL(MutexVector*) HeldMutexStack;
  static MutexVector& heldMutexStack();
};

#endif

} // namespace js

#endif // threading_Mutex_h
