/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GlobalObject_h___
#define GlobalObject_h___

#include "mozilla/DebugOnly.h"

#include "jsarray.h"
#include "jsbool.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsnum.h"

#include "builtin/RegExp.h"
#include "js/Vector.h"

extern JSObject *
js_InitObjectClass(JSContext *cx, js::HandleObject obj);

extern JSObject *
js_InitFunctionClass(JSContext *cx, js::HandleObject obj);

extern JSObject *
js_InitTypedArrayClasses(JSContext *cx, js::HandleObject obj);

namespace js {

class Debugger;

/*
 * Global object slots are reserved as follows:
 *
 * [0, JSProto_LIMIT)
 *   Stores the original value of the constructor for the corresponding
 *   JSProtoKey.
 * [JSProto_LIMIT, 2 * JSProto_LIMIT)
 *   Stores the prototype, if any, for the constructor for the corresponding
 *   JSProtoKey offset from JSProto_LIMIT.
 * [2 * JSProto_LIMIT, 3 * JSProto_LIMIT)
 *   Stores the current value of the global property named for the JSProtoKey
 *   for the corresponding JSProtoKey offset from 2 * JSProto_LIMIT.
 * [3 * JSProto_LIMIT, RESERVED_SLOTS)
 *   Various one-off values: ES5 13.2.3's [[ThrowTypeError]], RegExp statics,
 *   the original eval for this global object (implementing |var eval =
 *   otherWindow.eval; eval(...)| as an indirect eval), a bit indicating
 *   whether this object has been cleared (see JS_ClearScope), and a cache for
 *   whether eval is allowed (per the global's Content Security Policy).
 *
 * The first two ranges are necessary to implement js::FindClassObject,
 * FindClassPrototype, and spec language speaking in terms of "the original
 * Array prototype object", or "as if by the expression new Array()" referring
 * to the original Array constructor.  The third range stores the (writable and
 * even deletable) Object, Array, &c. properties (although a slot won't be used
 * again if its property is deleted and readded).
 */
class GlobalObject : public JSObject
{
    /*
     * Count of slots to store built-in constructors, prototypes, and initial
     * visible properties for the constructors.
     */
    static const unsigned STANDARD_CLASS_SLOTS  = JSProto_LIMIT * 3;

    /* Various function values needed by the engine. */
    static const unsigned EVAL                    = STANDARD_CLASS_SLOTS;
    static const unsigned CREATE_DATAVIEW_FOR_THIS = EVAL + 1;
    static const unsigned THROWTYPEERROR          = CREATE_DATAVIEW_FOR_THIS + 1;
    static const unsigned PROTO_GETTER            = THROWTYPEERROR + 1;

    /*
     * Instances of the internal createArrayFromBuffer function used by the
     * typed array code, one per typed array element type.
     */
    static const unsigned FROM_BUFFER_UINT8 = PROTO_GETTER + 1;
    static const unsigned FROM_BUFFER_INT8 = FROM_BUFFER_UINT8 + 1;
    static const unsigned FROM_BUFFER_UINT16 = FROM_BUFFER_INT8 + 1;
    static const unsigned FROM_BUFFER_INT16 = FROM_BUFFER_UINT16 + 1;
    static const unsigned FROM_BUFFER_UINT32 = FROM_BUFFER_INT16 + 1;
    static const unsigned FROM_BUFFER_INT32 = FROM_BUFFER_UINT32 + 1;
    static const unsigned FROM_BUFFER_FLOAT32 = FROM_BUFFER_INT32 + 1;
    static const unsigned FROM_BUFFER_FLOAT64 = FROM_BUFFER_FLOAT32 + 1;
    static const unsigned FROM_BUFFER_UINT8CLAMPED = FROM_BUFFER_FLOAT64 + 1;

    /* One-off properties stored after slots for built-ins. */
    static const unsigned ELEMENT_ITERATOR_PROTO  = FROM_BUFFER_UINT8CLAMPED + 1;
    static const unsigned GENERATOR_PROTO         = ELEMENT_ITERATOR_PROTO + 1;
    static const unsigned MAP_ITERATOR_PROTO      = GENERATOR_PROTO + 1;
    static const unsigned SET_ITERATOR_PROTO      = MAP_ITERATOR_PROTO + 1;
    static const unsigned COLLATOR_PROTO          = SET_ITERATOR_PROTO + 1;
    static const unsigned NUMBER_FORMAT_PROTO     = COLLATOR_PROTO + 1;
    static const unsigned DATE_TIME_FORMAT_PROTO  = NUMBER_FORMAT_PROTO + 1;
    static const unsigned REGEXP_STATICS          = DATE_TIME_FORMAT_PROTO + 1;
    static const unsigned FUNCTION_NS             = REGEXP_STATICS + 1;
    static const unsigned RUNTIME_CODEGEN_ENABLED = FUNCTION_NS + 1;
    static const unsigned DEBUGGERS               = RUNTIME_CODEGEN_ENABLED + 1;
    static const unsigned INTRINSICS              = DEBUGGERS + 1;

    /* Total reserved-slot count for global objects. */
    static const unsigned RESERVED_SLOTS = INTRINSICS + 1;

    void staticAsserts() {
        /*
         * The slot count must be in the public API for JSCLASS_GLOBAL_FLAGS,
         * and we aren't going to expose GlobalObject, so just assert that the
         * two values are synchronized.
         */
        JS_STATIC_ASSERT(JSCLASS_GLOBAL_SLOT_COUNT == RESERVED_SLOTS);
    }

    friend JSObject *
    ::js_InitObjectClass(JSContext *cx, js::HandleObject);
    friend JSObject *
    ::js_InitFunctionClass(JSContext *cx, js::HandleObject);

    /* Initialize the Function and Object classes.  Must only be called once! */
    JSObject *
    initFunctionAndObjectClasses(JSContext *cx);

    inline void setDetailsForKey(JSProtoKey key, JSObject *ctor, JSObject *proto);
    inline void setObjectClassDetails(JSFunction *ctor, JSObject *proto);
    inline void setFunctionClassDetails(JSFunction *ctor, JSObject *proto);

    inline void setThrowTypeError(JSFunction *fun);
    inline void setOriginalEval(JSObject *evalobj);
    inline void setProtoGetter(JSFunction *protoGetter);

    inline void setIntrinsicsHolder(JSObject *obj);

    Value getConstructor(JSProtoKey key) const {
        JS_ASSERT(key <= JSProto_LIMIT);
        return getSlot(key);
    }

    Value getPrototype(JSProtoKey key) const {
        JS_ASSERT(key <= JSProto_LIMIT);
        return getSlot(JSProto_LIMIT + key);
    }

    bool classIsInitialized(JSProtoKey key) const {
        bool inited = !getConstructor(key).isUndefined();
        JS_ASSERT(inited == !getPrototype(key).isUndefined());
        return inited;
    }

    bool functionObjectClassesInitialized() const {
        bool inited = classIsInitialized(JSProto_Function);
        JS_ASSERT(inited == classIsInitialized(JSProto_Object));
        return inited;
    }

    bool arrayClassInitialized() const {
        return classIsInitialized(JSProto_Array);
    }

    bool booleanClassInitialized() const {
        return classIsInitialized(JSProto_Boolean);
    }
    bool numberClassInitialized() const {
        return classIsInitialized(JSProto_Number);
    }
    bool stringClassInitialized() const {
        return classIsInitialized(JSProto_String);
    }
    bool regexpClassInitialized() const {
        return classIsInitialized(JSProto_RegExp);
    }
    bool arrayBufferClassInitialized() const {
        return classIsInitialized(JSProto_ArrayBuffer);
    }
    bool errorClassesInitialized() const {
        return classIsInitialized(JSProto_Error);
    }
    bool dataViewClassInitialized() const {
        return classIsInitialized(JSProto_DataView);
    }
    bool typedArrayClassesInitialized() const {
        // This alias exists only for clarity: in reality all the typed array
        // classes constitute a (semi-)coherent whole.
        return classIsInitialized(JSProto_DataView);
    }

    Value createArrayFromBufferHelper(uint32_t slot) const {
        JS_ASSERT(typedArrayClassesInitialized());
        JS_ASSERT(FROM_BUFFER_UINT8 <= slot && slot <= FROM_BUFFER_UINT8CLAMPED);
        return getSlot(slot);
    }

    inline void setCreateArrayFromBufferHelper(uint32_t slot, Handle<JSFunction*> fun);

  public:
    /* XXX Privatize me! */
    inline void setCreateDataViewForThis(Handle<JSFunction*> fun);

    template<typename T>
    inline void setCreateArrayFromBuffer(Handle<JSFunction*> fun);

  public:
    static GlobalObject *create(JSContext *cx, Class *clasp);

    /*
     * Create a constructor function with the specified name and length using
     * ctor, a method which creates objects with the given class.
     */
    JSFunction *
    createConstructor(JSContext *cx, JSNative ctor, JSAtom *name, unsigned length,
                      gc::AllocKind kind = JSFunction::FinalizeKind);

    /*
     * Create an object to serve as [[Prototype]] for instances of the given
     * class, using |Object.prototype| as its [[Prototype]].  Users creating
     * prototype objects with particular internal structure (e.g. reserved
     * slots guaranteed to contain values of particular types) must immediately
     * complete the minimal initialization to make the returned object safe to
     * touch.
     */
    JSObject *createBlankPrototype(JSContext *cx, js::Class *clasp);

    /*
     * Identical to createBlankPrototype, but uses proto as the [[Prototype]]
     * of the returned blank prototype.
     */
    JSObject *createBlankPrototypeInheriting(JSContext *cx, js::Class *clasp, JSObject &proto);

    JSObject *getOrCreateObjectPrototype(JSContext *cx) {
        if (functionObjectClassesInitialized())
            return &getPrototype(JSProto_Object).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!initFunctionAndObjectClasses(cx))
            return NULL;
        return &self->getPrototype(JSProto_Object).toObject();
    }

    JSObject *getOrCreateFunctionPrototype(JSContext *cx) {
        if (functionObjectClassesInitialized())
            return &getPrototype(JSProto_Function).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!initFunctionAndObjectClasses(cx))
            return NULL;
        return &self->getPrototype(JSProto_Function).toObject();
    }

    JSObject *getOrCreateArrayPrototype(JSContext *cx) {
        if (arrayClassInitialized())
            return &getPrototype(JSProto_Array).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitArrayClass(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_Array).toObject();
    }

    JSObject *getOrCreateBooleanPrototype(JSContext *cx) {
        if (booleanClassInitialized())
            return &getPrototype(JSProto_Boolean).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitBooleanClass(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_Boolean).toObject();
    }

    JSObject *getOrCreateNumberPrototype(JSContext *cx) {
        if (numberClassInitialized())
            return &getPrototype(JSProto_Number).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitNumberClass(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_Number).toObject();
    }

    JSObject *getOrCreateStringPrototype(JSContext *cx) {
        if (stringClassInitialized())
            return &getPrototype(JSProto_String).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitStringClass(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_String).toObject();
    }

    JSObject *getOrCreateRegExpPrototype(JSContext *cx) {
        if (regexpClassInitialized())
            return &getPrototype(JSProto_RegExp).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitRegExpClass(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_RegExp).toObject();
    }

    JSObject *getOrCreateArrayBufferPrototype(JSContext *cx) {
        if (arrayBufferClassInitialized())
            return &getPrototype(JSProto_ArrayBuffer).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitTypedArrayClasses(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_ArrayBuffer).toObject();
    }

    JSObject *getOrCreateCustomErrorPrototype(JSContext *cx, int exnType) {
        JSProtoKey key = GetExceptionProtoKey(exnType);
        if (errorClassesInitialized())
            return &getPrototype(key).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitExceptionClasses(cx, self))
            return NULL;
        return &self->getPrototype(key).toObject();
    }

    JSObject *getOrCreateIntlObject(JSContext *cx) {
        return getOrCreateObject(cx, JSProto_Intl, initIntlObject);
    }

    JSObject *getOrCreateCollatorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, COLLATOR_PROTO, initCollatorProto);
    }

    JSObject *getOrCreateNumberFormatPrototype(JSContext *cx) {
        return getOrCreateObject(cx, NUMBER_FORMAT_PROTO, initNumberFormatProto);
    }

    JSObject *getOrCreateDateTimeFormatPrototype(JSContext *cx) {
        return getOrCreateObject(cx, DATE_TIME_FORMAT_PROTO, initDateTimeFormatProto);
    }

    JSObject *getIteratorPrototype() {
        return &getPrototype(JSProto_Iterator).toObject();
    }

  private:
    typedef bool (*ObjectInitOp)(JSContext *cx, Handle<GlobalObject*> global);

    JSObject *getOrCreateObject(JSContext *cx, unsigned slot, ObjectInitOp init) {
        Value v = getSlotRef(slot);
        if (v.isObject())
            return &v.toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!init(cx, self))
            return NULL;
        return &self->getSlot(slot).toObject();
    }

  public:
    JSObject *getOrCreateIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, JSProto_LIMIT + JSProto_Iterator, initIteratorClasses);
    }

    JSObject *getOrCreateElementIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, ELEMENT_ITERATOR_PROTO, initIteratorClasses);
    }

    JSObject *getOrCreateGeneratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, GENERATOR_PROTO, initIteratorClasses);
    }

    JSObject *getOrCreateMapIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, MAP_ITERATOR_PROTO, initMapIteratorProto);
    }

    JSObject *getOrCreateSetIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, SET_ITERATOR_PROTO, initSetIteratorProto);
    }

    JSObject *getOrCreateDataViewPrototype(JSContext *cx) {
        if (dataViewClassInitialized())
            return &getPrototype(JSProto_DataView).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitTypedArrayClasses(cx, self))
            return NULL;
        return &self->getPrototype(JSProto_DataView).toObject();
    }

    JSObject *intrinsicsHolder() {
        JS_ASSERT(!getSlotRef(INTRINSICS).isUndefined());
        return &getSlotRef(INTRINSICS).toObject();
    }

    bool getIntrinsicValue(JSContext *cx, HandlePropertyName name, MutableHandleValue value) {
        RootedObject holder(cx, intrinsicsHolder());
        RootedId id(cx, NameToId(name));
        if (HasDataProperty(cx, holder, id, value.address()))
            return true;
        if (!cx->runtime()->cloneSelfHostedValue(cx, name, value))
            return false;
        mozilla::DebugOnly<bool> ok = JS_DefinePropertyById(cx, holder, id, value, NULL, NULL, 0);
        JS_ASSERT(ok);
        return true;
    }

    inline bool setIntrinsicValue(JSContext *cx, PropertyName *name, HandleValue value);

    inline RegExpStatics *getRegExpStatics() const;

    JSObject *getThrowTypeError() const {
        JS_ASSERT(functionObjectClassesInitialized());
        return &getSlot(THROWTYPEERROR).toObject();
    }

    Value createDataViewForThis() const {
        JS_ASSERT(dataViewClassInitialized());
        return getSlot(CREATE_DATAVIEW_FOR_THIS);
    }

    template<typename T>
    inline Value createArrayFromBuffer() const;

    Value protoGetter() const {
        JS_ASSERT(functionObjectClassesInitialized());
        return getSlot(PROTO_GETTER);
    }

    static bool isRuntimeCodeGenEnabled(JSContext *cx, Handle<GlobalObject*> global);

    const Value &getOriginalEval() const {
        JS_ASSERT(getSlot(EVAL).isObject());
        return getSlot(EVAL);
    }

    // Implemented in jsiter.cpp.
    static bool initIteratorClasses(JSContext *cx, Handle<GlobalObject*> global);

    // Implemented in builtin/MapObject.cpp.
    static bool initMapIteratorProto(JSContext *cx, Handle<GlobalObject*> global);
    static bool initSetIteratorProto(JSContext *cx, Handle<GlobalObject*> global);

    // Implemented in Intl.cpp.
    static bool initIntlObject(JSContext *cx, Handle<GlobalObject*> global);
    static bool initCollatorProto(JSContext *cx, Handle<GlobalObject*> global);
    static bool initNumberFormatProto(JSContext *cx, Handle<GlobalObject*> global);
    static bool initDateTimeFormatProto(JSContext *cx, Handle<GlobalObject*> global);

    static bool initStandardClasses(JSContext *cx, Handle<GlobalObject*> global);

    typedef js::Vector<js::Debugger *, 0, js::SystemAllocPolicy> DebuggerVector;

    /*
     * The collection of Debugger objects debugging this global. If this global
     * is not a debuggee, this returns either NULL or an empty vector.
     */
    DebuggerVector *getDebuggers();

    /*
     * The same, but create the empty vector if one does not already
     * exist. Returns NULL only on OOM.
     */
    static DebuggerVector *getOrCreateDebuggers(JSContext *cx, Handle<GlobalObject*> global);

    static bool addDebugger(JSContext *cx, Handle<GlobalObject*> global, Debugger *dbg);
};

/*
 * Define ctor.prototype = proto as non-enumerable, non-configurable, and
 * non-writable; define proto.constructor = ctor as non-enumerable but
 * configurable and writable.
 */
extern bool
LinkConstructorAndPrototype(JSContext *cx, JSObject *ctor, JSObject *proto);

/*
 * Define properties, then functions, on the object, then brand for tracing
 * benefits.
 */
extern bool
DefinePropertiesAndBrand(JSContext *cx, JSObject *obj,
                         const JSPropertySpec *ps, const JSFunctionSpec *fs);

typedef HashSet<GlobalObject *, DefaultHasher<GlobalObject *>, SystemAllocPolicy> GlobalObjectSet;

} // namespace js

inline bool
JSObject::isGlobal() const
{
    return !!(js::GetObjectClass(const_cast<JSObject*>(this))->flags & JSCLASS_IS_GLOBAL);
}

js::GlobalObject &
JSObject::asGlobal()
{
    JS_ASSERT(isGlobal());
    return *static_cast<js::GlobalObject *>(this);
}

#endif /* GlobalObject_h___ */
