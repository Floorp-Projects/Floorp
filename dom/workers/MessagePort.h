/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_messageport_h_
#define mozilla_dom_workers_messageport_h_

#include "Workers.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "nsDOMEventTargetHelper.h"

class nsIDOMEvent;
class nsPIDOMWindow;

BEGIN_WORKERS_NAMESPACE

class SharedWorker;

class MessagePort MOZ_FINAL : public nsDOMEventTargetHelper
{
  friend class SharedWorker;

  typedef mozilla::ErrorResult ErrorResult;

  nsRefPtr<SharedWorker> mSharedWorker;
  nsTArray<nsCOMPtr<nsIDOMEvent>> mQueuedEvents;
  uint64_t mSerial;
  bool mStarted;

public:
  static bool
  PrefEnabled();

  void
  PostMessage(JSContext* aCx, JS::HandleValue aMessage,
              const Optional<Sequence<JS::Value>>& aTransferable,
              ErrorResult& aRv);

  void
  Start();

  void
  Close()
  {
    AssertIsOnMainThread();

    if (!IsClosed()) {
      CloseInternal();
    }
  }

  uint64_t
  Serial() const
  {
    return mSerial;
  }

  void
  QueueEvent(nsIDOMEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MessagePort, nsDOMEventTargetHelper)

  EventHandlerNonNull*
  GetOnmessage()
  {
    AssertIsOnMainThread();

    return GetEventHandler(nsGkAtoms::onmessage, EmptyString());
  }

  void
  SetOnmessage(EventHandlerNonNull* aCallback)
  {
    AssertIsOnMainThread();

    SetEventHandler(nsGkAtoms::onmessage, EmptyString(), aCallback);

    Start();
  }

  bool
  IsClosed() const
  {
    AssertIsOnMainThread();

    return !mSharedWorker;
  }

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::HandleObject aScope) MOZ_OVERRIDE;

  virtual nsresult
  PreHandleEvent(nsEventChainPreVisitor& aVisitor) MOZ_OVERRIDE;

private:
  // This class can only be created by SharedWorker.
  MessagePort(nsPIDOMWindow* aWindow, SharedWorker* aSharedWorker,
              uint64_t aSerial);

  // This class is reference-counted and will be destroyed from Release().
  ~MessagePort();

  void
  CloseInternal();
};

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_messageport_h_
