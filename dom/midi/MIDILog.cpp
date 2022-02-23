/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MIDILog.h"

#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/MIDIPortBinding.h"

mozilla::LazyLogModule gWebMIDILog("WebMIDI");

void LogMIDIMessage(const mozilla::dom::MIDIMessage& aMessage,
                    const nsAString& aPortId,
                    mozilla::dom::MIDIPortType aDirection) {
  if (MOZ_LOG_TEST(gWebMIDILog, mozilla::LogLevel::Debug)) {
    if (MOZ_LOG_TEST(gWebMIDILog, mozilla::LogLevel::Verbose)) {
      uint32_t byteCount = aMessage.data().Length();
      nsAutoCString logMessage;
      // Log long messages inline with the timestamp and the length, log
      // longer messages a bit like xxd
      logMessage.AppendPrintf(
          "%s %s length=%u", NS_ConvertUTF16toUTF8(aPortId).get(),
          aDirection == mozilla::dom::MIDIPortType::Input ? "->" : "<-",
          byteCount);

      if (byteCount <= 3) {
        logMessage.AppendPrintf(" [");
        // Regular messages
        for (uint32_t i = 0; i < byteCount - 1; i++) {
          logMessage.AppendPrintf("%x ", aMessage.data()[i]);
        }
        logMessage.AppendPrintf("%x]", aMessage.data()[byteCount - 1]);
      } else {
        // Longer messages
        for (uint32_t i = 0; i < byteCount; i++) {
          if (!(i % 8)) {
            logMessage.AppendPrintf("\n%08u:\t", i);
          }
          logMessage.AppendPrintf("%x ", aMessage.data()[i]);
        }
      }
      MOZ_LOG(gWebMIDILog, mozilla::LogLevel::Verbose,
              ("%s", logMessage.get()));
    }
  }
}
