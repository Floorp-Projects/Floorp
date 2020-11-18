/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* TypeSet classes and functions. */

#ifndef vm_TypeSet_h
#define vm_TypeSet_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_ALWAYS_INLINE
#include "mozilla/Likely.h"      // MOZ_UNLIKELY

#include <initializer_list>
#include <stdint.h>  // intptr_t, uintptr_t, uint8_t, uint32_t
#include <stdio.h>   // FILE

#include "jstypes.h"  // JS_BITS_PER_WORD, JS_PUBLIC_API

#include "jit/IonTypes.h"  // jit::MIRType
#include "jit/JitOptions.h"
#include "js/GCAnnotations.h"  // JS_HAZ_GC_POINTER
#include "js/Id.h"
#include "js/ScalarType.h"  // js::Scalar::Type
#include "js/TracingAPI.h"  // JSTracer
#include "js/TypeDecls.h"   // IF_BIGINT
#include "js/Utility.h"     // UniqueChars
#include "js/Value.h"       // JSVAL_TYPE_*
#include "js/Vector.h"      // js::Vector
#include "util/DiagnosticAssertions.h"
#include "vm/TaggedProto.h"  // js::TaggedProto

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;

namespace JS {

class JS_PUBLIC_API Compartment;
class JS_PUBLIC_API Realm;
class JS_PUBLIC_API Zone;

}  // namespace JS

namespace js {

namespace jit {

class IonScript;
class TempAllocator;

}  // namespace jit

class AutoClearTypeInferenceStateOnOOM;
class AutoSweepBase;
class AutoSweepObjectGroup;
class LifoAlloc;
class ObjectGroup;
class SystemAllocPolicy;
class TypeZone;

/*
 * Type inference memory management overview.
 *
 * Type information about the values observed within scripts and about the
 * contents of the heap is accumulated as the program executes. Compilation
 * accumulates constraints relating type information on the heap with the
 * compilations that should be invalidated when those types change. Type
 * information and constraints are allocated in the zone's typeLifoAlloc,
 * and on GC all data referring to live things is copied into a new allocator.
 * Thus, type set and constraints only hold weak references.
 */

/* Flags and other state stored in ObjectGroup::Flags */
enum : uint32_t {
  /* Whether this group is associated with a single object. */
  OBJECT_FLAG_SINGLETON = 0x2,

  /*
   * Whether this group is used by objects whose singleton groups have not
   * been created yet.
   */
  OBJECT_FLAG_LAZY_SINGLETON = 0x4,

  /* Whether any objects this represents may have sparse indexes. */
  OBJECT_FLAG_SPARSE_INDEXES = 0x00010000,

  /* Whether any objects this represents may not have packed dense elements. */
  OBJECT_FLAG_NON_PACKED = 0x00020000,

  /*
   * Whether any objects this represents may be arrays whose length does not
   * fit in an int32.
   */
  OBJECT_FLAG_LENGTH_OVERFLOW = 0x00040000,

  /* Whether any objects have been iterated over. */
  OBJECT_FLAG_ITERATED = 0x00080000,

  /* Whether any object this represents may have non-extensible elements. */
  OBJECT_FLAG_NON_EXTENSIBLE_ELEMENTS = 0x00100000,

  // (0x00200000 is unused)

  // (0x00400000 is unused)

  /*
   * Whether objects with this type should be allocated directly in the
   * tenured heap.
   */
  OBJECT_FLAG_PRE_TENURE = 0x00800000,

  /* Whether objects with this type might have copy on write elements. */
  OBJECT_FLAG_COPY_ON_WRITE = 0x01000000,

  /*
   * Whether all properties of this object are considered unknown.
   * If set, all other flags in DYNAMIC_MASK will also be set.
   */
  OBJECT_FLAG_UNKNOWN_PROPERTIES = 0x04000000,

  /* Flags which indicate dynamic properties of represented objects. */
  OBJECT_FLAG_DYNAMIC_MASK = 0x07ff0000,

  // Mask/shift for the kind of addendum attached to this group.
  OBJECT_FLAG_ADDENDUM_MASK = 0x38000000,
  OBJECT_FLAG_ADDENDUM_SHIFT = 27,

  // Mask/shift for this group's generation. If out of sync with the
  // TypeZone's generation, this group hasn't been swept yet.
  OBJECT_FLAG_GENERATION_MASK = 0x40000000,
  OBJECT_FLAG_GENERATION_SHIFT = 30,
};
using ObjectGroupFlags = uint32_t;

enum class DOMObjectKind : uint8_t { Proxy, Native, Unknown };

} /* namespace js */

#endif /* vm_TypeSet_h */
