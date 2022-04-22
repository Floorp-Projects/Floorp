/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TaskSignal_h
#define mozilla_dom_TaskSignal_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/AbortSignal.h"
#include "mozilla/dom/WebTaskSchedulingBinding.h"
#include "WebTaskScheduler.h"

namespace mozilla::dom {
class TaskSignal : public AbortSignal {
 public:
  TaskSignal(nsIGlobalObject* aGlobal, TaskPriority aPriority)
      : AbortSignal(aGlobal, false, JS::UndefinedHandleValue),
        mPriority(aPriority),
        mPriorityChanging(false) {}

  IMPL_EVENT_HANDLER(prioritychange);

  TaskPriority Priority() const { return mPriority; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override {
    return TaskSignal_Binding::Wrap(aCx, this, aGivenProto);
  }

  void SetPriority(TaskPriority aPriority) { mPriority = aPriority; }

  bool IsTaskSignal() const override { return true; }

  bool PriorityChanging() const { return mPriorityChanging; }

  void SetPriorityChanging(bool aPriorityChanging) {
    mPriorityChanging = aPriorityChanging;
  }

  void SetWebTaskScheduler(WebTaskScheduler* aScheduler) {
    mSchedulers.AppendElement(aScheduler);
  }

  void RunPriorityChangeAlgorithms() {
    for (const WeakPtr<WebTaskScheduler>& scheduler : mSchedulers) {
      scheduler->RunTaskSignalPriorityChange(this);
    }
  }

 private:
  TaskPriority mPriority;

  bool mPriorityChanging;

  nsTArray<WeakPtr<WebTaskScheduler>> mSchedulers;

  ~TaskSignal() = default;
};
}  // namespace mozilla::dom
#endif
