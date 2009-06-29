// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions used to debug memory usage, leaks, and other memory issues.
// All methods are effectively no-ops unless this program is being run through
// a supported memory tool (currently, only Purify)

#ifndef BASE_MEMORY_DEBUG_H_
#define BASE_MEMORY_DEBUG_H_

#include "base/basictypes.h"

namespace base {

class MemoryDebug {
 public:
  // Since MIU messages are a lot of data, and we don't always want this data,
  // we have a global switch.  If disabled, *MemoryInUse are no-ops.
  static void SetMemoryInUseEnabled(bool enabled);

  // Dump information about all memory in use.
  static void DumpAllMemoryInUse();
  // Dump information about new memory in use since the last
  // call to DumpAllMemoryInUse() or DumpNewMemoryInUse().
  static void DumpNewMemoryInUse();

  // Dump information about all current memory leaks.
  static void DumpAllLeaks();
  // Dump information about new memory leaks since the last
  // call to DumpAllLeaks() or DumpNewLeaks()
  static void DumpNewLeaks();

  // Mark |size| bytes of memory as initialized, so it doesn't produce any UMRs
  // or UMCs.
  static void MarkAsInitialized(void* addr, size_t size);

 private:
  static bool memory_in_use_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryDebug);
};

}  // namespace base

#endif  // BASE_MEMORY_DEBUG_H_
