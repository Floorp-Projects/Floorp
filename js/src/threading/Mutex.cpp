/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "threading/Mutex.h"

#include "jsutil.h"

using namespace js;

#ifdef DEBUG

MOZ_THREAD_LOCAL(js::Mutex*) js::Mutex::HeldMutexStack;

/* static */
bool js::Mutex::Init() { return HeldMutexStack.init(); }

/* static */
void js::Mutex::ShutDown() { MOZ_ASSERT(HeldMutexStack.get() == nullptr); }

void js::Mutex::lock() {
  preLockChecks();
  impl_.lock();
  postLockChecks();
}

void js::Mutex::preLockChecks() const {
  Mutex* prev = HeldMutexStack.get();
  if (prev) {
    if (id_.order <= prev->id_.order) {
      fprintf(stderr,
              "Attempt to acquire mutex %s with order %d while holding %s with "
              "order %d\n",
              id_.name, id_.order, prev->id_.name, prev->id_.order);
      MOZ_CRASH("Mutex ordering violation");
    }
  }
}

void js::Mutex::postLockChecks() {
  MOZ_ASSERT(prev_ == nullptr);
  prev_ = HeldMutexStack.get();
  HeldMutexStack.set(this);
}

void js::Mutex::unlock() {
  preUnlockChecks();
  impl_.unlock();
}

void js::Mutex::preUnlockChecks() {
  Mutex* stack = HeldMutexStack.get();
  MOZ_ASSERT(stack == this);
  HeldMutexStack.set(prev_);
  prev_ = nullptr;
}

bool js::Mutex::ownedByCurrentThread() const {
  Mutex* stack = HeldMutexStack.get();

  while (stack) {
    if (stack == this) {
      return true;
    }

    stack = stack->prev_;
  }

  return false;
}

#endif
