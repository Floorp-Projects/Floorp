/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Inline members for javascript type inference. */

#ifndef vm_TypeInference_inl_h
#define vm_TypeInference_inl_h

#include "vm/TypeInference.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/Casting.h"
#include "mozilla/PodOperations.h"

#include <utility>  // for ::std::swap

#include "builtin/Symbol.h"
#include "gc/GC.h"
#include "jit/BaselineJIT.h"
#include "jit/IonScript.h"
#include "jit/JitScript.h"
#include "jit/JitZone.h"
#include "js/HeapAPI.h"
#include "util/DiagnosticAssertions.h"
#include "vm/ArrayObject.h"
#include "vm/BooleanObject.h"
#include "vm/JSFunction.h"
#include "vm/NativeObject.h"
#include "vm/NumberObject.h"
#include "vm/ObjectGroup.h"
#include "vm/PlainObject.h"  // js::PlainObject
#include "vm/Shape.h"
#include "vm/SharedArrayObject.h"
#include "vm/StringObject.h"
#include "vm/TypedArrayObject.h"

#include "jit/JitScript-inl.h"
#include "vm/JSContext-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/ObjectGroup-inl.h"

namespace js {

class MOZ_RAII AutoSuppressAllocationMetadataBuilder {
  JS::Zone* zone;
  bool saved;

 public:
  explicit AutoSuppressAllocationMetadataBuilder(JSContext* cx)
      : zone(cx->zone()), saved(zone->suppressAllocationMetadataBuilder) {
    zone->suppressAllocationMetadataBuilder = true;
  }

  ~AutoSuppressAllocationMetadataBuilder() {
    zone->suppressAllocationMetadataBuilder = saved;
  }
};

/*
 * Structure for type inference entry point functions. All functions which can
 * change type information must use this, and functions which depend on
 * intermediate types (i.e. JITs) can use this to ensure that intermediate
 * information is not collected and does not change.
 *
 * Ensures that GC cannot occur. Does additional sanity checking that inference
 * is not reentrant and that recompilations occur properly.
 */
struct MOZ_RAII AutoEnterAnalysis {
  // Prevent GC activity in the middle of analysis.
  gc::AutoSuppressGC suppressGC;

  // Prevent us from calling the objectMetadataCallback.
  js::AutoSuppressAllocationMetadataBuilder suppressMetadata;

  explicit AutoEnterAnalysis(JSContext* cx)
      : suppressGC(cx), suppressMetadata(cx) {}
};

}  // namespace js

#endif /* vm_TypeInference_inl_h */
