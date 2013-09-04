/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePort_h
#define mozilla_dom_MessagePort_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsDOMEventTargetHelper.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class DispatchEventRunnable;
class PostMessageRunnable;

class MessagePort MOZ_FINAL : public nsDOMEventTargetHelper
{
  friend class DispatchEventRunnable;
  friend class PostMessageRunnable;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_REALLY_FORWARD_NSIDOMEVENTTARGET(nsDOMEventTargetHelper)
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MessagePort,
                                           nsDOMEventTargetHelper)

  MessagePort(nsPIDOMWindow* aWindow);
  ~MessagePort();

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  void
  PostMessage(JSContext* aCx, JS::Handle<JS::Value> aMessage,
              const Optional<JS::Handle<JS::Value> >& aTransfer,
              ErrorResult& aRv);

  void
  Start();

  void
  Close();

  // The 'message' event handler has to call |Start()| method, so we
  // cannot use IMPL_EVENT_HANDLER macro here.
  EventHandlerNonNull*
  GetOnmessage();

  void
  SetOnmessage(EventHandlerNonNull* aCallback, ErrorResult& aRv);

  // Non WebIDL methods

  // This method entangles this MessagePort with another one.
  // If it is already entangled, it's disentangled first and enatangle to the
  // new one.
  void
  Entangle(MessagePort* aMessagePort);

  // Duplicate this message port. This method is used by the Structured Clone
  // Algorithm and makes the new MessagePort active with the entangled
  // MessagePort of this object.
  already_AddRefed<MessagePort>
  Clone(nsPIDOMWindow* aWindow);

private:
  // Dispatch events from the Message Queue using a nsRunnable.
  void Dispatch();

  nsRefPtr<DispatchEventRunnable> mDispatchRunnable;

  nsRefPtr<MessagePort> mEntangledPort;

  nsTArray<nsRefPtr<PostMessageRunnable> > mMessageQueue;
  bool mMessageQueueEnabled;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePort_h
