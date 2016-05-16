/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"

namespace switches {

// Can't find the switch you are looking for? try looking in
// base/base_switches.cc instead.

// The value of this switch tells the child process which
// IPC channel the browser expects to use to communicate with it.
const wchar_t kProcessChannelID[]              = L"channel";

}  // namespace switches
