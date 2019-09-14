/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <AppKit/AppKit.h>
#import <AppKit/NSEvent.h>
#import <IOKit/hidsystem/ev_keymap.h>

#include "MediaHardwareKeysEventSourceMac.h"

#include "mozilla/Logging.h"

extern mozilla::LazyLogModule gMediaControlLog;

// avoid redefined macro in unified build
#undef LOG
#define LOG(msg, ...)                        \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaHardwareKeysEventSourceMac=%p, " msg, this, ##__VA_ARGS__))

// This macro is used in static callback function, where we have to send `this`
// explicitly.
#define LOG2(msg, this, ...)                 \
  MOZ_LOG(gMediaControlLog, LogLevel::Debug, \
          ("MediaHardwareKeysEventSourceMac=%p, " msg, this, ##__VA_ARGS__))

static const char* ToMediaControlKeyStr(int aKeyCode) {
  switch (aKeyCode) {
    case NX_KEYTYPE_PLAY:
      return "Play";
    case NX_KEYTYPE_NEXT:
      return "Next";
    case NX_KEYTYPE_PREVIOUS:
      return "Previous";
    case NX_KEYTYPE_FAST:
      return "Fast";
    case NX_KEYTYPE_REWIND:
      return "Rewind";
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid action.");
  }
  return "UNKNOWN";
}

// The media keys subtype. No official docs found, but widely known.
// http://lists.apple.com/archives/cocoa-dev/2007/Aug/msg00499.html
const int kSystemDefinedEventMediaKeysSubtype = 8;

namespace mozilla {
namespace dom {

static MediaControlKeysEvent ToMediaControlKeysEvent(int aKeyCode) {
  switch (aKeyCode) {
    case NX_KEYTYPE_PLAY:
      return MediaControlKeysEvent::ePlayPause;
    case NX_KEYTYPE_NEXT:
    case NX_KEYTYPE_FAST:
      return MediaControlKeysEvent::eNext;
    case NX_KEYTYPE_PREVIOUS:
    case NX_KEYTYPE_REWIND:
      return MediaControlKeysEvent::ePrev;
    default:
      MOZ_ASSERT_UNREACHABLE("Invalid action.");
  }
  return MediaControlKeysEvent::eNone;
}

MediaHardwareKeysEventSourceMac::MediaHardwareKeysEventSourceMac() {
  LOG("Create MediaHardwareKeysEventSourceMac");
  StartEventTap();
}

MediaHardwareKeysEventSourceMac::~MediaHardwareKeysEventSourceMac() {
  LOG("Destroy MediaHardwareKeysEventSourceMac");
  StopEventTap();
}

void MediaHardwareKeysEventSourceMac::StartEventTap() {
  LOG("StartEventTap");
  MOZ_ASSERT(!mEventTap);
  MOZ_ASSERT(!mEventTapSource);

  // Add an event tap to intercept the system defined media key events.
  mEventTap =
      CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
                       CGEventMaskBit(NX_SYSDEFINED), EventTapCallback, this);
  if (!mEventTap) {
    LOG("Fail to create event tap");
    return;
  }

  mEventTapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, mEventTap, 0);
  if (!mEventTapSource) {
    LOG("Fail to create an event tap source.");
    return;
  }

  LOG("Add an event tap source to current loop");
  CFRunLoopAddSource(CFRunLoopGetCurrent(), mEventTapSource, kCFRunLoopCommonModes);
}

void MediaHardwareKeysEventSourceMac::StopEventTap() {
  LOG("StopEventTapIfNecessary");
  if (mEventTap) {
    CFMachPortInvalidate(mEventTap);
    mEventTap = nullptr;
  }
  if (mEventTapSource) {
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), mEventTapSource, kCFRunLoopCommonModes);
    CFRelease(mEventTapSource);
    mEventTapSource = nullptr;
  }
}

CGEventRef MediaHardwareKeysEventSourceMac::EventTapCallback(CGEventTapProxy proxy,
                                                             CGEventType type, CGEventRef event,
                                                             void* refcon) {
  // Re-enable event tap when receiving disabled events.
  MediaHardwareKeysEventSourceMac* source = static_cast<MediaHardwareKeysEventSourceMac*>(refcon);
  if (type == kCGEventTapDisabledByUserInput || type == kCGEventTapDisabledByTimeout) {
    MOZ_ASSERT(source->mEventTap);
    CGEventTapEnable(source->mEventTap, true);
    return event;
  }

  NSEvent* nsEvent = [NSEvent eventWithCGEvent:event];
  if (nsEvent == nil) {
    return event;
  }

  // Ignore not system defined media keys event.
  if ([nsEvent type] != NSSystemDefined ||
      [nsEvent subtype] != kSystemDefinedEventMediaKeysSubtype) {
    return event;
  }

  // Ignore media keys that aren't previous, next and play/pause.
  // Magical constants are from http://weblog.rogueamoeba.com/2007/09/29/
  // - keyCode = (([event data1] & 0xFFFF0000) >> 16)
  // - keyFlags = ([event data1] & 0x0000FFFF)
  // - keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA
  // - keyRepeat = (keyFlags & 0x1);
  const NSInteger data1 = [nsEvent data1];
  int keyCode = (data1 & 0xFFFF0000) >> 16;
  if (keyCode != NX_KEYTYPE_PLAY && keyCode != NX_KEYTYPE_NEXT && keyCode != NX_KEYTYPE_PREVIOUS &&
      keyCode != NX_KEYTYPE_FAST && keyCode != NX_KEYTYPE_REWIND) {
    return event;
  }

  // Ignore non-key pressed event (eg. key released).
  int keyFlags = data1 & 0x0000FFFF;
  bool isKeyPressed = ((keyFlags & 0xFF00) >> 8) == 0xA;
  if (!isKeyPressed) {
    return event;
  }

  // There is no listener waiting to process event.
  if (source->mListeners.IsEmpty()) {
    return event;
  }

  LOG2("Get media key %s", source, ToMediaControlKeyStr(keyCode));
  for (auto iter = source->mListeners.begin(); iter != source->mListeners.end(); ++iter) {
    (*iter)->OnKeyPressed(ToMediaControlKeysEvent(keyCode));
  }
  return event;
}

}  // namespace dom
}  // namespace mozilla
