// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_UTIL_ZONE_H_
#define V8_UTIL_ZONE_H_

#include "ds/LifoAlloc.h"

namespace v8 {
namespace internal {

// V8::Zone ~= LifoAlloc
class Zone {
 public:
  Zone(size_t defaultChunkSize) : lifoAlloc_(defaultChunkSize) {
    lifoAlloc_.setAsInfallibleByDefault();
  }

  void* New(size_t size) { return lifoAlloc_.alloc(size); }

  void DeleteAll() { lifoAlloc_.freeAll(); }

  // Returns true if the total memory allocated exceeds a threshold.
  static const size_t kExcessLimit = 256 * 1024 * 1024;
  bool excess_allocation() const {
    return lifoAlloc_.peakSizeOfExcludingThis() > kExcessLimit;
  }
private:
  js::LifoAlloc lifoAlloc_;
};

// Superclass for classes allocated in a Zone.
// Origin: https://github.com/v8/v8/blob/7b3332844212d78ee87a9426f3a6f7f781a8fbfa/src/zone/zone.h#L138-L155
class ZoneObject {
 public:
  // Allocate a new ZoneObject of 'size' bytes in the Zone.
  void* operator new(size_t size, Zone* zone) { return zone->New(size); }

  // Ideally, the delete operator should be private instead of
  // public, but unfortunately the compiler sometimes synthesizes
  // (unused) destructors for classes derived from ZoneObject, which
  // require the operator to be visible. MSVC requires the delete
  // operator to be public.

  // ZoneObjects should never be deleted individually; use
  // Zone::DeleteAll() to delete all zone objects in one go.
  void operator delete(void*, size_t) { MOZ_CRASH("unreachable"); }
  void operator delete(void* pointer, Zone* zone) { MOZ_CRASH("unreachable"); }
};

} // namespace internal
} // namespace v8

#endif  // V8_UTIL_FLAG_H_
