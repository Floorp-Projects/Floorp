/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_TypedObject_h
#define wasm_TypedObject_h

#include "mozilla/CheckedInt.h"

#include "gc/Allocator.h"
#include "vm/ArrayBufferObject.h"
#include "vm/JSObject.h"
#include "wasm/WasmTypes.h"

namespace js {

/* The prototype for typed objects. */
class TypedProto : public NativeObject {
 public:
  static const JSClass class_;
  static TypedProto* create(JSContext* cx);
};

class RttValue : public NativeObject {
 public:
  static const JSClass class_;

  enum Slot {
    Handle = 0,     // Type handle index
    Size = 1,       // Size of type in bytes
    Proto = 2,      // Prototype for instances, if any
    TraceList = 3,  // List of references for use in tracing
    Parent = 4,     // Parent rtt for runtime casting
    // Maximum number of slots
    SlotCount = 5,
  };

  static RttValue* createFromHandle(JSContext* cx, wasm::TypeHandle handle);
  static RttValue* createFromParent(JSContext* cx,
                                    js::Handle<RttValue*> parent);

  wasm::TypeHandle handle() const {
    return wasm::TypeHandle(uint32_t(getReservedSlot(Slot::Handle).toInt32()));
  }

  size_t size() const { return getReservedSlot(Slot::Size).toInt32(); }

  TypedProto& typedProto() const {
    return getReservedSlot(Slot::Proto).toObject().as<TypedProto>();
  }

  // RttValues may contain a list of their references for use during
  // scanning. Typed object trace hooks can use this to call an optimized
  // marking path that doesn't need to dispatch on the tracer kind for each
  // edge. This list is only specified when (a) the descriptor is short enough
  // that it can fit in an InlineTypedObject, and (b) the descriptor contains at
  // least one reference. Otherwise its value is undefined.
  //
  // The list is three consecutive arrays of uint32_t offsets, preceded by a
  // header consisting of the length of each array. The arrays store offsets of
  // string, object/anyref, and value references in the descriptor, in that
  // order.
  // TODO/AnyRef-boxing: once anyref has a more complicated structure, we must
  // revisit this.
  [[nodiscard]] bool hasTraceList() const {
    return !getFixedSlot(Slot::TraceList).isUndefined();
  }
  const uint32_t* traceList() const {
    MOZ_ASSERT(hasTraceList());
    return reinterpret_cast<uint32_t*>(
        getFixedSlot(Slot::TraceList).toPrivate());
  }

  RttValue* parent() const {
    return (RttValue*)getReservedSlot(Slot::Parent).toObjectOrNull();
  }

  const wasm::TypeDef& getType(JSContext* cx) const;

  [[nodiscard]] bool lookupProperty(JSContext* cx, jsid id, uint32_t* offset,
                                    wasm::FieldType* type);
  [[nodiscard]] bool hasProperty(JSContext* cx, jsid id) {
    uint32_t offset;
    wasm::FieldType type;
    return lookupProperty(cx, id, &offset, &type);
  }
  uint32_t propertyCount(JSContext* cx);

  void initInstance(JSContext* cx, uint8_t* mem);
  void traceInstance(JSTracer* trace, uint8_t* mem);

  static void finalize(JSFreeOp* fop, JSObject* obj);
};

using HandleRttValue = Handle<RttValue*>;
using RootedRttValue = Rooted<RttValue*>;

/* Base type for typed objects. */
class TypedObject : public JSObject {
 protected:
  GCPtr<RttValue*> rttValue_;

  static const ObjectOps objectOps_;

  [[nodiscard]] static bool obj_lookupProperty(
      JSContext* cx, HandleObject obj, HandleId id, MutableHandleObject objp,
      MutableHandle<PropertyResult> propp);

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
      MutableHandle<PropertyDescriptor> desc);

  [[nodiscard]] static bool obj_deleteProperty(JSContext* cx, HandleObject obj,
                                               HandleId id,
                                               ObjectOpResult& result);

  bool loadValue(JSContext* cx, size_t offset, wasm::FieldType type,
                 MutableHandleValue vp);

  uint8_t* typedMem() const;
  uint8_t* typedMemBase() const;

 public:
  [[nodiscard]] static bool obj_newEnumerate(JSContext* cx, HandleObject obj,
                                             MutableHandleIdVector properties,
                                             bool enumerableOnly);

  TypedProto& typedProto() const {
    // Typed objects' prototypes can't be modified.
    return staticPrototype()->as<TypedProto>();
  }
  RttValue& rttValue() const {
    MOZ_ASSERT(rttValue_);
    return *rttValue_;
  }

  MOZ_MUST_USE bool isRuntimeSubtype(js::Handle<RttValue*> rtt) const;

  static JS::Result<TypedObject*, JS::OOM> create(JSContext* cx,
                                                  js::gc::AllocKind kind,
                                                  js::gc::InitialHeap heap,
                                                  js::HandleShape shape);

  uint32_t offset() const;
  uint32_t size() const { return rttValue().size(); }
  uint8_t* typedMem(const JS::AutoRequireNoGC&) const { return typedMem(); }
  uint8_t* typedMem(size_t offset, const JS::AutoRequireNoGC& nogc) const {
    // It seems a bit surprising that one might request an offset
    // == size(), but it can happen when taking the "address of" a
    // 0-sized value. (In other words, we maintain the invariant
    // that `offset + size <= size()` -- this is always checked in
    // the caller's side.)
    MOZ_ASSERT(offset <= (size_t)size());
    return typedMem(nogc) + offset;
  }

  // Creates a new typed object whose memory is freshly allocated and
  // initialized with zeroes (or, in the case of references, an appropriate
  // default value).
  static TypedObject* createZeroed(JSContext* cx, HandleRttValue typeObj,
                                   gc::InitialHeap heap = gc::DefaultHeap);

  static constexpr size_t offsetOfRttValue() {
    return offsetof(TypedObject, rttValue_);
  }
};

using HandleTypedObject = Handle<TypedObject*>;
using RootedTypedObject = Rooted<TypedObject*>;

class OutlineTypedObject : public TypedObject {
  // The object which owns the data this object points to. Because this
  // pointer is managed in tandem with |data|, this is not a GCPtr and
  // barriers are managed directly.
  JSObject* owner_;

  // Data pointer to some offset in the owner's contents.
  uint8_t* data_;

  void setOwnerAndData(JSObject* owner, uint8_t* data);

  void setData(uint8_t* data) { data_ = data; }

 public:
  static constexpr gc::AllocKind allocKind = gc::AllocKind::OBJECT2;

  // JIT accessors.
  static size_t offsetOfData() { return offsetof(OutlineTypedObject, data_); }
  static size_t offsetOfOwner() { return offsetof(OutlineTypedObject, owner_); }

  static const JSClass class_;

  JSObject& owner() const {
    MOZ_ASSERT(owner_);
    return *owner_;
  }

  uint8_t* outOfLineTypedMem() const { return data_; }

 private:
  // Creates an unattached typed object or handle (depending on the
  // type parameter T). Note that it is only legal for unattached
  // handles to escape to the end user; for non-handles, the caller
  // should always invoke one of the `attach()` methods below.
  //
  // Arguments:
  // - type: type object for resulting object
  static OutlineTypedObject* createUnattached(
      JSContext* cx, HandleRttValue type,
      gc::InitialHeap heap = gc::DefaultHeap);

 public:
  static OutlineTypedObject* createZeroed(JSContext* cx, HandleRttValue rtt,
                                          gc::InitialHeap heap);

 private:
  // This method should only be used when `buffer` is the owner of the memory.
  void attach(ArrayBufferObject& buffer);

 public:
  static void obj_trace(JSTracer* trace, JSObject* object);
};

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

  static bool canAccommodateSize(size_t size) { return size <= MaxInlineBytes; }

  static bool canAccommodateType(RttValue* type) {
    return type->size() <= MaxInlineBytes;
  }

  uint8_t* inlineTypedMem(const JS::AutoRequireNoGC&) const {
    return inlineTypedMem();
  }

  static void obj_trace(JSTracer* trace, JSObject* object);
  static size_t obj_moved(JSObject* dst, JSObject* src);

  static size_t offsetOfDataStart() {
    return offsetof(InlineTypedObject, data_);
  }

  static InlineTypedObject* create(JSContext* cx, HandleRttValue rtt,
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
