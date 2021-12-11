/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/waitable_event.h"

#include <math.h>
#include <windows.h>

#include "base/logging.h"
#include "base/time.h"

namespace base {

WaitableEvent::WaitableEvent(bool manual_reset, bool signaled)
    : handle_(CreateEvent(NULL, manual_reset, signaled, NULL)) {
  // We're probably going to crash anyways if this is ever NULL, so we might as
  // well make our stack reports more informative by crashing here.
  CHECK(handle_);
}

WaitableEvent::~WaitableEvent() { CloseHandle(handle_); }

void WaitableEvent::Reset() { ResetEvent(handle_); }

void WaitableEvent::Signal() { SetEvent(handle_); }

bool WaitableEvent::IsSignaled() {
  return TimedWait(TimeDelta::FromMilliseconds(0));
}

bool WaitableEvent::Wait() {
  DWORD result = WaitForSingleObject(handle_, INFINITE);
  // It is most unexpected that this should ever fail.  Help consumers learn
  // about it if it should ever fail.
  DCHECK(result == WAIT_OBJECT_0) << "WaitForSingleObject failed";
  return result == WAIT_OBJECT_0;
}

bool WaitableEvent::TimedWait(const TimeDelta& max_time) {
  DCHECK(max_time >= TimeDelta::FromMicroseconds(0));
  // Be careful here.  TimeDelta has a precision of microseconds, but this API
  // is in milliseconds.  If there are 5.5ms left, should the delay be 5 or 6?
  // It should be 6 to avoid returning too early.
  double timeout = ceil(max_time.InMillisecondsF());
  DWORD result = WaitForSingleObject(handle_, static_cast<DWORD>(timeout));
  switch (result) {
    case WAIT_OBJECT_0:
      return true;
    case WAIT_TIMEOUT:
      return false;
  }
  // It is most unexpected that this should ever fail.  Help consumers learn
  // about it if it should ever fail.
  NOTREACHED() << "WaitForSingleObject failed";
  return false;
}

// static
size_t WaitableEvent::WaitMany(WaitableEvent** events, size_t count) {
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  CHECK(count <= MAXIMUM_WAIT_OBJECTS)
  << "Can only wait on " << MAXIMUM_WAIT_OBJECTS << " with WaitMany";

  for (size_t i = 0; i < count; ++i) handles[i] = events[i]->handle();

  DWORD result =
      WaitForMultipleObjects(count, handles,
                             FALSE,      // don't wait for all the objects
                             INFINITE);  // no timeout
  if (result >= WAIT_OBJECT_0 + count) {
    NOTREACHED() << "WaitForMultipleObjects failed: " << GetLastError();
    return 0;
  }

  return result - WAIT_OBJECT_0;
}

}  // namespace base
