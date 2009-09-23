// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory_debug.h"

#ifdef PURIFY
// this #define is used to prevent people from directly using pure.h
// instead of memory_debug.h
#define PURIFY_PRIVATE_INCLUDE
#include "base/third_party/purify/pure.h"
#endif

namespace base {

bool MemoryDebug::memory_in_use_ = false;

void MemoryDebug::SetMemoryInUseEnabled(bool enabled) {
  memory_in_use_ = enabled;
}

void MemoryDebug::DumpAllMemoryInUse() {
#ifdef PURIFY
  if (memory_in_use_)
    PurifyAllInuse();
#endif
}

void MemoryDebug::DumpNewMemoryInUse() {
#ifdef PURIFY
  if (memory_in_use_)
    PurifyNewInuse();
#endif
}

void MemoryDebug::DumpAllLeaks() {
#ifdef PURIFY
  PurifyAllLeaks();
#endif
}

void MemoryDebug::DumpNewLeaks() {
#ifdef PURIFY
  PurifyNewLeaks();
#endif
}

void MemoryDebug::MarkAsInitialized(void* addr, size_t size) {
#ifdef PURIFY
  PurifyMarkAsInitialized(addr, size);
#endif
}

}  // namespace base
