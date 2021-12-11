/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIInput_h
#define mozilla_dom_MIDIInput_h

#include "mozilla/dom/MIDIPort.h"

struct JSContext;

namespace mozilla {
namespace dom {

class MIDIPortInfo;

/**
 * Represents a MIDI Input Port, handles generating incoming message events.
 *
 */
class MIDIInput final : public MIDIPort {
 public:
  static MIDIInput* Create(nsPIDOMWindowInner* aWindow,
                           MIDIAccess* aMIDIAccessParent,
                           const MIDIPortInfo& aPortInfo,
                           const bool aSysexEnabled);
  ~MIDIInput() = default;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Since we need to be able to open the port on event handler assignment, we
  // can't use IMPL_EVENT_HANDLER. We have to implement the event handler
  // functions ourselves.

  // Getter for the event handler callback
  EventHandlerNonNull* GetOnmidimessage();
  // Setter for the event handler callback
  void SetOnmidimessage(EventHandlerNonNull* aCallback);

 private:
  MIDIInput(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent);
  // Takes an array of IPC MIDIMessage objects and turns them into
  // MIDIMessageEvents, which it then fires.
  void Receive(const nsTArray<MIDIMessage>& aMsgs) override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIInput_h
