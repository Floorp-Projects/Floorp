/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebSchedulerMainThread_h
#define mozilla_dom_WebSchedulerMainThread_h

#include "WebTaskScheduler.h"
#include "nsCycleCollectionParticipant.h"
#include "nsThreadUtils.h"

#include "mozilla/dom/AbortFollower.h"

namespace mozilla::dom {
class WebTaskMainThreadRunnable final : public Runnable {
 public:
  explicit WebTaskMainThreadRunnable(WebTaskSchedulerMainThread* aScheduler)
      : Runnable("WebTaskMainThreadRunnable"), mScheduler(aScheduler) {}

  MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHOD Run() override;

 private:
  ~WebTaskMainThreadRunnable() = default;
  WeakPtr<WebTaskSchedulerMainThread> mScheduler;
};

class WebTaskSchedulerMainThread final : public WebTaskScheduler {
 public:
  explicit WebTaskSchedulerMainThread(nsIGlobalObject* aParent)
      : WebTaskScheduler(aParent) {}

 private:
  nsresult SetTimeoutForDelayedTask(WebTask* aTask, uint64_t aDelay) override;
  bool DispatchEventLoopRunnable() override;

  ~WebTaskSchedulerMainThread() = default;
};
}  // namespace mozilla::dom
#endif
