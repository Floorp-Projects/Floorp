/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_WasmGcObject_h
#define wasm_WasmGcObject_h

#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"

#include "gc/GCProbes.h"
#include "gc/Pretenuring.h"
#include "gc/ZoneAllocator.h"  // AddCellMemory
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/Probes.h"
#include "wasm/WasmInstanceData.h"
#include "wasm/WasmMemory.h"
#include "wasm/WasmTypeDef.h"
#include "wasm/WasmValType.h"

using js::wasm::FieldType;

namespace js::wasm {

// For trailer blocks whose owning Wasm{Struct,Array}Objects make it into the
// tenured heap, we have to tell the tenured heap how big those trailers are
// in order to get major GCs to happen sufficiently frequently.  In an attempt
// to make the numbers more accurate, for each block we overstate the size by
// the following amount, on the assumption that:
//
// * mozjemalloc has an overhead of at least one word per block
//
// * the malloc-cache mechanism rounds up small block sizes to the nearest 16;
//   hence the average increase is 16 / 2.
static const size_t TrailerBlockOverhead = (16 / 2) + (1 * sizeof(void*));

}  // namespace js::wasm

namespace js {

//=========================================================================
// WasmGcObject

class WasmGcObject : public JSObject {
 protected:
  const wasm::SuperTypeVector* superTypeVector_;

  static const ObjectOps objectOps_;

  [[nodiscard]] static bool obj_lookupProperty(JSContext* cx, HandleObject obj,
                                               HandleId id,
                                               MutableHandleObject objp,
                                               PropertyResult* propp);

  [[nodiscard]] static bool obj_defineProperty(JSContext* cx, HandleObject obj,
                                               HandleId id,
                                               Handle<PropertyDescriptor> desc,
                                               ObjectOpResult& result);

  [[nodiscard]] static bool obj_hasProperty(JSContext* cx, HandleObject obj,
                                            HandleId id, bool* foundp);

  [[nodiscard]] static bool obj_getProperty(JSContext* cx, HandleObject obj,
                                            HandleValue receiver, HandleId id,
                                            MutableHandleValue vp);

  [[nodiscard]] static bool obj_setProperty(JSContext* cx, HandleObject obj,
                                            HandleId id, HandleValue v,
                                            HandleValue receiver,
                                            ObjectOpResult& result);

  [[nodiscard]] static bool obj_getOwnPropertyDescriptor(
      JSContext* cx, HandleObject obj, HandleId id,
      MutableHandle<mozilla::Maybe<PropertyDescriptor>> desc);

  [[nodiscard]] static bool obj_deleteProperty(JSContext* cx, HandleObject obj,
                                               HandleId id,
                                               ObjectOpResult& result);

  // PropOffset is a uint32_t that is used to carry information about the
  // location of an value from WasmGcObject::lookupProperty to
  // WasmGcObject::loadValue.  It is distinct from a normal uint32_t to
  // emphasise the fact that it cannot be interpreted as an offset in any
  // single contiguous area of memory:
  //
  // * If the object in question is a WasmStructObject, it is the value of
  //   `wasm::StructField::offset` for the relevant field, without regard to
  //   the inline/outline split.
  //
  // * If the object in question is a WasmArrayObject, then
  //   - u32 == UINT32_MAX (0xFFFF'FFFF) means the "length" property
  //     is requested
  //   - u32 < UINT32_MAX means the array element starting at that byte
  //     offset in WasmArrayObject::data_.  It is not an array index value.
  //   See WasmGcObject::lookupProperty for details.
  class PropOffset {
    uint32_t u32_;

   public:
    PropOffset() : u32_(0) {}
    uint32_t get() const { return u32_; }
    void set(uint32_t u32) { u32_ = u32; }
  };

  [[nodiscard]] static bool lookUpProperty(JSContext* cx,
                                           Handle<WasmGcObject*> obj, jsid id,
                                           PropOffset* offset, FieldType* type);

 public:
  [[nodiscard]] static bool loadValue(JSContext* cx, Handle<WasmGcObject*> obj,
                                      jsid id, MutableHandleValue vp);

  const wasm::SuperTypeVector& superTypeVector() const {
    return *superTypeVector_;
  }

  static constexpr size_t offsetOfSuperTypeVector() {
    return offsetof(WasmGcObject, superTypeVector_);
  }

  // These are both expensive in that they involve a double indirection.
  // Avoid them if possible.
  const wasm::TypeDef& typeDef() const { return *superTypeVector().typeDef(); }
  wasm::TypeDefKind kind() const { return superTypeVector().typeDef()->kind(); }

  [[nodiscard]] bool isRuntimeSubtypeOf(
      const wasm::TypeDef* parentTypeDef) const;

  [[nodiscard]] static bool obj_newEnumerate(JSContext* cx, HandleObject obj,
                                             MutableHandleIdVector properties,
                                             bool enumerableOnly);
};

//=========================================================================
// WasmArrayObject

// Class for a wasm array.  It contains a pointer to the array contents, that
// lives in the C++ heap.

class WasmArrayObject : public WasmGcObject {
 public:
  static const JSClass class_;

  // The number of elements in the array.
  uint32_t numElements_;

  // Owned data pointer, holding `numElements_` entries.  This is null if
  // `numElements_` is zero; otherwise it must be non-null.  See bug 1812283.
  uint8_t* data_;

  // AllocKind for object creation
  static gc::AllocKind allocKind();

  // Creates a new non-empty array typed object, optionally initialized to
  // zero, for the specified number of elements.  Reports an error if the
  // number of elements is too large, or if there is an out of memory error.
  // The element type, shape, class pointer, alloc site and alloc kind are
  // taken from `typeDefData`; the initial heap must be specified separately.
  // The number of elements is assumed and debug-asserted to be non-zero.
  template <bool ZeroFields>
  static WasmArrayObject* createArrayNonEmpty(
      JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
      js::gc::Heap initialHeap, uint32_t numElements);

  // Creates a new empty array typed object, for zero elements.  Reports an
  // error if there is an out of memory error.  The element type, shape, class
  // pointer, alloc site and alloc kind are taken from `typeDefData`; the
  // initial heap must be specified separately.  The number of elements is
  // assumed and debug-asserted to be zero.
  static WasmArrayObject* createArrayEmpty(
      JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
      js::gc::Heap initialHeap);

  // This just selects one of the above two routines, depending on
  // `numElements`.
  template <bool ZeroFields>
  static MOZ_ALWAYS_INLINE WasmArrayObject* createArray(
      JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
      js::gc::Heap initialHeap, uint32_t numElements) {
    return numElements == 0 ? createArrayEmpty(cx, typeDefData, initialHeap)
                            : createArrayNonEmpty<ZeroFields>(
                                  cx, typeDefData, initialHeap, numElements);
  }

  // JIT accessors
  static constexpr size_t offsetOfNumElements() {
    return offsetof(WasmArrayObject, numElements_);
  }
  static constexpr size_t offsetOfData() {
    return offsetof(WasmArrayObject, data_);
  }

  // Tracing and finalization
  static void obj_trace(JSTracer* trc, JSObject* object);
  static void obj_finalize(JS::GCContext* gcx, JSObject* object);
  static size_t obj_moved(JSObject* obj, JSObject* old);

  void storeVal(const wasm::Val& val, uint32_t itemIndex);
  void fillVal(const wasm::Val& val, uint32_t itemIndex, uint32_t len);
};

// Helper to mark all locations that assume that the type of
// WasmArrayObject::numElements is uint32_t.
#define STATIC_ASSERT_WASMARRAYELEMENTS_NUMELEMENTS_IS_U32 \
  static_assert(sizeof(js::WasmArrayObject::numElements_) == sizeof(uint32_t))

//=========================================================================
// WasmStructObject

// Class for a wasm struct.  It has inline data and, if the inline area is
// insufficient, a pointer to outline data that lives in the C++ heap.
// Computing the field offsets is somewhat tricky; see block comment on `class
// StructLayout` for background.

class WasmStructObject : public WasmGcObject {
 public:
  static const JSClass classInline_;
  static const JSClass classOutline_;

  // Owned pointer to a malloc'd block containing out-of-line fields, or
  // nullptr if none.  Note that MIR alias analysis assumes this is readonly
  // for the life of the object; do not change it once the object is created.
  // See MWasmLoadObjectField::congruentTo.
  uint8_t* outlineData_;

  // The inline (wasm-struct-level) data fields.  This must be a multiple of
  // 16 bytes long in order to ensure that no field gets split across the
  // inline-outline boundary.  As a refinement, we request this field to begin
  // at an 8-aligned offset relative to the start of the object, so as to
  // guarantee that `double` typed fields are not subject to misaligned-access
  // penalties on any target, whilst wasting at maximum 4 bytes of space.
  //
  // `inlineData_` is in reality a variable length block with maximum size
  // WasmStructObject_MaxInlineBytes bytes.  Do not add any (C++-level) fields
  // after this point!
  alignas(8) uint8_t inlineData_[0];

  // This tells us how big the object is if we know the number of inline bytes
  // it was created with.
  static inline size_t sizeOfIncludingInlineData(size_t sizeOfInlineData) {
    size_t n = sizeof(WasmStructObject) + sizeOfInlineData;
    MOZ_ASSERT(n <= JSObject::MAX_BYTE_SIZE);
    return n;
  }

  static const JSClass* classForTypeDef(const wasm::TypeDef* typeDef);
  static js::gc::AllocKind allocKindForTypeDef(const wasm::TypeDef* typeDef);

  // Creates a new struct typed object, optionally initialized to zero.
  // Reports if there is an out of memory error.  The structure's type, shape,
  // class pointer, alloc site and alloc kind are taken from `typeDefData`;
  // the initial heap must be specified separately.  It is assumed and debug-
  // asserted that `typeDefData` refers to a type that does not need OOL
  // storage.
  template <bool ZeroFields>
  static MOZ_ALWAYS_INLINE WasmStructObject* createStructIL(
      JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
      js::gc::Heap initialHeap);

  // Same as ::createStructIL, except it is assumed and debug-asserted that
  // `typeDefData` refers to a type that does need OOL storage.
  template <bool ZeroFields>
  static MOZ_ALWAYS_INLINE WasmStructObject* createStructOOL(
      JSContext* cx, wasm::TypeDefInstanceData* typeDefData,
      js::gc::Heap initialHeap);

  // Given the total number of data bytes required (including alignment
  // holes), return the number of inline and outline bytes required.
  static inline void getDataByteSizes(uint32_t totalBytes,
                                      uint32_t* inlineBytes,
                                      uint32_t* outlineBytes);

  // Convenience function; returns true iff ::getDataByteSizes would set
  // *outlineBytes to a non-zero value.
  static inline bool requiresOutlineBytes(uint32_t totalBytes);

  // Given the offset of a field, produce the offset in `inlineData_` or
  // `*outlineData_` to use, plus a bool indicating which area it is.
  // `fieldType` is for assertional purposes only.
  static inline void fieldOffsetToAreaAndOffset(FieldType fieldType,
                                                uint32_t fieldOffset,
                                                bool* areaIsOutline,
                                                uint32_t* areaOffset);

  // Given the offset of a field, return its actual address.  `fieldType` is
  // for assertional purposes only.
  inline uint8_t* fieldOffsetToAddress(FieldType fieldType,
                                       uint32_t fieldOffset) const;

  // JIT accessors
  static constexpr size_t offsetOfOutlineData() {
    return offsetof(WasmStructObject, outlineData_);
  }
  static constexpr size_t offsetOfInlineData() {
    return offsetof(WasmStructObject, inlineData_);
  }

  // Tracing and finalization
  static void obj_trace(JSTracer* trc, JSObject* object);
  static void obj_finalize(JS::GCContext* gcx, JSObject* object);
  static size_t obj_moved(JSObject* obj, JSObject* old);

  void storeVal(const wasm::Val& val, uint32_t fieldIndex);
};

// This is ensured by the use of `alignas` on `WasmStructObject::inlineData_`.
static_assert((offsetof(WasmStructObject, inlineData_) % 8) == 0);

// MaxInlineBytes must be a multiple of 16 for reasons described in the
// comment on `class StructLayout`.  This unfortunately can't be defined
// inside the class definition itself because the sizeof(..) expression isn't
// valid until after the end of the class definition.
const size_t WasmStructObject_MaxInlineBytes =
    ((JSObject::MAX_BYTE_SIZE - sizeof(WasmStructObject)) / 16) * 16;

static_assert((WasmStructObject_MaxInlineBytes % 16) == 0);

/*static*/
inline void WasmStructObject::getDataByteSizes(uint32_t totalBytes,
                                               uint32_t* inlineBytes,
                                               uint32_t* outlineBytes) {
  if (MOZ_UNLIKELY(totalBytes > WasmStructObject_MaxInlineBytes)) {
    *inlineBytes = WasmStructObject_MaxInlineBytes;
    *outlineBytes = totalBytes - WasmStructObject_MaxInlineBytes;
  } else {
    *inlineBytes = totalBytes;
    *outlineBytes = 0;
  }
}

/* static */
inline bool WasmStructObject::requiresOutlineBytes(uint32_t totalBytes) {
  uint32_t inlineBytes, outlineBytes;
  WasmStructObject::getDataByteSizes(totalBytes, &inlineBytes, &outlineBytes);
  return outlineBytes > 0;
}

/*static*/
inline void WasmStructObject::fieldOffsetToAreaAndOffset(FieldType fieldType,
                                                         uint32_t fieldOffset,
                                                         bool* areaIsOutline,
                                                         uint32_t* areaOffset) {
  if (fieldOffset < WasmStructObject_MaxInlineBytes) {
    *areaIsOutline = false;
    *areaOffset = fieldOffset;
  } else {
    *areaIsOutline = true;
    *areaOffset = fieldOffset - WasmStructObject_MaxInlineBytes;
  }
  // Assert that the first and last bytes for the field agree on which side of
  // the inline/outline boundary they live.
  MOZ_RELEASE_ASSERT(
      (fieldOffset < WasmStructObject_MaxInlineBytes) ==
      ((fieldOffset + fieldType.size() - 1) < WasmStructObject_MaxInlineBytes));
}

inline uint8_t* WasmStructObject::fieldOffsetToAddress(
    FieldType fieldType, uint32_t fieldOffset) const {
  bool areaIsOutline;
  uint32_t areaOffset;
  fieldOffsetToAreaAndOffset(fieldType, fieldOffset, &areaIsOutline,
                             &areaOffset);
  return ((uint8_t*)(areaIsOutline ? outlineData_ : &inlineData_[0])) +
         areaOffset;
}

// Ensure that faulting loads/stores for WasmStructObject and WasmArrayObject
// are in the NULL pointer guard page.
static_assert(WasmStructObject_MaxInlineBytes <= wasm::NullPtrGuardSize);
static_assert(sizeof(WasmArrayObject) <= wasm::NullPtrGuardSize);

}  // namespace js

//=========================================================================
// misc

namespace js {

inline bool IsWasmGcObjectClass(const JSClass* class_) {
  return class_ == &WasmArrayObject::class_ ||
         class_ == &WasmStructObject::classInline_ ||
         class_ == &WasmStructObject::classOutline_;
}

}  // namespace js

template <>
inline bool JSObject::is<js::WasmGcObject>() const {
  return js::IsWasmGcObjectClass(getClass());
}

template <>
inline bool JSObject::is<js::WasmStructObject>() const {
  const JSClass* class_ = getClass();
  return class_ == &js::WasmStructObject::classInline_ ||
         class_ == &js::WasmStructObject::classOutline_;
}

#endif /* wasm_WasmGcObject_h */
