/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

namespace base {

Result<Ok, LaunchError> LaunchApp(const std::vector<std::string>& argv,
                                  LaunchOptions&& options,
                                  ProcessHandle* process_handle) {
  return Err(LaunchError("LaunchApp is not supported on iOS"));
}

}  // namespace base
