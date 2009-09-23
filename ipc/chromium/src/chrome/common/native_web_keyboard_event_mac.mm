// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/native_web_keyboard_event.h"

#import <AppKit/AppKit.h>

#include "third_party/WebKit/WebKit/chromium/public/mac/WebInputEventFactory.h"

using WebKit::WebInputEventFactory;

NativeWebKeyboardEvent::NativeWebKeyboardEvent()
    : os_event(NULL) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(NSEvent* event)
    : WebKeyboardEvent(WebInputEventFactory::keyboardEvent(event)),
      os_event([event retain]) {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other),
      os_event([other.os_event retain]) {
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  NSObject* previous = os_event;
  os_event = [other.os_event retain];
  [previous release];

  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  [os_event release];
}
