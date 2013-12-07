/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_GlobalObject_h
#define vm_GlobalObject_h

#include "jsarray.h"
#include "jsbool.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsnum.h"

#include "builtin/RegExp.h"
#include "js/Vector.h"
#include "vm/ErrorObject.h"

extern JSObject *
js_InitObjectClass(JSContext *cx, js::HandleObject obj);

extern JSObject *
js_InitFunctionClass(JSContext *cx, js::HandleObject obj);

extern JSObject *
js_InitTypedArrayClasses(JSContext *cx, js::HandleObject obj);

extern JSObject *
js_InitTypedObjectClass(JSContext *cx, js::HandleObject obj);

namespace js {

class Debugger;

/*
 * Global object slots are reserved as follows:
 *
 * [0, APPLICATION_SLOTS)
 *   Pre-reserved slots in all global objects set aside for the embedding's
 *   use. As with all reserved slots these start out as UndefinedValue() and
 *   are traced for GC purposes. Apart from that the engine never touches
 *   these slots, so the embedding can do whatever it wants with them.
 * [APPLICATION_SLOTS, APPLICATION_SLOTS + JSProto_LIMIT)
 *   Stores the original value of the constructor for the corresponding
 *   JSProtoKey.
 * [APPLICATION_SLOTS + JSProto_LIMIT, APPLICATION_SLOTS + 2 * JSProto_LIMIT)
 *   Stores the prototype, if any, for the constructor for the corresponding
 *   JSProtoKey offset from JSProto_LIMIT.
 * [APPLICATION_SLOTS + 2 * JSProto_LIMIT, APPLICATION_SLOTS + 3 * JSProto_LIMIT)
 *   Stores the current value of the global property named for the JSProtoKey
 *   for the corresponding JSProtoKey offset from 2 * JSProto_LIMIT.
 * [APPLICATION_SLOTS + 3 * JSProto_LIMIT, RESERVED_SLOTS)
 *   Various one-off values: ES5 13.2.3's [[ThrowTypeError]], RegExp statics,
 *   the original eval for this global object (implementing |var eval =
 *   otherWindow.eval; eval(...)| as an indirect eval), a bit indicating
 *   whether this object has been cleared (see JS_ClearScope), and a cache for
 *   whether eval is allowed (per the global's Content Security Policy).
 *
 * The first two JSProto_LIMIT-sized ranges are necessary to implement
 * js::FindClassObject, and spec language speaking in terms of "the original
 * Array prototype object", or "as if by the expression new Array()" referring
 * to the original Array constructor. The third range stores the (writable and
 * even deletable) Object, Array, &c. properties (although a slot won't be used
 * again if its property is deleted and readded).
 */
class GlobalObject : public JSObject
{
    /* Count of slots set aside for application use. */
    static const unsigned APPLICATION_SLOTS = 3;

    /*
     * Count of slots to store built-in constructors, prototypes, and initial
     * visible properties for the constructors.
     */
    static const unsigned STANDARD_CLASS_SLOTS  = JSProto_LIMIT * 3;

    /* Various function values needed by the engine. */
    static const unsigned EVAL                    = APPLICATION_SLOTS + STANDARD_CLASS_SLOTS;
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
    static const unsigned ARRAY_ITERATOR_PROTO  = FROM_BUFFER_UINT8CLAMPED + 1;
    static const unsigned STRING_ITERATOR_PROTO  = ARRAY_ITERATOR_PROTO + 1;
    static const unsigned LEGACY_GENERATOR_OBJECT_PROTO = STRING_ITERATOR_PROTO + 1;
    static const unsigned STAR_GENERATOR_OBJECT_PROTO = LEGACY_GENERATOR_OBJECT_PROTO + 1;
    static const unsigned MAP_ITERATOR_PROTO      = STAR_GENERATOR_OBJECT_PROTO + 1;
    static const unsigned SET_ITERATOR_PROTO      = MAP_ITERATOR_PROTO + 1;
    static const unsigned COLLATOR_PROTO          = SET_ITERATOR_PROTO + 1;
    static const unsigned NUMBER_FORMAT_PROTO     = COLLATOR_PROTO + 1;
    static const unsigned DATE_TIME_FORMAT_PROTO  = NUMBER_FORMAT_PROTO + 1;
    static const unsigned REGEXP_STATICS          = DATE_TIME_FORMAT_PROTO + 1;
    static const unsigned WARNED_WATCH_DEPRECATED = REGEXP_STATICS + 1;
    static const unsigned RUNTIME_CODEGEN_ENABLED = WARNED_WATCH_DEPRECATED + 1;
    static const unsigned DEBUGGERS               = RUNTIME_CODEGEN_ENABLED + 1;
    static const unsigned INTRINSICS              = DEBUGGERS + 1;
    static const unsigned ARRAY_TYPE              = INTRINSICS + 1;

    /* Total reserved-slot count for global objects. */
    static const unsigned RESERVED_SLOTS = ARRAY_TYPE + 1;

    /*
     * The slot count must be in the public API for JSCLASS_GLOBAL_FLAGS, and
     * we won't expose GlobalObject, so just assert that the two values are
     * synchronized.
     */
    static_assert(JSCLASS_GLOBAL_SLOT_COUNT == RESERVED_SLOTS,
                  "global object slot counts are inconsistent");

    friend JSObject *
    ::js_InitObjectClass(JSContext *cx, js::HandleObject);
    friend JSObject *
    ::js_InitFunctionClass(JSContext *cx, js::HandleObject);

    /* Initialize the Function and Object classes.  Must only be called once! */
    JSObject *
    initFunctionAndObjectClasses(JSContext *cx);

    void setThrowTypeError(JSFunction *fun) {
        JS_ASSERT(getSlotRef(THROWTYPEERROR).isUndefined());
        setSlot(THROWTYPEERROR, ObjectValue(*fun));
    }

    void setOriginalEval(JSObject *evalobj) {
        JS_ASSERT(getSlotRef(EVAL).isUndefined());
        setSlot(EVAL, ObjectValue(*evalobj));
    }

    void setProtoGetter(JSFunction *protoGetter) {
        JS_ASSERT(getSlotRef(PROTO_GETTER).isUndefined());
        setSlot(PROTO_GETTER, ObjectValue(*protoGetter));
    }

    void setIntrinsicsHolder(JSObject *obj) {
        JS_ASSERT(getSlotRef(INTRINSICS).isUndefined());
        setSlot(INTRINSICS, ObjectValue(*obj));
    }

  public:
    Value getConstructor(JSProtoKey key) const {
        JS_ASSERT(key <= JSProto_LIMIT);
        return getSlotRefForCompilation(APPLICATION_SLOTS + key);
    }

    void setConstructor(JSProtoKey key, const Value &v) {
        JS_ASSERT(key <= JSProto_LIMIT);
        setSlot(APPLICATION_SLOTS + key, v);
    }

    Value getPrototype(JSProtoKey key) const {
        JS_ASSERT(key <= JSProto_LIMIT);
        return getSlotRefForCompilation(APPLICATION_SLOTS + JSProto_LIMIT + key);
    }

    void setPrototype(JSProtoKey key, const Value &value) {
        JS_ASSERT(key <= JSProto_LIMIT);
        setSlot(APPLICATION_SLOTS + JSProto_LIMIT + key, value);
    }

    static uint32_t constructorPropertySlot(JSProtoKey key) {
        JS_ASSERT(key <= JSProto_LIMIT);
        return APPLICATION_SLOTS + JSProto_LIMIT * 2 + key;
    }

    Value getConstructorPropertySlot(JSProtoKey key) {
        return getSlot(constructorPropertySlot(key));
    }

    void setConstructorPropertySlot(JSProtoKey key, const Value &ctor) {
        setSlot(constructorPropertySlot(key), ctor);
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

    /*
     * Lazy standard classes need a way to indicate they have been initialized.
     * Otherwise, when we delete them, we might accidentally recreate them via
     * a lazy initialization. We use the presence of an object in the
     * getConstructor(key) reserved slot to indicate that they've been
     * initialized.
     *
     * Note: A few builtin objects, like JSON and Math, are not constructors,
     * so getConstructor is a bit of a misnomer.
     */
    bool isStandardClassResolved(const js::Class *clasp) const {
        JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);

        // If the constructor is undefined, then it hasn't been initialized.
        MOZ_ASSERT(getConstructor(key).isUndefined() ||
                   getConstructor(key).isObject());
        return !getConstructor(key).isUndefined();
    }

  private:
    void setDetailsForKey(JSProtoKey key, JSObject *ctor, JSObject *proto) {
        JS_ASSERT(getConstructor(key).isUndefined());
        JS_ASSERT(getPrototype(key).isUndefined());
        JS_ASSERT(getConstructorPropertySlot(key).isUndefined());
        setConstructor(key, ObjectValue(*ctor));
        setPrototype(key, ObjectValue(*proto));
        setConstructorPropertySlot(key, ObjectValue(*ctor));
    }

    void setObjectClassDetails(JSFunction *ctor, JSObject *proto) {
        setDetailsForKey(JSProto_Object, ctor, proto);
    }

    void setFunctionClassDetails(JSFunction *ctor, JSObject *proto) {
        setDetailsForKey(JSProto_Function, ctor, proto);
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

    void setCreateArrayFromBufferHelper(uint32_t slot, Handle<JSFunction*> fun) {
        JS_ASSERT(getSlotRef(slot).isUndefined());
        setSlot(slot, ObjectValue(*fun));
    }

  public:
    /* XXX Privatize me! */
    void setCreateDataViewForThis(Handle<JSFunction*> fun) {
        JS_ASSERT(getSlotRef(CREATE_DATAVIEW_FOR_THIS).isUndefined());
        setSlot(CREATE_DATAVIEW_FOR_THIS, ObjectValue(*fun));
    }

    template<typename T>
    inline void setCreateArrayFromBuffer(Handle<JSFunction*> fun);

  public:
    static GlobalObject *create(JSContext *cx, const Class *clasp);

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
    JSObject *createBlankPrototype(JSContext *cx, const js::Class *clasp);

    /*
     * Identical to createBlankPrototype, but uses proto as the [[Prototype]]
     * of the returned blank prototype.
     */
    JSObject *createBlankPrototypeInheriting(JSContext *cx, const js::Class *clasp, JSObject &proto);

    JSObject *getOrCreateObjectPrototype(JSContext *cx) {
        if (functionObjectClassesInitialized())
            return &getPrototype(JSProto_Object).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!initFunctionAndObjectClasses(cx))
            return nullptr;
        return &self->getPrototype(JSProto_Object).toObject();
    }

    JSObject *getOrCreateFunctionPrototype(JSContext *cx) {
        if (functionObjectClassesInitialized())
            return &getPrototype(JSProto_Function).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!initFunctionAndObjectClasses(cx))
            return nullptr;
        return &self->getPrototype(JSProto_Function).toObject();
    }

    JSObject *getOrCreateArrayPrototype(JSContext *cx) {
        if (arrayClassInitialized())
            return &getPrototype(JSProto_Array).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitArrayClass(cx, self))
            return nullptr;
        return &self->getPrototype(JSProto_Array).toObject();
    }

    JSObject *maybeGetArrayPrototype() {
        if (arrayClassInitialized())
            return &getPrototype(JSProto_Array).toObject();
        return nullptr;
    }

    JSObject *getOrCreateBooleanPrototype(JSContext *cx) {
        if (booleanClassInitialized())
            return &getPrototype(JSProto_Boolean).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitBooleanClass(cx, self))
            return nullptr;
        return &self->getPrototype(JSProto_Boolean).toObject();
    }

    JSObject *getOrCreateNumberPrototype(JSContext *cx) {
        if (numberClassInitialized())
            return &getPrototype(JSProto_Number).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitNumberClass(cx, self))
            return nullptr;
        return &self->getPrototype(JSProto_Number).toObject();
    }

    JSObject *getOrCreateStringPrototype(JSContext *cx) {
        if (stringClassInitialized())
            return &getPrototype(JSProto_String).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitStringClass(cx, self))
            return nullptr;
        return &self->getPrototype(JSProto_String).toObject();
    }

    JSObject *getOrCreateRegExpPrototype(JSContext *cx) {
        if (regexpClassInitialized())
            return &getPrototype(JSProto_RegExp).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitRegExpClass(cx, self))
            return nullptr;
        return &self->getPrototype(JSProto_RegExp).toObject();
    }

    JSObject *maybeGetRegExpPrototype() {
        if (regexpClassInitialized())
            return &getPrototype(JSProto_RegExp).toObject();
        return nullptr;
    }

    JSObject *getOrCreateArrayBufferPrototype(JSContext *cx) {
        if (arrayBufferClassInitialized())
            return &getPrototype(JSProto_ArrayBuffer).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitTypedArrayClasses(cx, self))
            return nullptr;
        return &self->getPrototype(JSProto_ArrayBuffer).toObject();
    }

    JSObject *getOrCreateCustomErrorPrototype(JSContext *cx, JSExnType exnType) {
        JSProtoKey key = GetExceptionProtoKey(exnType);
        if (errorClassesInitialized())
            return &getPrototype(key).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!js_InitExceptionClasses(cx, self))
            return nullptr;
        return &self->getPrototype(key).toObject();
    }

    JSObject *getOrCreateIntlObject(JSContext *cx) {
        return getOrCreateObject(cx, APPLICATION_SLOTS + JSProto_Intl, initIntlObject);
    }

    JSObject &getTypedObject() {
        Value v = getConstructor(JSProto_TypedObject);
        // only gets called from contexts where TypedObject must be initialized
        JS_ASSERT(v.isObject());
        return v.toObject();
    }

    JSObject *getIteratorPrototype() {
        return &getPrototype(JSProto_Iterator).toObject();
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

    JSObject *getArrayType(JSContext *cx) {
        const Value &v = getReservedSlot(ARRAY_TYPE);

        MOZ_ASSERT(v.isObject(),
                   "GlobalObject::arrayType must only be called from "
                   "TypedObject code that can assume TypedObject has "
                   "been initialized");

        return &v.toObject();
    }

    void setArrayType(JSObject *obj) {
        initReservedSlot(ARRAY_TYPE, ObjectValue(*obj));
    }

  private:
    typedef bool (*ObjectInitOp)(JSContext *cx, Handle<GlobalObject*> global);

    JSObject *getOrCreateObject(JSContext *cx, unsigned slot, ObjectInitOp init) {
        Value v = getSlotRef(slot);
        if (v.isObject())
            return &v.toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!init(cx, self))
            return nullptr;
        return &self->getSlot(slot).toObject();
    }

    const HeapSlot &getSlotRefForCompilation(uint32_t slot) const {
        // This method should only be used for slots that are either eagerly
        // initialized on creation of the global or only change under the
        // compilation lock. Note that the dynamic slots pointer for global
        // objects can only change under the compilation lock.
        JS_ASSERT(slot < JSCLASS_RESERVED_SLOTS(getClass()));
        uint32_t fixed = numFixedSlotsForCompilation();
        if (slot < fixed)
            return fixedSlots()[slot];
        return slots[slot - fixed];
    }

  public:
    JSObject *getOrCreateIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, APPLICATION_SLOTS + JSProto_LIMIT + JSProto_Iterator,
                                 initIteratorClasses);
    }

    JSObject *getOrCreateArrayIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, ARRAY_ITERATOR_PROTO, initIteratorClasses);
    }

    JSObject *getOrCreateStringIteratorPrototype(JSContext *cx) {
        return getOrCreateObject(cx, STRING_ITERATOR_PROTO, initIteratorClasses);
    }

    JSObject *getOrCreateLegacyGeneratorObjectPrototype(JSContext *cx) {
        return getOrCreateObject(cx, LEGACY_GENERATOR_OBJECT_PROTO, initIteratorClasses);
    }

    JSObject *getOrCreateStarGeneratorObjectPrototype(JSContext *cx) {
        return getOrCreateObject(cx, STAR_GENERATOR_OBJECT_PROTO, initIteratorClasses);
    }

    JSObject *getOrCreateStarGeneratorFunctionPrototype(JSContext *cx) {
        return getOrCreateObject(cx, APPLICATION_SLOTS + JSProto_LIMIT + JSProto_GeneratorFunction,
                                 initIteratorClasses);
    }

    JSObject *getOrCreateStarGeneratorFunction(JSContext *cx) {
        return getOrCreateObject(cx, APPLICATION_SLOTS + JSProto_GeneratorFunction,
                                 initIteratorClasses);
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
            return nullptr;
        return &self->getPrototype(JSProto_DataView).toObject();
    }

    JSObject *intrinsicsHolder() {
        JS_ASSERT(!getSlotRefForCompilation(INTRINSICS).isUndefined());
        return &getSlotRefForCompilation(INTRINSICS).toObject();
    }

    bool maybeGetIntrinsicValue(PropertyName *name, Value *vp) {
        JSObject *holder = intrinsicsHolder();
        if (Shape *shape = holder->nativeLookupPure(name)) {
            *vp = holder->getSlot(shape->slot());
            return true;
        }
        return false;
    }

    bool getIntrinsicValue(JSContext *cx, HandlePropertyName name, MutableHandleValue value) {
        if (maybeGetIntrinsicValue(name, value.address()))
            return true;
        Rooted<GlobalObject*> self(cx, this);
        if (!cx->runtime()->cloneSelfHostedValue(cx, name, value))
            return false;
        RootedObject holder(cx, self->intrinsicsHolder());
        RootedId id(cx, NameToId(name));
        return JS_DefinePropertyById(cx, holder, id, value, nullptr, nullptr, 0);
    }

    bool setIntrinsicValue(JSContext *cx, PropertyName *name, HandleValue value) {
#ifdef DEBUG
        RootedObject self(cx, this);
        JS_ASSERT(cx->runtime()->isSelfHostingGlobal(self));
#endif
        RootedObject holder(cx, intrinsicsHolder());
        RootedValue valCopy(cx, value);
        return JSObject::setProperty(cx, holder, holder, name, &valCopy, false);
    }

    bool getSelfHostedFunction(JSContext *cx, HandleAtom selfHostedName, HandleAtom name,
                               unsigned nargs, MutableHandleValue funVal);

    RegExpStatics *getRegExpStatics() const {
        JSObject &resObj = getSlotRefForCompilation(REGEXP_STATICS).toObject();
        return static_cast<RegExpStatics *>(resObj.getPrivate(/* nfixed = */ 1));
    }

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

    // Warn about use of the deprecated watch/unwatch functions in the global
    // in which |obj| was created, if no prior warning was given.
    static bool warnOnceAboutWatch(JSContext *cx, HandleObject obj);

    static bool getOrCreateEval(JSContext *cx, Handle<GlobalObject*> global,
                                MutableHandleObject eval);

    // Infallibly test whether the given value is the eval function for this global.
    bool valueIsEval(Value val);

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
     * is not a debuggee, this returns either nullptr or an empty vector.
     */
    DebuggerVector *getDebuggers();

    /*
     * The same, but create the empty vector if one does not already
     * exist. Returns nullptr only on OOM.
     */
    static DebuggerVector *getOrCreateDebuggers(JSContext *cx, Handle<GlobalObject*> global);

    static bool addDebugger(JSContext *cx, Handle<GlobalObject*> global, Debugger *dbg);
};

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<uint8_t>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_UINT8, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<int8_t>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_INT8, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<uint16_t>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_UINT16, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<int16_t>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_INT16, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<uint32_t>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_UINT32, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<int32_t>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_INT32, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<float>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_FLOAT32, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<double>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_FLOAT64, fun);
}

template<>
inline void
GlobalObject::setCreateArrayFromBuffer<uint8_clamped>(Handle<JSFunction*> fun)
{
    setCreateArrayFromBufferHelper(FROM_BUFFER_UINT8CLAMPED, fun);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<uint8_t>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_UINT8);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<int8_t>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_INT8);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<uint16_t>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_UINT16);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<int16_t>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_INT16);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<uint32_t>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_UINT32);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<int32_t>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_INT32);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<float>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_FLOAT32);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<double>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_FLOAT64);
}

template<>
inline Value
GlobalObject::createArrayFromBuffer<uint8_clamped>() const
{
    return createArrayFromBufferHelper(FROM_BUFFER_UINT8CLAMPED);
}

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

template<>
inline bool
JSObject::is<js::GlobalObject>() const
{
    return !!(getClass()->flags & JSCLASS_IS_GLOBAL);
}

#endif /* vm_GlobalObject_h */
