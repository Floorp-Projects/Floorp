// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process.h"
#include "base/process_util.h"

namespace base {

void Process::Close() {
  process_ = 0;
  // if the process wasn't termiated (so we waited) or the state
  // wasn't already collected w/ a wait from process_utils, we're gonna
  // end up w/ a zombie when it does finally exit.
}

void Process::Terminate(int result_code) {
  // result_code isn't supportable.
  if (!process_)
    return;
  // We don't wait here. It's the responsibility of other code to reap the
  // child.
  KillProcess(process_, result_code, false);
}

bool Process::IsProcessBackgrounded() const {
  // http://code.google.com/p/chromium/issues/detail?id=8083
  return false;
}

bool Process::SetProcessBackgrounded(bool value) {
  // http://code.google.com/p/chromium/issues/detail?id=8083
  // Just say we did it to keep renderer happy at the moment.  Need to finish
  // cleaning this up w/in higher layers since windows is probably the only
  // one that can raise priorities w/o privileges.
  return true;
}

bool Process::ReduceWorkingSet() {
  // http://code.google.com/p/chromium/issues/detail?id=8083
  return false;
}

bool Process::UnReduceWorkingSet() {
  // http://code.google.com/p/chromium/issues/detail?id=8083
  return false;
}

bool Process::EmptyWorkingSet() {
  // http://code.google.com/p/chromium/issues/detail?id=8083
  return false;
}

ProcessId Process::pid() const {
  if (process_ == 0)
    return 0;

  return GetProcId(process_);
}

bool Process::is_current() const {
  return process_ == GetCurrentProcessHandle();
}

// static
Process Process::Current() {
  return Process(GetCurrentProcessHandle());
}

}  // namspace base
