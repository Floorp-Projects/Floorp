/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessagePort_h
#define mozilla_dom_MessagePort_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {

class DispatchEventRunnable;
class PostMessageRunnable;

class MessagePortBase : public DOMEventTargetHelper
{
protected:
  MessagePortBase(nsPIDOMWindow* aWindow);
  MessagePortBase();

public:

  virtual void
  PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                const Optional<Sequence<JS::Value>>& aTransferable,
                ErrorResult& aRv) = 0;

  virtual void
  Start() = 0;

  virtual void
  Close() = 0;

  // The 'message' event handler has to call |Start()| method, so we
  // cannot use IMPL_EVENT_HANDLER macro here.
  virtual EventHandlerNonNull*
  GetOnmessage() = 0;

  virtual void
  SetOnmessage(EventHandlerNonNull* aCallback) = 0;

  // Duplicate this message port. This method is used by the Structured Clone
  // Algorithm and makes the new MessagePort active with the entangled
  // MessagePort of this object.
  virtual already_AddRefed<MessagePortBase>
  Clone() = 0;
};

class MessagePort MOZ_FINAL : public MessagePortBase
{
  friend class DispatchEventRunnable;
  friend class PostMessageRunnable;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MessagePort,
                                           DOMEventTargetHelper)

  MessagePort(nsPIDOMWindow* aWindow);
  ~MessagePort();

  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual void
  PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                 const Optional<Sequence<JS::Value>>& aTransferable,
                 ErrorResult& aRv) MOZ_OVERRIDE;

  virtual void
  Start() MOZ_OVERRIDE;

  virtual void
  Close() MOZ_OVERRIDE;

  virtual EventHandlerNonNull*
  GetOnmessage() MOZ_OVERRIDE;

  virtual void
  SetOnmessage(EventHandlerNonNull* aCallback) MOZ_OVERRIDE;

  // Non WebIDL methods

  // This method entangles this MessagePort with another one.
  // If it is already entangled, it's disentangled first and enatangle to the
  // new one.
  void
  Entangle(MessagePort* aMessagePort);

  virtual already_AddRefed<MessagePortBase>
  Clone() MOZ_OVERRIDE;

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
