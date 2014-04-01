/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypedObject_h
#define builtin_TypedObject_h

#include "jsobj.h"

#include "builtin/TypedObjectConstants.h"
#include "vm/ArrayBufferObject.h"

/*
 * -------------
 * Typed Objects
 * -------------
 *
 * Typed objects are a special kind of JS object where the data is
 * given well-structured form. To use a typed object, users first
 * create *type objects* (no relation to the type objects used in TI)
 * that define the type layout. For example, a statement like:
 *
 *    var PointType = new StructType({x: uint8, y: uint8});
 *
 * would create a type object PointType that is a struct with
 * two fields, each of uint8 type.
 *
 * This comment typically assumes familiary with the API.  For more
 * info on the API itself, see the Harmony wiki page at
 * http://wiki.ecmascript.org/doku.php?id=harmony:typed_objects or the
 * ES6 spec (not finalized at the time of this writing).
 *
 * - Initialization:
 *
 * Currently, all "globals" related to typed objects are packaged
 * within a single "module" object `TypedObject`. This module has its
 * own js::Class and when that class is initialized, we also create
 * and define all other values (in `js_InitTypedObjectModuleClass()`).
 *
 * - Type objects, meta type objects, and type representations:
 *
 * There are a number of pre-defined type objects, one for each
 * scalar type (`uint8` etc). Each of these has its own class_,
 * defined in `DefineNumericClass()`.
 *
 * There are also meta type objects (`ArrayType`, `StructType`).
 * These constructors are not themselves type objects but rather the
 * means for the *user* to construct new typed objects.
 *
 * Each type object is associated with a *type representation* (see
 * TypeRepresentation.h). Type representations are canonical versions
 * of type objects. We attach them to TI type objects and (eventually)
 * use them for shape guards etc. They are purely internal to the
 * engine and are not exposed to end users (though self-hosted code
 * sometimes accesses them).
 *
 * - Typed objects:
 *
 * A typed object is an instance of a *type object* (note the past
 * participle). There is one class for *transparent* typed objects and
 * one for *opaque* typed objects. These classes are equivalent in
 * basically every way, except that requesting the backing buffer of
 * an opaque typed object yields null. We use distinct js::Classes to
 * avoid the need for an extra slot in every typed object.
 *
 * Note that whether a typed object is opaque is not directly
 * connected to its type. That is, opaque types are *always*
 * represented by opaque typed objects, but you may have opaque typed
 * objects for transparent types too. This can occur for two reasons:
 * (1) a transparent type may be embedded within an opaque type or (2)
 * users can choose to convert transparent typed objects into opaque
 * ones to avoid giving access to the buffer itself.
 *
 * Typed objects (no matter their class) are non-native objects that
 * fully override the property accessors etc. The overridden accessor
 * methods are the same in each and are defined in methods of
 * TypedObject.
 *
 * Typed objects may be attached or unattached. An unattached typed
 * object has no memory associated with it; it is basically a null
 * pointer. When first created, objects are always attached, but they
 * can become unattached if their buffer is neutered (note that this
 * implies that typed objects of opaque types can never be unattached).
 *
 * When a new typed object instance is created, fresh memory is
 * allocated and set as that typed object's private field. The object
 * is then considered the *owner* of that memory: when the object is
 * collected, its finalizer will free the memory. The fact that an
 * object `o` owns its memory is indicated by setting its reserved
 * slot JS_TYPEDOBJ_SLOT_OWNER to `o` (a trivial cycle, in other
 * words).
 *
 * Later, *derived* typed objects can be created, typically via an
 * access like `o.f` where `f` is some complex (non-scalar) type, but
 * also explicitly via Handle objects. In those cases, the memory
 * pointer of the derived object is set to alias the owner's memory
 * pointer, and the owner slot for the derived object is set to the
 * owner object, thus ensuring that the owner is not collected while
 * the derived object is alive. We always maintain the invariant that
 * JS_TYPEDOBJ_SLOT_OWNER is the true owner of the memory, meaning
 * that there is a shallow tree. This prevents an access pattern like
 * `a.b.c.d` from keeping all the intermediate objects alive.
 */

namespace js {

/*
 * Helper method for converting a double into other scalar
 * types in the same way that JavaScript would. In particular,
 * simple C casting from double to int32_t gets things wrong
 * for values like 0xF0000000.
 */
template <typename T>
static T ConvertScalar(double d)
{
    if (TypeIsFloatingPoint<T>()) {
        return T(d);
    } else if (TypeIsUnsigned<T>()) {
        uint32_t n = ToUint32(d);
        return T(n);
    } else {
        int32_t n = ToInt32(d);
        return T(n);
    }
}

class TypeDescr : public JSObject
{
  public:
    // This is *intentionally* not defined so as to produce link
    // errors if a is<FooTypeDescr>() etc goes wrong. Otherwise, the
    // default implementation resolves this to a reference to
    // FooTypeDescr::class_ which resolves to
    // JSObject::class_. Debugging the resulting errors leads to much
    // fun and rejoicing.
    static const Class class_;

    enum Kind {
        Scalar = JS_TYPEREPR_SCALAR_KIND,
        Reference = JS_TYPEREPR_REFERENCE_KIND,
        X4 = JS_TYPEREPR_X4_KIND,
        Struct = JS_TYPEREPR_STRUCT_KIND,
        SizedArray = JS_TYPEREPR_SIZED_ARRAY_KIND,
        UnsizedArray = JS_TYPEREPR_UNSIZED_ARRAY_KIND,
    };

    static bool isSized(Kind kind) {
        return kind > JS_TYPEREPR_MAX_UNSIZED_KIND;
    }

    JSAtom &stringRepr() const {
        return getReservedSlot(JS_DESCR_SLOT_STRING_REPR).toString()->asAtom();
    }

    TypeDescr::Kind kind() const {
        return (TypeDescr::Kind) getReservedSlot(JS_DESCR_SLOT_KIND).toInt32();
    }

    bool opaque() const {
        return getReservedSlot(JS_DESCR_SLOT_OPAQUE).toBoolean();
    }

    bool transparent() const {
        return !opaque();
    }

    int32_t alignment() {
        return getReservedSlot(JS_DESCR_SLOT_ALIGNMENT).toInt32();
    }
};

typedef Handle<TypeDescr*> HandleTypeDescr;

class SizedTypeDescr : public TypeDescr
{
  public:
    int32_t size() {
        return getReservedSlot(JS_DESCR_SLOT_SIZE).toInt32();
    }

    void initInstances(const JSRuntime *rt, uint8_t *mem, size_t length);
    void traceInstances(JSTracer *trace, uint8_t *mem, size_t length);
};

typedef Handle<SizedTypeDescr*> HandleSizedTypeDescr;

class SimpleTypeDescr : public SizedTypeDescr
{
};

// Type for scalar type constructors like `uint8`. All such type
// constructors share a common js::Class and JSFunctionSpec. Scalar
// types are non-opaque (their storage is visible unless combined with
// an opaque reference type.)
class ScalarTypeDescr : public SimpleTypeDescr
{
  public:
    // Must match order of JS_FOR_EACH_SCALAR_TYPE_REPR below
    enum Type {
        TYPE_INT8 = JS_SCALARTYPEREPR_INT8,
        TYPE_UINT8 = JS_SCALARTYPEREPR_UINT8,
        TYPE_INT16 = JS_SCALARTYPEREPR_INT16,
        TYPE_UINT16 = JS_SCALARTYPEREPR_UINT16,
        TYPE_INT32 = JS_SCALARTYPEREPR_INT32,
        TYPE_UINT32 = JS_SCALARTYPEREPR_UINT32,
        TYPE_FLOAT32 = JS_SCALARTYPEREPR_FLOAT32,
        TYPE_FLOAT64 = JS_SCALARTYPEREPR_FLOAT64,

        /*
         * Special type that's a uint8_t, but assignments are clamped to 0 .. 255.
         * Treat the raw data type as a uint8_t.
         */
        TYPE_UINT8_CLAMPED = JS_SCALARTYPEREPR_UINT8_CLAMPED,
    };
    static const int32_t TYPE_MAX = TYPE_UINT8_CLAMPED + 1;

    static const TypeDescr::Kind Kind = TypeDescr::Scalar;
    static const bool Opaque = false;
    static int32_t size(Type t);
    static int32_t alignment(Type t);
    static const char *typeName(Type type);

    static const Class class_;
    static const JSFunctionSpec typeObjectMethods[];

    ScalarTypeDescr::Type type() const {
        return (ScalarTypeDescr::Type) getReservedSlot(JS_DESCR_SLOT_TYPE).toInt32();
    }

    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

// Enumerates the cases of ScalarTypeDescr::Type which have
// unique C representation. In particular, omits Uint8Clamped since it
// is just a Uint8.
#define JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(macro_)                     \
    macro_(ScalarTypeDescr::TYPE_INT8,    int8_t,   int8)            \
    macro_(ScalarTypeDescr::TYPE_UINT8,   uint8_t,  uint8)           \
    macro_(ScalarTypeDescr::TYPE_INT16,   int16_t,  int16)           \
    macro_(ScalarTypeDescr::TYPE_UINT16,  uint16_t, uint16)          \
    macro_(ScalarTypeDescr::TYPE_INT32,   int32_t,  int32)           \
    macro_(ScalarTypeDescr::TYPE_UINT32,  uint32_t, uint32)          \
    macro_(ScalarTypeDescr::TYPE_FLOAT32, float,    float32)         \
    macro_(ScalarTypeDescr::TYPE_FLOAT64, double,   float64)

// Must be in same order as the enum ScalarTypeDescr::Type:
#define JS_FOR_EACH_SCALAR_TYPE_REPR(macro_)                                    \
    JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(macro_)                           \
    macro_(ScalarTypeDescr::TYPE_UINT8_CLAMPED, uint8_t, uint8Clamped)

// Type for reference type constructors like `Any`, `String`, and
// `Object`. All such type constructors share a common js::Class and
// JSFunctionSpec. All these types are opaque.
class ReferenceTypeDescr : public SimpleTypeDescr
{
  public:
    // Must match order of JS_FOR_EACH_REFERENCE_TYPE_REPR below
    enum Type {
        TYPE_ANY = JS_REFERENCETYPEREPR_ANY,
        TYPE_OBJECT = JS_REFERENCETYPEREPR_OBJECT,
        TYPE_STRING = JS_REFERENCETYPEREPR_STRING,
    };
    static const int32_t TYPE_MAX = TYPE_STRING + 1;
    static const char *typeName(Type type);

    static const TypeDescr::Kind Kind = TypeDescr::Reference;
    static const bool Opaque = true;
    static const Class class_;
    static int32_t size(Type t);
    static int32_t alignment(Type t);
    static const JSFunctionSpec typeObjectMethods[];

    ReferenceTypeDescr::Type type() const {
        return (ReferenceTypeDescr::Type) getReservedSlot(JS_DESCR_SLOT_TYPE).toInt32();
    }

    const char *typeName() const {
        return typeName(type());
    }

    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

#define JS_FOR_EACH_REFERENCE_TYPE_REPR(macro_)                    \
    macro_(ReferenceTypeDescr::TYPE_ANY,    HeapValue, Any)        \
    macro_(ReferenceTypeDescr::TYPE_OBJECT, HeapPtrObject, Object) \
    macro_(ReferenceTypeDescr::TYPE_STRING, HeapPtrString, string)

// Type descriptors whose instances are objects and hence which have
// an associated `prototype` property.
class ComplexTypeDescr : public SizedTypeDescr
{
  public:
    // Returns the prototype that instances of this type descriptor
    // will have.
    JSObject &instancePrototype() const {
        return getReservedSlot(JS_DESCR_SLOT_PROTO).toObject();
    }
};

/*
 * Type descriptors `float32x4` and `int32x4`
 */
class X4TypeDescr : public ComplexTypeDescr
{
  public:
    enum Type {
        TYPE_INT32 = JS_X4TYPEREPR_INT32,
        TYPE_FLOAT32 = JS_X4TYPEREPR_FLOAT32,
    };

    static const TypeDescr::Kind Kind = TypeDescr::X4;
    static const bool Opaque = false;
    static const Class class_;
    static int32_t size(Type t);
    static int32_t alignment(Type t);

    X4TypeDescr::Type type() const {
        return (X4TypeDescr::Type) getReservedSlot(JS_DESCR_SLOT_TYPE).toInt32();
    }

    static bool call(JSContext *cx, unsigned argc, Value *vp);
    static bool is(const Value &v);
};

#define JS_FOR_EACH_X4_TYPE_REPR(macro_)                             \
    macro_(X4TypeDescr::TYPE_INT32, int32_t, int32)                  \
    macro_(X4TypeDescr::TYPE_FLOAT32, float, float32)

bool IsTypedObjectClass(const Class *clasp); // Defined below
bool IsTypedObjectArray(JSObject& obj);

bool CreateUserSizeAndAlignmentProperties(JSContext *cx, HandleTypeDescr obj);

/*
 * Properties and methods of the `ArrayType` meta type object. There
 * is no `class_` field because `ArrayType` is just a native
 * constructor function.
 */
class ArrayMetaTypeDescr : public JSObject
{
  private:
    friend class UnsizedArrayTypeDescr;

    // Helper for creating a new ArrayType object, either sized or unsized.
    // The template parameter `T` should be either `UnsizedArrayTypeDescr`
    // or `SizedArrayTypeDescr`.
    //
    // - `arrayTypePrototype` - prototype for the new object to be created,
    //                          either ArrayType.prototype or
    //                          unsizedArrayType.__proto__ depending on
    //                          whether this is a sized or unsized array
    // - `elementType` - type object for the elements in the array
    // - `stringRepr` - canonical string representation for the array
    // - `size` - length of the array (0 if unsized)
    template<class T>
    static T *create(JSContext *cx,
                     HandleObject arrayTypePrototype,
                     HandleSizedTypeDescr elementType,
                     HandleAtom stringRepr,
                     int32_t size);

  public:
    // Properties and methods to be installed on ArrayType.prototype,
    // and hence inherited by all array type objects:
    static const JSPropertySpec typeObjectProperties[];
    static const JSFunctionSpec typeObjectMethods[];

    // Properties and methods to be installed on ArrayType.prototype.prototype,
    // and hence inherited by all array *typed* objects:
    static const JSPropertySpec typedObjectProperties[];
    static const JSFunctionSpec typedObjectMethods[];

    // This is the function that gets called when the user
    // does `new ArrayType(elem)`. It produces an array type object.
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
};

/*
 * Type descriptor created by `new ArrayType(typeObj)`
 *
 * These have a prototype, and hence *could* be a subclass of
 * `ComplexTypeDescr`, but it would require some reshuffling of the
 * hierarchy, and it's not worth the trouble since they will be going
 * away as part of bug 973238.
 */
class UnsizedArrayTypeDescr : public TypeDescr
{
  public:
    static const Class class_;
    static const TypeDescr::Kind Kind = TypeDescr::UnsizedArray;

    // This is the sized method on unsized array type objects.  It
    // produces a sized variant.
    static bool dimension(JSContext *cx, unsigned int argc, jsval *vp);

    SizedTypeDescr &elementType() {
        return getReservedSlot(JS_DESCR_SLOT_ARRAY_ELEM_TYPE).toObject().as<SizedTypeDescr>();
    }
};

/*
 * Type descriptor created by `unsizedArrayTypeObj.dimension()`
 */
class SizedArrayTypeDescr : public ComplexTypeDescr
{
  public:
    static const Class class_;
    static const TypeDescr::Kind Kind = TypeDescr::SizedArray;

    SizedTypeDescr &elementType() {
        return getReservedSlot(JS_DESCR_SLOT_ARRAY_ELEM_TYPE).toObject().as<SizedTypeDescr>();
    }

    int32_t length() {
        return getReservedSlot(JS_DESCR_SLOT_SIZED_ARRAY_LENGTH).toInt32();
    }
};

/*
 * Properties and methods of the `StructType` meta type object. There
 * is no `class_` field because `StructType` is just a native
 * constructor function.
 */
class StructMetaTypeDescr : public JSObject
{
  private:
    static JSObject *create(JSContext *cx, HandleObject structTypeGlobal,
                            HandleObject fields);

  public:
    // Properties and methods to be installed on StructType.prototype,
    // and hence inherited by all struct type objects:
    static const JSPropertySpec typeObjectProperties[];
    static const JSFunctionSpec typeObjectMethods[];

    // Properties and methods to be installed on StructType.prototype.prototype,
    // and hence inherited by all struct *typed* objects:
    static const JSPropertySpec typedObjectProperties[];
    static const JSFunctionSpec typedObjectMethods[];

    // This is the function that gets called when the user
    // does `new StructType(...)`. It produces a struct type object.
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
};

class StructTypeDescr : public ComplexTypeDescr
{
  public:
    static const Class class_;

    // Returns the number of fields defined in this struct.
    size_t fieldCount();

    // Set `*out` to the index of the field named `id` and returns true,
    // or return false if no such field exists.
    bool fieldIndex(jsid id, size_t *out);

    // Return the name of the field at index `index`.
    JSAtom &fieldName(size_t index);

    // Return the type descr of the field at index `index`.
    SizedTypeDescr &fieldDescr(size_t index);

    // Return the offset of the field at index `index`.
    int32_t fieldOffset(size_t index);
};

typedef Handle<StructTypeDescr*> HandleStructTypeDescr;

/*
 * This object exists in order to encapsulate the typed object types
 * somewhat, rather than sticking them all into the global object.
 * Eventually it will go away and become a module.
 */
class TypedObjectModuleObject : public JSObject {
  public:
    enum Slot {
        ArrayTypePrototype,
        StructTypePrototype,
        SlotCount
    };

    static const Class class_;

    static bool getSuitableClaspAndProto(JSContext *cx,
                                         TypeDescr::Kind kind,
                                         const Class **clasp,
                                         MutableHandleObject proto);
};

/*
 * Base type for typed objects and handles. Basically any type whose
 * contents consist of typed memory.
 */
class TypedObject : public ArrayBufferViewObject
{
  private:
    static const bool IsTypedObjectClass = true;

    template<class T>
    static bool obj_getArrayElement(JSContext *cx,
                                    Handle<TypedObject*> typedObj,
                                    Handle<TypeDescr*> typeDescr,
                                    uint32_t index,
                                    MutableHandleValue vp);

    template<class T>
    static bool obj_setArrayElement(JSContext *cx,
                                    Handle<TypedObject*> typedObj,
                                    Handle<TypeDescr*> typeDescr,
                                    uint32_t index,
                                    MutableHandleValue vp);

  protected:
    static void obj_trace(JSTracer *trace, JSObject *object);

    static bool obj_lookupGeneric(JSContext *cx, HandleObject obj,
                                  HandleId id, MutableHandleObject objp,
                                  MutableHandleShape propp);

    static bool obj_lookupProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name,
                                   MutableHandleObject objp,
                                   MutableHandleShape propp);

    static bool obj_lookupElement(JSContext *cx, HandleObject obj,
                                  uint32_t index, MutableHandleObject objp,
                                  MutableHandleShape propp);

    static bool obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name, HandleValue v,
                                   PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                               HandleId id, MutableHandleValue vp);

    static bool obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                                HandlePropertyName name, MutableHandleValue vp);

    static bool obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                               uint32_t index, MutableHandleValue vp);

    static bool obj_getUnsizedArrayElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                                         uint32_t index, MutableHandleValue vp);

    static bool obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                               MutableHandleValue vp, bool strict);
    static bool obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                MutableHandleValue vp, bool strict);
    static bool obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                               MutableHandleValue vp, bool strict);

    static bool obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                         HandleId id, unsigned *attrsp);
    static bool obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                         HandleId id, unsigned *attrsp);

    static bool obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                   bool *succeeded);
    static bool obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                  bool *succeeded);

    static bool obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                              MutableHandleValue statep, MutableHandleId idp);

  public:
    static size_t ownerOffset();

    // Each typed object contains a void* pointer pointing at the
    // binary data that it represents. (That data may be owned by this
    // object or this object may alias data owned by someone else.)
    // This function returns the offset in bytes within the object
    // where the `void*` pointer can be found. It is intended for use
    // by the JIT.
    static size_t dataOffset();

    // Helper for createUnattached()
    static TypedObject *createUnattachedWithClass(JSContext *cx,
                                                 const Class *clasp,
                                                 HandleTypeDescr type,
                                                 int32_t length);

    // Creates an unattached typed object or handle (depending on the
    // type parameter T). Note that it is only legal for unattached
    // handles to escape to the end user; for non-handles, the caller
    // should always invoke one of the `attach()` methods below.
    //
    // Arguments:
    // - type: type object for resulting object
    // - length: 0 unless this is an array, otherwise the length
    static TypedObject *createUnattached(JSContext *cx, HandleTypeDescr type,
                                        int32_t length);

    // Creates a typedObj that aliases the memory pointed at by `owner`
    // at the given offset. The typedObj will be a handle iff type is a
    // handle and a typed object otherwise.
    static TypedObject *createDerived(JSContext *cx,
                                      HandleSizedTypeDescr type,
                                      Handle<TypedObject*> typedContents,
                                      int32_t offset);

    // Creates a new typed object whose memory is freshly allocated
    // and initialized with zeroes (or, in the case of references, an
    // appropriate default value).
    static TypedObject *createZeroed(JSContext *cx,
                                     HandleTypeDescr typeObj,
                                     int32_t length);

    // User-accessible constructor (`new TypeDescriptor(...)`)
    // used for sized types. Note that the callee here is the *type descriptor*,
    // not the typedObj.
    static bool constructSized(JSContext *cx, unsigned argc, Value *vp);

    // As `constructSized`, but for unsized array types.
    static bool constructUnsized(JSContext *cx, unsigned argc, Value *vp);

    // Use this method when `buffer` is the owner of the memory.
    void attach(ArrayBufferObject &buffer, int32_t offset);

    // Otherwise, use this to attach to memory referenced by another typedObj.
    void attach(TypedObject &typedObj, int32_t offset);

    // Invoked when array buffer is transferred elsewhere
    void neuter(void *newData);

    int32_t offset() const {
        return getReservedSlot(JS_TYPEDOBJ_SLOT_BYTEOFFSET).toInt32();
    }

    ArrayBufferObject &owner() const {
        return getReservedSlot(JS_TYPEDOBJ_SLOT_OWNER).toObject().as<ArrayBufferObject>();
    }

    TypeDescr &typeDescr() const {
        return getReservedSlot(JS_TYPEDOBJ_SLOT_TYPE_DESCR).toObject().as<TypeDescr>();
    }

    uint8_t *typedMem() const {
        return (uint8_t*) getPrivate();
    }

    int32_t byteLength() const {
        return getReservedSlot(JS_TYPEDOBJ_SLOT_BYTELENGTH).toInt32();
    }

    int32_t length() const {
        return getReservedSlot(JS_TYPEDOBJ_SLOT_LENGTH).toInt32();
    }

    int32_t size() const {
        switch (typeDescr().kind()) {
          case TypeDescr::Scalar:
          case TypeDescr::X4:
          case TypeDescr::Reference:
          case TypeDescr::Struct:
          case TypeDescr::SizedArray:
            return typeDescr().as<SizedTypeDescr>().size();

          case TypeDescr::UnsizedArray: {
            SizedTypeDescr &elementType = typeDescr().as<UnsizedArrayTypeDescr>().elementType();
            return elementType.size() * length();
          }
        }
        MOZ_ASSUME_UNREACHABLE("unhandled typerepresentation kind");
    }

    uint8_t *typedMem(size_t offset) const {
        // It seems a bit surprising that one might request an offset
        // == size(), but it can happen when taking the "address of" a
        // 0-sized value. (In other words, we maintain the invariant
        // that `offset + size <= size()` -- this is always checked in
        // the caller's side.)
        JS_ASSERT(offset <= (size_t) size());
        return typedMem() + offset;
    }
};

typedef Handle<TypedObject*> HandleTypedObject;

class TransparentTypedObject : public TypedObject
{
  public:
    static const Class class_;
};

typedef Handle<TransparentTypedObject*> HandleTransparentTypedObject;

class OpaqueTypedObject : public TypedObject
{
  public:
    static const Class class_;
    static const JSFunctionSpec handleStaticMethods[];
};

/*
 * Usage: NewOpaqueTypedObject(typeObj)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewOpaqueTypedObject(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: NewDerivedTypedObject(typeObj, owner, offset)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewDerivedTypedObject(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: AttachTypedObject(typedObj, newDatum, newOffset)
 *
 * Moves `typedObj` to point at the memory referenced by `newDatum` with
 * the offset `newOffset`.
 */
bool AttachTypedObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo AttachTypedObjectJitInfo;

/*
 * Usage: SetTypedObjectOffset(typedObj, offset)
 *
 * Changes the offset for `typedObj` within its buffer to `offset`.
 * `typedObj` must already be attached.
 */
bool SetTypedObjectOffset(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo SetTypedObjectOffsetJitInfo;

/*
 * Usage: ObjectIsTypeDescr(obj)
 *
 * True if `obj` is a type object.
 */
bool ObjectIsTypeDescr(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypeDescrJitInfo;

/*
 * Usage: ObjectIsTypedObject(obj)
 *
 * True if `obj` is a transparent or opaque typed object.
 */
bool ObjectIsTypedObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypedObjectJitInfo;

/*
 * Usage: ObjectIsOpaqueTypedObject(obj)
 *
 * True if `obj` is an opaque typed object.
 */
bool ObjectIsOpaqueTypedObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsOpaqueTypedObjectJitInfo;

/*
 * Usage: ObjectIsTransparentTypedObject(obj)
 *
 * True if `obj` is a transparent typed object.
 */
bool ObjectIsTransparentTypedObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTransparentTypedObjectJitInfo;

/* Predicates on type descriptor objects.  In all cases, 'obj' must be a type descriptor. */

bool TypeDescrIsSimpleType(ThreadSafeContext *, unsigned argc, Value *vp);
extern const JSJitInfo TypeDescrIsSimpleTypeJitInfo;

bool TypeDescrIsArrayType(ThreadSafeContext *, unsigned argc, Value *vp);
extern const JSJitInfo TypeDescrIsArrayTypeJitInfo;

bool TypeDescrIsSizedArrayType(ThreadSafeContext *, unsigned argc, Value *vp);
extern const JSJitInfo TypeDescrIsSizedArrayTypeJitInfo;

bool TypeDescrIsUnsizedArrayType(ThreadSafeContext *, unsigned argc, Value *vp);
extern const JSJitInfo TypeDescrIsUnsizedArrayTypeJitInfo;

/*
 * Usage: TypedObjectIsAttached(obj)
 *
 * Given a TypedObject `obj`, returns true if `obj` is
 * "attached" (i.e., its data pointer is nullptr).
 */
bool TypedObjectIsAttached(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo TypedObjectIsAttachedJitInfo;

/*
 * Usage: ClampToUint8(v)
 *
 * Same as the C function ClampDoubleToUint8. `v` must be a number.
 */
bool ClampToUint8(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ClampToUint8JitInfo;

/*
 * Usage: Memcpy(targetDatum, targetOffset,
 *               sourceDatum, sourceOffset,
 *               size)
 *
 * Intrinsic function. Copies size bytes from the data for
 * `sourceDatum` at `sourceOffset` into the data for
 * `targetDatum` at `targetOffset`.
 *
 * Both `sourceDatum` and `targetDatum` must be attached.
 */
bool Memcpy(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo MemcpyJitInfo;

/*
 * Usage: GetTypedObjectModule()
 *
 * Returns the global "typed object" module, which provides access
 * to the various builtin type descriptors. These are currently
 * exported as immutable properties so it is safe for self-hosted code
 * to access them; eventually this should be linked into the module
 * system.
 */
bool GetTypedObjectModule(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: GetFloat32x4TypeDescr()
 *
 * Returns the float32x4 type object. SIMD pseudo-module must have
 * been initialized for this to be safe.
 */
bool GetFloat32x4TypeDescr(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: GetInt32x4TypeDescr()
 *
 * Returns the int32x4 type object. SIMD pseudo-module must have
 * been initialized for this to be safe.
 */
bool GetInt32x4TypeDescr(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: Store_int8(targetDatum, targetOffset, value)
 *        ...
 *        Store_uint8(targetDatum, targetOffset, value)
 *        ...
 *        Store_float32(targetDatum, targetOffset, value)
 *        Store_float64(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores `value` into the memory referenced by
 * `targetDatum` at the offset `targetOffset`.
 *
 * Assumes (and asserts) that:
 * - `targetDatum` is attached
 * - `targetOffset` is a valid offset within the bounds of `targetDatum`
 * - `value` is a number
 */
#define JS_STORE_SCALAR_CLASS_DEFN(_constant, T, _name)                       \
class StoreScalar##T {                                                        \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

/*
 * Usage: Store_Any(targetDatum, targetOffset, value)
 *        Store_Object(targetDatum, targetOffset, value)
 *        Store_string(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores `value` into the memory referenced by
 * `targetDatum` at the offset `targetOffset`.
 *
 * Assumes (and asserts) that:
 * - `targetDatum` is attached
 * - `targetOffset` is a valid offset within the bounds of `targetDatum`
 * - `value` is an object (`Store_Object`) or string (`Store_string`).
 */
#define JS_STORE_REFERENCE_CLASS_DEFN(_constant, T, _name)                    \
class StoreReference##T {                                                     \
  private:                                                                    \
    static void store(T* heap, const Value &v);                               \
                                                                              \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

/*
 * Usage: LoadScalar(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Loads value (which must be an int32 or uint32)
 * by `scalarTypeRepr` (which must be a type repr obj) and loads the
 * value at the memory for `targetDatum` at offset `targetOffset`.
 * `targetDatum` must be attached.
 */
#define JS_LOAD_SCALAR_CLASS_DEFN(_constant, T, _name)                        \
class LoadScalar##T {                                                         \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

/*
 * Usage: LoadReference(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores value (which must be an int32 or uint32)
 * by `scalarTypeRepr` (which must be a type repr obj) and stores the
 * value at the memory for `targetDatum` at offset `targetOffset`.
 * `targetDatum` must be attached.
 */
#define JS_LOAD_REFERENCE_CLASS_DEFN(_constant, T, _name)                     \
class LoadReference##T {                                                      \
  private:                                                                    \
    static void load(T* heap, MutableHandleValue v);                          \
                                                                              \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

// I was using templates for this stuff instead of macros, but ran
// into problems with the Unagi compiler.
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_STORE_SCALAR_CLASS_DEFN)
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_LOAD_SCALAR_CLASS_DEFN)
JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_STORE_REFERENCE_CLASS_DEFN)
JS_FOR_EACH_REFERENCE_TYPE_REPR(JS_LOAD_REFERENCE_CLASS_DEFN)

inline bool
IsTypedObjectClass(const Class *class_)
{
    return class_ == &TransparentTypedObject::class_ ||
           class_ == &OpaqueTypedObject::class_;
}

inline bool
IsSimpleTypeDescrClass(const Class* clasp)
{
    return clasp == &ScalarTypeDescr::class_ ||
           clasp == &ReferenceTypeDescr::class_;
}

inline bool
IsComplexTypeDescrClass(const Class* clasp)
{
    return clasp == &StructTypeDescr::class_ ||
           clasp == &SizedArrayTypeDescr::class_ ||
           clasp == &X4TypeDescr::class_;
}

inline bool
IsSizedTypeDescrClass(const Class* clasp)
{
    return IsSimpleTypeDescrClass(clasp) ||
           IsComplexTypeDescrClass(clasp);
}

inline bool
IsTypeDescrClass(const Class* clasp)
{
    return IsSizedTypeDescrClass(clasp) ||
           clasp == &UnsizedArrayTypeDescr::class_;
}

} // namespace js

JSObject *
js_InitTypedObjectModuleObject(JSContext *cx, JS::HandleObject obj);

template <>
inline bool
JSObject::is<js::SimpleTypeDescr>() const
{
    return IsSimpleTypeDescrClass(getClass());
}

template <>
inline bool
JSObject::is<js::SizedTypeDescr>() const
{
    return IsSizedTypeDescrClass(getClass());
}

template <>
inline bool
JSObject::is<js::ComplexTypeDescr>() const
{
    return IsComplexTypeDescrClass(getClass());
}

template <>
inline bool
JSObject::is<js::TypeDescr>() const
{
    return IsTypeDescrClass(getClass());
}

template <>
inline bool
JSObject::is<js::TypedObject>() const
{
    return IsTypedObjectClass(getClass());
}

template<>
inline bool
JSObject::is<js::SizedArrayTypeDescr>() const
{
    return getClass() == &js::SizedArrayTypeDescr::class_;
}

template<>
inline bool
JSObject::is<js::UnsizedArrayTypeDescr>() const
{
    return getClass() == &js::UnsizedArrayTypeDescr::class_;
}

#endif /* builtin_TypedObject_h */
