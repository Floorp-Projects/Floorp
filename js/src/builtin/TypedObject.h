/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_TypedObject_h
#define builtin_TypedObject_h

#include "jsobj.h"

#include "builtin/TypedObjectConstants.h"
#include "builtin/TypeRepresentation.h"

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
 * and define all other values (in `js_InitTypedObjectClass()`).
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
 * - Typed datums, objects, and handles:
 *
 * A typed object is an instance of a type object. A handle is a
 * relocatable pointer that points into other typed objects. Both of them
 * are basically represented the same way, though they have distinct
 * js::Class entries. They are both subtypes of `TypedDatum`.
 *
 * Both typed objects and handles are non-native objects that fully
 * override the property accessors etc. The overridden accessor
 * methods are the same in each and are defined in methods of
 * TypedDatum.
 *
 * Typed datums may be attached or unattached. An unattached typed
 * datum has no memory associated with it; it is basically a null
 * pointer.  This can only happen when a new handle is created, since
 * typed object instances are always associated with memory at the
 * point of creation.
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
 * This object exists in order to encapsulate the typed object types
 * somewhat, rather than sticking them all into the global object.
 * Eventually it will go away and become a module.
 */
extern const Class TypedObjectClass;

template <ScalarTypeRepresentation::Type type, typename T>
class NumericType
{
  private:
    static const Class * typeToClass();
  public:
    static bool reify(JSContext *cx, void *mem, MutableHandleValue vp);
    static bool call(JSContext *cx, unsigned argc, Value *vp);
};

/*
 * These are the classes of the scalar type descriptors, like `uint8`,
 * `uint16` etc. Each of these classes has exactly one instance that
 * is pre-created.
 */
extern const Class NumericTypeClasses[ScalarTypeRepresentation::TYPE_MAX];

/*
 * Type descriptor created by `new ArrayType(...)`
 */
class ArrayType : public JSObject
{
  private:
  public:
    static const Class class_;

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

    static JSObject *create(JSContext *cx, HandleObject arrayTypeGlobal,
                            HandleObject elementType, size_t length);
    static bool repeat(JSContext *cx, unsigned argc, Value *vp);
    static bool subarray(JSContext *cx, unsigned argc, Value *vp);

    static bool toSource(JSContext *cx, unsigned argc, Value *vp);

    static JSObject *elementType(JSContext *cx, HandleObject obj);
};

/*
 * Type descriptor created by `new StructType(...)`
 */
class StructType : public JSObject
{
  private:
    static JSObject *create(JSContext *cx, HandleObject structTypeGlobal,
                            HandleObject fields);

    /*
     * Sets up structType slots based on calculated memory size
     * and alignment and stores fieldmap as well.
     */
    static bool layout(JSContext *cx, HandleObject structType,
                       HandleObject fields);

  public:
    static const Class class_;

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

    static bool toSource(JSContext *cx, unsigned argc, Value *vp);

    static bool convertAndCopyTo(JSContext *cx,
                                 StructTypeRepresentation *typeRepr,
                                 HandleValue from, uint8_t *mem);
};

/*
 * Base type for typed objects and handles. Basically any type whose
 * contents consist of typed memory.
 */
class TypedDatum : public JSObject
{
  private:
    static const bool IsTypedDatumClass = true;

  protected:
    static void obj_finalize(js::FreeOp *op, JSObject *obj);

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

    static bool obj_lookupSpecial(JSContext *cx, HandleObject obj,
                                  HandleSpecialId sid,
                                  MutableHandleObject objp,
                                  MutableHandleShape propp);

    static bool obj_defineGeneric(JSContext *cx, HandleObject obj, HandleId id, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineProperty(JSContext *cx, HandleObject obj,
                                   HandlePropertyName name, HandleValue v,
                                   PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineElement(JSContext *cx, HandleObject obj, uint32_t index, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_defineSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, HandleValue v,
                                  PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

    static bool obj_getGeneric(JSContext *cx, HandleObject obj, HandleObject receiver,
                               HandleId id, MutableHandleValue vp);

    static bool obj_getProperty(JSContext *cx, HandleObject obj, HandleObject receiver,
                                HandlePropertyName name, MutableHandleValue vp);

    static bool obj_getElement(JSContext *cx, HandleObject obj, HandleObject receiver,
                               uint32_t index, MutableHandleValue vp);

    static bool obj_getSpecial(JSContext *cx, HandleObject obj, HandleObject receiver,
                               HandleSpecialId sid, MutableHandleValue vp);

    static bool obj_getElementIfPresent(JSContext *cx, HandleObject obj,
                                        HandleObject receiver, uint32_t index,
                                        MutableHandleValue vp, bool *present);
    static bool obj_setGeneric(JSContext *cx, HandleObject obj, HandleId id,
                               MutableHandleValue vp, bool strict);
    static bool obj_setProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                MutableHandleValue vp, bool strict);
    static bool obj_setElement(JSContext *cx, HandleObject obj, uint32_t index,
                               MutableHandleValue vp, bool strict);
    static bool obj_setSpecial(JSContext *cx, HandleObject obj,
                               HandleSpecialId sid, MutableHandleValue vp, bool strict);

    static bool obj_getGenericAttributes(JSContext *cx, HandleObject obj,
                                         HandleId id, unsigned *attrsp);
    static bool obj_setGenericAttributes(JSContext *cx, HandleObject obj,
                                         HandleId id, unsigned *attrsp);

    static bool obj_deleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                                   bool *succeeded);
    static bool obj_deleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                                  bool *succeeded);
    static bool obj_deleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                                  bool *succeeded);

    static bool obj_enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                              MutableHandleValue statep, MutableHandleId idp);

  public:
    // Returns the offset in bytes within the object where the `void*`
    // pointer can be found.
    static size_t dataOffset();

    static TypedDatum *createUnattachedWithClass(JSContext *cx,
                                                 const Class *clasp,
                                                 HandleObject type);

    // Creates an unattached typed object or handle (depending on the
    // type parameter T). Note that it is only legal for unattached
    // handles to escape to the end user; for non-handles, the caller
    // should always invoke one of the `attach()` methods below.
    //
    // Arguments:
    // - type: type object for resulting object
    template<class T>
    static T *createUnattached(JSContext *cx, HandleObject type);

    // Creates a datum that aliases the memory pointed at by `owner`
    // at the given offset. The datum will be a handle iff type is a
    // handle and a typed object otherwise.
    static TypedDatum *createDerived(JSContext *cx,
                                     HandleObject type,
                                     HandleObject typedContents,
                                     size_t offset);

    // If `this` is the owner of the memory, use this.
    void attach(void *mem);

    // Otherwise, use this to attach to memory referenced by another datum.
    void attach(JSObject &datum, uint32_t offset);
};

class TypedObject : public TypedDatum
{
  public:
    static const Class class_;

    // creates zeroed memory of size of type
    static JSObject *createZeroed(JSContext *cx, HandleObject type);

    // user-accessible constructor (`new TypeDescriptor(...)`)
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
};

class TypedHandle : public TypedDatum
{
  public:
    static const Class class_;
    static const JSFunctionSpec handleStaticMethods[];
};

/*
 * Usage: NewTypedHandle(typeObj)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewTypedHandle(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: NewDerivedTypedDatum(typeObj, owner, offset)
 *
 * Constructs a new, unattached instance of `Handle`.
 */
bool NewDerivedTypedDatum(JSContext *cx, unsigned argc, Value *vp);

/*
 * Usage: AttachHandle(handle, newOwner, newOffset)
 *
 * Moves `handle` to point at the memory owned by `newOwner` with
 * the offset `newOffset`.
 */
bool AttachHandle(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo AttachHandleJitInfo;

/*
 * Usage: ObjectIsTypeObject(obj)
 *
 * True if `obj` is a type object.
 */
bool ObjectIsTypeObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypeObjectJitInfo;

/*
 * Usage: ObjectIsTypeRepresentation(obj)
 *
 * True if `obj` is a type representation object.
 */
bool ObjectIsTypeRepresentation(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypeRepresentationJitInfo;

/*
 * Usage: ObjectIsTypedHandle(obj)
 *
 * True if `obj` is a handle.
 */
bool ObjectIsTypedHandle(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypedHandleJitInfo;

/*
 * Usage: ObjectIsTypedObject(obj)
 *
 * True if `obj` is a typed object.
 */
bool ObjectIsTypedObject(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ObjectIsTypedObjectJitInfo;

/*
 * Usage: IsAttached(obj)
 *
 * Given a TypedDatum `obj`, returns true if `obj` is
 * "attached" (i.e., its data pointer is NULL).
 */
bool IsAttached(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo IsAttachedJitInfo;

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
 * Usage: StoreScalar(targetDatum, targetOffset, value)
 *
 * Intrinsic function. Stores value (which must be an int32 or uint32)
 * by `scalarTypeRepr` (which must be a type repr obj) and stores the
 * value at the memory for `targetDatum` at offset `targetOffset`.
 * `targetDatum` must be attached.
 */
#define JS_STORE_SCALAR_CLASS_DEFN(_constant, T, _name)                       \
class StoreScalar##T {                                                        \
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

// I was using templates for this stuff instead of macros, but ran
// into problems with the Unagi compiler.
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_STORE_SCALAR_CLASS_DEFN)
JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(JS_LOAD_SCALAR_CLASS_DEFN)

} // namespace js

#endif /* builtin_TypedObject_h */

