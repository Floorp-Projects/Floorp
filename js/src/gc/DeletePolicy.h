/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_DeletePolicy_h
#define gc_DeletePolicy_h

#include "gc/ClearEdgesTracer.h"

namespace js {

/*
 * Provides a delete policy that can be used for objects which have their
 * lifetime managed by the GC so they can be safely destroyed outside of GC.
 *
 * This is necessary for example when initializing such an object may fail after
 * the initial allocation. The partially-initialized object must be destroyed,
 * but it may not be safe to do so at the current time as the store buffer may
 * contain pointers into it.
 *
 * This policy traces GC pointers in the object and clears them, making sure to
 * trigger barriers while doing so. This will remove any store buffer pointers
 * into the object and make it safe to delete.
 */
template <typename T>
struct GCManagedDeletePolicy {
  void operator()(const T* constPtr) {
    if (constPtr) {
      auto ptr = const_cast<T*>(constPtr);
      gc::ClearEdgesTracer trc;
      ptr->trace(&trc);
      js_delete(ptr);
    }
  }
};

}  // namespace js

#endif  // gc_DeletePolicy_h
