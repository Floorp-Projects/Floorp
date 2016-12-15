/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseDebugging_h
#define mozilla_dom_PromiseDebugging_h

#include "js/TypeDecls.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class ErrorResult;

namespace dom {
namespace workers {
class WorkerPrivate;
} // namespace workers

class Promise;
struct PromiseDebuggingStateHolder;
class GlobalObject;
class UncaughtRejectionObserver;
class FlushRejections;

void TriggerFlushRejections();

class PromiseDebugging
{
public:
  static void Init();
  static void Shutdown();

  static void GetState(GlobalObject&, JS::Handle<JSObject*> aPromise,
                       PromiseDebuggingStateHolder& aState,
                       ErrorResult& aRv);

  static void GetPromiseID(GlobalObject&, JS::Handle<JSObject*>, nsString&,
                           ErrorResult&);

  static void GetAllocationStack(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                 JS::MutableHandle<JSObject*> aStack,
                                 ErrorResult& aRv);
  static void GetRejectionStack(GlobalObject&, JS::Handle<JSObject*> aPromise,
                                JS::MutableHandle<JSObject*> aStack,
                                ErrorResult& aRv);
  static void GetFullfillmentStack(GlobalObject&,
                                   JS::Handle<JSObject*> aPromise,
                                   JS::MutableHandle<JSObject*> aStack,
                                   ErrorResult& aRv);

  // Mechanism for watching uncaught instances of Promise.
  static void AddUncaughtRejectionObserver(GlobalObject&,
                                           UncaughtRejectionObserver& aObserver);
  static bool RemoveUncaughtRejectionObserver(GlobalObject&,
                                              UncaughtRejectionObserver& aObserver);

  // Mark a Promise as having been left uncaught at script completion.
  static void AddUncaughtRejection(JS::HandleObject);
  // Mark a Promise previously added with `AddUncaughtRejection` as
  // eventually consumed.
  static void AddConsumedRejection(JS::HandleObject);
  // Propagate the informations from AddUncaughtRejection
  // and AddConsumedRejection to observers.
  static void FlushUncaughtRejections();

protected:
  static void FlushUncaughtRejectionsInternal();
  friend class FlushRejections;
  friend class mozilla::dom::workers::WorkerPrivate;
private:
  // Identity of the process.
  // This property is:
  // - set during initialization of the layout module,
  // prior to any Worker using it;
  // - read by both the main thread and the Workers;
  // - unset during shutdown of the layout module,
  // after any Worker has been shutdown.
  static nsString sIDPrefix;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseDebugging_h
