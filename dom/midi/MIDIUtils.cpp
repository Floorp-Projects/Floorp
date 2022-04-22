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
static const uint8_t kReservedStatuses[] = {0xf4, 0xf5, 0xf9, 0xfd};

namespace mozilla::dom::MIDIUtils {

static bool IsSystemRealtimeMessage(uint8_t aByte) {
  return (aByte & kSystemRealtimeMessage) == kSystemRealtimeMessage;
}

static bool IsCommandByte(uint8_t aByte) {
  return (aByte & kCommandByte) == kCommandByte;
}

static bool IsReservedStatus(uint8_t aByte) {
  for (const auto& msg : kReservedStatuses) {
    if (aByte == msg) {
      return true;
    }
  }

  return false;
}

// Checks validity of MIDIMessage passed to it. Throws debug warnings and
// returns false if message is not valid.
bool IsValidMessage(const MIDIMessage* aMsg) {
  if (aMsg->data().Length() == 0) {
    return false;
  }

  uint8_t cmd = aMsg->data()[0];
  // If first byte isn't a command, something is definitely wrong.
  if (!IsCommandByte(cmd)) {
    NS_WARNING("Constructed a MIDI packet where first byte is not command!");
    return false;
  }

  if (IsReservedStatus(cmd)) {
    NS_WARNING("Using a reserved message");
    return false;
  }

  if (cmd == kSysexMessageStart) {
    // All we can do with sysex is make sure it starts and ends with the
    // correct command bytes and that it does not contain other command bytes.
    if (aMsg->data()[aMsg->data().Length() - 1] != kSysexMessageEnd) {
      NS_WARNING("Last byte of Sysex Message not 0xF7!");
      return false;
    }

    for (size_t i = 1; i < aMsg->data().Length() - 2; i++) {
      if (IsCommandByte(aMsg->data()[i])) {
        return false;
      }
    }

    return true;
  }
  // For system realtime messages, the length should always be 1.
  if (IsSystemRealtimeMessage(cmd)) {
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

bool ParseMessages(const nsTArray<uint8_t>& aByteBuffer,
                   const TimeStamp& aTimestamp,
                   nsTArray<MIDIMessage>& aMsgArray) {
  bool inSysexMessage = false;
  UniquePtr<MIDIMessage> currentMsg = nullptr;
  for (const auto& byte : aByteBuffer) {
    if (IsSystemRealtimeMessage(byte)) {
      MIDIMessage rt_msg;
      rt_msg.data().AppendElement(byte);
      rt_msg.timestamp() = aTimestamp;
      if (!IsValidMessage(&rt_msg)) {
        return false;
      }
      aMsgArray.AppendElement(rt_msg);
      continue;
    }

    if (byte == kSysexMessageEnd) {
      if (!inSysexMessage) {
        NS_WARNING(
            "Got sysex message end with no sysex message being processed!");
        return false;
      }
      inSysexMessage = false;
    } else if (IsCommandByte(byte)) {
      if (currentMsg) {
        if (!IsValidMessage(currentMsg.get())) {
          return false;
        }

        aMsgArray.AppendElement(*currentMsg);
      }

      currentMsg = MakeUnique<MIDIMessage>();
      currentMsg->timestamp() = aTimestamp;
    }

    if (!currentMsg) {
      NS_WARNING("No command byte has been encountered yet!");
      return false;
    }

    currentMsg->data().AppendElement(byte);

    if (byte == kSysexMessageStart) {
      inSysexMessage = true;
    }
  }

  if (currentMsg) {
    if (!IsValidMessage(currentMsg.get())) {
      return false;
    }
    aMsgArray.AppendElement(*currentMsg);
  }

  return true;
}

bool IsSysexMessage(const MIDIMessage& aMsg) {
  if (aMsg.data().Length() == 0) {
    return false;
  }
  return aMsg.data()[0] == kSysexMessageStart;
}
}  // namespace mozilla::dom::MIDIUtils
