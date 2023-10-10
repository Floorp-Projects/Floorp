/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIInput.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDIInputBinding.h"
#include "mozilla/dom/MIDIMessageEvent.h"
#include "mozilla/dom/MIDIMessageEventBinding.h"

#include "MIDILog.h"

namespace mozilla::dom {

MIDIInput::MIDIInput(nsPIDOMWindowInner* aWindow)
    : MIDIPort(aWindow), mKeepAlive(false) {}

// static
RefPtr<MIDIInput> MIDIInput::Create(nsPIDOMWindowInner* aWindow,
                                    MIDIAccess* aMIDIAccessParent,
                                    const MIDIPortInfo& aPortInfo,
                                    const bool aSysexEnabled) {
  MOZ_ASSERT(static_cast<MIDIPortType>(aPortInfo.type()) ==
             MIDIPortType::Input);
  RefPtr<MIDIInput> port = new MIDIInput(aWindow);
  if (!port->Initialize(aPortInfo, aSysexEnabled, aMIDIAccessParent)) {
    return nullptr;
  }
  return port;
}

JSObject* MIDIInput::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return MIDIInput_Binding::Wrap(aCx, this, aGivenProto);
}

void MIDIInput::Receive(const nsTArray<MIDIMessage>& aMsgs) {
  if (!GetOwner()) {
    return;  // Ignore messages once we've been disconnected from the owner
  }

  nsCOMPtr<Document> doc = GetOwner()->GetDoc();
  if (!doc) {
    NS_WARNING("No document available to send MIDIMessageEvent to!");
    return;
  }
  for (const auto& msg : aMsgs) {
    RefPtr<MIDIMessageEvent> event(
        MIDIMessageEvent::Constructor(this, msg.timestamp(), msg.data()));
    DispatchTrustedEvent(event);
  }
}

void MIDIInput::StateChange() {
  if (Port()->ConnectionState() == MIDIPortConnectionState::Open ||
      (Port()->DeviceState() == MIDIPortDeviceState::Connected &&
       Port()->ConnectionState() == MIDIPortConnectionState::Pending)) {
    KeepAliveOnMidimessage();
  } else {
    DontKeepAliveOnMidimessage();
  }
}

void MIDIInput::EventListenerAdded(nsAtom* aType) {
  if (aType == nsGkAtoms::onmidimessage) {
    // HACK: the Web MIDI spec states that we should open a port only when
    // setting the midimessage event handler but Chrome does it even when
    // adding event listeners hence this.
    if (Port()->ConnectionState() != MIDIPortConnectionState::Open) {
      LOG("onmidimessage event listener added, sending implicit Open");
      Port()->SendOpen();
    }
  }

  DOMEventTargetHelper::EventListenerAdded(aType);
}

void MIDIInput::DisconnectFromOwner() {
  DontKeepAliveOnMidimessage();

  MIDIPort::DisconnectFromOwner();
}

void MIDIInput::KeepAliveOnMidimessage() {
  if (!mKeepAlive) {
    mKeepAlive = true;
    KeepAliveIfHasListenersFor(nsGkAtoms::onmidimessage);
  }
}

void MIDIInput::DontKeepAliveOnMidimessage() {
  if (mKeepAlive) {
    IgnoreKeepAliveIfHasListenersFor(nsGkAtoms::onmidimessage);
    mKeepAlive = false;
  }
}

}  // namespace mozilla::dom
