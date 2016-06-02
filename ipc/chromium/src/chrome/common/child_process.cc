/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/common/child_thread.h"

ChildProcess* ChildProcess::child_process_;

ChildProcess::ChildProcess(ChildThread* child_thread)
    : child_thread_(child_thread) {
  DCHECK(!child_process_);
  child_process_ = this;
  if (child_thread_.get())  // null in unittests.
    child_thread_->Run();
}

ChildProcess::~ChildProcess() {
  DCHECK(child_process_ == this);

  if (child_thread_.get())
    child_thread_->Stop();

  child_process_ = NULL;
}
