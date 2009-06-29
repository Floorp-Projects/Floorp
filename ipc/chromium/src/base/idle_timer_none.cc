// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A stub implementation of IdleTimer used to provide symbols needed by Chrome.
// It's unclear whether we need the idle timer in the linux port, so we're
// leaving it unported for now.

#include "base/idle_timer.h"

namespace base {

IdleTimer::IdleTimer(TimeDelta idle_time, bool repeat) {
}

IdleTimer::~IdleTimer() {
}

void IdleTimer::Start() {
}

void IdleTimer::Stop() {
}

}  // namespace base
