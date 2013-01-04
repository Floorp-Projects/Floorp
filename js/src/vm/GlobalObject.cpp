/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GlobalObject.h"

#include "jscntxt.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsfriendapi.h"
#include "jsmath.h"
#include "json.h"
#include "jsweakmap.h"

#include "builtin/Eval.h"
#include "builtin/Intl.h"
#include "builtin/MapObject.h"
#include "builtin/Object.h"
#include "builtin/RegExp.h"
#include "frontend/BytecodeEmitter.h"

#include "jsobjinlines.h"

#include "vm/GlobalObject-inl.h"
#include "vm/RegExpObject-inl.h"
#include "vm/RegExpStatics-inl.h"

#ifdef JS_METHODJIT
#include "methodjit/Retcon.h"
#endif

using namespace js;

JSObject *
js_InitObjectClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    return obj->asGlobal().getOrCreateObjectPrototype(cx);
}

JSObject *
js_InitFunctionClass(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isNative());

    return obj->asGlobal().getOrCreateFunctionPrototype(cx);
}

static JSBool
ThrowTypeError(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                 JSMSG_THROW_TYPE_ERROR);
    return false;
}

static bool
TestProtoGetterThis(const Value &v)
{
    return !v.isNullOrUndefined();
}

static bool
ProtoGetterImpl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(TestProtoGetterThis(args.thisv()));

    const Value &thisv = args.thisv();
    if (thisv.isPrimitive() && !BoxNonStrictThis(cx, args))
        return false;

    unsigned dummy;
    RootedObject obj(cx, &args.thisv().toObject());
    RootedId nid(cx, NameToId(cx->names().proto));
    RootedValue v(cx);
    if (!CheckAccess(cx, obj, nid, JSACC_PROTO, &v, &dummy))
        return false;

    args.rval().set(v);
    return true;
}

static JSBool
ProtoGetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, TestProtoGetterThis, ProtoGetterImpl, args);
}

namespace js {
size_t sSetProtoCalled = 0;
} // namespace js

static bool
TestProtoSetterThis(const Value &v)
{
    if (v.isNullOrUndefined())
        return false;

    /* These will work as if on a boxed primitive; dumb, but whatever. */
    if (!v.isObject())
        return true;

    /* Otherwise, only accept non-proxies. */
    return !v.toObject().isProxy();
}

static bool
ProtoSetterImpl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(TestProtoSetterThis(args.thisv()));

    const Value &thisv = args.thisv();
    if (thisv.isPrimitive()) {
        JS_ASSERT(!thisv.isNullOrUndefined());

        // Mutating a boxed primitive's [[Prototype]] has no side effects.
        args.rval().setUndefined();
        return true;
    }

    if (!cx->runningWithTrustedPrincipals())
        ++sSetProtoCalled;

    Rooted<JSObject*> obj(cx, &args.thisv().toObject());

    /* ES5 8.6.2 forbids changing [[Prototype]] if not [[Extensible]]. */
    if (!obj->isExtensible()) {
        obj->reportNotExtensible(cx);
        return false;
    }

    /*
     * Disallow mutating the [[Prototype]] of a proxy that wasn't simply
     * wrapping some other object.  Also disallow it on ArrayBuffer objects,
     * which due to their complicated delegate-object shenanigans can't easily
     * have a mutable [[Prototype]].
     */
    if (obj->isProxy() || obj->isArrayBuffer()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                             "Object", "__proto__ setter",
                             obj->isProxy() ? "Proxy" : "ArrayBuffer");
        return false;
    }

    /* Do nothing if __proto__ isn't being set to an object or null. */
    if (args.length() == 0 || !args[0].isObjectOrNull()) {
        args.rval().setUndefined();
        return true;
    }

    Rooted<JSObject*> newProto(cx, args[0].toObjectOrNull());

    unsigned dummy;
    RootedId nid(cx, NameToId(cx->names().proto));
    RootedValue v(cx);
    if (!CheckAccess(cx, obj, nid, JSAccessMode(JSACC_PROTO | JSACC_WRITE), &v, &dummy))
        return false;

    if (!SetProto(cx, obj, newProto, true))
        return false;

    args.rval().setUndefined();
    return true;
}

static JSBool
ProtoSetter(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, TestProtoSetterThis, ProtoSetterImpl, args);
}

JSObject *
GlobalObject::initFunctionAndObjectClasses(JSContext *cx)
{
    Rooted<GlobalObject*> self(cx, this);

    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    JS_ASSERT(isNative());

    cx->setDefaultCompartmentObjectIfUnset(self);

    RootedObject objectProto(cx);

    /*
     * Create |Object.prototype| first, mirroring CreateBlankProto but for the
     * prototype of the created object.
     */
    objectProto = NewObjectWithGivenProto(cx, &ObjectClass, NULL, self);
    if (!objectProto || !JSObject::setSingletonType(cx, objectProto))
        return NULL;

    /*
     * The default 'new' type of Object.prototype is required by type inference
     * to have unknown properties, to simplify handling of e.g. heterogenous
     * objects in JSON and script literals.
     */
    if (!setNewTypeUnknown(cx, objectProto))
        return NULL;

    /* Create |Function.prototype| next so we can create other functions. */
    RootedFunction functionProto(cx);
    {
        RawObject functionProto_ = NewObjectWithGivenProto(cx, &FunctionClass, objectProto, self);
        if (!functionProto_)
            return NULL;
        functionProto = functionProto_->toFunction();

        /*
         * Bizarrely, |Function.prototype| must be an interpreted function, so
         * give it the guts to be one.
         */
        {
            RawObject proto = js_NewFunction(cx, functionProto, NULL, 0, JSFunction::INTERPRETED,
                                             self, NullPtr());
            if (!proto)
                return NULL;
            JS_ASSERT(proto == functionProto);
            functionProto->setIsFunctionPrototype();
        }

        const char *rawSource = "() {\n}";
        size_t sourceLen = strlen(rawSource);
        jschar *source = InflateString(cx, rawSource, &sourceLen);
        if (!source)
            return NULL;
        ScriptSource *ss = cx->new_<ScriptSource>();
        if (!ss) {
            js_free(source);
            return NULL;
        }
        ScriptSourceHolder ssh(cx->runtime, ss);
        ss->setSource(source, sourceLen);

        CompileOptions options(cx);
        options.setNoScriptRval(true)
               .setVersion(JSVERSION_DEFAULT);
        RootedScript script(cx, JSScript::Create(cx,
                                                 /* enclosingScope = */ NullPtr(),
                                                 /* savedCallerFun = */ false,
                                                 options,
                                                 /* staticLevel = */ 0,
                                                 ss,
                                                 0,
                                                 ss->length()));
        if (!script || !JSScript::fullyInitTrivial(cx, script))
            return NULL;

        functionProto->initScript(script);
        functionProto->getType(cx)->interpretedFunction = functionProto;
        script->setFunction(functionProto);

        if (!JSObject::setSingletonType(cx, functionProto))
            return NULL;

        /*
         * The default 'new' type of Function.prototype is required by type
         * inference to have unknown properties, to simplify handling of e.g.
         * CloneFunctionObject.
         */
        if (!setNewTypeUnknown(cx, functionProto))
            return NULL;
    }

    /* Create the Object function now that we have a [[Prototype]] for it. */
    RootedFunction objectCtor(cx);
    {
        RootedObject ctor(cx, NewObjectWithGivenProto(cx, &FunctionClass, functionProto, self));
        if (!ctor)
            return NULL;
        RootedAtom objectAtom(cx, cx->names().Object);
        objectCtor = js_NewFunction(cx, ctor, obj_construct, 1, JSFunction::NATIVE_CTOR, self,
                                    objectAtom);
        if (!objectCtor)
            return NULL;
    }

    /*
     * Install |Object| and |Object.prototype| for the benefit of subsequent
     * code that looks for them.
     */
    self->setObjectClassDetails(objectCtor, objectProto);

    /* Create |Function| so it and |Function.prototype| can be installed. */
    RootedFunction functionCtor(cx);
    {
        // Note that ctor is rooted purely for the JS_ASSERT at the end
        RootedObject ctor(cx, NewObjectWithGivenProto(cx, &FunctionClass, functionProto, self));
        if (!ctor)
            return NULL;
        RootedAtom functionAtom(cx, cx->names().Function);
        functionCtor = js_NewFunction(cx, ctor, Function, 1, JSFunction::NATIVE_CTOR, self,
                                      functionAtom);
        if (!functionCtor)
            return NULL;
        JS_ASSERT(ctor == functionCtor);
    }

    /*
     * Install |Function| and |Function.prototype| so that we can freely create
     * functions and objects without special effort.
     */
    self->setFunctionClassDetails(functionCtor, functionProto);

    /*
     * The hard part's done: now go back and add all the properties these
     * primordial values have.
     */
    if (!LinkConstructorAndPrototype(cx, objectCtor, objectProto) ||
        !DefinePropertiesAndBrand(cx, objectProto, NULL, object_methods))
    {
        return NULL;
    }

    /*
     * Add an Object.prototype.__proto__ accessor property to implement that
     * extension (if it's actually enabled).  Cache the getter for this
     * function so that cross-compartment [[Prototype]]-getting is implemented
     * in one place.
     */
    RootedFunction getter(cx, js_NewFunction(cx, NullPtr(), ProtoGetter, 0, JSFunction::NATIVE_FUN,
                                             self, NullPtr()));
    if (!getter)
        return NULL;
#if JS_HAS_OBJ_PROTO_PROP
    RootedFunction setter(cx, js_NewFunction(cx, NullPtr(), ProtoSetter, 0, JSFunction::NATIVE_FUN,
                                             self, NullPtr()));
    if (!setter)
        return NULL;
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, objectProto,
                                  cx->names().proto, undefinedValue,
                                  JS_DATA_TO_FUNC_PTR(PropertyOp, getter.get()),
                                  JS_DATA_TO_FUNC_PTR(StrictPropertyOp, setter.get()),
                                  JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED))
    {
        return NULL;
    }
#endif /* JS_HAS_OBJ_PROTO_PROP */
    self->setProtoGetter(getter);


    if (!DefinePropertiesAndBrand(cx, objectCtor, NULL, object_static_methods) ||
        !LinkConstructorAndPrototype(cx, functionCtor, functionProto) ||
        !DefinePropertiesAndBrand(cx, functionProto, NULL, function_methods) ||
        !DefinePropertiesAndBrand(cx, functionCtor, NULL, NULL))
    {
        return NULL;
    }

    /* Add the global Function and Object properties now. */
    if (!self->addDataProperty(cx, NameToId(cx->names().Object), JSProto_Object + JSProto_LIMIT * 2, 0))
        return NULL;
    if (!self->addDataProperty(cx, NameToId(cx->names().Function), JSProto_Function + JSProto_LIMIT * 2, 0))
        return NULL;

    /* Heavy lifting done, but lingering tasks remain. */

    /* ES5 15.1.2.1. */
    RootedId evalId(cx, NameToId(cx->names().eval));
    RawObject evalobj = js_DefineFunction(cx, self, evalId, IndirectEval, 1, JSFUN_STUB_GSOPS);
    if (!evalobj)
        return NULL;
    self->setOriginalEval(evalobj);

    /* ES5 13.2.3: Construct the unique [[ThrowTypeError]] function object. */
    RootedFunction throwTypeError(cx, js_NewFunction(cx, NullPtr(), ThrowTypeError, 0,
                                                     JSFunction::NATIVE_FUN, self, NullPtr()));
    if (!throwTypeError)
        return NULL;
    if (!throwTypeError->preventExtensions(cx))
        return NULL;
    self->setThrowTypeError(throwTypeError);

    RootedObject intrinsicsHolder(cx);
    if (cx->runtime->isSelfHostingGlobal(self)) {
        intrinsicsHolder = this;
    } else {
        intrinsicsHolder = NewObjectWithClassProto(cx, &ObjectClass, NULL, self);
        if (!intrinsicsHolder)
            return NULL;
    }
    self->setIntrinsicsHolder(intrinsicsHolder);
    /* Define a property 'global' with the current global as its value. */
    RootedValue global(cx, ObjectValue(*self));
    if (!JSObject::defineProperty(cx, intrinsicsHolder, cx->names().global,
                                  global, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return NULL;
    }

    /*
     * The global object should have |Object.prototype| as its [[Prototype]].
     * Eventually we'd like to have standard classes be there from the start,
     * and thus we would know we were always setting what had previously been a
     * null [[Prototype]], but right now some code assumes it can set the
     * [[Prototype]] before standard classes have been initialized.  For now,
     * only set the [[Prototype]] if it hasn't already been set.
     */
    Rooted<TaggedProto> tagged(cx, TaggedProto(objectProto));
    if (self->shouldSplicePrototype(cx) && !self->splicePrototype(cx, tagged))
        return NULL;

    /*
     * Notify any debuggers about the creation of the script for
     * |Function.prototype| -- after all initialization, for simplicity.
     */
    RootedScript functionProtoScript(cx, functionProto->nonLazyScript());
    CallNewScriptHook(cx, functionProtoScript, functionProto);
    return functionProto;
}

GlobalObject *
GlobalObject::create(JSContext *cx, Class *clasp)
{
    JS_ASSERT(clasp->flags & JSCLASS_IS_GLOBAL);

    JSObject *obj = NewObjectWithGivenProto(cx, clasp, NULL, NULL);
    if (!obj)
        return NULL;

    Rooted<GlobalObject *> global(cx, &obj->asGlobal());

    cx->compartment->initGlobal(*global);

    if (!JSObject::setSingletonType(cx, global) || !global->setVarObj(cx))
        return NULL;
    if (!global->setDelegate(cx))
        return NULL;

    /* Construct a regexp statics object for this global object. */
    JSObject *res = RegExpStatics::create(cx, global);
    if (!res)
        return NULL;

    global->initSlot(REGEXP_STATICS, ObjectValue(*res));
    return global;
}

/* static */ bool
GlobalObject::initStandardClasses(JSContext *cx, Handle<GlobalObject*> global)
{
    /* Define a top-level property 'undefined' with the undefined value. */
    RootedValue undefinedValue(cx, UndefinedValue());
    if (!JSObject::defineProperty(cx, global, cx->names().undefined, undefinedValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, JSPROP_PERMANENT | JSPROP_READONLY))
    {
        return false;
    }

    if (!global->initFunctionAndObjectClasses(cx))
        return false;

    /* Initialize the rest of the standard objects and functions. */
    return js_InitArrayClass(cx, global) &&
           js_InitBooleanClass(cx, global) &&
           js_InitExceptionClasses(cx, global) &&
           js_InitMathClass(cx, global) &&
           js_InitNumberClass(cx, global) &&
           js_InitJSONClass(cx, global) &&
           js_InitRegExpClass(cx, global) &&
           js_InitStringClass(cx, global) &&
           js_InitTypedArrayClasses(cx, global) &&
#if JS_HAS_XML_SUPPORT
           (!VersionHasAllowXML(cx->findVersion()) || js_InitXMLClasses(cx, global)) &&
#endif
           js_InitIteratorClasses(cx, global) &&
           js_InitDateClass(cx, global) &&
           js_InitWeakMapClass(cx, global) &&
           js_InitProxyClass(cx, global) &&
           js_InitMapClass(cx, global) &&
           GlobalObject::initMapIteratorProto(cx, global) &&
           js_InitSetClass(cx, global) &&
           GlobalObject::initSetIteratorProto(cx, global) &&
#if ENABLE_INTL_API
           js_InitIntlClass(cx, global) &&
#endif
           true;
}

bool
GlobalObject::isRuntimeCodeGenEnabled(JSContext *cx)
{
    HeapSlot &v = getSlotRef(RUNTIME_CODEGEN_ENABLED);
    if (v.isUndefined()) {
        /*
         * If there are callbacks, make sure that the CSP callback is installed
         * and that it permits runtime code generation, then cache the result.
         */
        JSCSPEvalChecker allows = cx->runtime->securityCallbacks->contentSecurityPolicyAllows;
        v.set(this, RUNTIME_CODEGEN_ENABLED, BooleanValue(!allows || allows(cx)));
    }
    return !v.isFalse();
}

JSFunction *
GlobalObject::createConstructor(JSContext *cx, Native ctor, JSAtom *nameArg, unsigned length,
                                gc::AllocKind kind)
{
    RootedAtom name(cx, nameArg);
    RootedObject self(cx, this);
    return js_NewFunction(cx, NullPtr(), ctor, length, JSFunction::NATIVE_CTOR, self, name, kind);
}

static JSObject *
CreateBlankProto(JSContext *cx, Class *clasp, JSObject &proto, GlobalObject &global)
{
    JS_ASSERT(clasp != &ObjectClass);
    JS_ASSERT(clasp != &FunctionClass);

    RootedObject blankProto(cx, NewObjectWithGivenProto(cx, clasp, &proto, &global));
    if (!blankProto || !JSObject::setSingletonType(cx, blankProto))
        return NULL;

    return blankProto;
}

JSObject *
GlobalObject::createBlankPrototype(JSContext *cx, Class *clasp)
{
    Rooted<GlobalObject*> self(cx, this);
    JSObject *objectProto = getOrCreateObjectPrototype(cx);
    if (!objectProto)
        return NULL;

    return CreateBlankProto(cx, clasp, *objectProto, *self.get());
}

JSObject *
GlobalObject::createBlankPrototypeInheriting(JSContext *cx, Class *clasp, JSObject &proto)
{
    return CreateBlankProto(cx, clasp, proto, *this);
}

bool
js::LinkConstructorAndPrototype(JSContext *cx, JSObject *ctor_, JSObject *proto_)
{
    RootedObject ctor(cx, ctor_), proto(cx, proto_);

    RootedValue protoVal(cx, ObjectValue(*proto));
    RootedValue ctorVal(cx, ObjectValue(*ctor));

    return JSObject::defineProperty(cx, ctor, cx->names().classPrototype,
                                    protoVal, JS_PropertyStub, JS_StrictPropertyStub,
                                    JSPROP_PERMANENT | JSPROP_READONLY) &&
           JSObject::defineProperty(cx, proto, cx->names().constructor,
                                    ctorVal, JS_PropertyStub, JS_StrictPropertyStub, 0);
}

bool
js::DefinePropertiesAndBrand(JSContext *cx, JSObject *obj_,
                             const JSPropertySpec *ps, const JSFunctionSpec *fs)
{
    RootedObject obj(cx, obj_);

    if (ps && !JS_DefineProperties(cx, obj, const_cast<JSPropertySpec*>(ps)))
        return false;
    if (fs && !JS_DefineFunctions(cx, obj, const_cast<JSFunctionSpec*>(fs)))
        return false;
    return true;
}

static void
GlobalDebuggees_finalize(FreeOp *fop, RawObject obj)
{
    fop->delete_((GlobalObject::DebuggerVector *) obj->getPrivate());
}

static Class
GlobalDebuggees_class = {
    "GlobalDebuggee", JSCLASS_HAS_PRIVATE,
    JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, GlobalDebuggees_finalize
};

GlobalObject::DebuggerVector *
GlobalObject::getDebuggers()
{
    Value debuggers = getReservedSlot(DEBUGGERS);
    if (debuggers.isUndefined())
        return NULL;
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

    JSObject *obj = NewObjectWithGivenProto(cx, &GlobalDebuggees_class, NULL, global);
    if (!obj)
        return NULL;
    debuggers = cx->new_<DebuggerVector>();
    if (!debuggers)
        return NULL;
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
        global->compartment()->removeDebuggee(cx->runtime->defaultFreeOp(), global);
        return false;
    }
    return true;
}
