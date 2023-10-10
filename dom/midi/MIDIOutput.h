/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIOutput_h
#define mozilla_dom_MIDIOutput_h

#include "mozilla/dom/MIDIPort.h"

struct JSContext;

namespace mozilla {
class ErrorResult;

namespace dom {

class MIDIPortInfo;
class MIDIMessage;

/**
 * Represents a MIDI Output Port, handles sending message to devices.
 *
 */
class MIDIOutput final : public MIDIPort {
 public:
  static RefPtr<MIDIOutput> Create(nsPIDOMWindowInner* aWindow,
                                   MIDIAccess* aMIDIAccessParent,
                                   const MIDIPortInfo& aPortInfo,
                                   const bool aSysexEnabled);
  ~MIDIOutput() = default;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // Send a message to an output port
  void Send(const Sequence<uint8_t>& aData, const Optional<double>& aTimestamp,
            ErrorResult& aRv);
  // Clear any partially sent messages from the send queue
  void Clear();

 private:
  explicit MIDIOutput(nsPIDOMWindowInner* aWindow);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIOutput_h
