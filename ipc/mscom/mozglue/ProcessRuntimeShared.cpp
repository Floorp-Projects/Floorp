/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/ProcessRuntimeShared.h"

#include "mozilla/glue/WinUtils.h"

// We allow multiple ProcessRuntime instances to exist simultaneously (even
// on separate threads), but only one should be doing the process-wide
// initialization. These variables provide that mutual exclusion.
static mozilla::glue::Win32SRWLock gLock;
static bool gIsProcessInitialized = false;

namespace mozilla {
namespace mscom {
namespace detail {

MFBT_API bool& BeginProcessRuntimeInit() {
  gLock.LockExclusive();
  return gIsProcessInitialized;
}

MFBT_API void EndProcessRuntimeInit() { gLock.UnlockExclusive(); }

}  // namespace detail
}  // namespace mscom
}  // namespace mozilla
