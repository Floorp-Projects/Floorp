/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_WORKERS_EVENTWITHOPTIONSRUNNABLE_H_
#define MOZILLA_DOM_WORKERS_EVENTWITHOPTIONSRUNNABLE_H_

#include "WorkerCommon.h"
#include "WorkerRunnable.h"
#include "mozilla/dom/StructuredCloneHolder.h"

namespace mozilla {
class DOMEventTargetHelper;

namespace dom {
class Event;
class EventTarget;
class Worker;
class WorkerPrivate;

// Cargo-culted from MesssageEventRunnable.
// Intended to be used for the idiom where arbitrary options are transferred to
// the worker thread (with optional transfer functions), which are then used to
// build an event, which is then fired on the global worker scope.
class EventWithOptionsRunnable : public WorkerDebuggeeRunnable,
                                 public StructuredCloneHolder {
 public:
  explicit EventWithOptionsRunnable(Worker& aWorker);
  void InitOptions(JSContext* aCx, JS::Handle<JS::Value> aOptions,
                   const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  // Called on the worker thread. The event returned will be fired on the
  // worker's global scope. If a StrongWorkerRef needs to be retained, the
  // implementation can do so with the WorkerPrivate.
  virtual already_AddRefed<Event> BuildEvent(
      JSContext* aCx, nsIGlobalObject* aGlobal, EventTarget* aTarget,
      JS::Handle<JS::Value> aOptions) = 0;

  // Called on the worker thread
  virtual void OptionsDeserializeFailed(ErrorResult& aRv) {}

 protected:
  virtual ~EventWithOptionsRunnable();

 private:
  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;
  bool BuildAndFireEvent(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
                         DOMEventTargetHelper* aTarget);
};
}  // namespace dom
}  // namespace mozilla

#endif  // MOZILLA_DOM_WORKERS_EVENTWITHOPTIONSRUNNABLE_H_
