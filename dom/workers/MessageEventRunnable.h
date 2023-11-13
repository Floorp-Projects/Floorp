/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_MessageEventRunnable_h
#define mozilla_dom_workers_MessageEventRunnable_h

#include "WorkerCommon.h"
#include "WorkerRunnable.h"
#include "mozilla/dom/StructuredCloneHolder.h"

namespace mozilla {

class DOMEventTargetHelper;

namespace dom {

class MessageEventRunnable final : public WorkerDebuggeeRunnable,
                                   public StructuredCloneHolder {
 public:
  MessageEventRunnable(WorkerPrivate* aWorkerPrivate, Target aTarget);

  bool DispatchDOMEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                        DOMEventTargetHelper* aTarget, bool aIsMainThread);

 private:
  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;

  void DispatchError(JSContext* aCx, DOMEventTargetHelper* aTarget);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_MessageEventRunnable_h
