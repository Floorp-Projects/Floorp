/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_sharedworker_h__
#define mozilla_dom_workers_sharedworker_h__

#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/DOMEventTargetHelper.h"

#ifdef XP_WIN
#  undef PostMessage
#endif

class nsPIDOMWindowInner;

namespace mozilla {
class EventChainPreVisitor;

namespace dom {
class MessagePort;
class StringOrWorkerOptions;
class Event;

class SharedWorkerChild;

/**
 * DOM binding. Holds a SharedWorkerChild. Must exist on the main thread because
 * we only allow top-level windows to create SharedWorkers.
 */
class SharedWorker final : public DOMEventTargetHelper {
  using ErrorResult = mozilla::ErrorResult;
  using GlobalObject = mozilla::dom::GlobalObject;

  RefPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<SharedWorkerChild> mActor;
  RefPtr<MessagePort> mMessagePort;
  nsTArray<RefPtr<Event>> mFrozenEvents;
  bool mFrozen;

 public:
  static already_AddRefed<SharedWorker> Constructor(
      const GlobalObject& aGlobal, const nsAString& aScriptURL,
      const StringOrWorkerOptions& aOptions, ErrorResult& aRv);

  MessagePort* Port();

  bool IsFrozen() const { return mFrozen; }

  void QueueEvent(Event* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SharedWorker, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(error)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  void GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  void ErrorPropagation(nsresult aError);

  // Methods called from the window.

  void Close();

  void Suspend();

  void Resume();

  void Freeze();

  void Thaw();

 private:
  SharedWorker(nsPIDOMWindowInner* aWindow, SharedWorkerChild* aActor,
               MessagePort* aMessagePort);

  // This class is reference-counted and will be destroyed from Release().
  ~SharedWorker();

  // Only called by MessagePort.
  void PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                   const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_workers_sharedworker_h__
