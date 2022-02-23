/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDILog_h
#define mozilla_dom_MIDILog_h

#include <mozilla/Logging.h>
#include <nsStringFwd.h>

namespace mozilla::dom {
class MIDIMessage;
enum class MIDIPortType : uint8_t;
}  // namespace mozilla::dom

extern mozilla::LazyLogModule gWebMIDILog;

#define LOG(...) MOZ_LOG(gWebMIDILog, mozilla::LogLevel::Debug, (__VA_ARGS__));
#define LOGV(x, ...) \
  MOZ_LOG(gWebMIDILog, mozilla::LogLevel::Verbose, (__VA_ARGS__));

void LogMIDIMessage(const mozilla::dom::MIDIMessage& aMessage,
                    const nsAString& aPortId,
                    mozilla::dom::MIDIPortType aDirection);

#endif  // mozilla_dom_MIDILog_h
