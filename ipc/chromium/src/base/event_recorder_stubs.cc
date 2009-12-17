// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/event_recorder.h"

// This file implements a link stub for EventRecorder that can be used on
// platforms that don't have a working EventRecorder implementation.

namespace base {

EventRecorder* EventRecorder::current_;  // Our singleton.

bool EventRecorder::StartRecording(const FilePath& filename) {
  return true;
}

void EventRecorder::StopRecording() {
}

bool EventRecorder::StartPlayback(const FilePath& filename) {
  return false;
}

void EventRecorder::StopPlayback() {
}

}  // namespace
