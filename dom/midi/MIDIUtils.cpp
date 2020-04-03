/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/MIDIUtils.h"
#include "mozilla/UniquePtr.h"

// Taken from MIDI IMPLEMENTATION CHART INSTRUCTIONS, MIDI Spec v1.0, Pg. 97
static const uint8_t kCommandByte = 0x80;
static const uint8_t kSysexMessageStart = 0xF0;
static const uint8_t kSystemMessage = 0xF0;
static const uint8_t kSysexMessageEnd = 0xF7;
static const uint8_t kSystemRealtimeMessage = 0xF8;
// Represents the length of all possible command messages.
// Taken from MIDI Spec, Pg. 101v 1.0, Table 2
static const uint8_t kCommandLengths[] = {3, 3, 3, 3, 2, 2, 3};
// Represents the length of all possible system messages. The length of sysex
// messages is variable, so we just put zero since it won't be checked anyways.
// Taken from MIDI Spec v1.0, Pg. 105, Table 5
static const uint8_t kSystemLengths[] = {0, 2, 3, 2, 1, 1, 1, 1};

namespace mozilla {
namespace dom {
namespace MIDIUtils {

// Checks validity of MIDIMessage passed to it. Throws debug warnings and
// returns false if message is not valid.
bool IsValidMessage(const MIDIMessage* aMsg) {
  if (NS_WARN_IF(!aMsg)) {
    return false;
  }
  // Assert on parser problems
  MOZ_ASSERT(aMsg->data().Length() > 0,
             "Created a MIDI Message of Length 0. This should never happen!");
  uint8_t cmd = aMsg->data()[0];
  // If first byte isn't a command, something is definitely wrong.
  MOZ_ASSERT((cmd & kCommandByte) == kCommandByte,
             "Constructed a MIDI packet where first byte is not command!");
  if (cmd == kSysexMessageStart) {
    // All we can do with sysex is make sure it starts and  ends with the
    // correct command bytes.
    if (aMsg->data()[aMsg->data().Length() - 1] != kSysexMessageEnd) {
      NS_WARNING("Last byte of Sysex Message not 0xF7!");
      return false;
    }
    return true;
  }
  // For system realtime messages, the length should always be 1.
  if ((cmd & kSystemRealtimeMessage) == kSystemRealtimeMessage) {
    return aMsg->data().Length() == 1;
  }
  // Otherwise, just use the correct array for testing lengths. We can't tell
  // much about message validity other than that.
  if ((cmd & kSystemMessage) == kSystemMessage) {
    if (cmd - kSystemMessage >=
        static_cast<uint8_t>(ArrayLength(kSystemLengths))) {
      NS_WARNING("System Message Command byte not valid!");
      return false;
    }
    return aMsg->data().Length() == kSystemLengths[cmd - kSystemMessage];
  }
  // For non system commands, we only care about differences in the high nibble
  // of the first byte. Shift this down to give the index of the expected packet
  // length.
  uint8_t cmdIndex = (cmd - kCommandByte) >> 4;
  if (cmdIndex >= ArrayLength(kCommandLengths)) {
    // If our index is bigger than our array length, command byte is unknown;
    NS_WARNING("Unknown MIDI command!");
    return false;
  }
  return aMsg->data().Length() == kCommandLengths[cmdIndex];
}

uint32_t ParseMessages(const nsTArray<uint8_t>& aByteBuffer,
                       const TimeStamp& aTimestamp,
                       nsTArray<MIDIMessage>& aMsgArray) {
  uint32_t bytesRead = 0;
  bool inSysexMessage = false;
  UniquePtr<MIDIMessage> currentMsg;
  for (auto& byte : aByteBuffer) {
    bytesRead++;
    if ((byte & kSystemRealtimeMessage) == kSystemRealtimeMessage) {
      MIDIMessage rt_msg;
      rt_msg.data().AppendElement(byte);
      rt_msg.timestamp() = aTimestamp;
      aMsgArray.AppendElement(rt_msg);
      continue;
    }
    if (byte == kSysexMessageEnd) {
      if (!inSysexMessage) {
        MOZ_ASSERT(inSysexMessage);
        NS_WARNING(
            "Got sysex message end with no sysex message being processed!");
      }
      inSysexMessage = false;
    } else if (byte & kCommandByte) {
      if (currentMsg && IsValidMessage(currentMsg.get())) {
        aMsgArray.AppendElement(*currentMsg);
      }
      currentMsg = MakeUnique<MIDIMessage>();
      currentMsg->timestamp() = aTimestamp;
    }
    currentMsg->data().AppendElement(byte);
    if (byte == kSysexMessageStart) {
      inSysexMessage = true;
    }
  }
  if (currentMsg && IsValidMessage(currentMsg.get())) {
    aMsgArray.AppendElement(*currentMsg);
  }
  return bytesRead;
}

bool IsSysexMessage(const MIDIMessage& aMsg) {
  if (aMsg.data().Length() == 0) {
    return false;
  }
  if (aMsg.data()[0] == kSysexMessageStart) {
    return true;
  }
  return false;
}
}  // namespace MIDIUtils
}  // namespace dom
}  // namespace mozilla
