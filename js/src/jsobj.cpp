/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS object implementation.
 */

#include "jsobjinlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/TemplateLib.h"

#include <string.h>

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsopcode.h"
#include "jsprf.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswatchpoint.h"
#include "jswrapper.h"

#include "asmjs/AsmJSModule.h"
#include "builtin/Eval.h"
#include "builtin/Object.h"
#include "builtin/SymbolObject.h"
#include "frontend/BytecodeCompiler.h"
#include "gc/Marking.h"
#include "jit/BaselineJIT.h"
#include "js/MemoryMetrics.h"
#include "vm/ArgumentsObject.h"
#include "vm/Interpreter.h"
#include "vm/ProxyObject.h"
#include "vm/RegExpStaticsObject.h"
#include "vm/Shape.h"
#include "vm/TypedArrayCommon.h"

#include "jsatominlines.h"
#include "jsboolinlines.h"
#include "jscntxtinlines.h"
#include "jscompartmentinlines.h"

#include "vm/ArrayObject-inl.h"
#include "vm/BooleanObject-inl.h"
#include "vm/Interpreter-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/NumberObject-inl.h"
#include "vm/Runtime-inl.h"
#include "vm/Shape-inl.h"
#include "vm/StringObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::DebugOnly;
using mozilla::Maybe;

static JSObject *
CreateObjectConstructor(JSContext *cx, JSProtoKey key)
{
    Rooted<GlobalObject*> self(cx, cx->global());
    if (!GlobalObject::ensureConstructor(cx, self, JSProto_Function))
        return nullptr;

    RootedObject functionProto(cx, &self->getPrototype(JSProto_Function).toObject());

    /* Create the Object function now that we have a [[Prototype]] for it. */
    RootedObject ctor(cx, NewObjectWithGivenProto(cx, &JSFunction::class_, functionProto,
                                                  self, SingletonObject));
    if (!ctor)
        return nullptr;
    return NewFunction(cx, ctor, obj_construct, 1, JSFunction::NATIVE_CTOR, self,
                       HandlePropertyName(cx->names().Object));
}

static JSObject *
CreateObjectPrototype(JSContext *cx, JSProtoKey key)
{
    Rooted<GlobalObject*> self(cx, cx->global());

    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));
    MOZ_ASSERT(self->isNative());

    /*
     * Create |Object.prototype| first, mirroring CreateBlankProto but for the
     * prototype of the created object.
     */
    RootedObject objectProto(cx, NewObjectWithGivenProto(cx, &JSObject::class_, nullptr,
                                                         self, SingletonObject));
    if (!objectProto)
        return nullptr;

    /*
     * The default 'new' type of Object.prototype is required by type inference
     * to have unknown properties, to simplify handling of e.g. heterogenous
     * objects in JSON and script literals.
     */
    if (!JSObject::setNewTypeUnknown(cx, &JSObject::class_, objectProto))
        return nullptr;

    return objectProto;
}

static bool
FinishObjectClassInit(JSContext *cx, JS::HandleObject ctor, JS::HandleObject proto)
{
    Rooted<GlobalObject*> self(cx, cx->global());

    /* ES5 15.1.2.1. */
    RootedId evalId(cx, NameToId(cx->names().eval));
    JSObject *evalobj = DefineFunction(cx, self, evalId, IndirectEval, 1, JSFUN_STUB_GSOPS);
    if (!evalobj)
        return false;
    self->setOriginalEval(evalobj);

    RootedObject intrinsicsHolder(cx);
    bool isSelfHostingGlobal = cx->runtime()->isSelfHostingGlobal(self);
    if (isSelfHostingGlobal) {
        intrinsicsHolder = self;
    } else {
        intrinsicsHolder = NewObjectWithGivenProto(cx, &JSObject::class_, proto, self,
                                                   TenuredObject);
        if (!intrinsicsHolder)
            return false;
    }
    self->setIntrinsicsHolder(intrinsicsHolder);
    /* Define a property 'global' with the current global as its value. */
    RootedValue global(cx, ObjectValue(*self));
    if (!JSObject::defineProperty(cx, intrinsicsHolder, cx->names().global,
                                  global, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return false;
    }

    /*
     * Define self-hosted functions after setting the intrinsics holder
     * (which is needed to define self-hosted functions)
     */
    if (!isSelfHostingGlobal) {
        if (!JS_DefineFunctions(cx, ctor, object_static_selfhosted_methods))
            return false;
    }

    /*
     * The global object should have |Object.prototype| as its [[Prototype]].
     * Eventually we'd like to have standard classes be there from the start,
     * and thus we would know we were always setting what had previously been a
     * null [[Prototype]], but right now some code assumes it can set the
     * [[Prototype]] before standard classes have been initialized.  For now,
     * only set the [[Prototype]] if it hasn't already been set.
     */
    Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
    if (self->shouldSplicePrototype(cx) && !self->splicePrototype(cx, self->getClass(), tagged))
        return false;
    return true;
}


const Class JSObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,                 /* finalize */
    nullptr,                 /* call */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct */
    nullptr,                 /* trace */
    {
        CreateObjectConstructor,
        CreateObjectPrototype,
        object_static_methods,
        object_methods,
        object_properties,
        FinishObjectClassInit
    }
};

const Class* const js::ObjectClassPtr = &JSObject::class_;

JS_FRIEND_API(JSObject *)
JS_ObjectToInnerObject(JSContext *cx, HandleObject obj)
{
    if (!obj) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INACTIVE);
        return nullptr;
    }
    return GetInnerObject(obj);
}

JS_FRIEND_API(JSObject *)
JS_ObjectToOuterObject(JSContext *cx, HandleObject obj)
{
    assertSameCompartment(cx, obj);
    return GetOuterObject(cx, obj);
}

JSObject *
js::NonNullObject(JSContext *cx, const Value &v)
{
    if (v.isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT);
        return nullptr;
    }
    return &v.toObject();
}

const char *
js::InformalValueTypeName(const Value &v)
{
    if (v.isObject())
        return v.toObject().getClass()->name;
    if (v.isString())
        return "string";
    if (v.isSymbol())
        return "symbol";
    if (v.isNumber())
        return "number";
    if (v.isBoolean())
        return "boolean";
    if (v.isNull())
        return "null";
    if (v.isUndefined())
        return "undefined";
    return "value";
}

bool
js::NewPropertyDescriptorObject(JSContext *cx, Handle<PropertyDescriptor> desc,
                                MutableHandleValue vp)
{
    if (!desc.object()) {
        vp.setUndefined();
        return true;
    }

    Rooted<PropDesc> d(cx);

    d.initFromPropertyDescriptor(desc);
    RootedObject descObj(cx);
    if (!d.makeObject(cx, &descObj))
        return false;
    vp.setObject(*descObj);
    return true;
}

void
PropDesc::initFromPropertyDescriptor(Handle<PropertyDescriptor> desc)
{
    MOZ_ASSERT(isUndefined());

    if (!desc.object())
        return;

    isUndefined_ = false;
    attrs = uint8_t(desc.attributes());
    MOZ_ASSERT_IF(attrs & JSPROP_READONLY, !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
    if (desc.hasGetterOrSetterObject()) {
        hasGet_ = true;
        get_ = desc.hasGetterObject() && desc.getterObject()
               ? ObjectValue(*desc.getterObject())
               : UndefinedValue();
        hasSet_ = true;
        set_ = desc.hasSetterObject() && desc.setterObject()
               ? ObjectValue(*desc.setterObject())
               : UndefinedValue();
        hasValue_ = false;
        value_.setUndefined();
        hasWritable_ = false;
    } else {
        hasGet_ = false;
        get_.setUndefined();
        hasSet_ = false;
        set_.setUndefined();
        hasValue_ = !(desc.attributes() & JSPROP_IGNORE_VALUE);
        value_ = hasValue_ ? desc.value() : UndefinedValue();
        hasWritable_ = !(desc.attributes() & JSPROP_IGNORE_READONLY);
    }
    hasEnumerable_ = !(desc.attributes() & JSPROP_IGNORE_ENUMERATE);
    hasConfigurable_ = !(desc.attributes() & JSPROP_IGNORE_PERMANENT);
}

void
PropDesc::populatePropertyDescriptor(HandleObject obj, MutableHandle<PropertyDescriptor> desc) const
{
    if (isUndefined()) {
        desc.object().set(nullptr);
        return;
    }

    desc.value().set(hasValue() ? value() : UndefinedValue());
    desc.setGetter(getter());
    desc.setSetter(setter());

    // Make sure we honor the "has" notions in some way.
    unsigned attrs = attributes();
    if (!hasEnumerable())
        attrs |= JSPROP_IGNORE_ENUMERATE;
    if (!hasWritable())
        attrs |= JSPROP_IGNORE_READONLY;
    if (!hasConfigurable())
        attrs |= JSPROP_IGNORE_PERMANENT;
    if (!hasValue())
        attrs |= JSPROP_IGNORE_VALUE;
    desc.setAttributes(attrs);

    desc.object().set(obj);
}

bool
PropDesc::makeObject(JSContext *cx, MutableHandleObject obj)
{
    MOZ_ASSERT(!isUndefined());

    obj.set(NewBuiltinClassInstance(cx, &JSObject::class_));
    if (!obj)
        return false;

    const JSAtomState &names = cx->names();
    RootedValue configurableVal(cx, BooleanValue((attrs & JSPROP_PERMANENT) == 0));
    RootedValue enumerableVal(cx, BooleanValue((attrs & JSPROP_ENUMERATE) != 0));
    RootedValue writableVal(cx, BooleanValue((attrs & JSPROP_READONLY) == 0));
    if ((hasConfigurable() &&
         !JSObject::defineProperty(cx, obj, names.configurable, configurableVal)) ||
        (hasEnumerable() &&
         !JSObject::defineProperty(cx, obj, names.enumerable, enumerableVal)) ||
        (hasGet() &&
         !JSObject::defineProperty(cx, obj, names.get, getterValue())) ||
        (hasSet() &&
         !JSObject::defineProperty(cx, obj, names.set, setterValue())) ||
        (hasValue() &&
         !JSObject::defineProperty(cx, obj, names.value, value())) ||
        (hasWritable() &&
         !JSObject::defineProperty(cx, obj, names.writable, writableVal)))
    {
        return false;
    }

    return true;
}

bool
js::GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id,
                             MutableHandle<PropertyDescriptor> desc)
{
    if (obj->is<ProxyObject>())
        return Proxy::getOwnPropertyDescriptor(cx, obj, id, desc);

    RootedObject pobj(cx);
    RootedShape shape(cx);
    if (!HasOwnProperty<CanGC>(cx, obj->getOps()->lookupGeneric, obj, id, &pobj, &shape))
        return false;
    if (!shape) {
        desc.object().set(nullptr);
        return true;
    }

    bool doGet = true;
    if (pobj->isNative()) {
        desc.setAttributes(GetShapeAttributes(pobj, shape));
        if (desc.hasGetterOrSetterObject()) {
            MOZ_ASSERT(desc.isShared());
            doGet = false;
            if (desc.hasGetterObject())
                desc.setGetterObject(shape->getterObject());
            if (desc.hasSetterObject())
                desc.setSetterObject(shape->setterObject());
        } else {
            // This is either a straight-up data property or (rarely) a
            // property with a JSPropertyOp getter/setter. The latter must be
            // reported to the caller as a plain data property, so don't
            // populate desc.getter/setter, and mask away the SHARED bit.
            desc.attributesRef() &= ~JSPROP_SHARED;
        }
    } else {
        if (!JSObject::getGenericAttributes(cx, pobj, id, &desc.attributesRef()))
            return false;
    }

    RootedValue value(cx);
    if (doGet && !JSObject::getGeneric(cx, obj, obj, id, &value))
        return false;

    desc.value().set(value);
    desc.object().set(obj);
    return true;
}

bool
js::GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    Rooted<PropertyDescriptor> desc(cx);
    return GetOwnPropertyDescriptor(cx, obj, id, &desc) &&
           NewPropertyDescriptorObject(cx, desc, vp);
}

bool
js::GetFirstArgumentAsObject(JSContext *cx, const CallArgs &args, const char *method,
                             MutableHandleObject objp)
{
    if (args.length() == 0) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             method, "0", "s");
        return false;
    }

    HandleValue v = args[0];
    if (!v.isObject()) {
        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, v, NullPtr());
        if (!bytes)
            return false;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_UNEXPECTED_TYPE,
                             bytes, "not an object");
        js_free(bytes);
        return false;
    }

    objp.set(&v.toObject());
    return true;
}

static bool
HasProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp, bool *foundp)
{
    if (!JSObject::hasProperty(cx, obj, id, foundp))
        return false;
    if (!*foundp) {
        vp.setUndefined();
        return true;
    }

    /*
     * We must go through the method read barrier in case id is 'get' or 'set'.
     * There is no obvious way to defer cloning a joined function object whose
     * identity will be used by DefinePropertyOnObject, e.g., or reflected via
     * js::GetOwnPropertyDescriptor, as the getter or setter callable object.
     */
    return !!JSObject::getGeneric(cx, obj, obj, id, vp);
}

bool
PropDesc::initialize(JSContext *cx, const Value &origval, bool checkAccessors)
{
    MOZ_ASSERT(isUndefined());

    RootedValue v(cx, origval);

    /* 8.10.5 step 1 */
    if (v.isPrimitive()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_NONNULL_OBJECT);
        return false;
    }
    RootedObject desc(cx, &v.toObject());

    isUndefined_ = false;

    /*
     * Start with the proper defaults.  XXX shouldn't be necessary when we get
     * rid of PropDesc::attributes()
     */
    attrs = JSPROP_PERMANENT | JSPROP_READONLY;

    bool found = false;
    RootedId id(cx);

    /* 8.10.5 step 3 */
    id = NameToId(cx->names().enumerable);
    if (!HasProperty(cx, desc, id, &v, &found))
        return false;
    if (found) {
        hasEnumerable_ = true;
        if (ToBoolean(v))
            attrs |= JSPROP_ENUMERATE;
    }

    /* 8.10.5 step 4 */
    id = NameToId(cx->names().configurable);
    if (!HasProperty(cx, desc, id, &v, &found))
        return false;
    if (found) {
        hasConfigurable_ = true;
        if (ToBoolean(v))
            attrs &= ~JSPROP_PERMANENT;
    }

    /* 8.10.5 step 5 */
    id = NameToId(cx->names().value);
    if (!HasProperty(cx, desc, id, &v, &found))
        return false;
    if (found) {
        hasValue_ = true;
        value_ = v;
    }

    /* 8.10.6 step 6 */
    id = NameToId(cx->names().writable);
    if (!HasProperty(cx, desc, id, &v, &found))
        return false;
    if (found) {
        hasWritable_ = true;
        if (ToBoolean(v))
            attrs &= ~JSPROP_READONLY;
    }

    /* 8.10.7 step 7 */
    id = NameToId(cx->names().get);
    if (!HasProperty(cx, desc, id, &v, &found))
        return false;
    if (found) {
        hasGet_ = true;
        get_ = v;
        attrs |= JSPROP_GETTER | JSPROP_SHARED;
        attrs &= ~JSPROP_READONLY;
        if (checkAccessors && !checkGetter(cx))
            return false;
    }

    /* 8.10.7 step 8 */
    id = NameToId(cx->names().set);
    if (!HasProperty(cx, desc, id, &v, &found))
        return false;
    if (found) {
        hasSet_ = true;
        set_ = v;
        attrs |= JSPROP_SETTER | JSPROP_SHARED;
        attrs &= ~JSPROP_READONLY;
        if (checkAccessors && !checkSetter(cx))
            return false;
    }

    /* 8.10.7 step 9 */
    if ((hasGet() || hasSet()) && (hasValue() || hasWritable())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INVALID_DESCRIPTOR);
        return false;
    }

    MOZ_ASSERT_IF(attrs & JSPROP_READONLY, !(attrs & (JSPROP_GETTER | JSPROP_SETTER)));

    return true;
}

void
PropDesc::complete()
{
    MOZ_ASSERT(!isUndefined());

    if (isGenericDescriptor() || isDataDescriptor()) {
        if (!hasValue_) {
            hasValue_ = true;
            value_.setUndefined();
        }
        if (!hasWritable_) {
            hasWritable_ = true;
            attrs |= JSPROP_READONLY;
        }
    } else {
        if (!hasGet_) {
            hasGet_ = true;
            get_.setUndefined();
        }
        if (!hasSet_) {
            hasSet_ = true;
            set_.setUndefined();
        }
    }
    if (!hasEnumerable_) {
        hasEnumerable_ = true;
        attrs &= ~JSPROP_ENUMERATE;
    }
    if (!hasConfigurable_) {
        hasConfigurable_ = true;
        attrs |= JSPROP_PERMANENT;
    }
}

bool
js::Throw(JSContext *cx, jsid id, unsigned errorNumber)
{
    MOZ_ASSERT(js_ErrorFormatString[errorNumber].argCount == 1);

    JSString *idstr = IdToString(cx, id);
    if (!idstr)
       return false;
    JSAutoByteString bytes(cx, idstr);
    if (!bytes)
        return false;
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, errorNumber, bytes.ptr());
    return false;
}

bool
js::Throw(JSContext *cx, JSObject *obj, unsigned errorNumber)
{
    if (js_ErrorFormatString[errorNumber].argCount == 1) {
        RootedValue val(cx, ObjectValue(*obj));
        js_ReportValueErrorFlags(cx, JSREPORT_ERROR, errorNumber,
                                 JSDVG_IGNORE_STACK, val, NullPtr(),
                                 nullptr, nullptr);
    } else {
        MOZ_ASSERT(js_ErrorFormatString[errorNumber].argCount == 0);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, errorNumber);
    }
    return false;
}

static bool
Reject(JSContext *cx, unsigned errorNumber, bool throwError, jsid id, bool *rval)
{
    if (throwError)
        return Throw(cx, id, errorNumber);

    *rval = false;
    return true;
}

static bool
Reject(JSContext *cx, JSObject *obj, unsigned errorNumber, bool throwError, bool *rval)
{
    if (throwError)
        return Throw(cx, obj, errorNumber);

    *rval = false;
    return true;
}

static bool
Reject(JSContext *cx, HandleId id, unsigned errorNumber, bool throwError, bool *rval)
{
    if (throwError)
        return Throw(cx, id, errorNumber);

    *rval = false;
    return true;
}

static unsigned
ApplyOrDefaultAttributes(unsigned attrs, Handle<PropertyDescriptor> desc)
{
    bool present = !!desc.object();
    bool enumerable = present ? desc.isEnumerable() : false;
    bool writable = present ? !desc.isReadonly() : false;
    bool configurable = present ? !desc.isPermanent() : false;
    return ApplyAttributes(attrs, enumerable, writable, configurable);
}

// See comments on CheckDefineProperty in jsfriendapi.h.
//
// DefinePropertyOnObject has its own implementation of these checks.
//
JS_FRIEND_API(bool)
js::CheckDefineProperty(JSContext *cx, HandleObject obj, HandleId id, HandleValue value,
                        unsigned attrs, PropertyOp getter, StrictPropertyOp setter)
{
    if (!obj->isNative())
        return true;

    // ES5 8.12.9 Step 1. Even though we know obj is native, we use generic
    // APIs for shorter, more readable code.
    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, obj, id, &desc))
        return false;

    // Appropriately handle the potential for ignored attributes. Since the proxy code calls us
    // directly, these might flow in legitimately. Ensure that we compare against the values that
    // are intended.
    attrs = ApplyOrDefaultAttributes(attrs, desc) & ~JSPROP_IGNORE_VALUE;

    // This does not have to check obj's extensibility when !desc.obj (steps
    // 2-3) because the low-level methods JSObject::{add,put}Property check
    // for that.
    if (desc.object() && desc.isPermanent()) {
        // Steps 6-11, skipping step 10.a.ii. Prohibit redefining a permanent
        // property with different metadata, except to make a writable property
        // non-writable.
        if ((getter != desc.getter() && !(getter == JS_PropertyStub && !desc.getter())) ||
            (setter != desc.setter() && !(setter == JS_StrictPropertyStub && !desc.setter())) ||
            (attrs != desc.attributes() && attrs != (desc.attributes() | JSPROP_READONLY)))
        {
            return Throw(cx, id, JSMSG_CANT_REDEFINE_PROP);
        }

        // Step 10.a.ii. Prohibit changing the value of a non-configurable,
        // non-writable data property.
        if ((desc.attributes() & (JSPROP_GETTER | JSPROP_SETTER | JSPROP_READONLY)) == JSPROP_READONLY) {
            bool same;
            if (!SameValue(cx, value, desc.value(), &same))
                return false;
            if (!same)
                return JSObject::reportReadOnly(cx, id);
        }
    }
    return true;
}

static bool
DefinePropertyOnObject(JSContext *cx, HandleNativeObject obj, HandleId id, const PropDesc &desc,
                       bool throwError, bool *rval)
{
    /* 8.12.9 step 1. */
    RootedShape shape(cx);
    RootedObject obj2(cx);
    MOZ_ASSERT(!obj->getOps()->lookupGeneric);
    if (!HasOwnProperty<CanGC>(cx, nullptr, obj, id, &obj2, &shape))
        return false;

    MOZ_ASSERT(!obj->getOps()->defineProperty);

    /* 8.12.9 steps 2-4. */
    if (!shape) {
        bool extensible;
        if (!JSObject::isExtensible(cx, obj, &extensible))
            return false;
        if (!extensible)
            return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);

        *rval = true;

        if (desc.isGenericDescriptor() || desc.isDataDescriptor()) {
            MOZ_ASSERT(!obj->getOps()->defineProperty);
            RootedValue v(cx, desc.hasValue() ? desc.value() : UndefinedValue());
            return baseops::DefineGeneric(cx, obj, id, v,
                                          JS_PropertyStub, JS_StrictPropertyStub,
                                          desc.attributes());
        }

        MOZ_ASSERT(desc.isAccessorDescriptor());

        return baseops::DefineGeneric(cx, obj, id, UndefinedHandleValue,
                                      desc.getter(), desc.setter(), desc.attributes());
    }

    /* 8.12.9 steps 5-6 (note 5 is merely a special case of 6). */
    RootedValue v(cx);

    MOZ_ASSERT(obj == obj2);

    bool shapeDataDescriptor = true,
         shapeAccessorDescriptor = false,
         shapeWritable = true,
         shapeConfigurable = true,
         shapeEnumerable = true,
         shapeHasDefaultGetter = true,
         shapeHasDefaultSetter = true,
         shapeHasGetterValue = false,
         shapeHasSetterValue = false;
    uint8_t shapeAttributes = GetShapeAttributes(obj, shape);
    if (!IsImplicitDenseOrTypedArrayElement(shape)) {
        shapeDataDescriptor = shape->isDataDescriptor();
        shapeAccessorDescriptor = shape->isAccessorDescriptor();
        shapeWritable = shape->writable();
        shapeConfigurable = shape->configurable();
        shapeEnumerable = shape->enumerable();
        shapeHasDefaultGetter = shape->hasDefaultGetter();
        shapeHasDefaultSetter = shape->hasDefaultSetter();
        shapeHasGetterValue = shape->hasGetterValue();
        shapeHasSetterValue = shape->hasSetterValue();
        shapeAttributes = shape->attributes();
    }

    do {
        if (desc.isAccessorDescriptor()) {
            if (!shapeAccessorDescriptor)
                break;

            if (desc.hasGet()) {
                bool same;
                if (!SameValue(cx, desc.getterValue(), shape->getterOrUndefined(), &same))
                    return false;
                if (!same)
                    break;
            }

            if (desc.hasSet()) {
                bool same;
                if (!SameValue(cx, desc.setterValue(), shape->setterOrUndefined(), &same))
                    return false;
                if (!same)
                    break;
            }
        } else {
            /*
             * Determine the current value of the property once, if the current
             * value might actually need to be used or preserved later.  NB: we
             * guard on whether the current property is a data descriptor to
             * avoid calling a getter; we won't need the value if it's not a
             * data descriptor.
             */
            if (IsImplicitDenseOrTypedArrayElement(shape)) {
                v = obj->getDenseOrTypedArrayElement(JSID_TO_INT(id));
            } else if (shape->isDataDescriptor()) {
                /*
                 * We must rule out a non-configurable js::PropertyOp-guarded
                 * property becoming a writable unguarded data property, since
                 * such a property can have its value changed to one the getter
                 * and setter preclude.
                 *
                 * A desc lacking writable but with value is a data descriptor
                 * and we must reject it as if it had writable: true if current
                 * is writable.
                 */
                if (!shape->configurable() &&
                    (!shape->hasDefaultGetter() || !shape->hasDefaultSetter()) &&
                    desc.isDataDescriptor() &&
                    (desc.hasWritable() ? desc.writable() : shape->writable()))
                {
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
                }

                if (!NativeGet(cx, obj, obj2.as<NativeObject>(), shape, &v))
                    return false;
            }

            if (desc.isDataDescriptor()) {
                if (!shapeDataDescriptor)
                    break;

                bool same;
                if (desc.hasValue()) {
                    if (!SameValue(cx, desc.value(), v, &same))
                        return false;
                    if (!same) {
                        /*
                         * Insist that a non-configurable js::PropertyOp data
                         * property is frozen at exactly the last-got value.
                         *
                         * Duplicate the first part of the big conjunction that
                         * we tested above, rather than add a local bool flag.
                         * Likewise, don't try to keep shape->writable() in a
                         * flag we veto from true to false for non-configurable
                         * PropertyOp-based data properties and test before the
                         * SameValue check later on in order to re-use that "if
                         * (!SameValue) Reject" logic.
                         *
                         * This function is large and complex enough that it
                         * seems best to repeat a small bit of code and return
                         * Reject(...) ASAP, instead of being clever.
                         */
                        if (!shapeConfigurable &&
                            (!shape->hasDefaultGetter() || !shape->hasDefaultSetter()))
                        {
                            return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
                        }
                        break;
                    }
                }
                if (desc.hasWritable() && desc.writable() != shapeWritable)
                    break;
            } else {
                /* The only fields in desc will be handled below. */
                MOZ_ASSERT(desc.isGenericDescriptor());
            }
        }

        if (desc.hasConfigurable() && desc.configurable() != shapeConfigurable)
            break;
        if (desc.hasEnumerable() && desc.enumerable() != shapeEnumerable)
            break;

        /* The conditions imposed by step 5 or step 6 apply. */
        *rval = true;
        return true;
    } while (0);

    /* 8.12.9 step 7. */
    if (!shapeConfigurable) {
        if ((desc.hasConfigurable() && desc.configurable()) ||
            (desc.hasEnumerable() && desc.enumerable() != shape->enumerable())) {
            return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
        }
    }

    bool callDelProperty = false;

    if (desc.isGenericDescriptor()) {
        /* 8.12.9 step 8, no validation required */
    } else if (desc.isDataDescriptor() != shapeDataDescriptor) {
        /* 8.12.9 step 9. */
        if (!shapeConfigurable)
            return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
    } else if (desc.isDataDescriptor()) {
        /* 8.12.9 step 10. */
        MOZ_ASSERT(shapeDataDescriptor);
        if (!shapeConfigurable && !shape->writable()) {
            if (desc.hasWritable() && desc.writable())
                return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            if (desc.hasValue()) {
                bool same;
                if (!SameValue(cx, desc.value(), v, &same))
                    return false;
                if (!same)
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            }
        }

        callDelProperty = !shapeHasDefaultGetter || !shapeHasDefaultSetter;
    } else {
        /* 8.12.9 step 11. */
        MOZ_ASSERT(desc.isAccessorDescriptor() && shape->isAccessorDescriptor());
        if (!shape->configurable()) {
            if (desc.hasSet()) {
                bool same;
                if (!SameValue(cx, desc.setterValue(), shape->setterOrUndefined(), &same))
                    return false;
                if (!same)
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            }

            if (desc.hasGet()) {
                bool same;
                if (!SameValue(cx, desc.getterValue(), shape->getterOrUndefined(), &same))
                    return false;
                if (!same)
                    return Reject(cx, JSMSG_CANT_REDEFINE_PROP, throwError, id, rval);
            }
        }
    }

    /* 8.12.9 step 12. */
    unsigned attrs;
    PropertyOp getter;
    StrictPropertyOp setter;
    if (desc.isGenericDescriptor()) {
        unsigned changed = 0;
        if (desc.hasConfigurable())
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable())
            changed |= JSPROP_ENUMERATE;

        attrs = (shapeAttributes & ~changed) | (desc.attributes() & changed);
        getter = IsImplicitDenseOrTypedArrayElement(shape) ? JS_PropertyStub : shape->getter();
        setter = IsImplicitDenseOrTypedArrayElement(shape) ? JS_StrictPropertyStub : shape->setter();
    } else if (desc.isDataDescriptor()) {
        unsigned unchanged = 0;
        if (!desc.hasConfigurable())
            unchanged |= JSPROP_PERMANENT;
        if (!desc.hasEnumerable())
            unchanged |= JSPROP_ENUMERATE;
        /* Watch out for accessor -> data transformations here. */
        if (!desc.hasWritable() && shapeDataDescriptor)
            unchanged |= JSPROP_READONLY;

        if (desc.hasValue())
            v = desc.value();
        attrs = (desc.attributes() & ~unchanged) | (shapeAttributes & unchanged);
        getter = JS_PropertyStub;
        setter = JS_StrictPropertyStub;
    } else {
        MOZ_ASSERT(desc.isAccessorDescriptor());

        /* 8.12.9 step 12. */
        unsigned changed = 0;
        if (desc.hasConfigurable())
            changed |= JSPROP_PERMANENT;
        if (desc.hasEnumerable())
            changed |= JSPROP_ENUMERATE;
        if (desc.hasGet())
            changed |= JSPROP_GETTER | JSPROP_SHARED | JSPROP_READONLY;
        if (desc.hasSet())
            changed |= JSPROP_SETTER | JSPROP_SHARED | JSPROP_READONLY;

        attrs = (desc.attributes() & changed) | (shapeAttributes & ~changed);
        if (desc.hasGet()) {
            getter = desc.getter();
        } else {
            getter = (shapeHasDefaultGetter && !shapeHasGetterValue)
                     ? JS_PropertyStub
                     : shape->getter();
        }
        if (desc.hasSet()) {
            setter = desc.setter();
        } else {
            setter = (shapeHasDefaultSetter && !shapeHasSetterValue)
                     ? JS_StrictPropertyStub
                     : shape->setter();
        }
    }

    *rval = true;

    /*
     * Since "data" properties implemented using native C functions may rely on
     * side effects during setting, we must make them aware that they have been
     * "assigned"; deleting the property before redefining it does the trick.
     * See bug 539766, where we ran into problems when we redefined
     * arguments.length without making the property aware that its value had
     * been changed (which would have happened if we had deleted it before
     * redefining it or we had invoked its setter to change its value).
     */
    if (callDelProperty) {
        bool succeeded;
        if (!CallJSDeletePropertyOp(cx, obj2->getClass()->delProperty, obj2, id, &succeeded))
            return false;
    }

    return baseops::DefineGeneric(cx, obj, id, v, getter, setter, attrs);
}

/* ES6 20130308 draft 8.4.2.1 [[DefineOwnProperty]] */
static bool
DefinePropertyOnArray(JSContext *cx, Handle<ArrayObject*> arr, HandleId id, const PropDesc &desc,
                      bool throwError, bool *rval)
{
    /* Step 2. */
    if (id == NameToId(cx->names().length)) {
        // Canonicalize value, if necessary, before proceeding any further.  It
        // would be better if this were always/only done by ArraySetLength.
        // But canonicalization may throw a RangeError (or other exception, if
        // the value is an object with user-defined conversion semantics)
        // before other attributes are checked.  So as long as our internal
        // defineProperty hook doesn't match the ECMA one, this duplicate
        // checking can't be helped.
        RootedValue v(cx);
        if (desc.hasValue()) {
            uint32_t newLen;
            if (!CanonicalizeArrayLengthValue<SequentialExecution>(cx, desc.value(), &newLen))
                return false;
            v.setNumber(newLen);
        } else {
            v.setNumber(arr->length());
        }

        if (desc.hasConfigurable() && desc.configurable())
            return Reject(cx, id, JSMSG_CANT_REDEFINE_PROP, throwError, rval);
        if (desc.hasEnumerable() && desc.enumerable())
            return Reject(cx, id, JSMSG_CANT_REDEFINE_PROP, throwError, rval);

        if (desc.isAccessorDescriptor())
            return Reject(cx, id, JSMSG_CANT_REDEFINE_PROP, throwError, rval);

        unsigned attrs = arr->lookup(cx, id)->attributes();
        if (!arr->lengthIsWritable()) {
            if (desc.hasWritable() && desc.writable())
                return Reject(cx, id, JSMSG_CANT_REDEFINE_PROP, throwError, rval);
        } else {
            if (desc.hasWritable() && !desc.writable())
                attrs = attrs | JSPROP_READONLY;
        }

        return ArraySetLength<SequentialExecution>(cx, arr, id, attrs, v, throwError);
    }

    /* Step 3. */
    uint32_t index;
    if (js_IdIsIndex(id, &index)) {
        /* Step 3b. */
        uint32_t oldLen = arr->length();

        /* Steps 3a, 3e. */
        if (index >= oldLen && !arr->lengthIsWritable())
            return Reject(cx, arr, JSMSG_CANT_APPEND_TO_ARRAY, throwError, rval);

        /* Steps 3f-j. */
        return DefinePropertyOnObject(cx, arr, id, desc, throwError, rval);
    }

    /* Step 4. */
    return DefinePropertyOnObject(cx, arr, id, desc, throwError, rval);
}

bool
js::DefineProperty(JSContext *cx, HandleObject obj, HandleId id, const PropDesc &desc,
                   bool throwError, bool *rval)
{
    if (obj->is<ArrayObject>()) {
        Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());
        return DefinePropertyOnArray(cx, arr, id, desc, throwError, rval);
    }

    if (obj->getOps()->lookupGeneric) {
        if (obj->is<ProxyObject>()) {
            Rooted<PropertyDescriptor> pd(cx);
            desc.populatePropertyDescriptor(obj, &pd);
            pd.object().set(obj);
            return Proxy::defineProperty(cx, obj, id, &pd);
        }
        return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, throwError, rval);
    }

    return DefinePropertyOnObject(cx, obj.as<NativeObject>(), id, desc, throwError, rval);
}

bool
js::DefineOwnProperty(JSContext *cx, HandleObject obj, HandleId id, HandleValue descriptor,
                      bool *bp)
{
    Rooted<PropDesc> desc(cx);
    if (!desc.initialize(cx, descriptor))
        return false;

    bool rval;
    if (!DefineProperty(cx, obj, id, desc, true, &rval))
        return false;
    *bp = !!rval;
    return true;
}

bool
js::DefineOwnProperty(JSContext *cx, HandleObject obj, HandleId id,
                      Handle<PropertyDescriptor> descriptor, bool *bp)
{
    Rooted<PropDesc> desc(cx);

    desc.initFromPropertyDescriptor(descriptor);

    bool rval;
    if (!DefineProperty(cx, obj, id, desc, true, &rval))
        return false;
    *bp = !!rval;
    return true;
}


bool
js::ReadPropertyDescriptors(JSContext *cx, HandleObject props, bool checkAccessors,
                            AutoIdVector *ids, AutoPropDescVector *descs)
{
    if (!GetPropertyKeys(cx, props, JSITER_OWNONLY | JSITER_SYMBOLS, ids))
        return false;

    RootedId id(cx);
    for (size_t i = 0, len = ids->length(); i < len; i++) {
        id = (*ids)[i];
        Rooted<PropDesc> desc(cx);
        RootedValue v(cx);
        if (!JSObject::getGeneric(cx, props, props, id, &v) ||
            !desc.initialize(cx, v, checkAccessors) ||
            !descs->append(desc))
        {
            return false;
        }
    }
    return true;
}

bool
js::DefineProperties(JSContext *cx, HandleObject obj, HandleObject props)
{
    AutoIdVector ids(cx);
    AutoPropDescVector descs(cx);
    if (!ReadPropertyDescriptors(cx, props, true, &ids, &descs))
        return false;

    if (obj->is<ArrayObject>()) {
        bool dummy;
        Rooted<ArrayObject*> arr(cx, &obj->as<ArrayObject>());
        for (size_t i = 0, len = ids.length(); i < len; i++) {
            if (!DefinePropertyOnArray(cx, arr, ids[i], descs[i], true, &dummy))
                return false;
        }
        return true;
    }

    if (obj->getOps()->lookupGeneric) {
        if (obj->is<ProxyObject>()) {
            Rooted<PropertyDescriptor> pd(cx);
            for (size_t i = 0, len = ids.length(); i < len; i++) {
                descs[i].populatePropertyDescriptor(obj, &pd);
                if (!Proxy::defineProperty(cx, obj, ids[i], &pd))
                    return false;
            }
            return true;
        }
        bool dummy;
        return Reject(cx, obj, JSMSG_OBJECT_NOT_EXTENSIBLE, true, &dummy);
    }

    bool dummy;
    for (size_t i = 0, len = ids.length(); i < len; i++) {
        if (!DefinePropertyOnObject(cx, obj.as<NativeObject>(), ids[i], descs[i], true, &dummy))
            return false;
    }

    return true;
}

extern bool
js_PopulateObject(JSContext *cx, HandleObject newborn, HandleObject props)
{
    return DefineProperties(cx, newborn, props);
}

js::types::TypeObject*
JSObject::uninlinedGetType(JSContext *cx)
{
    return getType(cx);
}

void
JSObject::uninlinedSetType(js::types::TypeObject *newType)
{
    setType(newType);
}

/* static */ inline unsigned
JSObject::getSealedOrFrozenAttributes(unsigned attrs, ImmutabilityType it)
{
    /* Make all attributes permanent; if freezing, make data attributes read-only. */
    if (it == FREEZE && !(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
        return JSPROP_PERMANENT | JSPROP_READONLY;
    return JSPROP_PERMANENT;
}

/* static */ bool
JSObject::sealOrFreeze(JSContext *cx, HandleObject obj, ImmutabilityType it)
{
    assertSameCompartment(cx, obj);
    MOZ_ASSERT(it == SEAL || it == FREEZE);

    if (!JSObject::preventExtensions(cx, obj))
        return false;

    AutoIdVector props(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_HIDDEN | JSITER_OWNONLY | JSITER_SYMBOLS, &props))
        return false;

    /* preventExtensions must sparsify dense objects, so we can assign to holes without checks. */
    MOZ_ASSERT_IF(obj->isNative(), obj->as<NativeObject>().getDenseCapacity() == 0);

    if (obj->isNative() && !obj->as<NativeObject>().inDictionaryMode() && !IsAnyTypedArray(obj)) {
        HandleNativeObject nobj = obj.as<NativeObject>();

        /*
         * Seal/freeze non-dictionary objects by constructing a new shape
         * hierarchy mirroring the original one, which can be shared if many
         * objects with the same structure are sealed/frozen. If we use the
         * generic path below then any non-empty object will be converted to
         * dictionary mode.
         */
        RootedShape last(cx, EmptyShape::getInitialShape(cx, nobj->getClass(),
                                                         nobj->getTaggedProto(),
                                                         nobj->getParent(),
                                                         nobj->getMetadata(),
                                                         nobj->numFixedSlots(),
                                                         nobj->lastProperty()->getObjectFlags()));
        if (!last)
            return false;

        /* Get an in order list of the shapes in this object. */
        AutoShapeVector shapes(cx);
        for (Shape::Range<NoGC> r(nobj->lastProperty()); !r.empty(); r.popFront()) {
            if (!shapes.append(&r.front()))
                return false;
        }
        Reverse(shapes.begin(), shapes.end());

        for (size_t i = 0; i < shapes.length(); i++) {
            StackShape unrootedChild(shapes[i]);
            RootedGeneric<StackShape*> child(cx, &unrootedChild);
            child->attrs |= getSealedOrFrozenAttributes(child->attrs, it);

            if (!JSID_IS_EMPTY(child->propid) && it == FREEZE)
                MarkTypePropertyNonWritable(cx, nobj, child->propid);

            last = cx->compartment()->propertyTree.getChild(cx, last, *child);
            if (!last)
                return false;
        }

        MOZ_ASSERT(nobj->lastProperty()->slotSpan() == last->slotSpan());
        JS_ALWAYS_TRUE(NativeObject::setLastProperty(cx, nobj, last));
    } else {
        RootedId id(cx);
        for (size_t i = 0; i < props.length(); i++) {
            id = props[i];

            unsigned attrs;
            if (!getGenericAttributes(cx, obj, id, &attrs))
                return false;

            unsigned new_attrs = getSealedOrFrozenAttributes(attrs, it);

            /* If we already have the attributes we need, skip the setAttributes call. */
            if ((attrs | new_attrs) == attrs)
                continue;

            attrs |= new_attrs;
            if (!setGenericAttributes(cx, obj, id, &attrs))
                return false;
        }
    }

    // Ordinarily ArraySetLength handles this, but we're going behind its back
    // right now, so we must do this manually.  Neither the custom property
    // tree mutations nor the setGenericAttributes call in the above code will
    // do this for us.
    //
    // ArraySetLength also implements the capacity <= length invariant for
    // arrays with non-writable length.  We don't need to do anything special
    // for that, because capacity was zeroed out by preventExtensions.  (See
    // the assertion before the if-else above.)
    if (it == FREEZE && obj->is<ArrayObject>()) {
        if (!obj->as<ArrayObject>().maybeCopyElementsForWrite(cx))
            return false;
        obj->as<ArrayObject>().getElementsHeader()->setNonwritableArrayLength();
    }

    return true;
}

/* static */ bool
JSObject::isSealedOrFrozen(JSContext *cx, HandleObject obj, ImmutabilityType it, bool *resultp)
{
    bool extensible;
    if (!JSObject::isExtensible(cx, obj, &extensible))
        return false;
    if (extensible) {
        *resultp = false;
        return true;
    }

    if (IsAnyTypedArray(obj)) {
        if (it == SEAL) {
            // Typed arrays are always sealed.
            *resultp = true;
        } else {
            // Typed arrays cannot be frozen, but an empty typed array is
            // trivially frozen.
            *resultp = (AnyTypedArrayLength(obj) == 0);
        }
        return true;
    }

    AutoIdVector props(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_HIDDEN | JSITER_OWNONLY | JSITER_SYMBOLS, &props))
        return false;

    RootedId id(cx);
    for (size_t i = 0, len = props.length(); i < len; i++) {
        id = props[i];

        unsigned attrs;
        if (!getGenericAttributes(cx, obj, id, &attrs))
            return false;

        /*
         * If the property is configurable, this object is neither sealed nor
         * frozen. If the property is a writable data property, this object is
         * not frozen.
         */
        if (!(attrs & JSPROP_PERMANENT) ||
            (it == FREEZE && !(attrs & (JSPROP_READONLY | JSPROP_GETTER | JSPROP_SETTER))))
        {
            *resultp = false;
            return true;
        }
    }

    /* All properties checked out. This object is sealed/frozen. */
    *resultp = true;
    return true;
}

/* static */
const char *
JSObject::className(JSContext *cx, HandleObject obj)
{
    assertSameCompartment(cx, obj);

    if (obj->is<ProxyObject>())
        return Proxy::className(cx, obj);

    return obj->getClass()->name;
}

/*
 * Get the GC kind to use for scripted 'new' on the given class.
 * FIXME bug 547327: estimate the size from the allocation site.
 */
static inline gc::AllocKind
NewObjectGCKind(const js::Class *clasp)
{
    if (clasp == &ArrayObject::class_)
        return gc::FINALIZE_OBJECT8;
    if (clasp == &JSFunction::class_)
        return gc::FINALIZE_OBJECT2;
    return gc::FINALIZE_OBJECT4;
}

static inline JSObject *
NewObject(ExclusiveContext *cx, types::TypeObject *type_, JSObject *parent, gc::AllocKind kind,
          NewObjectKind newKind)
{
    const Class *clasp = type_->clasp();

    MOZ_ASSERT(clasp != &ArrayObject::class_);
    MOZ_ASSERT_IF(clasp == &JSFunction::class_,
                  kind == JSFunction::FinalizeKind || kind == JSFunction::ExtendedFinalizeKind);
    MOZ_ASSERT_IF(parent, &parent->global() == cx->global());

    RootedTypeObject type(cx, type_);

    JSObject *metadata = nullptr;
    if (!NewObjectMetadata(cx, &metadata))
        return nullptr;

    // For objects which can have fixed data following the object, only use
    // enough fixed slots to cover the number of reserved slots in the object,
    // regardless of the allocation kind specified.
    size_t nfixed = ClassCanHaveFixedData(clasp)
                    ? GetGCKindSlots(gc::GetGCObjectKind(clasp), clasp)
                    : GetGCKindSlots(kind, clasp);

    RootedShape shape(cx, EmptyShape::getInitialShape(cx, clasp, type->proto(),
                                                      parent, metadata, nfixed));
    if (!shape)
        return nullptr;

    gc::InitialHeap heap = GetInitialHeap(newKind, clasp);
    JSObject *obj = JSObject::create(cx, kind, heap, shape, type);
    if (!obj)
        return nullptr;

    if (newKind == SingletonObject) {
        RootedObject nobj(cx, obj);
        if (!JSObject::setSingletonType(cx, nobj))
            return nullptr;
        obj = nobj;
    }

    /*
     * This will cancel an already-running incremental GC from doing any more
     * slices, and it will prevent any future incremental GCs.
     */
    bool globalWithoutCustomTrace = clasp->trace == JS_GlobalObjectTraceHook &&
                                    !cx->compartment()->options().getTrace();
    if (clasp->trace &&
        !globalWithoutCustomTrace &&
        !(clasp->flags & JSCLASS_IMPLEMENTS_BARRIERS))
    {
        if (!cx->shouldBeJSContext())
            return nullptr;
        JSRuntime *rt = cx->asJSContext()->runtime();
        rt->gc.disallowIncrementalGC();

#ifdef DEBUG
        if (rt->gc.gcMode() == JSGC_MODE_INCREMENTAL) {
            fprintf(stderr,
                    "The class %s has a trace hook but does not declare the\n"
                    "JSCLASS_IMPLEMENTS_BARRIERS flag. Please ensure that it correctly\n"
                    "implements write barriers and then set the flag.\n",
                    clasp->name);
            MOZ_CRASH();
        }
#endif
    }

    probes::CreateObject(cx, obj);
    return obj;
}

void
NewObjectCache::fillProto(EntryIndex entry, const Class *clasp, js::TaggedProto proto,
                          gc::AllocKind kind, NativeObject *obj)
{
    MOZ_ASSERT_IF(proto.isObject(), !proto.toObject()->is<GlobalObject>());
    MOZ_ASSERT(obj->getTaggedProto() == proto);
    return fill(entry, clasp, proto.raw(), kind, obj);
}

JSObject *
js::NewObjectWithGivenProto(ExclusiveContext *cxArg, const js::Class *clasp,
                            js::TaggedProto protoArg, JSObject *parentArg,
                            gc::AllocKind allocKind, NewObjectKind newKind)
{
    if (CanBeFinalizedInBackground(allocKind, clasp))
        allocKind = GetBackgroundAllocKind(allocKind);

    NewObjectCache::EntryIndex entry = -1;
    uint64_t gcNumber = 0;
    if (JSContext *cx = cxArg->maybeJSContext()) {
        JSRuntime *rt = cx->runtime();
        NewObjectCache &cache = rt->newObjectCache;
        if (protoArg.isObject() &&
            newKind == GenericObject &&
            clasp->isNative() &&
            !cx->compartment()->hasObjectMetadataCallback() &&
            (!parentArg || parentArg == protoArg.toObject()->getParent()) &&
            !protoArg.toObject()->is<GlobalObject>())
        {
            if (cache.lookupProto(clasp, protoArg.toObject(), allocKind, &entry)) {
                JSObject *obj = cache.newObjectFromHit<NoGC>(cx, entry, GetInitialHeap(newKind, clasp));
                if (obj) {
                    return obj;
                } else {
                    Rooted<TaggedProto> proto(cxArg, protoArg);
                    RootedObject parent(cxArg, parentArg);
                    obj = cache.newObjectFromHit<CanGC>(cx, entry, GetInitialHeap(newKind, clasp));
                    MOZ_ASSERT(!obj);
                    parentArg = parent;
                    protoArg = proto;
                }
            } else {
                gcNumber = rt->gc.gcNumber();
            }
        }
    }

    Rooted<TaggedProto> proto(cxArg, protoArg);
    RootedObject parent(cxArg, parentArg);

    types::TypeObject *type = cxArg->getNewType(clasp, proto, nullptr);
    if (!type)
        return nullptr;

    /*
     * Default parent to the parent of the prototype, which was set from
     * the parent of the prototype's constructor.
     */
    if (!parent && proto.isObject())
        parent = proto.toObject()->getParent();

    RootedObject obj(cxArg, NewObject(cxArg, type, parent, allocKind, newKind));
    if (!obj)
        return nullptr;

    if (entry != -1 && !obj->as<NativeObject>().hasDynamicSlots() &&
        cxArg->asJSContext()->runtime()->gc.gcNumber() == gcNumber)
    {
        cxArg->asJSContext()->runtime()->newObjectCache.fillProto(entry, clasp,
                                                                  proto, allocKind,
                                                                  &obj->as<NativeObject>());
    }

    return obj;
}

static JSProtoKey
ClassProtoKeyOrAnonymousOrNull(const js::Class *clasp)
{
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null)
        return key;
    if (clasp->flags & JSCLASS_IS_ANONYMOUS)
        return JSProto_Object;
    return JSProto_Null;
}

JSObject *
js::NewObjectWithClassProtoCommon(ExclusiveContext *cxArg,
                                  const js::Class *clasp, JSObject *protoArg, JSObject *parentArg,
                                  gc::AllocKind allocKind, NewObjectKind newKind)
{
    if (protoArg)
        return NewObjectWithGivenProto(cxArg, clasp, TaggedProto(protoArg), parentArg, allocKind, newKind);

    if (CanBeFinalizedInBackground(allocKind, clasp))
        allocKind = GetBackgroundAllocKind(allocKind);

    if (!parentArg)
        parentArg = cxArg->global();

    /*
     * Use the object cache, except for classes without a cached proto key.
     * On these objects, FindProto will do a dynamic property lookup to get
     * global[className].prototype, where changes to either the className or
     * prototype property would render the cached lookup incorrect. For classes
     * with a proto key, the prototype created during class initialization is
     * stored in an immutable slot on the global (except for ClearScope, which
     * will flush the new object cache).
     */
    JSProtoKey protoKey = ClassProtoKeyOrAnonymousOrNull(clasp);

    NewObjectCache::EntryIndex entry = -1;
    uint64_t gcNumber = 0;
    if (JSContext *cx = cxArg->maybeJSContext()) {
        JSRuntime *rt = cx->runtime();
        NewObjectCache &cache = rt->newObjectCache;
        if (parentArg->is<GlobalObject>() &&
            protoKey != JSProto_Null &&
            newKind == GenericObject &&
            clasp->isNative() &&
            !cx->compartment()->hasObjectMetadataCallback())
        {
            if (cache.lookupGlobal(clasp, &parentArg->as<GlobalObject>(), allocKind, &entry)) {
                JSObject *obj = cache.newObjectFromHit<NoGC>(cx, entry, GetInitialHeap(newKind, clasp));
                if (obj) {
                    return obj;
                } else {
                    RootedObject parent(cxArg, parentArg);
                    RootedObject proto(cxArg, protoArg);
                    obj = cache.newObjectFromHit<CanGC>(cx, entry, GetInitialHeap(newKind, clasp));
                    MOZ_ASSERT(!obj);
                    protoArg = proto;
                    parentArg = parent;
                }
            } else {
                gcNumber = rt->gc.gcNumber();
            }
        }
    }

    RootedObject parent(cxArg, parentArg);
    RootedObject proto(cxArg, protoArg);

    if (!FindProto(cxArg, clasp, &proto))
        return nullptr;

    Rooted<TaggedProto> taggedProto(cxArg, TaggedProto(proto));
    types::TypeObject *type = cxArg->getNewType(clasp, taggedProto);
    if (!type)
        return nullptr;

    JSObject *obj = NewObject(cxArg, type, parent, allocKind, newKind);
    if (!obj)
        return nullptr;

    if (entry != -1 && !obj->as<NativeObject>().hasDynamicSlots() &&
        cxArg->asJSContext()->runtime()->gc.gcNumber() == gcNumber)
    {
        cxArg->asJSContext()->runtime()->newObjectCache.fillGlobal(entry, clasp,
                                                                   &parent->as<GlobalObject>(),
                                                                   allocKind, &obj->as<NativeObject>());
    }

    return obj;
}

/*
 * Create a plain object with the specified type. This bypasses getNewType to
 * avoid losing creation site information for objects made by scripted 'new'.
 */
JSObject *
js::NewObjectWithType(JSContext *cx, HandleTypeObject type, JSObject *parent, gc::AllocKind allocKind,
                      NewObjectKind newKind)
{
    MOZ_ASSERT(parent);

    MOZ_ASSERT(allocKind <= gc::FINALIZE_OBJECT_LAST);
    if (CanBeFinalizedInBackground(allocKind, type->clasp()))
        allocKind = GetBackgroundAllocKind(allocKind);

    NewObjectCache &cache = cx->runtime()->newObjectCache;

    NewObjectCache::EntryIndex entry = -1;
    if (parent == type->proto().toObject()->getParent() &&
        newKind == GenericObject &&
        type->clasp()->isNative() &&
        !cx->compartment()->hasObjectMetadataCallback())
    {
        if (cache.lookupType(type, allocKind, &entry)) {
            JSObject *obj = cache.newObjectFromHit<NoGC>(cx, entry, GetInitialHeap(newKind, type->clasp()));
            if (obj) {
                return obj;
            } else {
                obj = cache.newObjectFromHit<CanGC>(cx, entry, GetInitialHeap(newKind, type->clasp()));
                parent = type->proto().toObject()->getParent();
            }
        }
    }

    JSObject *obj = NewObject(cx, type, parent, allocKind, newKind);
    if (!obj)
        return nullptr;

    if (entry != -1 && !obj->as<NativeObject>().hasDynamicSlots())
        cache.fillType(entry, type, allocKind, &obj->as<NativeObject>());

    return obj;
}

bool
js::NewObjectScriptedCall(JSContext *cx, MutableHandleObject pobj)
{
    jsbytecode *pc;
    RootedScript script(cx, cx->currentScript(&pc));
    gc::AllocKind allocKind = NewObjectGCKind(&JSObject::class_);
    NewObjectKind newKind = script
                            ? UseNewTypeForInitializer(script, pc, &JSObject::class_)
                            : GenericObject;
    RootedObject obj(cx, NewBuiltinClassInstance(cx, &JSObject::class_, allocKind, newKind));
    if (!obj)
        return false;

    if (script) {
        /* Try to specialize the type of the object to the scripted call site. */
        if (!types::SetInitializerObjectType(cx, script, pc, obj, newKind))
            return false;
    }

    pobj.set(obj);
    return true;
}

JSObject*
js::CreateThis(JSContext *cx, const Class *newclasp, HandleObject callee)
{
    RootedValue protov(cx);
    if (!JSObject::getProperty(cx, callee, callee, cx->names().prototype, &protov))
        return nullptr;

    JSObject *proto = protov.isObjectOrNull() ? protov.toObjectOrNull() : nullptr;
    JSObject *parent = callee->getParent();
    gc::AllocKind kind = NewObjectGCKind(newclasp);
    return NewObjectWithClassProto(cx, newclasp, proto, parent, kind);
}

static inline NativeObject *
CreateThisForFunctionWithType(JSContext *cx, HandleTypeObject type, JSObject *parent,
                              NewObjectKind newKind)
{
    if (types::TypeNewScript *newScript = type->newScript()) {
        if (newScript->analyzed()) {
            // The definite properties analysis has been performed for this
            // type, so get the shape and finalize kind to use from the
            // TypeNewScript's template.
            RootedNativeObject templateObject(cx, newScript->templateObject());
            MOZ_ASSERT(templateObject->type() == type);

            RootedNativeObject res(cx, CopyInitializerObject(cx, templateObject, newKind));
            if (!res)
                return nullptr;

            if (newKind == SingletonObject) {
                Rooted<TaggedProto> proto(cx, TaggedProto(templateObject->getProto()));
                if (!res->splicePrototype(cx, &JSObject::class_, proto))
                    return nullptr;
            } else {
                res->setType(type);
            }
            return res;
        }

        // The initial objects registered with a TypeNewScript can't be in the
        // nursery.
        if (newKind == GenericObject)
            newKind = MaybeSingletonObject;

        // Not enough objects with this type have been created yet, so make a
        // plain object and register it with the type. Use the maximum number
        // of fixed slots, as is also required by the TypeNewScript.
        gc::AllocKind allocKind = GuessObjectGCKind(NativeObject::MAX_FIXED_SLOTS);
        NativeObject *res = NewNativeObjectWithType(cx, type, parent, allocKind, newKind);
        if (!res)
            return nullptr;

        if (newKind != SingletonObject)
            newScript->registerNewObject(res);

        return res;
    }

    gc::AllocKind allocKind = NewObjectGCKind(&JSObject::class_);
    return NewNativeObjectWithType(cx, type, parent, allocKind, newKind);
}

NativeObject *
js::CreateThisForFunctionWithProto(JSContext *cx, HandleObject callee, JSObject *proto,
                                   NewObjectKind newKind /* = GenericObject */)
{
    RootedNativeObject res(cx);

    if (proto) {
        RootedTypeObject type(cx, cx->getNewType(&JSObject::class_, TaggedProto(proto), &callee->as<JSFunction>()));
        if (!type)
            return nullptr;

        if (type->newScript() && !type->newScript()->analyzed()) {
            bool regenerate;
            if (!type->newScript()->maybeAnalyze(cx, type, &regenerate))
                return nullptr;
            if (regenerate) {
                // The script was analyzed successfully and may have changed
                // the new type table, so refetch the type.
                type = cx->getNewType(&JSObject::class_, TaggedProto(proto), &callee->as<JSFunction>());
                MOZ_ASSERT(type && type->newScript());
            }
        }

        res = CreateThisForFunctionWithType(cx, type, callee->getParent(), newKind);
    } else {
        gc::AllocKind allocKind = NewObjectGCKind(&JSObject::class_);
        res = NewNativeObjectWithClassProto(cx, &JSObject::class_, proto,
                                            callee->getParent(), allocKind, newKind);
    }

    if (res) {
        JSScript *script = callee->as<JSFunction>().getOrCreateScript(cx);
        if (!script)
            return nullptr;
        TypeScript::SetThis(cx, script, types::Type::ObjectType(res));
    }

    return res;
}

NativeObject *
js::CreateThisForFunction(JSContext *cx, HandleObject callee, NewObjectKind newKind)
{
    RootedValue protov(cx);
    if (!JSObject::getProperty(cx, callee, callee, cx->names().prototype, &protov))
        return nullptr;
    JSObject *proto;
    if (protov.isObject())
        proto = &protov.toObject();
    else
        proto = nullptr;
    NativeObject *obj = CreateThisForFunctionWithProto(cx, callee, proto, newKind);

    if (obj && newKind == SingletonObject) {
        RootedNativeObject nobj(cx, obj);

        /* Reshape the singleton before passing it as the 'this' value. */
        NativeObject::clear(cx, nobj);

        JSScript *calleeScript = callee->as<JSFunction>().nonLazyScript();
        TypeScript::SetThis(cx, calleeScript, types::Type::ObjectType(nobj));

        return nobj;
    }

    return obj;
}

/* static */ bool
JSObject::nonNativeSetProperty(JSContext *cx, HandleObject obj,
                               HandleId id, MutableHandleValue vp, bool strict)
{
    if (MOZ_UNLIKELY(obj->watched())) {
        WatchpointMap *wpmap = cx->compartment()->watchpointMap;
        if (wpmap && !wpmap->triggerWatchpoint(cx, obj, id, vp))
            return false;
    }
    return obj->getOps()->setGeneric(cx, obj, id, vp, strict);
}

/* static */ bool
JSObject::nonNativeSetElement(JSContext *cx, HandleObject obj,
                              uint32_t index, MutableHandleValue vp, bool strict)
{
    if (MOZ_UNLIKELY(obj->watched())) {
        RootedId id(cx);
        if (!IndexToId(cx, index, &id))
            return false;

        WatchpointMap *wpmap = cx->compartment()->watchpointMap;
        if (wpmap && !wpmap->triggerWatchpoint(cx, obj, id, vp))
            return false;
    }
    return obj->getOps()->setElement(cx, obj, index, vp, strict);
}

JS_FRIEND_API(bool)
JS_CopyPropertyFrom(JSContext *cx, HandleId id, HandleObject target,
                    HandleObject obj)
{
    // |obj| and |cx| are generally not same-compartment with |target| here.
    assertSameCompartment(cx, obj, id);
    Rooted<JSPropertyDescriptor> desc(cx);

    if (!GetOwnPropertyDescriptor(cx, obj, id, &desc))
        return false;
    MOZ_ASSERT(desc.object());

    // Silently skip JSPropertyOp-implemented accessors.
    if (desc.getter() && !desc.hasGetterObject())
        return true;
    if (desc.setter() && !desc.hasSetterObject())
        return true;

    JSAutoCompartment ac(cx, target);
    RootedId wrappedId(cx, id);
    if (!cx->compartment()->wrap(cx, &desc))
        return false;

    bool ignored;
    return DefineOwnProperty(cx, target, wrappedId, desc, &ignored);
}

JS_FRIEND_API(bool)
JS_CopyPropertiesFrom(JSContext *cx, HandleObject target, HandleObject obj)
{
    JSAutoCompartment ac(cx, obj);

    AutoIdVector props(cx);
    if (!GetPropertyKeys(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &props))
        return false;

    for (size_t i = 0; i < props.length(); ++i) {
        if (!JS_CopyPropertyFrom(cx, props[i], target, obj))
            return false;
    }

    return true;
}

static bool
CopyProxyObject(JSContext *cx, Handle<ProxyObject *> from, Handle<ProxyObject *> to)
{
    MOZ_ASSERT(from->getClass() == to->getClass());

    if (from->is<WrapperObject>() &&
        (Wrapper::wrapperHandler(from)->flags() &
         Wrapper::CROSS_COMPARTMENT))
    {
        to->setCrossCompartmentPrivate(GetProxyPrivate(from));
    } else {
        RootedValue v(cx, GetProxyPrivate(from));
        if (!cx->compartment()->wrap(cx, &v))
            return false;
        to->setSameCompartmentPrivate(v);
    }

    RootedValue v(cx);
    for (size_t n = 0; n < PROXY_EXTRA_SLOTS; n++) {
        v = GetProxyExtra(from, n);
        if (!cx->compartment()->wrap(cx, &v))
            return false;
        SetProxyExtra(to, n, v);
    }

    return true;
}

JSObject *
js::CloneObject(JSContext *cx, HandleObject obj, Handle<js::TaggedProto> proto, HandleObject parent)
{
    if (!obj->isNative() && !obj->is<ProxyObject>()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_CANT_CLONE_OBJECT);
        return nullptr;
    }

    RootedObject clone(cx);
    if (obj->isNative()) {
        clone = NewObjectWithGivenProto(cx, obj->getClass(), proto, parent);
        if (!clone)
            return nullptr;

        if (clone->is<JSFunction>() && (obj->compartment() != clone->compartment())) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_CANT_CLONE_OBJECT);
            return nullptr;
        }

        if (obj->as<NativeObject>().hasPrivate())
            clone->as<NativeObject>().setPrivate(obj->as<NativeObject>().getPrivate());
    } else {
        ProxyOptions options;
        options.setClass(obj->getClass());

        clone = ProxyObject::New(cx, GetProxyHandler(obj), JS::NullHandleValue, proto, parent, options);
        if (!clone)
            return nullptr;

        if (!CopyProxyObject(cx, obj.as<ProxyObject>(), clone.as<ProxyObject>()))
            return nullptr;
    }

    return clone;
}

NativeObject *
js::DeepCloneObjectLiteral(JSContext *cx, HandleNativeObject obj, NewObjectKind newKind)
{
    /* NB: Keep this in sync with XDRObjectLiteral. */
    MOZ_ASSERT_IF(obj->hasSingletonType(),
                  JS::CompartmentOptionsRef(cx).getSingletonsAsTemplates());
    MOZ_ASSERT(obj->is<JSObject>() || obj->is<ArrayObject>());

    // Result of the clone function.
    RootedNativeObject clone(cx);

    // Temporary element/slot which would be stored in the cloned object.
    RootedValue v(cx);
    RootedNativeObject deepObj(cx);

    if (obj->is<ArrayObject>()) {
        clone = NewDenseUnallocatedArray(cx, obj->as<ArrayObject>().length(), nullptr, newKind);
    } else {
        // Object literals are tenured by default as holded by the JSScript.
        MOZ_ASSERT(obj->isTenured());
        AllocKind kind = obj->asTenured().getAllocKind();
        Rooted<TypeObject*> typeObj(cx, obj->getType(cx));
        if (!typeObj)
            return nullptr;
        RootedObject parent(cx, obj->getParent());
        clone = NewNativeObjectWithGivenProto(cx, &JSObject::class_,
                                              TaggedProto(typeObj->proto().toObject()),
                                              parent, kind, newKind);
    }

    // Allocate the same number of slots.
    if (!clone || !clone->ensureElements(cx, obj->getDenseCapacity()))
        return nullptr;

    // Copy the number of initialized elements.
    uint32_t initialized = obj->getDenseInitializedLength();
    if (initialized)
        clone->setDenseInitializedLength(initialized);

    // Recursive copy of dense element.
    for (uint32_t i = 0; i < initialized; ++i) {
        v = obj->getDenseElement(i);
        if (v.isObject()) {
            deepObj = &v.toObject().as<NativeObject>();
            deepObj = js::DeepCloneObjectLiteral(cx, deepObj, newKind);
            if (!deepObj) {
                JS_ReportOutOfMemory(cx);
                return nullptr;
            }
            v.setObject(*deepObj);
        }
        clone->initDenseElement(i, v);
    }

    MOZ_ASSERT(obj->compartment() == clone->compartment());
    MOZ_ASSERT(!obj->hasPrivate());
    RootedShape shape(cx, obj->lastProperty());
    size_t span = shape->slotSpan();
    clone->setLastProperty(cx, clone, shape);
    for (size_t i = 0; i < span; i++) {
        v = obj->getSlot(i);
        if (v.isObject()) {
            deepObj = &v.toObject().as<NativeObject>();
            deepObj = js::DeepCloneObjectLiteral(cx, deepObj, newKind);
            if (!deepObj)
                return nullptr;
            v.setObject(*deepObj);
        }
        clone->setSlot(i, v);
    }

    if (obj->hasSingletonType()) {
        if (!JSObject::setSingletonType(cx, clone))
            return nullptr;
    } else if (obj->is<ArrayObject>()) {
        FixArrayType(cx, &clone->as<ArrayObject>());
    } else {
        FixObjectType(cx, clone);
    }

    if (obj->is<ArrayObject>() && obj->denseElementsAreCopyOnWrite()) {
        if (!ObjectElements::MakeElementsCopyOnWrite(cx, clone))
            return nullptr;
    }

    return clone;
}

template<XDRMode mode>
bool
js::XDRObjectLiteral(XDRState<mode> *xdr, MutableHandleNativeObject obj)
{
    /* NB: Keep this in sync with DeepCloneObjectLiteral. */

    JSContext *cx = xdr->cx();
    MOZ_ASSERT_IF(mode == XDR_ENCODE && obj->hasSingletonType(),
                  JS::CompartmentOptionsRef(cx).getSingletonsAsTemplates());

    // Distinguish between objects and array classes.
    uint32_t isArray = 0;
    {
        if (mode == XDR_ENCODE) {
            MOZ_ASSERT(obj->is<JSObject>() || obj->is<ArrayObject>());
            isArray = obj->getClass() == &ArrayObject::class_ ? 1 : 0;
        }

        if (!xdr->codeUint32(&isArray))
            return false;
    }

    if (isArray) {
        uint32_t length;

        if (mode == XDR_ENCODE)
            length = obj->as<ArrayObject>().length();

        if (!xdr->codeUint32(&length))
            return false;

        if (mode == XDR_DECODE)
            obj.set(NewDenseUnallocatedArray(cx, length, NULL, js::MaybeSingletonObject));

    } else {
        // Code the alloc kind of the object.
        AllocKind kind;
        {
            if (mode == XDR_ENCODE) {
                MOZ_ASSERT(obj->getClass() == &JSObject::class_);
                MOZ_ASSERT(obj->isTenured());
                kind = obj->asTenured().getAllocKind();
            }

            if (!xdr->codeEnum32(&kind))
                return false;

            if (mode == XDR_DECODE) {
                obj.set(NewNativeBuiltinClassInstance(cx, &JSObject::class_, kind,
                                                      MaybeSingletonObject));
                if (!obj)
                    return false;
            }
        }
    }

    {
        uint32_t capacity;
        if (mode == XDR_ENCODE)
            capacity = obj->getDenseCapacity();
        if (!xdr->codeUint32(&capacity))
            return false;
        if (mode == XDR_DECODE) {
            if (!obj->ensureElements(cx, capacity)) {
                JS_ReportOutOfMemory(cx);
                return false;
            }
        }
    }

    uint32_t initialized;
    {
        if (mode == XDR_ENCODE)
            initialized = obj->getDenseInitializedLength();
        if (!xdr->codeUint32(&initialized))
            return false;
        if (mode == XDR_DECODE) {
            if (initialized)
                obj->setDenseInitializedLength(initialized);
        }
    }

    RootedValue tmpValue(cx);

    // Recursively copy dense elements.
    {
        for (unsigned i = 0; i < initialized; i++) {
            if (mode == XDR_ENCODE)
                tmpValue = obj->getDenseElement(i);

            if (!xdr->codeConstValue(&tmpValue))
                return false;

            if (mode == XDR_DECODE)
                obj->initDenseElement(i, tmpValue);
        }
    }

    MOZ_ASSERT(!obj->hasPrivate());
    RootedShape shape(cx, obj->lastProperty());

    // Code the number of slots in the vector.
    unsigned nslot = 0;

    // Code ids of the object in order. As opposed to DeepCloneObjectLiteral we
    // cannot just re-use the shape of the original bytecode value and we have
    // to write down the shape as well as the corresponding values.  Ideally we
    // would have a mechanism to serialize the shape too.
    js::AutoIdVector ids(cx);
    {
        if (mode == XDR_ENCODE && !shape->isEmptyShape()) {
            nslot = shape->slotSpan();
            if (!ids.reserve(nslot))
                return false;

            for (unsigned i = 0; i < nslot; i++)
                ids.infallibleAppend(JSID_VOID);

            for (Shape::Range<NoGC> it(shape); !it.empty(); it.popFront()) {
                // If we have reached the native property of the array class, we
                // exit as the remaining would only be reserved slots.
                if (!it.front().hasSlot()) {
                    MOZ_ASSERT(isArray);
                    break;
                }

                MOZ_ASSERT(it.front().hasDefaultGetter());
                ids[it.front().slot()].set(it.front().propid());
            }
        }

        if (!xdr->codeUint32(&nslot))
            return false;

        RootedAtom atom(cx);
        RootedId id(cx);
        uint32_t idType = 0;
        for (unsigned i = 0; i < nslot; i++) {
            if (mode == XDR_ENCODE) {
                id = ids[i];
                if (JSID_IS_INT(id))
                    idType = JSID_TYPE_INT;
                else if (JSID_IS_ATOM(id))
                    idType = JSID_TYPE_STRING;
                else
                    MOZ_CRASH("Symbol property is not yet supported by XDR.");

                tmpValue = obj->getSlot(i);
            }

            if (!xdr->codeUint32(&idType))
                return false;

            if (idType == JSID_TYPE_STRING) {
                if (mode == XDR_ENCODE)
                    atom = JSID_TO_ATOM(id);
                if (!XDRAtom(xdr, &atom))
                    return false;
                if (mode == XDR_DECODE)
                    id = AtomToId(atom);
            } else {
                MOZ_ASSERT(idType == JSID_TYPE_INT);
                uint32_t indexVal;
                if (mode == XDR_ENCODE)
                    indexVal = uint32_t(JSID_TO_INT(id));
                if (!xdr->codeUint32(&indexVal))
                    return false;
                if (mode == XDR_DECODE)
                    id = INT_TO_JSID(int32_t(indexVal));
            }

            if (!xdr->codeConstValue(&tmpValue))
                return false;

            if (mode == XDR_DECODE) {
                if (!DefineNativeProperty(cx, obj, id, tmpValue, NULL, NULL, JSPROP_ENUMERATE))
                    return false;
            }
        }

        MOZ_ASSERT_IF(mode == XDR_DECODE, !obj->inDictionaryMode());
    }

    // Fix up types, distinguishing singleton-typed objects.
    uint32_t isSingletonTyped;
    if (mode == XDR_ENCODE)
        isSingletonTyped = obj->hasSingletonType() ? 1 : 0;
    if (!xdr->codeUint32(&isSingletonTyped))
        return false;

    if (mode == XDR_DECODE) {
        if (isSingletonTyped) {
            if (!JSObject::setSingletonType(cx, obj))
                return false;
        } else if (isArray) {
            FixArrayType(cx, &obj->as<ArrayObject>());
        } else {
            FixObjectType(cx, obj);
        }
    }

    {
        uint32_t frozen;
        bool extensible;
        if (mode == XDR_ENCODE) {
            if (!JSObject::isExtensible(cx, obj, &extensible))
                return false;
            frozen = extensible ? 0 : 1;
        }
        if (!xdr->codeUint32(&frozen))
            return false;
        if (mode == XDR_DECODE && frozen == 1) {
            if (!JSObject::freeze(cx, obj))
                return false;
        }
    }

    if (isArray) {
        uint32_t copyOnWrite;
        if (mode == XDR_ENCODE)
            copyOnWrite = obj->denseElementsAreCopyOnWrite();
        if (!xdr->codeUint32(&copyOnWrite))
            return false;
        if (mode == XDR_DECODE && copyOnWrite) {
            if (!ObjectElements::MakeElementsCopyOnWrite(cx, obj))
                return false;
        }
    }

    return true;
}

template bool
js::XDRObjectLiteral(XDRState<XDR_ENCODE> *xdr, MutableHandleNativeObject obj);

template bool
js::XDRObjectLiteral(XDRState<XDR_DECODE> *xdr, MutableHandleNativeObject obj);

JSObject *
js::CloneObjectLiteral(JSContext *cx, HandleObject parent, HandleObject srcObj)
{
    if (srcObj->getClass() == &JSObject::class_) {
        AllocKind kind = GetBackgroundAllocKind(GuessObjectGCKind(srcObj->as<NativeObject>().numFixedSlots()));
        MOZ_ASSERT_IF(srcObj->isTenured(), kind == srcObj->asTenured().getAllocKind());

        JSObject *proto = cx->global()->getOrCreateObjectPrototype(cx);
        if (!proto)
            return nullptr;
        Rooted<TypeObject*> typeObj(cx, cx->getNewType(&JSObject::class_, TaggedProto(proto)));
        if (!typeObj)
            return nullptr;

        RootedShape shape(cx, srcObj->lastProperty());
        return NewReshapedObject(cx, typeObj, parent, kind, shape);
    }

    RootedArrayObject srcArray(cx, &srcObj->as<ArrayObject>());
    MOZ_ASSERT(srcArray->denseElementsAreCopyOnWrite());
    MOZ_ASSERT(srcArray->getElementsHeader()->ownerObject() == srcObj);

    size_t length = srcArray->as<ArrayObject>().length();
    RootedArrayObject res(cx, NewDenseFullyAllocatedArray(cx, length, nullptr, MaybeSingletonObject));
    if (!res)
        return nullptr;

    RootedId id(cx);
    RootedValue value(cx);
    for (size_t i = 0; i < length; i++) {
        // The only markable values in copy on write arrays are atoms, which
        // can be freely copied between compartments.
        value = srcArray->getDenseElement(i);
        MOZ_ASSERT_IF(value.isMarkable(),
                      value.toGCThing()->isTenured() &&
                      cx->runtime()->isAtomsZone(value.toGCThing()->asTenured().zone()));

        id = INT_TO_JSID(i);
        if (!JSObject::defineGeneric(cx, res, id, value, nullptr, nullptr, JSPROP_ENUMERATE))
            return nullptr;
    }

    if (!ObjectElements::MakeElementsCopyOnWrite(cx, res))
        return nullptr;

    return res;
}

void
NativeObject::fillInAfterSwap(JSContext *cx, const Vector<Value> &values, void *priv)
{
    // This object has just been swapped with some other object, and its shape
    // no longer reflects its allocated size. Correct this information and
    // fill the slots in with the specified values.
    MOZ_ASSERT(slotSpan() == values.length());

    // Make sure the shape's numFixedSlots() is correct.
    size_t nfixed = gc::GetGCKindSlots(asTenured().getAllocKind(), getClass());
    if (nfixed != shape_->numFixedSlots()) {
        if (!generateOwnShape(cx))
            CrashAtUnhandlableOOM("fillInAfterSwap");
        shape_->setNumFixedSlots(nfixed);
    }

    if (hasPrivate())
        setPrivate(priv);
    else
        MOZ_ASSERT(!priv);

    if (slots_) {
        js_free(slots_);
        slots_ = nullptr;
    }

    if (size_t ndynamic = dynamicSlotsCount(nfixed, values.length(), getClass())) {
        slots_ = cx->zone()->pod_malloc<HeapSlot>(ndynamic);
        if (!slots_)
            CrashAtUnhandlableOOM("fillInAfterSwap");
        Debug_SetSlotRangeToCrashOnTouch(slots_, ndynamic);
    }

    initSlotRange(0, values.begin(), values.length());
}

/* Use this method with extreme caution. It trades the guts of two objects. */
bool
JSObject::swap(JSContext *cx, HandleObject a, HandleObject b)
{
    // Ensure swap doesn't cause a finalizer to not be run.
    MOZ_ASSERT(IsBackgroundFinalized(a->asTenured().getAllocKind()) ==
               IsBackgroundFinalized(b->asTenured().getAllocKind()));
    MOZ_ASSERT(a->compartment() == b->compartment());

    AutoCompartment ac(cx, a);

    if (!a->getType(cx))
        CrashAtUnhandlableOOM("JSObject::swap");
    if (!b->getType(cx))
        CrashAtUnhandlableOOM("JSObject::swap");

    /*
     * Neither object may be in the nursery, but ensure we update any embedded
     * nursery pointers in either object.
     */
#ifdef JSGC_GENERATIONAL
    MOZ_ASSERT(!IsInsideNursery(a) && !IsInsideNursery(b));
    cx->runtime()->gc.storeBuffer.putWholeCellFromMainThread(a);
    cx->runtime()->gc.storeBuffer.putWholeCellFromMainThread(b);
#endif

    unsigned r = NotifyGCPreSwap(a, b);

    // Do the fundamental swapping of the contents of two objects.
    MOZ_ASSERT(a->compartment() == b->compartment());
    MOZ_ASSERT(a->is<JSFunction>() == b->is<JSFunction>());

    // Don't try to swap functions with different sizes.
    MOZ_ASSERT_IF(a->is<JSFunction>(), a->tenuredSizeOfThis() == b->tenuredSizeOfThis());

    // Watch for oddball objects that have special organizational issues and
    // can't be swapped.
    MOZ_ASSERT(!a->is<RegExpObject>() && !b->is<RegExpObject>());
    MOZ_ASSERT(!a->is<ArrayObject>() && !b->is<ArrayObject>());
    MOZ_ASSERT(!a->is<ArrayBufferObject>() && !b->is<ArrayBufferObject>());
    MOZ_ASSERT(!a->is<TypedArrayObject>() && !b->is<TypedArrayObject>());
    MOZ_ASSERT(!a->is<TypedObject>() && !b->is<TypedObject>());

    if (a->tenuredSizeOfThis() == b->tenuredSizeOfThis()) {
        // When both objects are the same size, just do a plain swap of their
        // contents.
        size_t size = a->tenuredSizeOfThis();

        char tmp[mozilla::tl::Max<sizeof(JSFunction), sizeof(JSObject_Slots16)>::value];
        MOZ_ASSERT(size <= sizeof(tmp));

        js_memcpy(tmp, a, size);
        js_memcpy(a, b, size);
        js_memcpy(b, tmp, size);
    } else {
        // Avoid GC in here to avoid confusing the tracing code with our
        // intermediate state.
        AutoSuppressGC suppress(cx);

        // When the objects have different sizes, they will have different
        // numbers of fixed slots before and after the swap, so the slots for
        // native objects will need to be rearranged.
        NativeObject *na = a->isNative() ? &a->as<NativeObject>() : nullptr;
        NativeObject *nb = b->isNative() ? &b->as<NativeObject>() : nullptr;

        // Remember the original values from the objects.
        Vector<Value> avals(cx);
        void *apriv = nullptr;
        if (na) {
            apriv = na->hasPrivate() ? na->getPrivate() : nullptr;
            for (size_t i = 0; i < na->slotSpan(); i++) {
                if (!avals.append(na->getSlot(i)))
                    CrashAtUnhandlableOOM("JSObject::swap");
            }
        }
        Vector<Value> bvals(cx);
        void *bpriv = nullptr;
        if (nb) {
            bpriv = nb->hasPrivate() ? nb->getPrivate() : nullptr;
            for (size_t i = 0; i < nb->slotSpan(); i++) {
                if (!bvals.append(nb->getSlot(i)))
                    CrashAtUnhandlableOOM("JSObject::swap");
            }
        }

        // Swap the main fields of the objects, whether they are native objects or proxies.
        char tmp[sizeof(JSObject_Slots0)];
        js_memcpy(&tmp, a, sizeof tmp);
        js_memcpy(a, b, sizeof tmp);
        js_memcpy(b, &tmp, sizeof tmp);

        if (na)
            b->as<NativeObject>().fillInAfterSwap(cx, avals, apriv);
        if (nb)
            a->as<NativeObject>().fillInAfterSwap(cx, bvals, bpriv);
    }

    // Dictionary shapes can point back to their containing objects, so fix
    // those pointers up.
    if (a->isNative() && a->as<NativeObject>().inDictionaryMode())
        a->lastProperty()->listp = &a->shape_;
    if (b->isNative() && b->as<NativeObject>().inDictionaryMode())
        b->lastProperty()->listp = &b->shape_;

    // Swapping the contents of two objects invalidates type sets which contain
    // either of the objects, so mark all such sets as unknown.
    MarkTypeObjectUnknownProperties(cx, a->type(), !a->hasSingletonType());
    MarkTypeObjectUnknownProperties(cx, b->type(), !b->hasSingletonType());

#ifdef JSGC_INCREMENTAL
    /*
     * We need a write barrier here. If |a| was marked and |b| was not, then
     * after the swap, |b|'s guts would never be marked. The write barrier
     * solves this.
     *
     * Normally write barriers happen before the write. However, that's not
     * necessary here because nothing is being destroyed. We're just swapping.
     */
    JS::Zone *zone = a->zone();
    if (zone->needsIncrementalBarrier()) {
        MarkChildren(zone->barrierTracer(), a);
        MarkChildren(zone->barrierTracer(), b);
    }
#endif

    NotifyGCPostSwap(a, b, r);
    return true;
}

static bool
DefineStandardSlot(JSContext *cx, HandleObject obj, JSProtoKey key, JSAtom *atom,
                   HandleValue v, uint32_t attrs, bool &named)
{
    RootedId id(cx, AtomToId(atom));

    if (key != JSProto_Null) {
        /*
         * Initializing an actual standard class on a global object. If the
         * property is not yet present, force it into a new one bound to a
         * reserved slot. Otherwise, go through the normal property path.
         */
        Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

        if (!global->lookup(cx, id)) {
            global->setConstructorPropertySlot(key, v);

            uint32_t slot = GlobalObject::constructorPropertySlot(key);
            if (!NativeObject::addProperty(cx, global, id, JS_PropertyStub, JS_StrictPropertyStub, slot, attrs, 0))
                return false;

            named = true;
            return true;
        }
    }

    named = JSObject::defineGeneric(cx, obj, id,
                                    v, JS_PropertyStub, JS_StrictPropertyStub, attrs);
    return named;
}

static void
SetClassObject(JSObject *obj, JSProtoKey key, JSObject *cobj, JSObject *proto)
{
    MOZ_ASSERT(!obj->getParent());
    if (!obj->is<GlobalObject>())
        return;

    obj->as<GlobalObject>().setConstructor(key, ObjectOrNullValue(cobj));
    obj->as<GlobalObject>().setPrototype(key, ObjectOrNullValue(proto));
}

static void
ClearClassObject(JSObject *obj, JSProtoKey key)
{
    MOZ_ASSERT(!obj->getParent());
    if (!obj->is<GlobalObject>())
        return;

    obj->as<GlobalObject>().setConstructor(key, UndefinedValue());
    obj->as<GlobalObject>().setPrototype(key, UndefinedValue());
}

static NativeObject *
DefineConstructorAndPrototype(JSContext *cx, HandleObject obj, JSProtoKey key, HandleAtom atom,
                              JSObject *protoProto, const Class *clasp,
                              Native constructor, unsigned nargs,
                              const JSPropertySpec *ps, const JSFunctionSpec *fs,
                              const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs,
                              NativeObject **ctorp, AllocKind ctorKind)
{
    /*
     * Create a prototype object for this class.
     *
     * FIXME: lazy standard (built-in) class initialization and even older
     * eager boostrapping code rely on all of these properties:
     *
     * 1. NewObject attempting to compute a default prototype object when
     *    passed null for proto; and
     *
     * 2. NewObject tolerating no default prototype (null proto slot value)
     *    due to this js_InitClass call coming from js_InitFunctionClass on an
     *    otherwise-uninitialized global.
     *
     * 3. NewObject allocating a JSFunction-sized GC-thing when clasp is
     *    &JSFunction::class_, not a JSObject-sized (smaller) GC-thing.
     *
     * The JS_NewObjectForGivenProto and JS_NewObject APIs also allow clasp to
     * be &JSFunction::class_ (we could break compatibility easily). But
     * fixing (3) is not enough without addressing the bootstrapping dependency
     * on (1) and (2).
     */

    /*
     * Create the prototype object.  (GlobalObject::createBlankPrototype isn't
     * used because it parents the prototype object to the global and because
     * it uses WithProto::Given.  FIXME: Undo dependencies on this parentage
     * [which already needs to happen for bug 638316], figure out nicer
     * semantics for null-protoProto, and use createBlankPrototype.)
     */
    RootedNativeObject proto(cx, NewNativeObjectWithClassProto(cx, clasp, protoProto, obj, SingletonObject));
    if (!proto)
        return nullptr;

    /* After this point, control must exit via label bad or out. */
    RootedNativeObject ctor(cx);
    bool named = false;
    bool cached = false;
    if (!constructor) {
        /*
         * Lacking a constructor, name the prototype (e.g., Math) unless this
         * class (a) is anonymous, i.e. for internal use only; (b) the class
         * of obj (the global object) is has a reserved slot indexed by key;
         * and (c) key is not the null key.
         */
        if (!(clasp->flags & JSCLASS_IS_ANONYMOUS) || !obj->is<GlobalObject>() ||
            key == JSProto_Null)
        {
            uint32_t attrs = (clasp->flags & JSCLASS_IS_ANONYMOUS)
                           ? JSPROP_READONLY | JSPROP_PERMANENT
                           : 0;
            RootedValue value(cx, ObjectValue(*proto));
            if (!DefineStandardSlot(cx, obj, key, atom, value, attrs, named))
                goto bad;
        }

        ctor = proto;
    } else {
        /*
         * Create the constructor, not using GlobalObject::createConstructor
         * because the constructor currently must have |obj| as its parent.
         * (FIXME: remove this dependency on the exact identity of the parent,
         * perhaps as part of bug 638316.)
         */
        RootedFunction fun(cx, NewFunction(cx, js::NullPtr(), constructor, nargs,
                                           JSFunction::NATIVE_CTOR, obj, atom, ctorKind));
        if (!fun)
            goto bad;

        /*
         * Set the class object early for standard class constructors. Type
         * inference may need to access these, and js::GetBuiltinPrototype will
         * fail if it tries to do a reentrant reconstruction of the class.
         */
        if (key != JSProto_Null) {
            SetClassObject(obj, key, fun, proto);
            cached = true;
        }

        RootedValue value(cx, ObjectValue(*fun));
        if (!DefineStandardSlot(cx, obj, key, atom, value, 0, named))
            goto bad;

        /*
         * Optionally construct the prototype object, before the class has
         * been fully initialized.  Allow the ctor to replace proto with a
         * different object, as is done for operator new.
         */
        ctor = fun;
        if (!LinkConstructorAndPrototype(cx, ctor, proto))
            goto bad;

        /* Bootstrap Function.prototype (see also JS_InitStandardClasses). */
        Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
        if (ctor->getClass() == clasp && !ctor->splicePrototype(cx, clasp, tagged))
            goto bad;
    }

    if (!DefinePropertiesAndFunctions(cx, proto, ps, fs) ||
        (ctor != proto && !DefinePropertiesAndFunctions(cx, ctor, static_ps, static_fs)))
    {
        goto bad;
    }

    /* If this is a standard class, cache its prototype. */
    if (!cached && key != JSProto_Null)
        SetClassObject(obj, key, ctor, proto);

    if (ctorp)
        *ctorp = ctor;
    return proto;

bad:
    if (named) {
        bool succeeded;
        RootedId id(cx, AtomToId(atom));
        JSObject::deleteGeneric(cx, obj, id, &succeeded);
    }
    if (cached)
        ClearClassObject(obj, key);
    return nullptr;
}

NativeObject *
js_InitClass(JSContext *cx, HandleObject obj, JSObject *protoProto_,
             const Class *clasp, Native constructor, unsigned nargs,
             const JSPropertySpec *ps, const JSFunctionSpec *fs,
             const JSPropertySpec *static_ps, const JSFunctionSpec *static_fs,
             NativeObject **ctorp, AllocKind ctorKind)
{
    RootedObject protoProto(cx, protoProto_);

    /* Assert mandatory function pointer members. */
    MOZ_ASSERT(clasp->addProperty);
    MOZ_ASSERT(clasp->delProperty);
    MOZ_ASSERT(clasp->getProperty);
    MOZ_ASSERT(clasp->setProperty);
    MOZ_ASSERT(clasp->enumerate);
    MOZ_ASSERT(clasp->resolve);
    MOZ_ASSERT(clasp->convert);

    RootedAtom atom(cx, Atomize(cx, clasp->name, strlen(clasp->name)));
    if (!atom)
        return nullptr;

    /*
     * All instances of the class will inherit properties from the prototype
     * object we are about to create (in DefineConstructorAndPrototype), which
     * in turn will inherit from protoProto.
     *
     * When initializing a standard class (other than Object), if protoProto is
     * null, default to Object.prototype. The engine's internal uses of
     * js_InitClass depend on this nicety.
     */
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(clasp);
    if (key != JSProto_Null &&
        !protoProto &&
        !GetBuiltinPrototype(cx, JSProto_Object, &protoProto))
    {
        return nullptr;
    }

    return DefineConstructorAndPrototype(cx, obj, key, atom, protoProto, clasp, constructor, nargs,
                                         ps, fs, static_ps, static_fs, ctorp, ctorKind);
}

void
JSObject::fixupAfterMovingGC()
{
    /*
     * If this is a copy-on-write elements we may need to fix up both the
     * elements' pointer back to the owner object, and the elements pointer
     * itself if it points to inline elements in another object.
     */
    if (is<NativeObject>() && as<NativeObject>().hasDynamicElements()) {
        ObjectElements *header = as<NativeObject>().getElementsHeader();
        if (header->isCopyOnWrite()) {
            HeapPtrNativeObject &owner = header->ownerObject();
            if (IsForwarded(owner.get()))
                owner = Forwarded(owner.get());
            as<NativeObject>().elements_ = owner->getElementsHeader()->elements();
        }
    }
}

bool
js::SetClassAndProto(JSContext *cx, HandleObject obj,
                     const Class *clasp, Handle<js::TaggedProto> proto)
{
    /*
     * Regenerate shapes for all of the scopes along the old prototype chain,
     * in case any entries were filled by looking up through obj. Stop when a
     * non-native object is found, prototype lookups will not be cached across
     * these.
     *
     * How this shape change is done is very delicate; the change can be made
     * either by marking the object's prototype as uncacheable (such that the
     * property cache and JIT'ed ICs cannot assume the shape determines the
     * prototype) or by just generating a new shape for the object. Choosing
     * the former is bad if the object is on the prototype chain of other
     * objects, as the uncacheable prototype can inhibit iterator caches on
     * those objects and slow down prototype accesses. Choosing the latter is
     * bad if there are many similar objects to this one which will have their
     * prototype mutated, as the generateOwnShape forces the object into
     * dictionary mode and similar property lineages will be repeatedly cloned.
     *
     * :XXX: bug 707717 make this code less brittle.
     */
    RootedObject oldproto(cx, obj);
    while (oldproto && oldproto->isNative()) {
        if (oldproto->hasSingletonType()) {
            if (!oldproto->as<NativeObject>().generateOwnShape(cx))
                return false;
        } else {
            if (!oldproto->setUncacheableProto(cx))
                return false;
        }
        oldproto = oldproto->getProto();
    }

    if (obj->hasSingletonType()) {
        /*
         * Just splice the prototype, but mark the properties as unknown for
         * consistent behavior.
         */
        if (!obj->splicePrototype(cx, clasp, proto))
            return false;
        MarkTypeObjectUnknownProperties(cx, obj->type());
        return true;
    }

    if (proto.isObject()) {
        RootedObject protoObj(cx, proto.toObject());
        if (!JSObject::setNewTypeUnknown(cx, clasp, protoObj))
            return false;
    }

    TypeObject *type = cx->getNewType(clasp, proto);
    if (!type)
        return false;

    /*
     * Setting __proto__ on an object that has escaped and may be referenced by
     * other heap objects can only be done if the properties of both objects
     * are unknown. Type sets containing this object will contain the original
     * type but not the new type of the object, so we need to go and scan the
     * entire compartment for type sets which have these objects and mark them
     * as containing generic objects.
     */
    MarkTypeObjectUnknownProperties(cx, obj->type(), true);
    MarkTypeObjectUnknownProperties(cx, type, true);

    obj->setType(type);

    return true;
}

static bool
MaybeResolveConstructor(ExclusiveContext *cxArg, Handle<GlobalObject*> global, JSProtoKey key)
{
    if (global->isStandardClassResolved(key))
        return true;
    if (!cxArg->shouldBeJSContext())
        return false;

    JSContext *cx = cxArg->asJSContext();
    return GlobalObject::resolveConstructor(cx, global, key);
}

bool
js::GetBuiltinConstructor(ExclusiveContext *cx, JSProtoKey key, MutableHandleObject objp)
{
    MOZ_ASSERT(key != JSProto_Null);
    Rooted<GlobalObject*> global(cx, cx->global());
    if (!MaybeResolveConstructor(cx, global, key))
        return false;

    objp.set(&global->getConstructor(key).toObject());
    return true;
}

bool
js::GetBuiltinPrototype(ExclusiveContext *cx, JSProtoKey key, MutableHandleObject protop)
{
    MOZ_ASSERT(key != JSProto_Null);
    Rooted<GlobalObject*> global(cx, cx->global());
    if (!MaybeResolveConstructor(cx, global, key))
        return false;

    protop.set(&global->getPrototype(key).toObject());
    return true;
}

static bool
IsStandardPrototype(JSObject *obj, JSProtoKey key)
{
    GlobalObject &global = obj->global();
    Value v = global.getPrototype(key);
    return v.isObject() && obj == &v.toObject();
}

JSProtoKey
JS::IdentifyStandardInstance(JSObject *obj)
{
    // Note: The prototype shares its JSClass with instances.
    MOZ_ASSERT(!obj->is<CrossCompartmentWrapperObject>());
    JSProtoKey key = StandardProtoKeyOrNull(obj);
    if (key != JSProto_Null && !IsStandardPrototype(obj, key))
        return key;
    return JSProto_Null;
}

JSProtoKey
JS::IdentifyStandardPrototype(JSObject *obj)
{
    // Note: The prototype shares its JSClass with instances.
    MOZ_ASSERT(!obj->is<CrossCompartmentWrapperObject>());
    JSProtoKey key = StandardProtoKeyOrNull(obj);
    if (key != JSProto_Null && IsStandardPrototype(obj, key))
        return key;
    return JSProto_Null;
}

JSProtoKey
JS::IdentifyStandardInstanceOrPrototype(JSObject *obj)
{
    return StandardProtoKeyOrNull(obj);
}

JSProtoKey
JS::IdentifyStandardConstructor(JSObject *obj)
{
    // Note that NATIVE_CTOR does not imply that we are a standard constructor,
    // but the converse is true (at least until we start having self-hosted
    // constructors for standard classes). This lets us avoid a costly loop for
    // many functions (which, depending on the call site, may be the common case).
    if (!obj->is<JSFunction>() || !(obj->as<JSFunction>().flags() & JSFunction::NATIVE_CTOR))
        return JSProto_Null;

    GlobalObject &global = obj->global();
    for (size_t k = 0; k < JSProto_LIMIT; ++k) {
        JSProtoKey key = static_cast<JSProtoKey>(k);
        if (global.getConstructor(key) == ObjectValue(*obj))
            return key;
    }

    return JSProto_Null;
}

bool
js::FindClassObject(ExclusiveContext *cx, MutableHandleObject protop, const Class *clasp)
{
    JSProtoKey protoKey = ClassProtoKeyOrAnonymousOrNull(clasp);
    if (protoKey != JSProto_Null) {
        MOZ_ASSERT(JSProto_Null < protoKey);
        MOZ_ASSERT(protoKey < JSProto_LIMIT);
        return GetBuiltinConstructor(cx, protoKey, protop);
    }

    JSAtom *atom = Atomize(cx, clasp->name, strlen(clasp->name));
    if (!atom)
        return false;
    RootedId id(cx, AtomToId(atom));

    RootedObject pobj(cx);
    RootedShape shape(cx);
    if (!LookupNativeProperty(cx, cx->global(), id, &pobj, &shape))
        return false;
    RootedValue v(cx);
    if (shape && pobj->isNative()) {
        if (shape->hasSlot())
            v = pobj->as<NativeObject>().getSlot(shape->slot());
    }
    if (v.isObject())
        protop.set(&v.toObject());
    return true;
}

bool
JSObject::isCallable() const
{
    if (is<JSFunction>())
        return true;
    return callHook() != nullptr;
}

bool
JSObject::isConstructor() const
{
    if (is<JSFunction>()) {
        const JSFunction &fun = as<JSFunction>();
        return fun.isNativeConstructor() || fun.isInterpretedConstructor();
    }
    return constructHook() != nullptr;
}

JSNative
JSObject::callHook() const
{
    const js::Class *clasp = getClass();

    if (clasp->call)
        return clasp->call;

    if (is<js::ProxyObject>()) {
        const js::ProxyObject &p = as<js::ProxyObject>();
        if (p.handler()->isCallable(const_cast<JSObject*>(this)))
            return js::proxy_Call;
    }
    return nullptr;
}

JSNative
JSObject::constructHook() const
{
    const js::Class *clasp = getClass();

    if (clasp->construct)
        return clasp->construct;

    if (is<js::ProxyObject>()) {
        const js::ProxyObject &p = as<js::ProxyObject>();
        if (p.handler()->isConstructor(const_cast<JSObject*>(this)))
            return js::proxy_Construct;
    }
    return nullptr;
}

/* static */ bool
JSObject::defineGeneric(ExclusiveContext *cx, HandleObject obj,
                        HandleId id, HandleValue value,
                        JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    MOZ_ASSERT(!(attrs & JSPROP_NATIVE_ACCESSORS));
    js::DefineGenericOp op = obj->getOps()->defineGeneric;
    if (op) {
        if (!cx->shouldBeJSContext())
            return false;
        return op(cx->asJSContext(), obj, id, value, getter, setter, attrs);
    }
    return baseops::DefineGeneric(cx, obj.as<NativeObject>(), id, value, getter, setter, attrs);
}

/* static */ bool
JSObject::defineProperty(ExclusiveContext *cx, HandleObject obj,
                         PropertyName *name, HandleValue value,
                         JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    RootedId id(cx, NameToId(name));
    return defineGeneric(cx, obj, id, value, getter, setter, attrs);
}

/* static */ bool
JSObject::defineElement(ExclusiveContext *cx, HandleObject obj,
                        uint32_t index, HandleValue value,
                        JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs)
{
    js::DefineElementOp op = obj->getOps()->defineElement;
    if (op) {
        if (!cx->shouldBeJSContext())
            return false;
        return op(cx->asJSContext(), obj, index, value, getter, setter, attrs);
    }
    return baseops::DefineElement(cx, obj.as<NativeObject>(), index, value, getter, setter, attrs);
}

/* static */ bool
JSObject::lookupGeneric(JSContext *cx, HandleObject obj, js::HandleId id,
                        MutableHandleObject objp, MutableHandleShape propp)
{
    /* NB: The logic of lookupGeneric is implicitly reflected in
     *     BaselineIC.cpp's |EffectlesslyLookupProperty| logic.
     *     If this changes, please remember to update the logic there as well.
     */
    LookupGenericOp op = obj->getOps()->lookupGeneric;
    if (op)
        return op(cx, obj, id, objp, propp);
    return baseops::LookupProperty<CanGC>(cx, obj.as<NativeObject>(), id, objp, propp);
}

bool
js::LookupName(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
               MutableHandleObject objp, MutableHandleObject pobjp, MutableHandleShape propp)
{
    RootedId id(cx, NameToId(name));

    for (RootedObject scope(cx, scopeChain); scope; scope = scope->enclosingScope()) {
        if (!JSObject::lookupGeneric(cx, scope, id, pobjp, propp))
            return false;
        if (propp) {
            objp.set(scope);
            return true;
        }
    }

    objp.set(nullptr);
    pobjp.set(nullptr);
    propp.set(nullptr);
    return true;
}

bool
js::LookupNameNoGC(JSContext *cx, PropertyName *name, JSObject *scopeChain,
                   JSObject **objp, JSObject **pobjp, Shape **propp)
{
    AutoAssertNoException nogc(cx);

    MOZ_ASSERT(!*objp && !*pobjp && !*propp);

    for (JSObject *scope = scopeChain; scope; scope = scope->enclosingScope()) {
        if (scope->getOps()->lookupGeneric)
            return false;
        if (!LookupPropertyInline<NoGC>(cx, &scope->as<NativeObject>(), NameToId(name), pobjp, propp))
            return false;
        if (*propp) {
            *objp = scope;
            return true;
        }
    }

    return true;
}

bool
js::LookupNameWithGlobalDefault(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
                                MutableHandleObject objp)
{
    RootedId id(cx, NameToId(name));

    RootedObject pobj(cx);
    RootedShape shape(cx);

    RootedObject scope(cx, scopeChain);
    for (; !scope->is<GlobalObject>(); scope = scope->enclosingScope()) {
        if (!JSObject::lookupGeneric(cx, scope, id, &pobj, &shape))
            return false;
        if (shape)
            break;
    }

    objp.set(scope);
    return true;
}

bool
js::LookupNameUnqualified(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
                          MutableHandleObject objp)
{
    RootedId id(cx, NameToId(name));

    RootedObject pobj(cx);
    RootedShape shape(cx);

    RootedObject scope(cx, scopeChain);
    for (; !scope->isUnqualifiedVarObj(); scope = scope->enclosingScope()) {
        if (!JSObject::lookupGeneric(cx, scope, id, &pobj, &shape))
            return false;
        if (shape)
            break;
    }

    // See note above UninitializedLexicalObject.
    if (pobj == scope && IsUninitializedLexicalSlot(scope, shape)) {
        scope = UninitializedLexicalObject::create(cx, scope);
        if (!scope)
            return false;
    }

    objp.set(scope);
    return true;
}

template <AllowGC allowGC>
bool
js::HasOwnProperty(JSContext *cx, LookupGenericOp lookup,
                   typename MaybeRooted<JSObject*, allowGC>::HandleType obj,
                   typename MaybeRooted<jsid, allowGC>::HandleType id,
                   typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
                   typename MaybeRooted<Shape*, allowGC>::MutableHandleType propp)
{
    if (lookup) {
        if (!allowGC)
            return false;
        if (!lookup(cx,
                    MaybeRooted<JSObject*, allowGC>::toHandle(obj),
                    MaybeRooted<jsid, allowGC>::toHandle(id),
                    MaybeRooted<JSObject*, allowGC>::toMutableHandle(objp),
                    MaybeRooted<Shape*, allowGC>::toMutableHandle(propp)))
        {
            return false;
        }
    } else {
        typename MaybeRooted<NativeObject*, allowGC>::HandleType nobj =
            MaybeRooted<JSObject*, allowGC>::template downcastHandle<NativeObject>(obj);

        bool done;
        if (!LookupOwnPropertyInline<allowGC>(cx, nobj, id, objp, propp, &done))
            return false;
        if (!done) {
            objp.set(nullptr);
            propp.set(nullptr);
            return true;
        }
    }

    if (!propp)
        return true;

    if (objp == obj)
        return true;

    JSObject *outer = nullptr;
    if (js::ObjectOp op = objp->getClass()->ext.outerObject) {
        if (!allowGC)
            return false;
        RootedObject inner(cx, objp);
        outer = op(cx, inner);
        if (!outer)
            return false;
    }

    if (outer != objp)
        propp.set(nullptr);
    return true;
}

template bool
js::HasOwnProperty<CanGC>(JSContext *cx, LookupGenericOp lookup,
                          HandleObject obj, HandleId id,
                          MutableHandleObject objp, MutableHandleShape propp);

template bool
js::HasOwnProperty<NoGC>(JSContext *cx, LookupGenericOp lookup,
                         JSObject *obj, jsid id,
                         FakeMutableHandle<JSObject*> objp, FakeMutableHandle<Shape*> propp);

bool
js::HasOwnProperty(JSContext *cx, HandleObject obj, HandleId id, bool *resultp)
{
    RootedObject pobj(cx);
    RootedShape shape(cx);
    if (!HasOwnProperty<CanGC>(cx, obj->getOps()->lookupGeneric, obj, id, &pobj, &shape))
        return false;
    *resultp = (shape != nullptr);
    return true;
}

static MOZ_ALWAYS_INLINE bool
LookupPropertyPureInline(JSObject *obj, jsid id, NativeObject **objp, Shape **propp)
{
    if (!obj->isNative())
        return false;

    NativeObject *current = &obj->as<NativeObject>();
    while (true) {
        /* Search for a native dense element, typed array element, or property. */

        if (JSID_IS_INT(id) && current->containsDenseElement(JSID_TO_INT(id))) {
            *objp = current;
            MarkDenseOrTypedArrayElementFound<NoGC>(propp);
            return true;
        }

        if (IsAnyTypedArray(current)) {
            uint64_t index;
            if (IsTypedArrayIndex(id, &index)) {
                if (index < AnyTypedArrayLength(obj)) {
                    *objp = current;
                    MarkDenseOrTypedArrayElementFound<NoGC>(propp);
                } else {
                    *objp = nullptr;
                    *propp = nullptr;
                }
                return true;
            }
        }

        if (Shape *shape = current->lookupPure(id)) {
            *objp = current;
            *propp = shape;
            return true;
        }

        /* Fail if there's a resolve hook. */
        if (current->getClass()->resolve != JS_ResolveStub)
            return false;

        JSObject *proto = current->getProto();

        if (!proto)
            break;
        if (!proto->isNative())
            return false;

        current = &proto->as<NativeObject>();
    }

    *objp = nullptr;
    *propp = nullptr;
    return true;
}

static MOZ_ALWAYS_INLINE bool
NativeGetPureInline(NativeObject *pobj, Shape *shape, Value *vp)
{
    if (shape->hasSlot()) {
        *vp = pobj->getSlot(shape->slot());
        MOZ_ASSERT(!vp->isMagic());
    } else {
        vp->setUndefined();
    }

    /* Fail if we have a custom getter. */
    return shape->hasDefaultGetter();
}

bool
js::LookupPropertyPure(JSObject *obj, jsid id, NativeObject **objp, Shape **propp)
{
    return LookupPropertyPureInline(obj, id, objp, propp);
}

/*
 * A pure version of GetPropertyHelper that can be called from parallel code
 * without locking. This code path cannot GC. This variant returns false
 * whenever a side-effect might have occured in the effectful version. This
 * includes, but is not limited to:
 *
 *  - Any object in the lookup chain has a non-stub resolve hook.
 *  - Any object in the lookup chain is non-native.
 *  - The property has a getter.
 */
bool
js::GetPropertyPure(ThreadSafeContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /* Deal with native objects. */
    NativeObject *obj2;
    Shape *shape;
    if (!LookupPropertyPureInline(obj, id, &obj2, &shape))
        return false;

    if (!shape) {
        /* Fail if we have a non-stub class op hooks. */
        if (obj->getClass()->getProperty && obj->getClass()->getProperty != JS_PropertyStub)
            return false;

        if (obj->getOps()->getElement)
            return false;

        /* Vanilla native object, return undefined. */
        vp->setUndefined();
        return true;
    }

    if (IsImplicitDenseOrTypedArrayElement(shape)) {
        *vp = obj2->getDenseOrTypedArrayElement(JSID_TO_INT(id));
        return true;
    }

    /* Special case 'length' on Array and TypedArray. */
    if (JSID_IS_ATOM(id, cx->names().length)) {
        if (obj->is<ArrayObject>()) {
            vp->setNumber(obj->as<ArrayObject>().length());
            return true;
        }

        if (IsAnyTypedArray(obj)) {
            vp->setNumber(AnyTypedArrayLength(obj));
            return true;
        }
    }

    return NativeGetPureInline(obj2, shape, vp);
}

static bool
MOZ_ALWAYS_INLINE
GetElementPure(ThreadSafeContext *cx, JSObject *obj, uint32_t index, Value *vp)
{
    if (index <= JSID_INT_MAX)
        return GetPropertyPure(cx, obj, INT_TO_JSID(index), vp);
    return false;
}

/*
 * A pure version of GetObjectElementOperation that can be called from
 * parallel code without locking. This variant returns false whenever a
 * side-effect might have occurred.
 */
bool
js::GetObjectElementOperationPure(ThreadSafeContext *cx, JSObject *obj, const Value &prop,
                                  Value *vp)
{
    uint32_t index;
    if (IsDefinitelyIndex(prop, &index))
        return GetElementPure(cx, obj, index, vp);

    /* Atomizing the property value is effectful and not threadsafe. */
    if (!prop.isString() || !prop.toString()->isAtom())
        return false;

    JSAtom *name = &prop.toString()->asAtom();
    if (name->isIndex(&index))
        return GetElementPure(cx, obj, index, vp);

    return GetPropertyPure(cx, obj, NameToId(name->asPropertyName()), vp);
}

bool
JSObject::reportReadOnly(ThreadSafeContext *cxArg, jsid id, unsigned report)
{
    if (cxArg->isForkJoinContext())
        return cxArg->asForkJoinContext()->reportError(report);

    if (!cxArg->isJSContext())
        return true;

    JSContext *cx = cxArg->asJSContext();
    RootedValue val(cx, IdToValue(id));
    return js_ReportValueErrorFlags(cx, report, JSMSG_READ_ONLY,
                                    JSDVG_IGNORE_STACK, val, js::NullPtr(),
                                    nullptr, nullptr);
}

bool
JSObject::reportNotConfigurable(ThreadSafeContext *cxArg, jsid id, unsigned report)
{
    if (cxArg->isForkJoinContext())
        return cxArg->asForkJoinContext()->reportError(report);

    if (!cxArg->isJSContext())
        return true;

    JSContext *cx = cxArg->asJSContext();
    RootedValue val(cx, IdToValue(id));
    return js_ReportValueErrorFlags(cx, report, JSMSG_CANT_DELETE,
                                    JSDVG_IGNORE_STACK, val, js::NullPtr(),
                                    nullptr, nullptr);
}

bool
JSObject::reportNotExtensible(ThreadSafeContext *cxArg, unsigned report)
{
    if (cxArg->isForkJoinContext())
        return cxArg->asForkJoinContext()->reportError(report);

    if (!cxArg->isJSContext())
        return true;

    JSContext *cx = cxArg->asJSContext();
    RootedValue val(cx, ObjectValue(*this));
    return js_ReportValueErrorFlags(cx, report, JSMSG_OBJECT_NOT_EXTENSIBLE,
                                    JSDVG_IGNORE_STACK, val, js::NullPtr(),
                                    nullptr, nullptr);
}

bool
JSObject::callMethod(JSContext *cx, HandleId id, unsigned argc, Value *argv, MutableHandleValue vp)
{
    RootedValue fval(cx);
    RootedObject obj(cx, this);
    if (!JSObject::getGeneric(cx, obj, obj, id, &fval))
        return false;
    return Invoke(cx, ObjectValue(*obj), fval, argc, argv, vp);
}

bool
js::WatchGuts(JSContext *cx, JS::HandleObject origObj, JS::HandleId id, JS::HandleObject callable)
{
    RootedObject obj(cx, GetInnerObject(origObj));
    if (obj->isNative()) {
        // Use sparse indexes for watched objects, as dense elements can be
        // written to without checking the watchpoint map.
        if (!NativeObject::sparsifyDenseElements(cx, obj.as<NativeObject>()))
            return false;

        types::MarkTypePropertyNonData(cx, obj, id);
    }

    WatchpointMap *wpmap = cx->compartment()->watchpointMap;
    if (!wpmap) {
        wpmap = cx->runtime()->new_<WatchpointMap>();
        if (!wpmap || !wpmap->init()) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        cx->compartment()->watchpointMap = wpmap;
    }

    return wpmap->watch(cx, obj, id, js::WatchHandler, callable);
}

bool
baseops::Watch(JSContext *cx, JS::HandleObject obj, JS::HandleId id, JS::HandleObject callable)
{
    if (!obj->isNative() || IsAnyTypedArray(obj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_WATCH,
                             obj->getClass()->name);
        return false;
    }

    return WatchGuts(cx, obj, id, callable);
}

bool
js::UnwatchGuts(JSContext *cx, JS::HandleObject origObj, JS::HandleId id)
{
    // Looking in the map for an unsupported object will never hit, so we don't
    // need to check for nativeness or watchable-ness here.
    RootedObject obj(cx, GetInnerObject(origObj));
    if (WatchpointMap *wpmap = cx->compartment()->watchpointMap)
        wpmap->unwatch(obj, id, nullptr, nullptr);
    return true;
}

bool
baseops::Unwatch(JSContext *cx, JS::HandleObject obj, JS::HandleId id)
{
    return UnwatchGuts(cx, obj, id);
}

bool
js::HasDataProperty(JSContext *cx, NativeObject *obj, jsid id, Value *vp)
{
    if (JSID_IS_INT(id) && obj->containsDenseElement(JSID_TO_INT(id))) {
        *vp = obj->getDenseElement(JSID_TO_INT(id));
        return true;
    }

    if (Shape *shape = obj->lookup(cx, id)) {
        if (shape->hasDefaultGetter() && shape->hasSlot()) {
            *vp = obj->getSlot(shape->slot());
            return true;
        }
    }

    return false;
}

/*
 * Gets |obj[id]|.  If that value's not callable, returns true and stores a
 * non-primitive value in *vp.  If it's callable, calls it with no arguments
 * and |obj| as |this|, returning the result in *vp.
 *
 * This is a mini-abstraction for ES5 8.12.8 [[DefaultValue]], either steps 1-2
 * or steps 3-4.
 */
static bool
MaybeCallMethod(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    if (!JSObject::getGeneric(cx, obj, obj, id, vp))
        return false;
    if (!IsCallable(vp)) {
        vp.setObject(*obj);
        return true;
    }
    return Invoke(cx, ObjectValue(*obj), vp, 0, nullptr, vp);
}

JS_FRIEND_API(bool)
js::DefaultValue(JSContext *cx, HandleObject obj, JSType hint, MutableHandleValue vp)
{
    MOZ_ASSERT(hint == JSTYPE_NUMBER || hint == JSTYPE_STRING || hint == JSTYPE_VOID);

    Rooted<jsid> id(cx);

    const Class *clasp = obj->getClass();
    if (hint == JSTYPE_STRING) {
        id = NameToId(cx->names().toString);

        /* Optimize (new String(...)).toString(). */
        if (clasp == &StringObject::class_) {
            StringObject *nobj = &obj->as<StringObject>();
            if (ClassMethodIsNative(cx, nobj, &StringObject::class_, id, js_str_toString)) {
                vp.setString(nobj->unbox());
                return true;
            }
        }

        if (!MaybeCallMethod(cx, obj, id, vp))
            return false;
        if (vp.isPrimitive())
            return true;

        id = NameToId(cx->names().valueOf);
        if (!MaybeCallMethod(cx, obj, id, vp))
            return false;
        if (vp.isPrimitive())
            return true;
    } else {

        /* Optimize new String(...).valueOf(). */
        if (clasp == &StringObject::class_) {
            id = NameToId(cx->names().valueOf);
            StringObject *nobj = &obj->as<StringObject>();
            if (ClassMethodIsNative(cx, nobj, &StringObject::class_, id, js_str_toString)) {
                vp.setString(nobj->unbox());
                return true;
            }
        }

        /* Optimize new Number(...).valueOf(). */
        if (clasp == &NumberObject::class_) {
            id = NameToId(cx->names().valueOf);
            NumberObject *nobj = &obj->as<NumberObject>();
            if (ClassMethodIsNative(cx, nobj, &NumberObject::class_, id, js_num_valueOf)) {
                vp.setNumber(nobj->unbox());
                return true;
            }
        }

        id = NameToId(cx->names().valueOf);
        if (!MaybeCallMethod(cx, obj, id, vp))
            return false;
        if (vp.isPrimitive())
            return true;

        id = NameToId(cx->names().toString);
        if (!MaybeCallMethod(cx, obj, id, vp))
            return false;
        if (vp.isPrimitive())
            return true;
    }

    /* Avoid recursive death when decompiling in js_ReportValueError. */
    RootedString str(cx);
    if (hint == JSTYPE_STRING) {
        str = JS_InternString(cx, clasp->name);
        if (!str)
            return false;
    } else {
        str = nullptr;
    }

    RootedValue val(cx, ObjectValue(*obj));
    js_ReportValueError2(cx, JSMSG_CANT_CONVERT_TO, JSDVG_SEARCH_STACK, val, str,
                         hint == JSTYPE_VOID
                         ? "primitive type"
                         : hint == JSTYPE_STRING ? "string" : "number");
    return false;
}

JS_FRIEND_API(bool)
JS_EnumerateState(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
                  MutableHandleValue statep, JS::MutableHandleId idp)
{
    /* If the class has a custom JSCLASS_NEW_ENUMERATE hook, call it. */
    const Class *clasp = obj->getClass();
    JSEnumerateOp enumerate = clasp->enumerate;
    if (clasp->flags & JSCLASS_NEW_ENUMERATE) {
        MOZ_ASSERT(enumerate != JS_EnumerateStub);
        return ((JSNewEnumerateOp) enumerate)(cx, obj, enum_op, statep, idp);
    }

    if (!enumerate(cx, obj))
        return false;

    /* Tell InitNativeIterator to treat us like a native object. */
    MOZ_ASSERT(enum_op == JSENUMERATE_INIT || enum_op == JSENUMERATE_INIT_ALL);
    statep.setMagic(JS_NATIVE_ENUMERATE);
    return true;
}

bool
js::IsDelegate(JSContext *cx, HandleObject obj, const js::Value &v, bool *result)
{
    if (v.isPrimitive()) {
        *result = false;
        return true;
    }
    return IsDelegateOfObject(cx, obj, &v.toObject(), result);
}

bool
js::IsDelegateOfObject(JSContext *cx, HandleObject protoObj, JSObject* obj, bool *result)
{
    RootedObject obj2(cx, obj);
    for (;;) {
        if (!JSObject::getProto(cx, obj2, &obj2))
            return false;
        if (!obj2) {
            *result = false;
            return true;
        }
        if (obj2 == protoObj) {
            *result = true;
            return true;
        }
    }
}

JSObject *
js::GetBuiltinPrototypePure(GlobalObject *global, JSProtoKey protoKey)
{
    MOZ_ASSERT(JSProto_Null <= protoKey);
    MOZ_ASSERT(protoKey < JSProto_LIMIT);

    if (protoKey != JSProto_Null) {
        const Value &v = global->getPrototype(protoKey);
        if (v.isObject())
            return &v.toObject();
    }

    return nullptr;
}

/*
 * The first part of this function has been hand-expanded and optimized into
 * NewBuiltinClassInstance in jsobjinlines.h.
 */
bool
js::FindClassPrototype(ExclusiveContext *cx, MutableHandleObject protop, const Class *clasp)
{
    protop.set(nullptr);
    JSProtoKey protoKey = ClassProtoKeyOrAnonymousOrNull(clasp);
    if (protoKey != JSProto_Null)
        return GetBuiltinPrototype(cx, protoKey, protop);

    RootedObject ctor(cx);
    if (!FindClassObject(cx, &ctor, clasp))
        return false;

    if (ctor && ctor->is<JSFunction>()) {
        JSFunction *nctor = &ctor->as<JSFunction>();
        RootedValue v(cx);
        if (cx->isJSContext()) {
            if (!JSObject::getProperty(cx->asJSContext(),
                                       ctor, ctor, cx->names().prototype, &v))
            {
                return false;
            }
        } else {
            Shape *shape = nctor->lookup(cx, cx->names().prototype);
            if (!shape || !NativeGetPureInline(nctor, shape, v.address()))
                return false;
        }
        if (v.isObject())
            protop.set(&v.toObject());
    }
    return true;
}

JSObject *
js::PrimitiveToObject(JSContext *cx, const Value &v)
{
    if (v.isString()) {
        Rooted<JSString*> str(cx, v.toString());
        return StringObject::create(cx, str);
    }
    if (v.isNumber())
        return NumberObject::create(cx, v.toNumber());
    if (v.isBoolean())
        return BooleanObject::create(cx, v.toBoolean());
    MOZ_ASSERT(v.isSymbol());
    return SymbolObject::create(cx, v.toSymbol());
}

/* Callers must handle the already-object case. */
JSObject *
js::ToObjectSlow(JSContext *cx, HandleValue val, bool reportScanStack)
{
    MOZ_ASSERT(!val.isMagic());
    MOZ_ASSERT(!val.isObject());

    if (val.isNullOrUndefined()) {
        if (reportScanStack) {
            js_ReportIsNullOrUndefined(cx, JSDVG_SEARCH_STACK, val, NullPtr());
        } else {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                                 val.isNull() ? "null" : "undefined", "object");
        }
        return nullptr;
    }

    return PrimitiveToObject(cx, val);
}

void
js_GetObjectSlotName(JSTracer *trc, char *buf, size_t bufsize)
{
    MOZ_ASSERT(trc->debugPrinter() == js_GetObjectSlotName);

    JSObject *obj = (JSObject *)trc->debugPrintArg();
    uint32_t slot = uint32_t(trc->debugPrintIndex());

    Shape *shape;
    if (obj->isNative()) {
        shape = obj->lastProperty();
        while (shape && (!shape->hasSlot() || shape->slot() != slot))
            shape = shape->previous();
    } else {
        shape = nullptr;
    }

    if (!shape) {
        do {
            const char *slotname = nullptr;
            const char *pattern = nullptr;
            if (obj->is<GlobalObject>()) {
                pattern = "CLASS_OBJECT(%s)";
                if (false)
                    ;
#define TEST_SLOT_MATCHES_PROTOTYPE(name,code,init,clasp) \
                else if ((code) == slot) { slotname = js_##name##_str; }
                JS_FOR_EACH_PROTOTYPE(TEST_SLOT_MATCHES_PROTOTYPE)
#undef TEST_SLOT_MATCHES_PROTOTYPE
            } else {
                pattern = "%s";
                if (obj->is<ScopeObject>()) {
                    if (slot == ScopeObject::enclosingScopeSlot()) {
                        slotname = "enclosing_environment";
                    } else if (obj->is<CallObject>()) {
                        if (slot == CallObject::calleeSlot())
                            slotname = "callee_slot";
                    } else if (obj->is<DeclEnvObject>()) {
                        if (slot == DeclEnvObject::lambdaSlot())
                            slotname = "named_lambda";
                    } else if (obj->is<DynamicWithObject>()) {
                        if (slot == DynamicWithObject::objectSlot())
                            slotname = "with_object";
                        else if (slot == DynamicWithObject::thisSlot())
                            slotname = "with_this";
                    }
                }
            }

            if (slotname)
                JS_snprintf(buf, bufsize, pattern, slotname);
            else
                JS_snprintf(buf, bufsize, "**UNKNOWN SLOT %ld**", (long)slot);
        } while (false);
    } else {
        jsid propid = shape->propid();
        if (JSID_IS_INT(propid)) {
            JS_snprintf(buf, bufsize, "%ld", (long)JSID_TO_INT(propid));
        } else if (JSID_IS_ATOM(propid)) {
            PutEscapedString(buf, bufsize, JSID_TO_ATOM(propid), 0);
        } else if (JSID_IS_SYMBOL(propid)) {
            JS_snprintf(buf, bufsize, "**SYMBOL KEY**");
        } else {
            JS_snprintf(buf, bufsize, "**FINALIZED ATOM KEY**");
        }
    }
}

bool
js_ReportGetterOnlyAssignment(JSContext *cx, bool strict)
{
    return JS_ReportErrorFlagsAndNumber(cx,
                                        strict
                                        ? JSREPORT_ERROR
                                        : JSREPORT_WARNING | JSREPORT_STRICT,
                                        js_GetErrorMessage, nullptr,
                                        JSMSG_GETTER_ONLY);
}

#ifdef DEBUG

/*
 * Routines to print out values during debugging.  These are FRIEND_API to help
 * the debugger find them and to support temporarily hacking js_Dump* calls
 * into other code.
 */

static void
dumpValue(const Value &v)
{
    if (v.isNull())
        fprintf(stderr, "null");
    else if (v.isUndefined())
        fprintf(stderr, "undefined");
    else if (v.isInt32())
        fprintf(stderr, "%d", v.toInt32());
    else if (v.isDouble())
        fprintf(stderr, "%g", v.toDouble());
    else if (v.isString())
        v.toString()->dump();
    else if (v.isSymbol())
        v.toSymbol()->dump();
    else if (v.isObject() && v.toObject().is<JSFunction>()) {
        JSFunction *fun = &v.toObject().as<JSFunction>();
        if (fun->displayAtom()) {
            fputs("<function ", stderr);
            FileEscapedString(stderr, fun->displayAtom(), 0);
        } else {
            fputs("<unnamed function", stderr);
        }
        if (fun->hasScript()) {
            JSScript *script = fun->nonLazyScript();
            fprintf(stderr, " (%s:%d)",
                    script->filename() ? script->filename() : "", (int) script->lineno());
        }
        fprintf(stderr, " at %p>", (void *) fun);
    } else if (v.isObject()) {
        JSObject *obj = &v.toObject();
        const Class *clasp = obj->getClass();
        fprintf(stderr, "<%s%s at %p>",
                clasp->name,
                (clasp == &JSObject::class_) ? "" : " object",
                (void *) obj);
    } else if (v.isBoolean()) {
        if (v.toBoolean())
            fprintf(stderr, "true");
        else
            fprintf(stderr, "false");
    } else if (v.isMagic()) {
        fprintf(stderr, "<invalid");
#ifdef DEBUG
        switch (v.whyMagic()) {
          case JS_ELEMENTS_HOLE:     fprintf(stderr, " elements hole");      break;
          case JS_NATIVE_ENUMERATE:  fprintf(stderr, " native enumeration"); break;
          case JS_NO_ITER_VALUE:     fprintf(stderr, " no iter value");      break;
          case JS_GENERATOR_CLOSING: fprintf(stderr, " generator closing");  break;
          case JS_OPTIMIZED_OUT:     fprintf(stderr, " optimized out");      break;
          default:                   fprintf(stderr, " ?!");                 break;
        }
#endif
        fprintf(stderr, ">");
    } else {
        fprintf(stderr, "unexpected value");
    }
}

JS_FRIEND_API(void)
js_DumpValue(const Value &val)
{
    dumpValue(val);
    fputc('\n', stderr);
}

JS_FRIEND_API(void)
js_DumpId(jsid id)
{
    fprintf(stderr, "jsid %p = ", (void *) JSID_BITS(id));
    dumpValue(IdToValue(id));
    fputc('\n', stderr);
}

static void
DumpProperty(NativeObject *obj, Shape &shape)
{
    jsid id = shape.propid();
    uint8_t attrs = shape.attributes();

    fprintf(stderr, "    ((js::Shape *) %p) ", (void *) &shape);
    if (attrs & JSPROP_ENUMERATE) fprintf(stderr, "enumerate ");
    if (attrs & JSPROP_READONLY) fprintf(stderr, "readonly ");
    if (attrs & JSPROP_PERMANENT) fprintf(stderr, "permanent ");
    if (attrs & JSPROP_SHARED) fprintf(stderr, "shared ");

    if (shape.hasGetterValue())
        fprintf(stderr, "getterValue=%p ", (void *) shape.getterObject());
    else if (!shape.hasDefaultGetter())
        fprintf(stderr, "getterOp=%p ", JS_FUNC_TO_DATA_PTR(void *, shape.getterOp()));

    if (shape.hasSetterValue())
        fprintf(stderr, "setterValue=%p ", (void *) shape.setterObject());
    else if (!shape.hasDefaultSetter())
        fprintf(stderr, "setterOp=%p ", JS_FUNC_TO_DATA_PTR(void *, shape.setterOp()));

    if (JSID_IS_ATOM(id) || JSID_IS_INT(id) || JSID_IS_SYMBOL(id))
        dumpValue(js::IdToValue(id));
    else
        fprintf(stderr, "unknown jsid %p", (void *) JSID_BITS(id));

    uint32_t slot = shape.hasSlot() ? shape.maybeSlot() : SHAPE_INVALID_SLOT;
    fprintf(stderr, ": slot %d", slot);
    if (shape.hasSlot()) {
        fprintf(stderr, " = ");
        dumpValue(obj->getSlot(slot));
    } else if (slot != SHAPE_INVALID_SLOT) {
        fprintf(stderr, " (INVALID!)");
    }
    fprintf(stderr, "\n");
}

bool
JSObject::uninlinedIsProxy() const
{
    return is<ProxyObject>();
}

void
JSObject::dump()
{
    JSObject *obj = this;
    fprintf(stderr, "object %p\n", (void *) obj);
    const Class *clasp = obj->getClass();
    fprintf(stderr, "class %p %s\n", (const void *)clasp, clasp->name);

    fprintf(stderr, "flags:");
    if (obj->isDelegate()) fprintf(stderr, " delegate");
    if (!obj->is<ProxyObject>() && !obj->nonProxyIsExtensible()) fprintf(stderr, " not_extensible");
    if (obj->isIndexed()) fprintf(stderr, " indexed");
    if (obj->isBoundFunction()) fprintf(stderr, " bound_function");
    if (obj->isQualifiedVarObj()) fprintf(stderr, " varobj");
    if (obj->isUnqualifiedVarObj()) fprintf(stderr, " unqualified_varobj");
    if (obj->watched()) fprintf(stderr, " watched");
    if (obj->isIteratedSingleton()) fprintf(stderr, " iterated_singleton");
    if (obj->isNewTypeUnknown()) fprintf(stderr, " new_type_unknown");
    if (obj->hasUncacheableProto()) fprintf(stderr, " has_uncacheable_proto");
    if (obj->hadElementsAccess()) fprintf(stderr, " had_elements_access");

    if (obj->isNative()) {
        NativeObject *nobj = &obj->as<NativeObject>();
        if (nobj->inDictionaryMode())
            fprintf(stderr, " inDictionaryMode");
        if (nobj->hasShapeTable())
            fprintf(stderr, " hasShapeTable");
    }
    fprintf(stderr, "\n");

    if (obj->isNative()) {
        NativeObject *nobj = &obj->as<NativeObject>();
        uint32_t slots = nobj->getDenseInitializedLength();
        if (slots) {
            fprintf(stderr, "elements\n");
            for (uint32_t i = 0; i < slots; i++) {
                fprintf(stderr, " %3d: ", i);
                dumpValue(nobj->getDenseElement(i));
                fprintf(stderr, "\n");
                fflush(stderr);
            }
        }
    }

    fprintf(stderr, "proto ");
    TaggedProto proto = obj->getTaggedProto();
    if (proto.isLazy())
        fprintf(stderr, "<lazy>");
    else
        dumpValue(ObjectOrNullValue(proto.toObjectOrNull()));
    fputc('\n', stderr);

    fprintf(stderr, "parent ");
    dumpValue(ObjectOrNullValue(obj->getParent()));
    fputc('\n', stderr);

    if (clasp->flags & JSCLASS_HAS_PRIVATE)
        fprintf(stderr, "private %p\n", obj->as<NativeObject>().getPrivate());

    if (!obj->isNative())
        fprintf(stderr, "not native\n");

    uint32_t reservedEnd = JSCLASS_RESERVED_SLOTS(clasp);
    uint32_t slots = obj->isNative() ? obj->as<NativeObject>().slotSpan() : 0;
    uint32_t stop = obj->isNative() ? reservedEnd : slots;
    if (stop > 0)
        fprintf(stderr, obj->isNative() ? "reserved slots:\n" : "slots:\n");
    for (uint32_t i = 0; i < stop; i++) {
        fprintf(stderr, " %3d ", i);
        if (i < reservedEnd)
            fprintf(stderr, "(reserved) ");
        fprintf(stderr, "= ");
        dumpValue(obj->as<NativeObject>().getSlot(i));
        fputc('\n', stderr);
    }

    if (obj->isNative()) {
        fprintf(stderr, "properties:\n");
        Vector<Shape *, 8, SystemAllocPolicy> props;
        for (Shape::Range<NoGC> r(obj->lastProperty()); !r.empty(); r.popFront())
            props.append(&r.front());
        for (size_t i = props.length(); i-- != 0;)
            DumpProperty(&obj->as<NativeObject>(), *props[i]);
    }
    fputc('\n', stderr);
}

static void
MaybeDumpObject(const char *name, JSObject *obj)
{
    if (obj) {
        fprintf(stderr, "  %s: ", name);
        dumpValue(ObjectValue(*obj));
        fputc('\n', stderr);
    }
}

static void
MaybeDumpValue(const char *name, const Value &v)
{
    if (!v.isNull()) {
        fprintf(stderr, "  %s: ", name);
        dumpValue(v);
        fputc('\n', stderr);
    }
}

JS_FRIEND_API(void)
js_DumpInterpreterFrame(JSContext *cx, InterpreterFrame *start)
{
    /* This should only called during live debugging. */
    ScriptFrameIter i(cx, ScriptFrameIter::GO_THROUGH_SAVED);
    if (!start) {
        if (i.done()) {
            fprintf(stderr, "no stack for cx = %p\n", (void*) cx);
            return;
        }
    } else {
        while (!i.done() && !i.isJit() && i.interpFrame() != start)
            ++i;

        if (i.done()) {
            fprintf(stderr, "fp = %p not found in cx = %p\n",
                    (void *)start, (void *)cx);
            return;
        }
    }

    for (; !i.done(); ++i) {
        if (i.isJit())
            fprintf(stderr, "JIT frame\n");
        else
            fprintf(stderr, "InterpreterFrame at %p\n", (void *) i.interpFrame());

        if (i.isFunctionFrame()) {
            fprintf(stderr, "callee fun: ");
            dumpValue(i.calleev());
        } else {
            fprintf(stderr, "global frame, no callee");
        }
        fputc('\n', stderr);

        fprintf(stderr, "file %s line %u\n",
                i.script()->filename(), (unsigned) i.script()->lineno());

        if (jsbytecode *pc = i.pc()) {
            fprintf(stderr, "  pc = %p\n", pc);
            fprintf(stderr, "  current op: %s\n", js_CodeName[*pc]);
            MaybeDumpObject("staticScope", i.script()->getStaticScope(pc));
        }
        MaybeDumpValue("this", i.thisv(cx));
        if (!i.isJit()) {
            fprintf(stderr, "  rval: ");
            dumpValue(i.interpFrame()->returnValue());
            fputc('\n', stderr);
        }

        fprintf(stderr, "  flags:");
        if (i.isConstructing())
            fprintf(stderr, " constructing");
        if (!i.isJit() && i.interpFrame()->isDebuggerFrame())
            fprintf(stderr, " debugger");
        if (i.isEvalFrame())
            fprintf(stderr, " eval");
        if (!i.isJit() && i.interpFrame()->isGeneratorFrame())
            fprintf(stderr, " generator");
        fputc('\n', stderr);

        fprintf(stderr, "  scopeChain: (JSObject *) %p\n", (void *) i.scopeChain());

        fputc('\n', stderr);
    }
}

#endif /* DEBUG */

JS_FRIEND_API(void)
js_DumpBacktrace(JSContext *cx)
{
    Sprinter sprinter(cx);
    sprinter.init();
    size_t depth = 0;
    for (ScriptFrameIter i(cx); !i.done(); ++i, ++depth) {
        const char *filename = JS_GetScriptFilename(i.script());
        unsigned line = PCToLineNumber(i.script(), i.pc());
        JSScript *script = i.script();
        sprinter.printf("#%d %14p   %s:%d (%p @ %d)\n",
                        depth, (i.isJit() ? 0 : i.interpFrame()), filename, line,
                        script, script->pcToOffset(i.pc()));
    }
    fprintf(stdout, "%s", sprinter.string());
}

void
JSObject::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::ClassInfo *info)
{
    if (is<NativeObject>() && as<NativeObject>().hasDynamicSlots())
        info->objectsMallocHeapSlots += mallocSizeOf(as<NativeObject>().slots_);

    if (is<NativeObject>() && as<NativeObject>().hasDynamicElements()) {
        js::ObjectElements *elements = as<NativeObject>().getElementsHeader();
        if (!elements->isCopyOnWrite() || elements->ownerObject() == this)
            info->objectsMallocHeapElementsNonAsmJS += mallocSizeOf(elements);
    }

    // Other things may be measured in the future if DMD indicates it is worthwhile.
    if (is<JSFunction>() ||
        is<JSObject>() ||
        is<ArrayObject>() ||
        is<CallObject>() ||
        is<RegExpObject>() ||
        is<ProxyObject>())
    {
        // Do nothing.  But this function is hot, and we win by getting the
        // common cases out of the way early.  Some stats on the most common
        // classes, as measured during a vanilla browser session:
        // - (53.7%, 53.7%): Function
        // - (18.0%, 71.7%): Object
        // - (16.9%, 88.6%): Array
        // - ( 3.9%, 92.5%): Call
        // - ( 2.8%, 95.3%): RegExp
        // - ( 1.0%, 96.4%): Proxy

    } else if (is<ArgumentsObject>()) {
        info->objectsMallocHeapMisc += as<ArgumentsObject>().sizeOfMisc(mallocSizeOf);
    } else if (is<RegExpStaticsObject>()) {
        info->objectsMallocHeapMisc += as<RegExpStaticsObject>().sizeOfData(mallocSizeOf);
    } else if (is<PropertyIteratorObject>()) {
        info->objectsMallocHeapMisc += as<PropertyIteratorObject>().sizeOfMisc(mallocSizeOf);
    } else if (is<ArrayBufferObject>()) {
        ArrayBufferObject::addSizeOfExcludingThis(this, mallocSizeOf, info);
    } else if (is<SharedArrayBufferObject>()) {
        SharedArrayBufferObject::addSizeOfExcludingThis(this, mallocSizeOf, info);
    } else if (is<AsmJSModuleObject>()) {
        as<AsmJSModuleObject>().addSizeOfMisc(mallocSizeOf, &info->objectsNonHeapCodeAsmJS,
                                              &info->objectsMallocHeapMisc);
#ifdef JS_HAS_CTYPES
    } else {
        // This must be the last case.
        info->objectsMallocHeapMisc +=
            js::SizeOfDataIfCDataObject(mallocSizeOf, const_cast<JSObject *>(this));
#endif
    }
}

bool
JSObject::hasIdempotentProtoChain() const
{
    // Return false if obj (or an object on its proto chain) is non-native or
    // has a resolve or lookup hook.
    JSObject *obj = const_cast<JSObject *>(this);
    while (true) {
        if (!obj->isNative())
            return false;

        JSResolveOp resolve = obj->getClass()->resolve;
        if (resolve != JS_ResolveStub && resolve != (JSResolveOp) js::fun_resolve)
            return false;

        if (obj->getOps()->lookupProperty || obj->getOps()->lookupGeneric || obj->getOps()->lookupElement)
            return false;

        obj = obj->getProto();
        if (!obj)
            return true;
    }
}

void
JSObject::markChildren(JSTracer *trc)
{
    MarkTypeObject(trc, &type_, "type");

    MarkShape(trc, &shape_, "shape");

    const Class *clasp = type_->clasp();
    if (clasp->trace)
        clasp->trace(trc, this);

    if (shape_->isNative()) {
        NativeObject *nobj = &as<NativeObject>();
        MarkObjectSlots(trc, nobj, 0, nobj->slotSpan());

        do {
            if (nobj->denseElementsAreCopyOnWrite()) {
                HeapPtrNativeObject &owner = nobj->getElementsHeader()->ownerObject();
                if (owner != nobj) {
                    MarkObject(trc, &owner, "objectElementsOwner");
                    break;
                }
            }

            gc::MarkArraySlots(trc,
                               nobj->getDenseInitializedLength(),
                               nobj->getDenseElementsAllowCopyOnWrite(),
                               "objectElements");
        } while (false);
    }
}
