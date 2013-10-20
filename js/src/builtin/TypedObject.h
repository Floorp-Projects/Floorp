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
    /**
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

/* Binary data objects and handles */
class BinaryBlock
{
  private:
    // Creates a binary data object of the given type and class, but with
    // a nullptr memory pointer. Caller must use setPrivate() to set the
    // memory pointer properly.
    static JSObject *createNull(JSContext *cx, HandleObject type,
                                HandleValue owner);

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
    static const Class class_;

    // Returns the offset in bytes within the object where the `void*`
    // pointer can be found.
    static size_t dataOffset();

    static bool isBlock(HandleObject val);
    static uint8_t *mem(HandleObject val);

    // creates zeroed memory of size of type
    static JSObject *createZeroed(JSContext *cx, HandleObject type);

    // creates a block that aliases the memory owned by `owner` at the
    // given offset
    static JSObject *createDerived(JSContext *cx, HandleObject type,
                                   HandleObject owner, size_t offset);

    // user-accessible constructor (`new TypeDescriptor(...)`)
    static bool construct(JSContext *cx, unsigned argc, Value *vp);
};


// Usage: ClampToUint8(v)
//
// Same as the C function ClampDoubleToUint8. `v` must be a number.
bool ClampToUint8(ThreadSafeContext *cx, unsigned argc, Value *vp);
extern const JSJitInfo ClampToUint8JitInfo;

// Usage: Memcpy(targetTypedObj, targetOffset,
//               sourceTypedObj, sourceOffset,
//               size)
//
// Intrinsic function. Copies size bytes from the data for
// `sourceTypedObj` at `sourceOffset` into the data for
// `targetTypedObj` at `targetOffset`.
bool Memcpy(ThreadSafeContext *cx, unsigned argc, Value *vp);

extern const JSJitInfo MemcpyJitInfo;

// Usage: StoreScalar(targetTypedObj, targetOffset, value)
//
// Intrinsic function. Stores value (which must be an int32 or uint32)
// by `scalarTypeRepr` (which must be a type repr obj) and stores the
// value at the memory for `targetTypedObj` at offset `targetOffset`.
#define JS_STORE_SCALAR_CLASS_DEFN(_constant, T, _name)                       \
class StoreScalar##T {                                                        \
  public:                                                                     \
    static bool Func(ThreadSafeContext *cx, unsigned argc, Value *vp);        \
    static const JSJitInfo JitInfo;                                           \
};

// Usage: LoadScalar(targetTypedObj, targetOffset, value)
//
// Intrinsic function. Loads value (which must be an int32 or uint32)
// by `scalarTypeRepr` (which must be a type repr obj) and loads the
// value at the memory for `targetTypedObj` at offset `targetOffset`.
// `targetTypedObj` must be attached.
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

