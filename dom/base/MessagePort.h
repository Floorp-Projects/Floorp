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

class MessagePort MOZ_FINAL : public nsDOMEventTargetHelper
{
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

  IMPL_EVENT_HANDLER(message)

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
  nsRefPtr<MessagePort> mEntangledPort;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessagePort_h
