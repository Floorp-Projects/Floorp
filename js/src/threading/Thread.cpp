/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "threading/Thread.h"
#include "mozilla/Assertions.h"

namespace js {

Thread::~Thread() { MOZ_RELEASE_ASSERT(!joinable()); }

Thread::Thread(Thread&& aOther) : idMutex_(mutexid::ThreadId) {
  LockGuard<Mutex> lock(aOther.idMutex_);
  id_ = aOther.id_;
  aOther.id_ = Id();
  options_ = aOther.options_;
}

Thread& Thread::operator=(Thread&& aOther) {
  LockGuard<Mutex> lock(idMutex_);
  MOZ_RELEASE_ASSERT(!joinable(lock));
  id_ = aOther.id_;
  aOther.id_ = Id();
  options_ = aOther.options_;
  return *this;
}

Thread::Id Thread::get_id() {
  LockGuard<Mutex> lock(idMutex_);
  return id_;
}

bool Thread::joinable(LockGuard<Mutex>& lock) { return id_ != Id(); }

bool Thread::joinable() {
  LockGuard<Mutex> lock(idMutex_);
  return joinable(lock);
}

}  // namespace js
