/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef wasm_TypedObject_h
#define wasm_TypedObject_h

#include "mozilla/CheckedInt.h"

#include "gc/Allocator.h"
#include "gc/WeakMap.h"
#include "js/Conversions.h"
#include "js/experimental/JitInfo.h"  // JSJitInfo
#include "js/ScalarType.h"            // js::Scalar::Type
#include "vm/ArrayBufferObject.h"
#include "vm/JSObject.h"
#include "vm/Uint8Clamped.h"

namespace js {

class WasmNamespaceObject;

extern bool InitTypedObjectSlots(JSContext* cx,
                                 Handle<WasmNamespaceObject*> namespaceObject);

/* The prototype for typed objects. */
class TypedProto : public NativeObject {
 public:
  static const JSClass class_;

 protected:
  friend class ArrayMetaTypeDescr;
  friend class StructMetaTypeDescr;

  static TypedProto* create(JSContext* cx);
};

enum class TypeKind : int32_t { Scalar, Reference, Struct, Array };

class TypeDescr : public NativeObject {
 public:
  // Some slots apply to all type objects and some are specific to
  // particular kinds of type objects. For simplicity we use the same
  // number of slots no matter what kind of type descriptor we are
  // working with, even though this is mildly wasteful.
  enum Slot {
    // Slots on all type objects
    Kind = 0,        // Kind of the type descriptor
    StringRepr = 1,  // Atomized string representation
    Alignment = 2,   // Alignment in bytes
    Size = 3,        // Size in bytes, else 0
    Proto = 4,       // Prototype for instances, if any
    TraceList = 5,   // List of references for use in tracing

    // Slots on scalars, references
    Type = 6,  // Type code

    // Slots on array descriptors
    ArrayElemType = 6,
    ArrayLength = 7,

    // Slots on struct type objects
    StructFieldNames = 6,
    StructFieldTypes = 7,
    StructFieldOffsets = 8,
    StructFieldMuts = 9,

    // Maximum number of slots for any descriptor
    SlotCount = 10,
  };

  TypedProto& typedProto() const {
    return getReservedSlot(Slot::Proto).toObject().as<TypedProto>();
  }

  JSAtom& stringRepr() const {
    return getReservedSlot(Slot::StringRepr).toString()->asAtom();
  }

  TypeKind kind() const {
    return (TypeKind)getReservedSlot(Slot::Kind).toInt32();
  }

  uint32_t alignment() const {
    int32_t i = getReservedSlot(Slot::Alignment).toInt32();
    MOZ_ASSERT(i >= 0);
    return uint32_t(i);
  }

  uint32_t size() const {
    int32_t i = getReservedSlot(Slot::Size).toInt32();
    MOZ_ASSERT(i >= 0);
    return uint32_t(i);
  }

  // Whether id is an 'own' property of objects with this descriptor.
  MOZ_MUST_USE bool hasProperty(const JSAtomState& names, jsid id);

  // Type descriptors may contain a list of their references for use during
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
  MOZ_MUST_USE bool hasTraceList() const {
    return !getFixedSlot(Slot::TraceList).isUndefined();
  }
  const uint32_t* traceList() const {
    MOZ_ASSERT(hasTraceList());
    return reinterpret_cast<uint32_t*>(
        getFixedSlot(Slot::TraceList).toPrivate());
  }

  void initInstance(uint8_t* mem);
  void traceInstance(JSTracer* trace, uint8_t* mem);

  static void finalize(JSFreeOp* fop, JSObject* obj);
};

using HandleTypeDescr = Handle<TypeDescr*>;

class SimpleTypeDescr : public TypeDescr {};
// Type descriptors whose instances are objects and hence which have
// an associated `prototype` property.
class ComplexTypeDescr : public TypeDescr {};

// Type for scalar type constructors like `uint8`. All such type
// constructors share a common JSClass and JSFunctionSpec. Scalar
// types are non-opaque (their storage is visible unless combined with
// an opaque reference type.)
class ScalarTypeDescr : public SimpleTypeDescr {
 public:
  using Type = Scalar::Type;

  static const TypeKind Kind = TypeKind::Scalar;
  static uint32_t size(Type t);
  static uint32_t alignment(Type t);
  static const char* typeName(Type type);

  static const JSClass class_;

  Type type() const { return Type(getReservedSlot(Slot::Type).toInt32()); }

  static MOZ_MUST_USE bool call(JSContext* cx, unsigned argc, Value* vp);
};

// Enumerates the cases of ScalarTypeDescr::Type which have unique C
// representation and which are representable as JS Number values.
#define JS_FOR_EACH_SCALAR_NUMBER_TYPE_REPR(MACRO_)                         \
  MACRO_(Scalar::Int32, int32_t, int32, WasmNamespaceObject::Int32Desc)     \
  MACRO_(Scalar::Float32, float, float32, WasmNamespaceObject::Float32Desc) \
  MACRO_(Scalar::Float64, double, float64, WasmNamespaceObject::Float64Desc)

#define JS_FOR_EACH_SCALAR_BIGINT_TYPE_REPR(MACRO_) \
  MACRO_(Scalar::BigInt64, int64_t, bigint64, WasmNamespaceObject::Int64Desc)

// Must be in same order as the enum ScalarTypeDescr::Type:
#define JS_FOR_EACH_SCALAR_TYPE_REPR(MACRO_)  \
  JS_FOR_EACH_SCALAR_NUMBER_TYPE_REPR(MACRO_) \
  JS_FOR_EACH_SCALAR_BIGINT_TYPE_REPR(MACRO_)

enum class ReferenceType {
  TYPE_OBJECT,
  TYPE_WASM_ANYREF,
};

// Type for reference type constructors like `Any`, `String`, and
// `Object`. All such type constructors share a common JSClass and
// JSFunctionSpec. All these types are opaque.
class ReferenceTypeDescr : public SimpleTypeDescr {
 public:
  // Must match order of JS_FOR_EACH_REFERENCE_TYPE_REPR below
  using Type = ReferenceType;
  static const char* typeName(Type type);

  static const int32_t TYPE_MAX = int32_t(ReferenceType::TYPE_WASM_ANYREF) + 1;
  static const TypeKind Kind = TypeKind::Reference;
  static const JSClass class_;
  static uint32_t size(Type t);
  static uint32_t alignment(Type t);

  ReferenceType type() const {
    return (ReferenceType)getReservedSlot(Slot::Type).toInt32();
  }

  const char* typeName() const { return typeName(type()); }

  static MOZ_MUST_USE bool call(JSContext* cx, unsigned argc, Value* vp);
};

// TODO/AnyRef-boxing: With boxed immediates and strings, GCPtrObject may not be
// appropriate.
#define JS_FOR_EACH_REFERENCE_TYPE_REPR(MACRO_)                    \
  MACRO_(ReferenceType::TYPE_OBJECT, GCPtrObject, Object,          \
         WasmNamespaceObject::ObjectDesc)                          \
  MACRO_(ReferenceType::TYPE_WASM_ANYREF, GCPtrObject, WasmAnyRef, \
         WasmNamespaceObject::WasmAnyRefDesc)

class ArrayTypeDescr;

/*
 * Properties and methods of the `ArrayType` meta type object. There
 * is no `class_` field because `ArrayType` is just a native
 * constructor function.
 */
class ArrayMetaTypeDescr : public NativeObject {
 private:
  // Helper for creating a new ArrayType object.
  //
  // - `arrayTypePrototype` - prototype for the new object to be created
  // - `elementType` - type object for the elements in the array
  // - `stringRepr` - canonical string representation for the array
  // - `size` - length of the array
  static ArrayTypeDescr* create(JSContext* cx, HandleObject arrayTypePrototype,
                                HandleTypeDescr elementType,
                                HandleAtom stringRepr, int32_t size,
                                int32_t length);

 public:
  // This is the function that gets called when the user
  // does `new ArrayType(elem)`. It is currently disallowed.
  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);
};

/*
 * Type descriptor created by `new ArrayType(type, n)`
 */
class ArrayTypeDescr : public ComplexTypeDescr {
 public:
  static const JSClass class_;
  static const TypeKind Kind = TypeKind::Array;

  TypeDescr& elementType() const {
    return getReservedSlot(Slot::ArrayElemType).toObject().as<TypeDescr>();
  }

  uint32_t length() const {
    int32_t i = getReservedSlot(Slot::ArrayLength).toInt32();
    MOZ_ASSERT(i >= 0);
    return uint32_t(i);
  }

  static int32_t offsetOfLength() {
    return getFixedSlotOffset(Slot::ArrayLength);
  }
};

struct StructFieldProps {
  StructFieldProps() : isMutable(0), alignAsInt64(0), alignAsV128(0) {}
  uint32_t isMutable : 1;
  uint32_t alignAsInt64 : 1;
  uint32_t alignAsV128 : 1;
};

class StructTypeDescr;

/*
 * Properties and methods of the `StructType` meta type object. There
 * is no `class_` field because `StructType` is just a native
 * constructor function.
 */
class StructMetaTypeDescr : public NativeObject {
 public:
  // The prototype cannot be null.
  // The names in `ids` must all be non-numeric.
  // The type objects in `fieldTypeObjs` must all be TypeDescr objects.
  static StructTypeDescr* createFromArrays(
      JSContext* cx, HandleObject structTypePrototype, HandleIdVector ids,
      HandleValueVector fieldTypeObjs, Vector<StructFieldProps>& fieldProps);

  // This is the function that gets called when the user
  // does `new StructType(...)`. It is currently disallowed.
  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  class Layout {
    // Can call addField() directly.
    friend class StructMetaTypeDescr;

    mozilla::CheckedInt32 sizeSoFar = 0;
    int32_t structAlignment = 1;

    mozilla::CheckedInt32 addField(int32_t fieldAlignment, int32_t fieldSize);

   public:
    // The field adders return the offset of the the field.
    mozilla::CheckedInt32 addScalar(Scalar::Type type);
    mozilla::CheckedInt32 addReference(ReferenceType type);

    // The close method rounds up the structure size to the appropriate
    // alignment and returns that size.  If `alignment` is not NULL then
    // return the structure alignment through that pointer.
    mozilla::CheckedInt32 close(int32_t* alignment = nullptr);
  };
};

class StructTypeDescr : public ComplexTypeDescr {
 public:
  static const JSClass class_;

  // Returns the number of fields defined in this struct.
  size_t fieldCount() const;

  // Set `*out` to the index of the field named `id` and returns true,
  // or return false if no such field exists.
  MOZ_MUST_USE bool fieldIndex(jsid id, size_t* out) const;

  // Return the name of the field at index `index`.
  JSAtom& fieldName(size_t index) const;

  // Return the type descr of the field at index `index`.
  TypeDescr& fieldDescr(size_t index) const;

  // Return the offset of the field at index `index`.
  size_t fieldOffset(size_t index) const;

  // Return the mutability of the field at index `index`.
  bool fieldIsMutable(size_t index) const;

  static bool call(JSContext* cx, unsigned argc, Value* vp);

 private:
  ArrayObject& fieldInfoObject(size_t slot) const {
    return getReservedSlot(slot).toObject().as<ArrayObject>();
  }
};

using HandleStructTypeDescr = Handle<StructTypeDescr*>;

/* Base type for typed objects. */
class TypedObject : public JSObject {
  static MOZ_MUST_USE bool obj_getArrayElement(JSContext* cx,
                                               Handle<TypedObject*> typedObj,
                                               Handle<TypeDescr*> typeDescr,
                                               uint32_t index,
                                               MutableHandleValue vp);

 protected:
  static const ObjectOps objectOps_;

  static MOZ_MUST_USE bool obj_lookupProperty(
      JSContext* cx, HandleObject obj, HandleId id, MutableHandleObject objp,
      MutableHandle<PropertyResult> propp);

  static MOZ_MUST_USE bool obj_defineProperty(JSContext* cx, HandleObject obj,
                                              HandleId id,
                                              Handle<PropertyDescriptor> desc,
                                              ObjectOpResult& result);

  static MOZ_MUST_USE bool obj_hasProperty(JSContext* cx, HandleObject obj,
                                           HandleId id, bool* foundp);

  static MOZ_MUST_USE bool obj_getProperty(JSContext* cx, HandleObject obj,
                                           HandleValue receiver, HandleId id,
                                           MutableHandleValue vp);

  static MOZ_MUST_USE bool obj_getElement(JSContext* cx, HandleObject obj,
                                          HandleValue receiver, uint32_t index,
                                          MutableHandleValue vp);

  static MOZ_MUST_USE bool obj_setProperty(JSContext* cx, HandleObject obj,
                                           HandleId id, HandleValue v,
                                           HandleValue receiver,
                                           ObjectOpResult& result);

  static MOZ_MUST_USE bool obj_getOwnPropertyDescriptor(
      JSContext* cx, HandleObject obj, HandleId id,
      MutableHandle<PropertyDescriptor> desc);

  static MOZ_MUST_USE bool obj_deleteProperty(JSContext* cx, HandleObject obj,
                                              HandleId id,
                                              ObjectOpResult& result);

  bool loadValue(JSContext* cx, size_t offset, HandleTypeDescr type,
                 MutableHandleValue vp);

  uint8_t* typedMem() const;
  uint8_t* typedMemBase() const;

 public:
  static MOZ_MUST_USE bool obj_newEnumerate(JSContext* cx, HandleObject obj,
                                            MutableHandleIdVector properties,
                                            bool enumerableOnly);

  TypedProto& typedProto() const {
    // Typed objects' prototypes can't be modified.
    return staticPrototype()->as<TypedProto>();
  }

  TypeDescr& typeDescr() const { return group()->typeDescr(); }

  static JS::Result<TypedObject*, JS::OOM> create(JSContext* cx,
                                                  js::gc::AllocKind kind,
                                                  js::gc::InitialHeap heap,
                                                  js::HandleShape shape,
                                                  js::HandleObjectGroup group);

  uint32_t offset() const;
  uint32_t length() const;
  uint8_t* typedMem(const JS::AutoRequireNoGC&) const { return typedMem(); }

  uint32_t size() const { return typeDescr().size(); }

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
  static TypedObject* createZeroed(JSContext* cx, HandleTypeDescr typeObj,
                                   gc::InitialHeap heap = gc::DefaultHeap);

  // User-accessible constructor (`new TypeDescriptor(...)`). Note that the
  // callee here is the type descriptor.
  static MOZ_MUST_USE bool construct(JSContext* cx, unsigned argc, Value* vp);

  Shape** addressOfShapeFromGC() { return shape_.unbarrieredAddress(); }
};

using HandleTypedObject = Handle<TypedObject*>;

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
      JSContext* cx, HandleTypeDescr type,
      gc::InitialHeap heap = gc::DefaultHeap);

 public:
  static OutlineTypedObject* createZeroed(JSContext* cx, HandleTypeDescr descr,
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
  static inline gc::AllocKind allocKindForTypeDescriptor(TypeDescr* descr);

  static bool canAccommodateSize(size_t size) { return size <= MaxInlineBytes; }

  static bool canAccommodateType(TypeDescr* type) {
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

  static InlineTypedObject* create(JSContext* cx, HandleTypeDescr descr,
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

inline bool IsSimpleTypeDescrClass(const JSClass* clasp) {
  return clasp == &ScalarTypeDescr::class_ ||
         clasp == &ReferenceTypeDescr::class_;
}

inline bool IsComplexTypeDescrClass(const JSClass* clasp) {
  return clasp == &StructTypeDescr::class_ || clasp == &ArrayTypeDescr::class_;
}

inline bool IsTypeDescrClass(const JSClass* clasp) {
  return IsSimpleTypeDescrClass(clasp) || IsComplexTypeDescrClass(clasp);
}

}  // namespace js

template <>
inline bool JSObject::is<js::SimpleTypeDescr>() const {
  return js::IsSimpleTypeDescrClass(getClass());
}

template <>
inline bool JSObject::is<js::ComplexTypeDescr>() const {
  return js::IsComplexTypeDescrClass(getClass());
}

template <>
inline bool JSObject::is<js::TypeDescr>() const {
  return js::IsTypeDescrClass(getClass());
}

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
