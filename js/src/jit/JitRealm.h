/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_JitRealm_h
#define jit_JitRealm_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/MemoryReporting.h"

#include <stddef.h>
#include <stdint.h>

#include "gc/Barrier.h"
#include "gc/ZoneAllocator.h"
#include "js/GCHashTable.h"
#include "js/RootingAPI.h"
#include "js/TracingAPI.h"
#include "js/TypeDecls.h"

namespace js {

MOZ_COLD void ReportOutOfMemory(JSContext* cx);

namespace jit {

class JitCode;

class JitRealm {
 public:
  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const;
};

}  // namespace jit
}  // namespace js

#endif /* jit_JitRealm_h */
