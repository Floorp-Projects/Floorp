/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_sharedworker_h__
#define mozilla_dom_workers_sharedworker_h__

#include "Workers.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/workers/bindings/MessagePort.h"
#include "mozilla/DOMEventTargetHelper.h"

class nsIDOMEvent;
class nsPIDOMWindow;

namespace mozilla {
class EventChainPreVisitor;
} // namespace mozilla

BEGIN_WORKERS_NAMESPACE

class MessagePort;
class RuntimeService;
class WorkerPrivate;

class SharedWorker final : public DOMEventTargetHelper
{
  friend class MessagePort;
  friend class RuntimeService;

  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::GlobalObject GlobalObject;

  nsRefPtr<WorkerPrivate> mWorkerPrivate;
  nsRefPtr<MessagePort> mMessagePort;
  nsTArray<nsCOMPtr<nsIDOMEvent>> mFrozenEvents;
  uint64_t mSerial;
  bool mFrozen;

public:
  static already_AddRefed<SharedWorker>
  Constructor(const GlobalObject& aGlobal, JSContext* aCx,
              const nsAString& aScriptURL, const Optional<nsAString>& aName,
              ErrorResult& aRv);

  already_AddRefed<mozilla::dom::workers::MessagePort>
  Port();

  uint64_t
  Serial() const
  {
    return mSerial;
  }

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
  PreHandleEvent(EventChainPreVisitor& aVisitor) override;

  WorkerPrivate*
  GetWorkerPrivate() const
  {
    return mWorkerPrivate;
  }

private:
  // This class can only be created from the RuntimeService.
  SharedWorker(nsPIDOMWindow* aWindow,
               WorkerPrivate* aWorkerPrivate);

  // This class is reference-counted and will be destroyed from Release().
  ~SharedWorker();

  // Only called by MessagePort.
  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv);

  // Only called by RuntimeService.
  void
  NoteDeadWorker(JSContext* aCx);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_sharedworker_h__
