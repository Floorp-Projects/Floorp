/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_sharedworker_h__
#define mozilla_dom_workers_sharedworker_h__

#include "Workers.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/DOMEventTargetHelper.h"

class nsIDOMEvent;
class nsPIDOMWindowInner;

namespace mozilla {
class EventChainPreVisitor;

namespace dom {
class MessagePort;
}
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class RuntimeService;
class WorkerPrivate;

class SharedWorker final : public DOMEventTargetHelper
{
  friend class RuntimeService;

  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::GlobalObject GlobalObject;

  RefPtr<WorkerPrivate> mWorkerPrivate;
  RefPtr<MessagePort> mMessagePort;
  nsTArray<nsCOMPtr<nsIDOMEvent>> mFrozenEvents;
  bool mFrozen;

public:
  static already_AddRefed<SharedWorker>
  Constructor(const GlobalObject& aGlobal, const nsAString& aScriptURL,
              const Optional<nsAString>& aName, ErrorResult& aRv);

  MessagePort*
  Port();

  bool
  IsFrozen() const
  {
    return mFrozen;
  }

  void
  Freeze();

  void
  Thaw();

  void
  QueueEvent(nsIDOMEvent* aEvent);

  void
  Close();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SharedWorker, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(error)

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual nsresult
  GetEventTargetParent(EventChainPreVisitor& aVisitor) override;

  WorkerPrivate*
  GetWorkerPrivate() const
  {
    return mWorkerPrivate;
  }

private:
  // This class can only be created from the RuntimeService.
  SharedWorker(nsPIDOMWindowInner* aWindow,
               WorkerPrivate* aWorkerPrivate,
               MessagePort* aMessagePort);

  // This class is reference-counted and will be destroyed from Release().
  ~SharedWorker();

  // Only called by MessagePort.
  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Sequence<JSObject*>& aTransferable, ErrorResult& aRv);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_sharedworker_h__
