/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2023 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef wasm_anyref_h
#define wasm_anyref_h

#include <utility>

#include "js/HeapAPI.h"
#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"

// #include "NamespaceImports.h"

class JSObject;
class JSString;

namespace js {
namespace gc {
struct Cell;
};  // namespace gc

namespace wasm {

// [SMDOC] AnyRef
//
// An AnyRef is a boxed value that can represent any wasm reference type and any
// host type that the host system allows to flow into and out of wasm
// transparently.  It is a pointer-sized datum that has the same representation
// as all its subtypes (funcref, externref, eqref, (ref T), et al) due to the
// non-coercive subtyping of the wasm type system.  Its current representation
// is a plain JSObject*, and the private JSObject subtype WasmValueBox is used
// to box non-object non-null JS values.
//
// The C++/wasm boundary always uses a 'void*' type to express AnyRef values, to
// emphasize the pointer-ness of the value.  The C++ code must transform the
// void* into an AnyRef by calling AnyRef::fromCompiledCode(), and transform an
// AnyRef into a void* by calling AnyRef::toCompiledCode().  Once in C++, we use
// AnyRef everywhere.  A JS Value is transformed into an AnyRef by calling
// AnyRef::fromJSValue(), and the AnyRef is transformed into a JS Value by
// calling AnyRef::toJSValue().
//
// NOTE that AnyRef values may point to GC'd storage and as such need to be
// rooted if they are kept live in boxed form across code that may cause GC!
// Use RootedAnyRef / HandleAnyRef / MutableHandleAnyRef where necessary.
//
// The lowest bits of the pointer value are used for tagging, to allow for some
// representation optimizations and to distinguish various types.
//
// For version 0, we simply equate AnyRef and JSObject* (this means that there
// are technically no tags at all yet).  We use a simple boxing scheme that
// wraps a JS value that is not already JSObject in a distinguishable JSObject
// that holds the value, see WasmTypes.cpp for details.  Knowledge of this
// mapping is embedded in CodeGenerator.cpp (in WasmBoxValue and
// WasmAnyRefFromJSObject) and in WasmStubs.cpp (in functions Box* and Unbox*).

// The kind of value stored in an AnyRef. This is not 1:1 with the pointer tag
// of AnyRef as this separates the 'Null' and 'Object' cases which are
// collapsed in the pointer tag.
enum class AnyRefKind : uint8_t {
  Null,
  Object,
};

// The pointer tag of an AnyRef.
enum class AnyRefTag : uint8_t {
  // This value is either a JSObject& or a null pointer.
  ObjectOrNull = 0x0,
};
static constexpr uintptr_t TAG_MASK = 0x3;
static constexpr uintptr_t TAG_SHIFT = 2;
static_assert(TAG_SHIFT <= gc::CellAlignShift, "not enough free bits");

static constexpr uintptr_t AddAnyRefTag(uintptr_t value, AnyRefTag tag) {
  MOZ_ASSERT(!(value & TAG_MASK));
  return value | uintptr_t(tag);
}
static constexpr AnyRefTag GetAnyRefTag(uintptr_t value) {
  return (AnyRefTag)(value & TAG_MASK);
}
static constexpr uintptr_t RemoveAnyRefTag(uintptr_t value) {
  return value & ~TAG_MASK;
}

// The representation of a null reference value throughout the compiler for
// when we need an integer constant. This is asserted to be equivalent to
// nullptr in wasm::Init.
static constexpr uintptr_t NULLREF_VALUE = 0;
static_assert(GetAnyRefTag(NULLREF_VALUE) == AnyRefTag::ObjectOrNull);

static constexpr uintptr_t INVALIDREF_VALUE = UINTPTR_MAX & ~TAG_MASK;
static_assert(GetAnyRefTag(INVALIDREF_VALUE) == AnyRefTag::ObjectOrNull);

// A reference to any wasm reference type or host (JS) value. AnyRef is
// optimized for efficient access to wasm GC objects and possibly a tagged
// integer type (in the future).
//
// See the above documentation comment for more details.
class AnyRef {
  uintptr_t value_;

  // Get the pointer tag stored in value_.
  AnyRefTag pointerTag() const { return GetAnyRefTag(value_); }

  explicit AnyRef(uintptr_t value) : value_(value) {}

 public:
  explicit AnyRef() : value_(NULLREF_VALUE) {}
  MOZ_IMPLICIT AnyRef(std::nullptr_t) : value_(NULLREF_VALUE) {}

  // The null AnyRef value.
  static AnyRef null() { return AnyRef(NULLREF_VALUE); }

  // An invalid AnyRef cannot arise naturally from wasm and so can be used as
  // a sentinel value to indicate failure from an AnyRef-returning function.
  static AnyRef invalid() { return AnyRef(INVALIDREF_VALUE); }

  // Given a JSObject* that comes from JS, turn it into AnyRef.
  static AnyRef fromJSObjectOrNull(JSObject* objectOrNull) {
    MOZ_ASSERT(GetAnyRefTag((uintptr_t)objectOrNull) ==
               AnyRefTag::ObjectOrNull);
    return AnyRef((uintptr_t)objectOrNull);
  }

  // Given a JSObject& that comes from JS, turn it into AnyRef.
  static AnyRef fromJSObject(JSObject& object) {
    MOZ_ASSERT(GetAnyRefTag((uintptr_t)&object) == AnyRefTag::ObjectOrNull);
    return AnyRef((uintptr_t)&object);
  }

  // Given a void* that comes from compiled wasm code, turn it into AnyRef.
  static AnyRef fromCompiledCode(void* pointer) {
    return AnyRef((uintptr_t)pointer);
  }

  // Given a JS value, turn it into AnyRef. This returns false if boxing the
  // value failed due to an OOM.
  static bool fromJSValue(JSContext* cx, JS::HandleValue value,
                          JS::MutableHandle<AnyRef> result);

  // Returns whether a JS value will need to be boxed.
  static bool valueNeedsBoxing(JS::HandleValue value) {
    return !value.isObjectOrNull();
  }

  // Box a JS Value that needs boxing.
  static JSObject* boxValue(JSContext* cx, JS::HandleValue value);

  bool operator==(const AnyRef& rhs) const {
    return this->value_ == rhs.value_;
  }
  bool operator!=(const AnyRef& rhs) const { return !(*this == rhs); }

  // Check if this AnyRef is the invalid value.
  bool isInvalid() const {
    return *this == AnyRef::invalid();
  }

  AnyRefKind kind() const {
    if (value_ == NULLREF_VALUE) {
      return AnyRefKind::Null;
    }
    switch (pointerTag()) {
      case AnyRefTag::ObjectOrNull: {
        // The invalid pattern uses the ObjectOrNull tag, check for it here.
        MOZ_ASSERT(!isInvalid());
        // We ruled out the null case above
        return AnyRefKind::Object;
      }
      default: {
        MOZ_CRASH("unknown AnyRef tag");
      }
    }
  }

  bool isNull() const { return value_ == NULLREF_VALUE; }
  bool isGCThing() const { return kind() == AnyRefKind::Object; }
  bool isJSObject() const { return kind() == AnyRefKind::Object; }

  gc::Cell* toGCThing() const {
    MOZ_ASSERT(isGCThing());
    return (gc::Cell*)RemoveAnyRefTag(value_);
  }
  JSObject& toJSObject() const {
    MOZ_ASSERT(isJSObject());
    return *(JSObject*)value_;
  }
  JSObject* toJSObjectOrNull() const {
    MOZ_ASSERT(!isInvalid());
    return (JSObject*)value_;
  }

  // Convert from AnyRef to a JS Value. This currently does not require any
  // allocation. If this changes in the future, this function will become
  // more complicated.
  JS::Value toJSValue() const;

  // Get the raw value for returning to wasm code.
  void* forCompiledCode() const { return (void*)value_; }

  // Get the raw value for diagnostics.
  uintptr_t rawValue() const { return value_; }

  // Internal details of the boxing format used by WasmStubs.cpp
  static const JSClass* valueBoxClass();
  static size_t valueBoxOffsetOfValue();
};

using RootedAnyRef = JS::Rooted<AnyRef>;
using HandleAnyRef = JS::Handle<AnyRef>;
using MutableHandleAnyRef = JS::MutableHandle<AnyRef>;

// TODO/AnyRef-boxing: With boxed immediates and strings, these will be defined
// as MOZ_CRASH or similar so that we can find all locations that need to be
// fixed.

#define ASSERT_ANYREF_IS_JSOBJECT (void)(0)
#define STATIC_ASSERT_ANYREF_IS_JSOBJECT static_assert(1, "AnyRef is JSObject")

}  // namespace wasm

template <class Wrapper>
class WrappedPtrOperations<wasm::AnyRef, Wrapper> {
  const wasm::AnyRef& value() const {
    return static_cast<const Wrapper*>(this)->get();
  }

 public:
  bool isNull() const { return value().isNull(); }
};

// If the Value is a GC pointer type, call |f| with the pointer cast to that
// type and return the result wrapped in a Maybe, otherwise return None().
template <typename F>
auto MapGCThingTyped(const wasm::AnyRef& val, F&& f) {
  switch (val.kind()) {
    case wasm::AnyRefKind::Object:
      return mozilla::Some(f(&val.toJSObject()));
    case wasm::AnyRefKind::Null: {
      using ReturnType = decltype(f(static_cast<JSObject*>(nullptr)));
      return mozilla::Maybe<ReturnType>();
    }
  }
  MOZ_CRASH();
}

template <typename F>
bool ApplyGCThingTyped(const wasm::AnyRef& val, F&& f) {
  return MapGCThingTyped(val,
                         [&f](auto t) {
                           f(t);
                           return true;
                         })
      .isSome();
}

}  // namespace js

namespace JS {

template <>
struct GCPolicy<js::wasm::AnyRef> {
  static void trace(JSTracer* trc, js::wasm::AnyRef* v, const char* name) {
    // This should only be called as part of root marking since that's the only
    // time we should trace unbarriered GC thing pointers. This will assert if
    // called at other times.
    TraceRoot(trc, v, name);
  }
  static bool isValid(const js::wasm::AnyRef& v) {
    return !v.isGCThing() || js::gc::IsCellPointerValid(v.toGCThing());
  }
};

}  // namespace JS

#endif  // wasm_anyref_h
