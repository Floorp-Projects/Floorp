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
  static void finalize(JSFreeOp* fop, JSObject* obj);
};

using MutableHandleRttValue = MutableHandle<RttValue*>;
using HandleRttValue = Handle<RttValue*>;
using RootedRttValue = Rooted<RttValue*>;

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

  bool loadValue(JSContext* cx, size_t offset, wasm::FieldType type,
                 MutableHandleValue vp);

  uint8_t* typedMem() const;

  template <typename V>
  void visitReferences(V& visitor);

  void initDefault();

 public:
  // Creates a new struct typed object initialized to zero.
  static TypedObject* createStruct(JSContext* cx, HandleRttValue rtt,
                                   gc::InitialHeap heap = gc::DefaultHeap);

  // Creates a new array typed object initialized to zero of specified length.
  static TypedObject* createArray(JSContext* cx, HandleRttValue rtt,
                                  uint32_t length,
                                  gc::InitialHeap heap = gc::DefaultHeap);

  // Internal create used by JSObject
  static TypedObject* create(JSContext* cx, js::gc::AllocKind kind,
                             js::gc::InitialHeap heap, js::HandleShape shape);

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

using HandleTypedObject = Handle<TypedObject*>;
using RootedTypedObject = Rooted<TypedObject*>;

class OutlineTypedObject : public TypedObject {
  // Owned data pointer
  uint8_t* data_;

  static OutlineTypedObject* create(JSContext* cx, HandleRttValue rtt,
                                    size_t byteLength,
                                    gc::InitialHeap heap = gc::DefaultHeap);

  void setArrayLength(uint32_t length) { *(uint32_t*)(data_) = length; }

 public:
  static const JSClass class_;

  static OutlineTypedObject* createStruct(JSContext* cx, HandleRttValue rtt,
                                          gc::InitialHeap heap);
  static OutlineTypedObject* createArray(JSContext* cx, HandleRttValue rtt,
                                         uint32_t length, gc::InitialHeap heap);

  // JIT accessors.
  static size_t offsetOfData() { return offsetof(OutlineTypedObject, data_); }

  static constexpr size_t offsetOfArrayLength() { return 0; }
  using ArrayLength = uint32_t;

  uint8_t* outOfLineTypedMem() const { return data_; }

  ArrayLength arrayLength() const {
    return *(ArrayLength*)(data_ + offsetOfArrayLength());
  }

  static gc::AllocKind allocKind();

  static void obj_trace(JSTracer* trc, JSObject* object);
  static void obj_finalize(JSFreeOp* fop, JSObject* object);
};

// Helper to mark all locations that assume the type of the array length header
// for a typed object.
#define STATIC_ASSERT_ARRAYLENGTH_IS_U32 \
  static_assert(1, "ArrayLength is uint32_t")

// Class for a typed object whose data is allocated inline.
class InlineTypedObject : public TypedObject {
  friend class TypedObject;

  // Start of the inline data, which immediately follows the shape and type.
  uint8_t data_[1];

 public:
  static const JSClass class_;

  static const size_t MaxInlineBytes =
      JSObject::MAX_BYTE_SIZE - sizeof(TypedObject);

 protected:
  uint8_t* inlineTypedMem() const { return (uint8_t*)&data_; }

 public:
  static inline gc::AllocKind allocKindForRttValue(RttValue* rtt);

  static bool canAccommodateType(HandleRttValue rtt) {
    return rtt->kind() == wasm::TypeDefKind::Struct &&
           rtt->typeDef().structType().size_ <= MaxInlineBytes;
  }

  static bool canAccommodateSize(size_t size) { return size <= MaxInlineBytes; }

  uint8_t* inlineTypedMem(const JS::AutoRequireNoGC&) const {
    return inlineTypedMem();
  }

  static void obj_trace(JSTracer* trc, JSObject* object);
  static size_t obj_moved(JSObject* dst, JSObject* src);

  static size_t offsetOfDataStart() {
    return offsetof(InlineTypedObject, data_);
  }

  static InlineTypedObject* createStruct(
      JSContext* cx, HandleRttValue rtt,
      gc::InitialHeap heap = gc::DefaultHeap);
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
