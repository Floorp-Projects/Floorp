// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/native_web_keyboard_event.h"

#include "third_party/WebKit/WebKit/chromium/public/win/WebInputEventFactory.h"

using WebKit::WebInputEventFactory;
using WebKit::WebKeyboardEvent;

NativeWebKeyboardEvent::NativeWebKeyboardEvent() {
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    : WebKeyboardEvent(
          WebInputEventFactory::keyboardEvent(hwnd, message, wparam, lparam)) {
  os_event.hwnd = hwnd;
  os_event.message = message;
  os_event.wParam = wparam;
  os_event.lParam = lparam;
}

NativeWebKeyboardEvent::NativeWebKeyboardEvent(
    const NativeWebKeyboardEvent& other)
    : WebKeyboardEvent(other) {
  os_event.hwnd = other.os_event.hwnd;
  os_event.message = other.os_event.message;
  os_event.wParam = other.os_event.wParam;
  os_event.lParam = other.os_event.lParam;
}

NativeWebKeyboardEvent& NativeWebKeyboardEvent::operator=(
    const NativeWebKeyboardEvent& other) {
  WebKeyboardEvent::operator=(other);

  os_event.hwnd = other.os_event.hwnd;
  os_event.message = other.os_event.message;
  os_event.wParam = other.os_event.wParam;
  os_event.lParam = other.os_event.lParam;
  return *this;
}

NativeWebKeyboardEvent::~NativeWebKeyboardEvent() {
  // Noop under windows
}
