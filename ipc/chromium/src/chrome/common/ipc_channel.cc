/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel.h"

#include <limits>

#include "base/atomic_sequence_num.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"

namespace {

// Global atomic used to guarantee channel IDs are unique.
base::StaticAtomicSequenceNumber g_last_id;

}  // namespace

namespace IPC {

// static
std::wstring Channel::GenerateUniqueRandomChannelID() {
  // Note: the string must start with the current process id, this is how
  // some child processes determine the pid of the parent.
  //
  // This is composed of a unique incremental identifier, the process ID of
  // the creator, an identifier for the child instance, and a strong random
  // component. The strong random component prevents other processes from
  // hijacking or squatting on predictable channel names.

  return StringPrintf(L"%d.%u.%d",
      base::GetCurrentProcId(),
      g_last_id.GetNext(),
      base::RandInt(0, std::numeric_limits<int32_t>::max()));
}

}  // namespace IPC