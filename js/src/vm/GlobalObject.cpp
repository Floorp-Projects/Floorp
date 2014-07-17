/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/GlobalObject.h"

#include "jscntxt.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsfriendapi.h"
#include "jsmath.h"
#include "json.h"
#include "jsprototypes.h"
#include "jsweakmap.h"

#include "builtin/Eval.h"
#if EXPOSE_INTL_API
# include "builtin/Intl.h"
#endif
#include "builtin/MapObject.h"
#include "builtin/Object.h"
#include "builtin/RegExp.h"
#include "builtin/SIMD.h"
#include "builtin/SymbolObject.h"
#include "builtin/TypedObject.h"
#include "vm/HelperThreads.h"
#include "vm/PIC.h"
#include "vm/RegExpStatics.h"
#include "vm/StopIterationObject.h"
#include "vm/WeakMapObject.h"

#include "jscompartmentinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/ObjectImpl-inl.h"

using namespace js;

struct ProtoTableEntry {
    const Class *clasp;
    ClassInitializerOp init;
};

#define DECLARE_PROTOTYPE_CLASS_INIT(name,code,init,clasp) \
    extern JSObject *init(JSContext *cx, Handle<JSObject*> obj);
JS_FOR_EACH_PROTOTYPE(DECLARE_PROTOTYPE_CLASS_INIT)
#undef DECLARE_PROTOTYPE_CLASS_INIT

JSObject *
js_InitViaClassSpec(JSContext *cx, Handle<JSObject*> obj)
{
    MOZ_CRASH("js_InitViaClassSpec() should not be called.");
}

static const ProtoTableEntry protoTable[JSProto_LIMIT] = {
#define INIT_FUNC(name,code,init,clasp) { clasp, init },
#define INIT_FUNC_DUMMY(name,code,init,clasp) { nullptr, nullptr },
    JS_FOR_PROTOTYPES(INIT_FUNC, INIT_FUNC_DUMMY)
#undef INIT_FUNC_DUMMY
#undef INIT_FUNC
};

JS_FRIEND_API(const js::Class *)
js::ProtoKeyToClass(JSProtoKey key)
{
    MOZ_ASSERT(key < JSProto_LIMIT);
    return protoTable[key].clasp;
}

// This method is not in the header file to avoid having to include
// TypedObject.h from GlobalObject.h. It is not generally perf
// sensitive.
TypedObjectModuleObject&
js::GlobalObject::getTypedObjectModule() const {
    Value v = getConstructor(JSProto_TypedObject);
    // only gets called from contexts where TypedObject must be initialized
    JS_ASSERT(v.isObject());
    return v.toObject().as<TypedObjectModuleObject>();
}




/* static */ bool
GlobalObject::ensureConstructor(JSContext *cx, Handle<GlobalObject*> global, JSProtoKey key)
{
    if (global->isStandardClassResolved(key))
        return true;
    return resolveConstructor(cx, global, key);
}

/* static*/ bool
GlobalObject::resolveConstructor(JSContext *cx, Handle<GlobalObject*> global, JSProtoKey key)
{
    MOZ_ASSERT(!global->isStandardClassResolved(key));

    // There are two different kinds of initialization hooks. One of them is
    // the class js_InitFoo hook, defined in a JSProtoKey-keyed table at the
    // top of this file. The other lives in the ClassSpec for classes that
    // define it. Classes may use one or the other, but not both.
    ClassInitializerOp init = protoTable[key].init;
    if (init == js_InitViaClassSpec)
        init = nullptr;

    const Class *clasp = ProtoKeyToClass(key);

    // Some classes have no init routine, which means that they're disabled at
    // compile-time. We could try to enforce that callers never pass such keys
    // to resolveConstructor, but that would cramp the style of consumers like
    // GlobalObject::initStandardClasses that want to just carpet-bomb-call
    // ensureConstructor with every JSProtoKey. So it's easier to just handle
    // it here.
    bool haveSpec = clasp && clasp->spec.defined();
    if (!init && !haveSpec)
        return true;

    // See if there's an old-style initialization hook.
    if (init) {
        MOZ_ASSERT(!haveSpec);
        return init(cx, global);
    }

    //
    // Ok, we're doing it with a class spec.
    //

    // We need to create the prototype first, and immediately stash it in the
    // slot. This is so the following bootstrap ordering is possible:
    // * Object.prototype
    // * Function.prototype
    // * Function
    // * Object
    //
    // We get the above when Object is resolved before Function. If Function
    // is resolved before Object, we'll end up re-entering resolveConstructor
    // for Function, which is a problem. So if Function is being resolved before
    // Object.prototype exists, we just resolve Object instead, since we know that
    // Function will also be resolved before we return.
    if (key == JSProto_Function && global->getPrototype(JSProto_Object).isUndefined())
        return resolveConstructor(cx, global, JSProto_Object);

    // We don't always have a prototype (i.e. Math and JSON). If we don't,
    // |createPrototype|, |prototypeFunctions|, and |prototypeProperties|
    // should all be null.
    RootedObject proto(cx);
    if (clasp->spec.createPrototype) {
        proto = clasp->spec.createPrototype(cx, key);
        if (!proto)
            return false;

        global->setPrototype(key, ObjectValue(*proto));
    }

    // Create the constructor.
    RootedObject ctor(cx, clasp->spec.createConstructor(cx, key));
    if (!ctor)
        return false;

    RootedId id(cx, NameToId(ClassName(key, cx)));
    if (!global->addDataProperty(cx, id, constructorPropertySlot(key), 0))
        return false;

    global->setConstructor(key, ObjectValue(*ctor));
    global->setConstructorPropertySlot(key, ObjectValue(*ctor));

    // Define any specified functions and properties, unless we're a dependent
    // standard class (in which case they live on the prototype).
    if (!StandardClassIsDependent(key)) {
        if (const JSFunctionSpec *funs = clasp->spec.prototypeFunctions) {
            if (!JS_DefineFunctions(cx, proto, funs))
                return false;
        }
        if (const JSPropertySpec *props = clasp->spec.prototypeProperties) {
            if (!JS_DefineProperties(cx, proto, props))
                return false;
        }
        if (const JSFunctionSpec *funs = clasp->spec.constructorFunctions) {
            if (!JS_DefineFunctions(cx, ctor, funs))
                return false;
        }
    }

    // If the prototype exists, link it with the constructor.
    if (proto && !LinkConstructorAndPrototype(cx, ctor, proto))
        return false;

    // Call the post-initialization hook, if provided.
    if (clasp->spec.finishInit && !clasp->spec.finishInit(cx, ctor, proto))
        return false;

    // Stash type information, so that what we do here is equivalent to
    // initBuiltinConstructor.
    types::AddTypePropertyId(cx, global, id, ObjectValue(*ctor));

    return true;
}

/* static */ bool
GlobalObject::initBuiltinConstructor(JSContext *cx, Handle<GlobalObject*> global,
                                     JSProtoKey key, HandleObject ctor, HandleObject proto)
{
    JS_ASSERT(!global->nativeEmpty()); // reserved slots already allocated
    JS_ASSERT(key != JSProto_Null);
    JS_ASSERT(ctor);
    JS_ASSERT(proto);

    RootedId id(cx, NameToId(ClassName(key, cx)));
    JS_ASSERT(!global->nativeLookup(cx, id));

    if (!global->addDataProperty(cx, id, constructorPropertySlot(key), 0))
        return false;

    global->setConstructor(key, ObjectValue(*ctor));
    global->setPrototype(key, ObjectValue(*proto));
    global->setConstructorPropertySlot(key, ObjectValue(*ctor));

    types::AddTypePropertyId(cx, global, id, ObjectValue(*ctor));
    return true;
}

GlobalObject *
GlobalObject::create(JSContext *cx, const Class *clasp)
{
    JS_ASSERT(clasp->flags & JSCLASS_IS_GLOBAL);
    JS_ASSERT(clasp->trace == JS_GlobalObjectTraceHook);

    JSObject *obj = NewObjectWithGivenProto(cx, clasp, nullptr, nullptr, SingletonObject);
    if (!obj)
        return nullptr;

    Rooted<GlobalObject *> global(cx, &obj->as<GlobalObject>());

    cx->compartment()->initGlobal(*global);

    if (!global->setQualifiedVarObj(cx))
        return nullptr;
    if (!global->setUnqualifiedVarObj(cx))
        return nullptr;
    if (!global->setDelegate(cx))
        return nullptr;

    return global;
}

/* static */ bool
GlobalObject::getOrCreateEval(JSContext *cx, Handle<GlobalObject*> global,
                              MutableHandleObject eval)
{
    if (!global->getOrCreateObjectPrototype(cx))
        return false;
    eval.set(&global->getSlot(EVAL).toObject());
    return true;
}

bool
GlobalObject::valueIsEval(Value val)
{
    Value eval = getSlot(EVAL);
    return eval.isObject() && eval == val;
}

/* static */ bool
GlobalObject::initStandardClasses(JSContext *cx, Handle<GlobalObject*> global)
{
    /* Define a top-level property 'undefined' with the undefined value. */
    if (!JSObject::defineProperty(cx, global, cx->names().undefined, UndefinedHandleValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return false;
    }

    for (size_t k = 0; k < JSProto_LIMIT; ++k) {
        if (!ensureConstructor(cx, global, static_cast<JSProtoKey>(k)))
            return false;
    }
    return true;
}

/* static */ bool
GlobalObject::isRuntimeCodeGenEnabled(JSContext *cx, Handle<GlobalObject*> global)
{
    HeapSlot &v = global->getSlotRef(RUNTIME_CODEGEN_ENABLED);
    if (v.isUndefined()) {
        /*
         * If there are callbacks, make sure that the CSP callback is installed
         * and that it permits runtime code generation, then cache the result.
         */
        JSCSPEvalChecker allows = cx->runtime()->securityCallbacks->contentSecurityPolicyAllows;
        Value boolValue = BooleanValue(!allows || allows(cx));
        v.set(global, HeapSlot::Slot, RUNTIME_CODEGEN_ENABLED, boolValue);
    }
    return !v.isFalse();
}

/* static */ bool
GlobalObject::warnOnceAbout(JSContext *cx, HandleObject obj, uint32_t slot, unsigned errorNumber)
{
    Rooted<GlobalObject*> global(cx, &obj->global());
    HeapSlot &v = global->getSlotRef(slot);
    if (v.isUndefined()) {
        if (!JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, js_GetErrorMessage, nullptr,
                                          errorNumber))
        {
            return false;
        }
        v.init(global, HeapSlot::Slot, slot, BooleanValue(true));
    }
    return true;
}

JSFunction *
GlobalObject::createConstructor(JSContext *cx, Native ctor, JSAtom *nameArg, unsigned length,
                                gc::AllocKind kind)
{
    RootedAtom name(cx, nameArg);
    RootedObject self(cx, this);
    return NewFunction(cx, NullPtr(), ctor, length, JSFunction::NATIVE_CTOR, self, name, kind);
}

static JSObject *
CreateBlankProto(JSContext *cx, const Class *clasp, JSObject &proto, GlobalObject &global)
{
    JS_ASSERT(clasp != &JSFunction::class_);

    RootedObject blankProto(cx, NewObjectWithGivenProto(cx, clasp, &proto, &global, SingletonObject));
    if (!blankProto || !blankProto->setDelegate(cx))
        return nullptr;

    return blankProto;
}

JSObject *
GlobalObject::createBlankPrototype(JSContext *cx, const Class *clasp)
{
    Rooted<GlobalObject*> self(cx, this);
    JSObject *objectProto = getOrCreateObjectPrototype(cx);
    if (!objectProto)
        return nullptr;

    return CreateBlankProto(cx, clasp, *objectProto, *self.get());
}

JSObject *
GlobalObject::createBlankPrototypeInheriting(JSContext *cx, const Class *clasp, JSObject &proto)
{
    return CreateBlankProto(cx, clasp, proto, *this);
}

bool
js::LinkConstructorAndPrototype(JSContext *cx, JSObject *ctor_, JSObject *proto_)
{
    RootedObject ctor(cx, ctor_), proto(cx, proto_);

    RootedValue protoVal(cx, ObjectValue(*proto));
    RootedValue ctorVal(cx, ObjectValue(*ctor));

    return JSObject::defineProperty(cx, ctor, cx->names().prototype,
                                    protoVal, JS_PropertyStub, JS_StrictPropertyStub,
                                    JSPROP_PERMANENT | JSPROP_READONLY) &&
           JSObject::defineProperty(cx, proto, cx->names().constructor,
                                    ctorVal, JS_PropertyStub, JS_StrictPropertyStub, 0);
}

bool
js::DefinePropertiesAndFunctions(JSContext *cx, HandleObject obj,
                                 const JSPropertySpec *ps, const JSFunctionSpec *fs)
{
    if (ps && !JS_DefineProperties(cx, obj, ps))
        return false;
    if (fs && !JS_DefineFunctions(cx, obj, fs))
        return false;
    return true;
}

static void
GlobalDebuggees_finalize(FreeOp *fop, JSObject *obj)
{
    fop->delete_((GlobalObject::DebuggerVector *) obj->getPrivate());
}

static const Class
GlobalDebuggees_class = {
    "GlobalDebuggee", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, GlobalDebuggees_finalize
};

GlobalObject::DebuggerVector *
GlobalObject::getDebuggers()
{
    Value debuggers = getReservedSlot(DEBUGGERS);
    if (debuggers.isUndefined())
        return nullptr;
    JS_ASSERT(debuggers.toObject().getClass() == &GlobalDebuggees_class);
    return (DebuggerVector *) debuggers.toObject().getPrivate();
}

/* static */ GlobalObject::DebuggerVector *
GlobalObject::getOrCreateDebuggers(JSContext *cx, Handle<GlobalObject*> global)
{
    assertSameCompartment(cx, global);
    DebuggerVector *debuggers = global->getDebuggers();
    if (debuggers)
        return debuggers;

    JSObject *obj = NewObjectWithGivenProto(cx, &GlobalDebuggees_class, nullptr, global);
    if (!obj)
        return nullptr;
    debuggers = cx->new_<DebuggerVector>();
    if (!debuggers)
        return nullptr;
    obj->setPrivate(debuggers);
    global->setReservedSlot(DEBUGGERS, ObjectValue(*obj));
    return debuggers;
}

/* static */ bool
GlobalObject::addDebugger(JSContext *cx, Handle<GlobalObject*> global, Debugger *dbg)
{
    DebuggerVector *debuggers = getOrCreateDebuggers(cx, global);
    if (!debuggers)
        return false;
#ifdef DEBUG
    for (Debugger **p = debuggers->begin(); p != debuggers->end(); p++)
        JS_ASSERT(*p != dbg);
#endif
    if (debuggers->empty() && !global->compartment()->addDebuggee(cx, global))
        return false;
    if (!debuggers->append(dbg)) {
        (void) global->compartment()->removeDebuggee(cx, global);
        return false;
    }
    return true;
}

/* static */ JSObject *
GlobalObject::getOrCreateForOfPICObject(JSContext *cx, Handle<GlobalObject *> global)
{
    assertSameCompartment(cx, global);
    JSObject *forOfPIC = global->getForOfPICObject();
    if (forOfPIC)
        return forOfPIC;

    forOfPIC = ForOfPIC::createForOfPICObject(cx, global);
    if (!forOfPIC)
        return nullptr;
    global->setReservedSlot(FOR_OF_PIC_CHAIN, ObjectValue(*forOfPIC));
    return forOfPIC;
}

bool
GlobalObject::hasRegExpStatics() const
{
    return !getSlot(REGEXP_STATICS).isUndefined();
}

RegExpStatics *
GlobalObject::getRegExpStatics(ExclusiveContext *cx) const
{
    MOZ_ASSERT(cx);
    Rooted<GlobalObject*> self(cx, const_cast<GlobalObject*>(this));

    JSObject *resObj = nullptr;
    const Value &val = this->getSlot(REGEXP_STATICS);
    if (!val.isObject()) {
        MOZ_ASSERT(val.isUndefined());
        resObj = RegExpStatics::create(cx, self);
        if (!resObj)
            return nullptr;

        self->initSlot(REGEXP_STATICS, ObjectValue(*resObj));
    } else {
        resObj = &val.toObject();
    }
    return static_cast<RegExpStatics*>(resObj->getPrivate(/* nfixed = */ 1));
}

RegExpStatics *
GlobalObject::getAlreadyCreatedRegExpStatics() const
{
    const Value &val = this->getSlot(REGEXP_STATICS);
    MOZ_ASSERT(val.isObject());
    return static_cast<RegExpStatics*>(val.toObject().getPrivate(/* nfixed = */ 1));
}

bool
GlobalObject::getSelfHostedFunction(JSContext *cx, HandleAtom selfHostedName, HandleAtom name,
                                    unsigned nargs, MutableHandleValue funVal)
{
    RootedId shId(cx, AtomToId(selfHostedName));
    RootedObject holder(cx, cx->global()->intrinsicsHolder());

    if (cx->global()->maybeGetIntrinsicValue(shId, funVal.address()))
        return true;

    JSFunction *fun = NewFunction(cx, NullPtr(), nullptr, nargs, JSFunction::INTERPRETED_LAZY,
                                  holder, name, JSFunction::ExtendedFinalizeKind, SingletonObject);
    if (!fun)
        return false;
    fun->setIsSelfHostedBuiltin();
    fun->setExtendedSlot(0, StringValue(selfHostedName));
    funVal.setObject(*fun);

    return cx->global()->addIntrinsicValue(cx, shId, funVal);
}

bool
GlobalObject::addIntrinsicValue(JSContext *cx, HandleId id, HandleValue value)
{
    RootedObject holder(cx, intrinsicsHolder());

    uint32_t slot = holder->slotSpan();
    RootedShape last(cx, holder->lastProperty());
    Rooted<UnownedBaseShape*> base(cx, last->base()->unowned());

    StackShape child(base, id, slot, 0, 0);
    RootedShape shape(cx, cx->compartment()->propertyTree.getChild(cx, last, child));
    if (!shape)
        return false;

    if (!JSObject::setLastProperty(cx, holder, shape))
        return false;

    holder->setSlot(shape->slot(), value);
    return true;
}
