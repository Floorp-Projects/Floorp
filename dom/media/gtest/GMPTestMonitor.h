/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __GMPTestMonitor_h__
#define __GMPTestMonitor_h__

#include "nsThreadUtils.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/SpinEventLoopUntil.h"

class GMPTestMonitor {
 public:
  GMPTestMonitor() : mFinished(false) {}

  void AwaitFinished() {
    MOZ_ASSERT(NS_IsMainThread());
    mozilla::SpinEventLoopUntil("GMPTestMonitor::AwaitFinished"_ns,
                                [&]() { return mFinished; });
    mFinished = false;
  }

 private:
  void MarkFinished() {
    MOZ_ASSERT(NS_IsMainThread());
    mFinished = true;
  }

 public:
  void SetFinished() {
    mozilla::SchedulerGroup::Dispatch(mozilla::TaskCategory::Other,
                                      mozilla::NewNonOwningRunnableMethod(
                                          "GMPTestMonitor::MarkFinished", this,
                                          &GMPTestMonitor::MarkFinished));
  }

 private:
  bool mFinished;
};

#endif  // __GMPTestMonitor_h__
