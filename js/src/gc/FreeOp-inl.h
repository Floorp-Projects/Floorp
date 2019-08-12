/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_FreeOp_inl_h
#define gc_FreeOp_inl_h

#include "gc/FreeOp.h"

#include "gc/ZoneAllocator.h"
#include "js/RefCounted.h"

namespace js {

inline void FreeOp::free_(gc::Cell* cell, void* p, size_t nbytes,
                          MemoryUse use) {
  if (p) {
    removeCellMemory(cell, nbytes, use);
    js_free(p);
  }
}

inline void FreeOp::freeLater(gc::Cell* cell, void* p, size_t nbytes,
                              MemoryUse use) {
  if (p) {
    removeCellMemory(cell, nbytes, use);
    queueForFreeLater(p);
  }
}

inline void FreeOp::queueForFreeLater(void* p) {
  // Default FreeOps are not constructed on the stack, and will hold onto the
  // pointers to free indefinitely.
  MOZ_ASSERT(!isDefaultFreeOp());

  // It's not safe to free this allocation immediately, so we must crash if we
  // can't append to the list.
  AutoEnterOOMUnsafeRegion oomUnsafe;
  if (!freeLaterList.append(p)) {
    oomUnsafe.crash("FreeOp::freeLater");
  }
}

template <class T>
inline void FreeOp::release(gc::Cell* cell, T* p, size_t nbytes,
                            MemoryUse use) {
  if (p) {
    removeCellMemory(cell, nbytes, use);
    p->Release();
  }
}

inline void FreeOp::removeCellMemory(gc::Cell* cell, size_t nbytes,
                                     MemoryUse use) {
  RemoveCellMemory(cell, nbytes, use, isCollecting());
}

}  // namespace js

#endif  // gc_FreeOp_inl_h
