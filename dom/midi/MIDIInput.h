/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIInput_h
#define mozilla_dom_MIDIInput_h

#include "mozilla/dom/MIDIPort.h"

struct JSContext;

namespace mozilla::dom {

class MIDIPortInfo;

/**
 * Represents a MIDI Input Port, handles generating incoming message events.
 *
 */
class MIDIInput final : public MIDIPort {
 public:
  static RefPtr<MIDIInput> Create(nsPIDOMWindowInner* aWindow,
                                  MIDIAccess* aMIDIAccessParent,
                                  const MIDIPortInfo& aPortInfo,
                                  const bool aSysexEnabled);
  ~MIDIInput() = default;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  IMPL_EVENT_HANDLER(midimessage);

  void StateChange() override;
  void EventListenerAdded(nsAtom* aType) override;
  void DisconnectFromOwner() override;

 private:
  explicit MIDIInput(nsPIDOMWindowInner* aWindow);
  // Takes an array of IPC MIDIMessage objects and turns them into
  // MIDIMessageEvents, which it then fires.
  void Receive(const nsTArray<MIDIMessage>& aMsgs) override;

  void KeepAliveOnMidimessage();
  void DontKeepAliveOnMidimessage();

  // If true this object will be kept alive even without direct JS references
  bool mKeepAlive;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIInput_h
