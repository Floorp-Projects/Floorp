/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_TypedObject_h
#define wasm_TypedObject_h

#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"

#include "gc/Allocator.h"
#include "gc/WeakMap.h"
#include "vm/ArrayBufferObject.h"
#include "vm/JSObject.h"
#include "wasm/WasmTypeDef.h"
#include "wasm/WasmValType.h"

namespace js {

class TypedObject;

class RttValue : public NativeObject {
 private:
  static RttValue* create(JSContext* cx, wasm::TypeHandle handle);

 public:
  static const JSClass class_;

  enum Slot {
    TypeContext = 0,  // Manually refcounted reference to TypeContext
    TypeDef = 1,      // Raw pointer to TypeDef owned by TypeContext
    Parent = 2,       // Parent rtt for runtime casting
    Children = 3,     // Child rtts for rtt.sub caching
    // Maximum number of slots
    SlotCount = 4,
  };

  static RttValue* rttCanon(JSContext* cx, wasm::TypeHandle handle);
  static RttValue* rttSub(JSContext* cx, js::Handle<RttValue*> parent,
                          js::Handle<RttValue*> subCanon);

  bool isNewborn() { return getReservedSlot(Slot::TypeContext).isUndefined(); }

  const wasm::TypeDef& typeDef() const {
    return *(const wasm::TypeDef*)getReservedSlot(Slot::TypeDef).toPrivate();
  }

  const wasm::TypeContext* typeContext() const {
    return (const wasm::TypeContext*)getReservedSlot(Slot::TypeContext)
        .toPrivate();
  }

  wasm::TypeHandle typeHandle() const {
    return wasm::TypeHandle(typeContext(), typeDef());
  }

  wasm::TypeDefKind kind() const { return typeDef().kind(); }

  RttValue* parent() const {
    return (RttValue*)getReservedSlot(Slot::Parent).toObjectOrNull();
  }

  ObjectWeakMap* maybeChildren() const {
    return (ObjectWeakMap*)getReservedSlot(Slot::Children).toPrivate();
  }
  ObjectWeakMap& children() const { return *maybeChildren(); }
  bool ensureChildren(JSContext* cx);

  [[nodiscard]] bool lookupProperty(JSContext* cx,
                                    js::Handle<TypedObject*> object, jsid id,
                                    uint32_t* offset, wasm::FieldType* type);
  [[nodiscard]] bool hasProperty(JSContext* cx, js::Handle<TypedObject*> object,
                                 jsid id) {
    uint32_t offset;
    wasm::FieldType type;
    return lookupProperty(cx, object, id, &offset, &type);
  }

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JS::GCContext* gcx, JSObject* obj);
};

/* Base type for typed objects. */
class TypedObject : public JSObject {
 protected:
  GCPtr<RttValue*> rttValue_;

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

  template <typename T>
  static T* create(JSContext* cx, gc::AllocKind allocKind,
                   gc::InitialHeap heap);

  void initDefault();

  bool loadValue(JSContext* cx, size_t offset, wasm::FieldType type,
                 MutableHandleValue vp);

  uint8_t* typedMem() const;

  template <typename V>
  void visitReferences(V& visitor);

 public:
  // Creates a new struct typed object initialized to zero. Reports if there
  // is an out of memory error.
  static TypedObject* createStruct(JSContext* cx, Handle<RttValue*> rtt,
                                   gc::InitialHeap heap = gc::DefaultHeap);

  // Creates a new array typed object initialized to zero for the specified
  // number of elements.  Reports an error if the number of elements is too
  // large, or if there is an out of memory.
  static TypedObject* createArray(JSContext* cx, Handle<RttValue*> rtt,
                                  uint32_t numElements,
                                  gc::InitialHeap heap = gc::DefaultHeap);

  RttValue& rttValue() const {
    MOZ_ASSERT(rttValue_);
    return *rttValue_;
  }

  [[nodiscard]] bool isRuntimeSubtype(js::Handle<RttValue*> rtt) const;

  static constexpr size_t offsetOfRttValue() {
    return offsetof(TypedObject, rttValue_);
  }

  [[nodiscard]] static bool obj_newEnumerate(JSContext* cx, HandleObject obj,
                                             MutableHandleIdVector properties,
                                             bool enumerableOnly);
};

// Class for a typed object whose data is allocated in the malloc heap.
class OutlineTypedObject : public TypedObject {
 public:
  // This holds a value indicating the number of elements in the block (array)
  // that `data_` points at.
  using NumElements = uint32_t;

 private:
  // Owned data pointer.  In the case where this TypedObject is being used to
  // represent a wasm array, the pointed-to block must be at least 4 bytes
  // long since the first 4 bytes are used to store the number of array
  // elements present.  They start at `&data_[4]`.  In the case where this
  // TypedObject is being used to represent a wasm struct, the 4-byte limit
  // does not apply.
  uint8_t* data_;

 protected:
  friend class TypedObject;

  // `numBytes` is the required size of the block pointed to by `data`.
  static OutlineTypedObject* create(JSContext* cx, Handle<RttValue*> rtt,
                                    size_t numBytes,
                                    gc::InitialHeap heap = gc::DefaultHeap);

  // This can possibly be removed, specialised or renamed, as part of the
  // cleanup scheduled to happen in bug 1774836.
  uint8_t* outOfLineTypedMem() const { return data_; }

  void setNumElements(NumElements numElements) {
    *(NumElements*)(data_ + offsetOfNumElements()) = numElements;
  }

 public:
  static const JSClass class_;

  // The address of the first element in the array
  uint8_t* addressOfElementZero() const {
    return data_ + offsetOfNumElements() + sizeof(NumElements);
  }

  // The number of elements in the array
  NumElements numElements() const {
    return *(NumElements*)(data_ + offsetOfNumElements());
  }

  // AllocKind for object creation
  static gc::AllocKind allocKind();

  // JIT accessors

  // Offset of `data_` relative to the start of the object.
  static constexpr size_t offsetOfData() {
    return offsetof(OutlineTypedObject, data_);
  }
  // Offset inside `data_` of its size-in-bytes field.
  static constexpr size_t offsetOfNumElements() { return 0; }

  // Tracing and finalization
  static void obj_trace(JSTracer* trc, JSObject* object);
  static void obj_finalize(JS::GCContext* gcx, JSObject* object);
};

// Helper to mark all locations that assume the type of the array length header
// for a typed object.
#define STATIC_ASSERT_NUMELEMENTS_IS_U32                       \
  static_assert(sizeof(js::OutlineTypedObject::NumElements) == \
                sizeof(uint32_t));

// Class for a typed object whose data is allocated inline.
class InlineTypedObject : public TypedObject {
  // Start of the inline data, which immediately follows the shape and type.
  uint8_t data_[1];

 protected:
  friend class TypedObject;

  static const size_t MaxInlineBytes =
      JSObject::MAX_BYTE_SIZE - sizeof(TypedObject);

  static InlineTypedObject* create(JSContext* cx, Handle<RttValue*> rtt,
                                   gc::InitialHeap heap = gc::DefaultHeap);

  uint8_t* inlineTypedMem() const { return (uint8_t*)&data_; }

 public:
  static const JSClass class_;

  // AllocKind for object creation
  static inline gc::AllocKind allocKindForRttValue(RttValue* rtt);

  // Check if the following byte size could be allocated in an InlineTypedObject
  static bool canAccommodateSize(size_t size) { return size <= MaxInlineBytes; }

  // JIT accessors
  static size_t offsetOfDataStart() {
    return offsetof(InlineTypedObject, data_);
  }

  // Tracing and finalization
  static void obj_trace(JSTracer* trc, JSObject* object);
  static size_t obj_moved(JSObject* dst, JSObject* src);
};

inline bool IsTypedObjectClass(const JSClass* class_) {
  return class_ == &OutlineTypedObject::class_ ||
         class_ == &InlineTypedObject::class_;
}

inline bool IsOutlineTypedObjectClass(const JSClass* class_) {
  return class_ == &OutlineTypedObject::class_;
}

inline bool IsInlineTypedObjectClass(const JSClass* class_) {
  return class_ == &InlineTypedObject::class_;
}

}  // namespace js

template <>
inline bool JSObject::is<js::TypedObject>() const {
  return js::IsTypedObjectClass(getClass());
}

template <>
inline bool JSObject::is<js::OutlineTypedObject>() const {
  return js::IsOutlineTypedObjectClass(getClass());
}

template <>
inline bool JSObject::is<js::InlineTypedObject>() const {
  return js::IsInlineTypedObjectClass(getClass());
}

#endif /* wasm_TypedObject_h */
