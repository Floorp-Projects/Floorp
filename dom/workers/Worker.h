/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Worker_h
#define mozilla_dom_Worker_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/DebuggerNotificationBinding.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WeakPtr.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

namespace mozilla::dom {

class EventWithOptionsRunnable;
struct StructuredSerializeOptions;
struct WorkerOptions;
class WorkerPrivate;

class Worker : public DOMEventTargetHelper, public SupportsWeakPtr {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(Worker,
                                                         DOMEventTargetHelper)
  static already_AddRefed<Worker> Constructor(const GlobalObject& aGlobal,
                                              const nsAString& aScriptURL,
                                              const WorkerOptions& aOptions,
                                              ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Maybe<EventCallbackDebuggerNotificationType> GetDebuggerNotificationType()
      const override {
    return Some(EventCallbackDebuggerNotificationType::Worker);
  }

  // True if the worker is not yet closing from the perspective of this, the
  // owning thread, and therefore it's okay to post a message to the worker.
  // This is not a guarantee that the worker will process the message.
  //
  // This method will return false if `globalThis.close()` is invoked on the
  // worker before that method returns control to the caller and without waiting
  // for any task to be queued on this thread and run; this biases us to avoid
  // doing wasteful work but does mean if you are exposing something to content
  // that is specified to only transition as the result of a task, then you
  // should not use this method.
  //
  // The method name comes from
  // https://html.spec.whatwg.org/multipage/web-messaging.html#eligible-for-messaging
  // and is intended to convey whether it's okay to begin to take the steps to
  // create an `EventWithOptionsRunnable` to pass to `PostEventWithOptions`.
  // Note that early returning based on calling this method without performing
  // the structured serialization steps that would otherwise run is potentially
  // observable to content if content is in control of any of the payload in
  // such a way that an object with getters or a proxy could be provided.
  //
  // There is an identically named method on nsIGlobalObject and the semantics
  // are intentionally similar but please make sure you document your
  // assumptions when calling either method.
  bool IsEligibleForMessaging();

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);

  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const StructuredSerializeOptions& aOptions,
                   ErrorResult& aRv);

  // Callers must call `IsEligibleForMessaging` before constructing an
  // `EventWithOptionsRunnable` subclass.
  void PostEventWithOptions(JSContext* aCx, JS::Handle<JS::Value> aOptions,
                            const Sequence<JSObject*>& aTransferable,
                            EventWithOptionsRunnable* aRunnable,
                            ErrorResult& aRv);

  void Terminate();

  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(message)
  IMPL_EVENT_HANDLER(messageerror)

 protected:
  Worker(nsIGlobalObject* aGlobalObject,
         already_AddRefed<WorkerPrivate> aWorkerPrivate);
  ~Worker();

  friend class EventWithOptionsRunnable;
  RefPtr<WorkerPrivate> mWorkerPrivate;
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_Worker_h */
