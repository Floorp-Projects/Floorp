/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_sharedworker_h__
#define mozilla_dom_workers_sharedworker_h__

#include "Workers.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "nsDOMEventTargetHelper.h"

class nsIDOMEvent;
class nsPIDOMWindow;

BEGIN_WORKERS_NAMESPACE

class MessagePort;
class RuntimeService;
class WorkerPrivate;

class SharedWorker MOZ_FINAL : public nsDOMEventTargetHelper
{
  friend class MessagePort;
  friend class RuntimeService;

  typedef mozilla::ErrorResult ErrorResult;
  typedef mozilla::dom::GlobalObject GlobalObject;

  WorkerPrivate* mWorkerPrivate;
  nsRefPtr<MessagePort> mMessagePort;
  nsTArray<nsCOMPtr<nsIDOMEvent>> mSuspendedEvents;
  uint64_t mSerial;
  bool mSuspended;

public:
  static bool
  PrefEnabled();

  static already_AddRefed<SharedWorker>
  Constructor(const GlobalObject& aGlobal, JSContext* aCx,
              const nsAString& aScriptURL, const Optional<nsAString>& aName,
              ErrorResult& aRv);

  already_AddRefed<MessagePort>
  Port();

  uint64_t
  Serial() const
  {
    return mSerial;
  }

  bool
  IsSuspended() const
  {
    return mSuspended;
  }

  void
  Suspend();

  void
  Resume();

  void
  QueueEvent(nsIDOMEvent* aEvent);

  void
  Close();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SharedWorker, nsDOMEventTargetHelper)

  IMPL_EVENT_HANDLER(error)

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::HandleObject aScope) MOZ_OVERRIDE;

  virtual nsresult
  PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

private:
  // This class can only be created from the RuntimeService.
  SharedWorker(nsPIDOMWindow* aWindow, WorkerPrivate* aWorkerPrivate);

  // This class is reference-counted and will be destroyed from Release().
  ~SharedWorker();

  // Only called by MessagePort.
  void
  PostMessage(JSContext* aCx, JS::HandleValue aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv);

  // Only called by RuntimeService.
  void
  NoteDeadWorker(JSContext* aCx);
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_sharedworker_h__
