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
#include "vm/ArrayBufferObject.h"
#include "vm/ErrorObject.h"

extern JSObject *
js_InitSharedArrayBufferClass(JSContext *cx, js::HandleObject obj);

namespace js {

class Debugger;
class TypedObjectModuleObject;

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
    static const unsigned APPLICATION_SLOTS = JSCLASS_GLOBAL_APPLICATION_SLOTS;

    /*
     * Count of slots to store built-in constructors, prototypes, and initial
     * visible properties for the constructors.
     */
    static const unsigned STANDARD_CLASS_SLOTS  = JSProto_LIMIT * 3;

    /* Various function values needed by the engine. */
    static const unsigned EVAL                    = APPLICATION_SLOTS + STANDARD_CLASS_SLOTS;
    static const unsigned CREATE_DATAVIEW_FOR_THIS = EVAL + 1;
    static const unsigned THROWTYPEERROR          = CREATE_DATAVIEW_FOR_THIS + 1;

    /*
     * Instances of the internal createArrayFromBuffer function used by the
     * typed array code, one per typed array element type.
     */
    static const unsigned FROM_BUFFER_UINT8 = THROWTYPEERROR + 1;
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
    static const unsigned WARNED_PROTO_SETTING_SLOW = WARNED_WATCH_DEPRECATED + 1;
    static const unsigned RUNTIME_CODEGEN_ENABLED = WARNED_PROTO_SETTING_SLOW + 1;
    static const unsigned DEBUGGERS               = RUNTIME_CODEGEN_ENABLED + 1;
    static const unsigned INTRINSICS              = DEBUGGERS + 1;
    static const unsigned FLOAT32X4_TYPE_DESCR   = INTRINSICS + 1;
    static const unsigned INT32X4_TYPE_DESCR     = FLOAT32X4_TYPE_DESCR + 1;
    static const unsigned FOR_OF_PIC_CHAIN        = INT32X4_TYPE_DESCR + 1;

    /* Total reserved-slot count for global objects. */
    static const unsigned RESERVED_SLOTS = FOR_OF_PIC_CHAIN + 1;

    /*
     * The slot count must be in the public API for JSCLASS_GLOBAL_FLAGS, and
     * we won't expose GlobalObject, so just assert that the two values are
     * synchronized.
     */
    static_assert(JSCLASS_GLOBAL_SLOT_COUNT == RESERVED_SLOTS,
                  "global object slot counts are inconsistent");

    // Emit the specified warning if the given slot in |obj|'s global isn't
    // true, then set the slot to true.  Thus calling this method warns once
    // for each global object it's called on, and every other call does
    // nothing.
    static bool
    warnOnceAbout(JSContext *cx, HandleObject obj, uint32_t slot, unsigned errorNumber);


  public:
    void setThrowTypeError(JSFunction *fun) {
        JS_ASSERT(getSlotRef(THROWTYPEERROR).isUndefined());
        setSlot(THROWTYPEERROR, ObjectValue(*fun));
    }

    void setOriginalEval(JSObject *evalobj) {
        JS_ASSERT(getSlotRef(EVAL).isUndefined());
        setSlot(EVAL, ObjectValue(*evalobj));
    }

    void setIntrinsicsHolder(JSObject *obj) {
        JS_ASSERT(getSlotRef(INTRINSICS).isUndefined());
        setSlot(INTRINSICS, ObjectValue(*obj));
    }

    Value getConstructor(JSProtoKey key) const {
        JS_ASSERT(key <= JSProto_LIMIT);
        return getSlot(APPLICATION_SLOTS + key);
    }
    static bool ensureConstructor(JSContext *cx, Handle<GlobalObject*> global, JSProtoKey key);
    static bool resolveConstructor(JSContext *cx, Handle<GlobalObject*> global, JSProtoKey key);
    static bool initBuiltinConstructor(JSContext *cx, Handle<GlobalObject*> global,
                                       JSProtoKey key, HandleObject ctor, HandleObject proto);

    void setConstructor(JSProtoKey key, const Value &v) {
        JS_ASSERT(key <= JSProto_LIMIT);
        setSlot(APPLICATION_SLOTS + key, v);
    }

    Value getPrototype(JSProtoKey key) const {
        JS_ASSERT(key <= JSProto_LIMIT);
        return getSlot(APPLICATION_SLOTS + JSProto_LIMIT + key);
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
    bool isStandardClassResolved(JSProtoKey key) const {
        // If the constructor is undefined, then it hasn't been initialized.
        MOZ_ASSERT(getConstructor(key).isUndefined() ||
                   getConstructor(key).isObject());
        return !getConstructor(key).isUndefined();
    }

  private:
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
    bool sharedArrayBufferClassInitialized() const {
        return classIsInitialized(JSProto_SharedArrayBuffer);
    }
    bool errorClassesInitialized() const {
        return classIsInitialized(JSProto_Error);
    }
    bool dataViewClassInitialized() const {
        return classIsInitialized(JSProto_DataView);
    }

    Value createArrayFromBufferHelper(uint32_t slot) const {
        JS_ASSERT(FROM_BUFFER_UINT8 <= slot && slot <= FROM_BUFFER_UINT8CLAMPED);
        JS_ASSERT(!getSlot(slot).isUndefined());
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
        if (!ensureConstructor(cx, self, JSProto_Object))
            return nullptr;
        return &self->getPrototype(JSProto_Object).toObject();
    }

    JSObject *getOrCreateFunctionPrototype(JSContext *cx) {
        if (functionObjectClassesInitialized())
            return &getPrototype(JSProto_Function).toObject();
        Rooted<GlobalObject*> self(cx, this);
        if (!ensureConstructor(cx, self, JSProto_Object))
            return nullptr;
        return &self->getPrototype(JSProto_Function).toObject();
    }

    static JSObject *getOrCreateArrayPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_Array))
            return nullptr;
        return &global->getPrototype(JSProto_Array).toObject();
    }

    JSObject *maybeGetArrayPrototype() {
        if (arrayClassInitialized())
            return &getPrototype(JSProto_Array).toObject();
        return nullptr;
    }

    static JSObject *getOrCreateBooleanPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_Boolean))
            return nullptr;
        return &global->getPrototype(JSProto_Boolean).toObject();
    }

    static JSObject *getOrCreateNumberPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_Number))
            return nullptr;
        return &global->getPrototype(JSProto_Number).toObject();
    }

    static JSObject *getOrCreateStringPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_String))
            return nullptr;
        return &global->getPrototype(JSProto_String).toObject();
    }

    static JSObject *getOrCreateSymbolPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_Symbol))
            return nullptr;
        return &global->getPrototype(JSProto_Symbol).toObject();
    }

    static JSObject *getOrCreateRegExpPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_RegExp))
            return nullptr;
        return &global->getPrototype(JSProto_RegExp).toObject();
    }

    JSObject *maybeGetRegExpPrototype() {
        if (regexpClassInitialized())
            return &getPrototype(JSProto_RegExp).toObject();
        return nullptr;
    }

    static JSObject *getOrCreateArrayBufferPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_ArrayBuffer))
            return nullptr;
        return &global->getPrototype(JSProto_ArrayBuffer).toObject();
    }

    JSObject *getOrCreateSharedArrayBufferPrototype(JSContext *cx, Handle<GlobalObject*> global) {
        if (!ensureConstructor(cx, global, JSProto_SharedArrayBuffer))
            return nullptr;
        return &global->getPrototype(JSProto_SharedArrayBuffer).toObject();
    }

    static JSObject *getOrCreateCustomErrorPrototype(JSContext *cx,
                                                     Handle<GlobalObject*> global,
                                                     JSExnType exnType)
    {
        JSProtoKey key = GetExceptionProtoKey(exnType);
        if (!ensureConstructor(cx, global, key))
            return nullptr;
        return &global->getPrototype(key).toObject();
    }

    JSObject *getOrCreateIntlObject(JSContext *cx) {
        return getOrCreateObject(cx, APPLICATION_SLOTS + JSProto_Intl, initIntlObject);
    }

    JSObject *getOrCreateTypedObjectModule(JSContext *cx) {
        return getOrCreateObject(cx, APPLICATION_SLOTS + JSProto_TypedObject, initTypedObjectModule);
    }

    void setFloat32x4TypeDescr(JSObject &obj) {
        JS_ASSERT(getSlotRef(FLOAT32X4_TYPE_DESCR).isUndefined());
        setSlot(FLOAT32X4_TYPE_DESCR, ObjectValue(obj));
    }

    JSObject &float32x4TypeDescr() {
        JS_ASSERT(getSlotRef(FLOAT32X4_TYPE_DESCR).isObject());
        return getSlotRef(FLOAT32X4_TYPE_DESCR).toObject();
    }

    void setInt32x4TypeDescr(JSObject &obj) {
        JS_ASSERT(getSlotRef(INT32X4_TYPE_DESCR).isUndefined());
        setSlot(INT32X4_TYPE_DESCR, ObjectValue(obj));
    }

    JSObject &int32x4TypeDescr() {
        JS_ASSERT(getSlotRef(INT32X4_TYPE_DESCR).isObject());
        return getSlotRef(INT32X4_TYPE_DESCR).toObject();
    }

    TypedObjectModuleObject &getTypedObjectModule() const;

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

  public:
    static JSObject *getOrCreateIteratorPrototype(JSContext *cx,
                                                  Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(APPLICATION_SLOTS + JSProto_LIMIT + JSProto_Iterator).toObject();
    }

    static JSObject *getOrCreateArrayIteratorPrototype(JSContext *cx,
                                                       Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(ARRAY_ITERATOR_PROTO).toObject();
    }

    static JSObject *getOrCreateStringIteratorPrototype(JSContext *cx,
                                                        Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(STRING_ITERATOR_PROTO).toObject();
    }

    static JSObject *getOrCreateLegacyGeneratorObjectPrototype(JSContext *cx,
                                                               Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(LEGACY_GENERATOR_OBJECT_PROTO).toObject();
    }

    static JSObject *getOrCreateStarGeneratorObjectPrototype(JSContext *cx,
                                                             Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(STAR_GENERATOR_OBJECT_PROTO).toObject();
    }

    static JSObject *getOrCreateStarGeneratorFunctionPrototype(JSContext *cx,
                                                               Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(APPLICATION_SLOTS + JSProto_LIMIT + JSProto_GeneratorFunction).toObject();
    }

    static JSObject *getOrCreateStarGeneratorFunction(JSContext *cx,
                                                      Handle<GlobalObject*> global)
    {
        if (!ensureConstructor(cx, global, JSProto_Iterator))
            return nullptr;
        return &global->getSlot(APPLICATION_SLOTS + JSProto_GeneratorFunction).toObject();
    }

    static JSObject *getOrCreateMapIteratorPrototype(JSContext *cx,
                                                     Handle<GlobalObject*> global)
    {
        return global->getOrCreateObject(cx, MAP_ITERATOR_PROTO, initMapIteratorProto);
    }

    static JSObject *getOrCreateSetIteratorPrototype(JSContext *cx,
                                                     Handle<GlobalObject*> global)
    {
        return global->getOrCreateObject(cx, SET_ITERATOR_PROTO, initSetIteratorProto);
    }

    JSObject *getOrCreateDataViewPrototype(JSContext *cx) {
        Rooted<GlobalObject*> self(cx, this);
        if (!ensureConstructor(cx, self, JSProto_DataView))
            return nullptr;
        return &self->getPrototype(JSProto_DataView).toObject();
    }

    JSObject *intrinsicsHolder() {
        JS_ASSERT(!getSlot(INTRINSICS).isUndefined());
        return &getSlot(INTRINSICS).toObject();
    }

    bool maybeGetIntrinsicValue(jsid id, Value *vp) {
        JSObject *holder = intrinsicsHolder();

        if (Shape *shape = holder->nativeLookupPure(id)) {
            *vp = holder->getSlot(shape->slot());
            return true;
        }
        return false;
    }
    bool maybeGetIntrinsicValue(PropertyName *name, Value *vp) {
        return maybeGetIntrinsicValue(NameToId(name), vp);
    }

    static bool getIntrinsicValue(JSContext *cx, Handle<GlobalObject*> global,
                                  HandlePropertyName name, MutableHandleValue value)
    {
        if (global->maybeGetIntrinsicValue(name, value.address()))
            return true;
        if (!cx->runtime()->cloneSelfHostedValue(cx, name, value))
            return false;
        RootedId id(cx, NameToId(name));
        return global->addIntrinsicValue(cx, id, value);
    }

    bool addIntrinsicValue(JSContext *cx, HandleId id, HandleValue value);

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

    bool hasRegExpStatics() const;
    RegExpStatics *getRegExpStatics(ExclusiveContext *cx) const;
    RegExpStatics *getAlreadyCreatedRegExpStatics() const;

    JSObject *getThrowTypeError() const {
        const Value v = getReservedSlot(THROWTYPEERROR);
        MOZ_ASSERT(v.isObject(),
                   "attempting to access [[ThrowTypeError]] too early");
        return &v.toObject();
    }

    Value createDataViewForThis() const {
        JS_ASSERT(dataViewClassInitialized());
        return getSlot(CREATE_DATAVIEW_FOR_THIS);
    }

    template<typename T>
    inline Value createArrayFromBuffer() const;

    static bool isRuntimeCodeGenEnabled(JSContext *cx, Handle<GlobalObject*> global);

    // Warn about use of the deprecated watch/unwatch functions in the global
    // in which |obj| was created, if no prior warning was given.
    static bool warnOnceAboutWatch(JSContext *cx, HandleObject obj) {
        // Temporarily disabled until we've provided a watch/unwatch workaround for
        // debuggers like Firebug (bug 934669).
        //return warnOnceAbout(cx, obj, WARNED_WATCH_DEPRECATED, JSMSG_OBJECT_WATCH_DEPRECATED);
        return true;
    }

    // Warn about use of the given __proto__ setter to attempt to mutate an
    // object's [[Prototype]], if no prior warning was given.
    static bool warnOnceAboutPrototypeMutation(JSContext *cx, HandleObject protoSetter) {
        return warnOnceAbout(cx, protoSetter, WARNED_PROTO_SETTING_SLOW, JSMSG_PROTO_SETTING_SLOW);
    }

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

    // Implemented in builtin/TypedObject.cpp
    static bool initTypedObjectModule(JSContext *cx, Handle<GlobalObject*> global);

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

    inline JSObject *getForOfPICObject() {
        Value forOfPIC = getReservedSlot(FOR_OF_PIC_CHAIN);
        if (forOfPIC.isUndefined())
            return nullptr;
        return &forOfPIC.toObject();
    }
    static JSObject *getOrCreateForOfPICObject(JSContext *cx, Handle<GlobalObject*> global);

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
 * Define properties and/or functions on any object. Either ps or fs, or both,
 * may be null.
 */
extern bool
DefinePropertiesAndFunctions(JSContext *cx, HandleObject obj,
                             const JSPropertySpec *ps, const JSFunctionSpec *fs);

typedef HashSet<GlobalObject *, DefaultHasher<GlobalObject *>, SystemAllocPolicy> GlobalObjectSet;

/*
 * Convenience templates to generic constructor and prototype creation functions
 * for ClassSpecs.
 */

template<JSNative ctor, unsigned length, gc::AllocKind kind>
JSObject *
GenericCreateConstructor(JSContext *cx, JSProtoKey key)
{
    // Note - We duplicate the trick from ClassName() so that we don't need to
    // include jsatominlines.h here.
    PropertyName *name = (&cx->names().Null)[key];
    return cx->global()->createConstructor(cx, ctor, name, length, kind);
}

template<const Class *clasp>
JSObject *
GenericCreatePrototype(JSContext *cx, JSProtoKey key)
{
    MOZ_ASSERT(key != JSProto_Object);
    JSProtoKey parentKey = ParentKeyForStandardClass(key);
    if (!GlobalObject::ensureConstructor(cx, cx->global(), parentKey))
        return nullptr;
    JSObject *parentProto = &cx->global()->getPrototype(parentKey).toObject();
    return cx->global()->createBlankPrototypeInheriting(cx, clasp, *parentProto);
}

inline JSProtoKey
StandardProtoKeyOrNull(const JSObject *obj)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(obj->getClass());
    if (key == JSProto_Error)
        return GetExceptionProtoKey(obj->as<ErrorObject>().type());
    return key;
}

} // namespace js

template<>
inline bool
JSObject::is<js::GlobalObject>() const
{
    return !!(getClass()->flags & JSCLASS_IS_GLOBAL);
}

#endif /* vm_GlobalObject_h */
