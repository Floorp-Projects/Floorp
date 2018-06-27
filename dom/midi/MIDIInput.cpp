/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIInput.h"
#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDIInputBinding.h"
#include "mozilla/dom/MIDIMessageEvent.h"
#include "mozilla/dom/MIDIMessageEventBinding.h"
#include "nsDOMNavigationTiming.h"

namespace mozilla {
namespace dom {

MIDIInput::MIDIInput(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent)
  : MIDIPort(aWindow, aMIDIAccessParent)
{
}

//static
MIDIInput*
MIDIInput::Create(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent,
                  const MIDIPortInfo& aPortInfo, const bool aSysexEnabled)
{
  MOZ_ASSERT(static_cast<MIDIPortType>(aPortInfo.type()) == MIDIPortType::Input);
  auto port = new MIDIInput(aWindow, aMIDIAccessParent);
  if (!port->Initialize(aPortInfo, aSysexEnabled)) {
    return nullptr;
  }
  return port;
}

JSObject*
MIDIInput::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return MIDIInput_Binding::Wrap(aCx, this, aGivenProto);
}

void
MIDIInput::Receive(const nsTArray<MIDIMessage>& aMsgs)
{
  nsCOMPtr<nsIDocument> doc = GetOwner() ? GetOwner()->GetDoc() : nullptr;
  if (!doc) {
    NS_WARNING("No document available to send MIDIMessageEvent to!");
    return;
  }
  for (auto& msg:aMsgs) {
    RefPtr<MIDIMessageEvent> event(
      MIDIMessageEvent::Constructor(this, msg.timestamp(), msg.data()));
    DispatchTrustedEvent(event);
  }
}

EventHandlerNonNull*
MIDIInput::GetOnmidimessage()
{
  if (NS_IsMainThread()) {
    return GetEventHandler(nsGkAtoms::onmidimessage, EmptyString());
  }
  return GetEventHandler(nullptr, NS_LITERAL_STRING("midimessage"));
}

void
MIDIInput::SetOnmidimessage(EventHandlerNonNull* aCallback)
{
  if (NS_IsMainThread()) {
    SetEventHandler(nsGkAtoms::onmidimessage, EmptyString(), aCallback);
  } else {
    SetEventHandler(nullptr, NS_LITERAL_STRING("midimessage"), aCallback);
  }
  if (mPort->ConnectionState() != MIDIPortConnectionState::Open) {
    mPort->SendOpen();
  }
}


} // namespace dom
} // namespace mozilla
