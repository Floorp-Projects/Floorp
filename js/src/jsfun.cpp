/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS function support.
 */

#include "jsfuninlines.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Range.h"

#include <string.h>

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jswrapper.h"

#include "builtin/Eval.h"
#include "builtin/Object.h"
#include "builtin/SelfHostingDefines.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/TokenStream.h"
#include "gc/Marking.h"
#include "gc/Policy.h"
#include "jit/InlinableNatives.h"
#include "jit/Ion.h"
#include "jit/JitFrameIterator.h"
#include "js/CallNonGenericMethod.h"
#include "js/Proxy.h"
#include "vm/AsyncFunction.h"
#include "vm/Debugger.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/SelfHosting.h"
#include "vm/Shape.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/StringBuffer.h"
#include "vm/WrapperObject.h"
#include "vm/Xdr.h"

#include "jsscriptinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::frontend;

using mozilla::ArrayLength;
using mozilla::Maybe;
using mozilla::PodCopy;
using mozilla::RangedPtr;
using mozilla::Some;

static bool
fun_enumerate(JSContext* cx, HandleObject obj)
{
    MOZ_ASSERT(obj->is<JSFunction>());

    RootedId id(cx);
    bool found;

    if (!obj->isBoundFunction() && !obj->as<JSFunction>().isArrow()) {
        id = NameToId(cx->names().prototype);
        if (!HasProperty(cx, obj, id, &found))
            return false;
    }

    id = NameToId(cx->names().length);
    if (!HasProperty(cx, obj, id, &found))
        return false;

    id = NameToId(cx->names().name);
    if (!HasProperty(cx, obj, id, &found))
        return false;

    return true;
}

bool
IsFunction(HandleValue v)
{
    return v.isObject() && v.toObject().is<JSFunction>();
}

static bool
AdvanceToActiveCallLinear(JSContext* cx, NonBuiltinScriptFrameIter& iter, HandleFunction fun)
{
    MOZ_ASSERT(!fun->isBuiltin());

    for (; !iter.done(); ++iter) {
        if (!iter.isFunctionFrame())
            continue;
        if (iter.matchCallee(cx, fun))
            return true;
    }
    return false;
}

static void
ThrowTypeErrorBehavior(JSContext* cx)
{
    JS_ReportErrorFlagsAndNumberASCII(cx, JSREPORT_ERROR, GetErrorMessage, nullptr,
                                     JSMSG_THROW_TYPE_ERROR);
}

static bool
IsFunctionInStrictMode(JSFunction* fun)
{
    // Interpreted functions have a strict flag.
    if (fun->isInterpreted() && fun->strict())
        return true;

    // Only asm.js functions can also be strict.
    return IsAsmJSStrictModeModuleOrFunction(fun);
}

// Beware: this function can be invoked on *any* function! That includes
// natives, strict mode functions, bound functions, arrow functions,
// self-hosted functions and constructors, asm.js functions, functions with
// destructuring arguments and/or a rest argument, and probably a few more I
// forgot. Turn back and save yourself while you still can. It's too late for
// me.
static bool
ArgumentsRestrictions(JSContext* cx, HandleFunction fun)
{
    // Throw if the function is a builtin (note: this doesn't include asm.js),
    // a strict mode function, or a bound function.
    // TODO (bug 1057208): ensure semantics are correct for all possible
    // pairings of callee/caller.
    if (fun->isBuiltin() || IsFunctionInStrictMode(fun) || fun->isBoundFunction()) {
        ThrowTypeErrorBehavior(cx);
        return false;
    }

    // Otherwise emit a strict warning about |f.arguments| to discourage use of
    // this non-standard, performance-harmful feature.
    if (!JS_ReportErrorFlagsAndNumberASCII(cx, JSREPORT_WARNING | JSREPORT_STRICT, GetErrorMessage,
                                           nullptr, JSMSG_DEPRECATED_USAGE, js_arguments_str))
    {
        return false;
    }

    return true;
}

bool
ArgumentsGetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsFunction(args.thisv()));

    RootedFunction fun(cx, &args.thisv().toObject().as<JSFunction>());
    if (!ArgumentsRestrictions(cx, fun))
        return false;

    // Return null if this function wasn't found on the stack.
    NonBuiltinScriptFrameIter iter(cx);
    if (!AdvanceToActiveCallLinear(cx, iter, fun)) {
        args.rval().setNull();
        return true;
    }

    Rooted<ArgumentsObject*> argsobj(cx, ArgumentsObject::createUnexpected(cx, iter));
    if (!argsobj)
        return false;

    // Disabling compiling of this script in IonMonkey.  IonMonkey doesn't
    // guarantee |f.arguments| can be fully recovered, so we try to mitigate
    // observing this behavior by detecting its use early.
    JSScript* script = iter.script();
    jit::ForbidCompilation(cx, script);

    args.rval().setObject(*argsobj);
    return true;
}

static bool
ArgumentsGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsFunction, ArgumentsGetterImpl>(cx, args);
}

bool
ArgumentsSetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsFunction(args.thisv()));

    RootedFunction fun(cx, &args.thisv().toObject().as<JSFunction>());
    if (!ArgumentsRestrictions(cx, fun))
        return false;

    // If the function passes the gauntlet, return |undefined|.
    args.rval().setUndefined();
    return true;
}

static bool
ArgumentsSetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsFunction, ArgumentsSetterImpl>(cx, args);
}

// Beware: this function can be invoked on *any* function! That includes
// natives, strict mode functions, bound functions, arrow functions,
// self-hosted functions and constructors, asm.js functions, functions with
// destructuring arguments and/or a rest argument, and probably a few more I
// forgot. Turn back and save yourself while you still can. It's too late for
// me.
static bool
CallerRestrictions(JSContext* cx, HandleFunction fun)
{
    // Throw if the function is a builtin (note: this doesn't include asm.js),
    // a strict mode function, or a bound function.
    // TODO (bug 1057208): ensure semantics are correct for all possible
    // pairings of callee/caller.
    if (fun->isBuiltin() || IsFunctionInStrictMode(fun) || fun->isBoundFunction()) {
        ThrowTypeErrorBehavior(cx);
        return false;
    }

    // Otherwise emit a strict warning about |f.caller| to discourage use of
    // this non-standard, performance-harmful feature.
    if (!JS_ReportErrorFlagsAndNumberASCII(cx, JSREPORT_WARNING | JSREPORT_STRICT, GetErrorMessage,
                                           nullptr, JSMSG_DEPRECATED_USAGE, js_caller_str))
    {
        return false;
    }

    return true;
}

bool
CallerGetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsFunction(args.thisv()));

    // Beware!  This function can be invoked on *any* function!  It can't
    // assume it'll never be invoked on natives, strict mode functions, bound
    // functions, or anything else that ordinarily has immutable .caller
    // defined with [[ThrowTypeError]].
    RootedFunction fun(cx, &args.thisv().toObject().as<JSFunction>());
    if (!CallerRestrictions(cx, fun))
        return false;

    // Also return null if this function wasn't found on the stack.
    NonBuiltinScriptFrameIter iter(cx);
    if (!AdvanceToActiveCallLinear(cx, iter, fun)) {
        args.rval().setNull();
        return true;
    }

    ++iter;
    while (!iter.done() && iter.isEvalFrame())
        ++iter;

    if (iter.done() || !iter.isFunctionFrame()) {
        args.rval().setNull();
        return true;
    }

    RootedObject caller(cx, iter.callee(cx));
    if (!cx->compartment()->wrap(cx, &caller))
        return false;

    // Censor the caller if we don't have full access to it.  If we do, but the
    // caller is a function with strict mode code, throw a TypeError per ES5.
    // If we pass these checks, we can return the computed caller.
    {
        JSObject* callerObj = CheckedUnwrap(caller);
        if (!callerObj) {
            args.rval().setNull();
            return true;
        }

        JSFunction* callerFun = &callerObj->as<JSFunction>();
        MOZ_ASSERT(!callerFun->isBuiltin(), "non-builtin iterator returned a builtin?");

        if (callerFun->strict()) {
            JS_ReportErrorFlagsAndNumberASCII(cx, JSREPORT_ERROR, GetErrorMessage, nullptr,
                                              JSMSG_CALLER_IS_STRICT);
            return false;
        }
    }

    args.rval().setObject(*caller);
    return true;
}

static bool
CallerGetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsFunction, CallerGetterImpl>(cx, args);
}

bool
CallerSetterImpl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsFunction(args.thisv()));

    // Beware!  This function can be invoked on *any* function!  It can't
    // assume it'll never be invoked on natives, strict mode functions, bound
    // functions, or anything else that ordinarily has immutable .caller
    // defined with [[ThrowTypeError]].
    RootedFunction fun(cx, &args.thisv().toObject().as<JSFunction>());
    if (!CallerRestrictions(cx, fun))
        return false;

    // Return |undefined| unless an error must be thrown.
    args.rval().setUndefined();

    // We can almost just return |undefined| here -- but if the caller function
    // was strict mode code, we still have to throw a TypeError.  This requires
    // computing the caller, checking that no security boundaries are crossed,
    // and throwing a TypeError if the resulting caller is strict.

    NonBuiltinScriptFrameIter iter(cx);
    if (!AdvanceToActiveCallLinear(cx, iter, fun))
        return true;

    ++iter;
    while (!iter.done() && iter.isEvalFrame())
        ++iter;

    if (iter.done() || !iter.isFunctionFrame())
        return true;

    RootedObject caller(cx, iter.callee(cx));
    if (!cx->compartment()->wrap(cx, &caller)) {
        cx->clearPendingException();
        return true;
    }

    // If we don't have full access to the caller, or the caller is not strict,
    // return undefined.  Otherwise throw a TypeError.
    JSObject* callerObj = CheckedUnwrap(caller);
    if (!callerObj)
        return true;

    JSFunction* callerFun = &callerObj->as<JSFunction>();
    MOZ_ASSERT(!callerFun->isBuiltin(), "non-builtin iterator returned a builtin?");

    if (callerFun->strict()) {
        JS_ReportErrorFlagsAndNumberASCII(cx, JSREPORT_ERROR, GetErrorMessage, nullptr,
                                          JSMSG_CALLER_IS_STRICT);
        return false;
    }

    return true;
}

static bool
CallerSetter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsFunction, CallerSetterImpl>(cx, args);
}

static const JSPropertySpec function_properties[] = {
    JS_PSGS("arguments", ArgumentsGetter, ArgumentsSetter, 0),
    JS_PSGS("caller", CallerGetter, CallerSetter, 0),
    JS_PS_END
};

static bool
ResolveInterpretedFunctionPrototype(JSContext* cx, HandleFunction fun, HandleId id)
{
    MOZ_ASSERT(fun->isInterpreted() || fun->isAsmJSNative());
    MOZ_ASSERT(id == NameToId(cx->names().prototype));

    // Assert that fun is not a compiler-created function object, which
    // must never leak to script or embedding code and then be mutated.
    // Also assert that fun is not bound, per the ES5 15.3.4.5 ref above.
    MOZ_ASSERT(!IsInternalFunctionObject(*fun));
    MOZ_ASSERT(!fun->isBoundFunction());

    // Make the prototype object an instance of Object with the same parent as
    // the function object itself, unless the function is an ES6 generator.  In
    // that case, per the 15 July 2013 ES6 draft, section 15.19.3, its parent is
    // the GeneratorObjectPrototype singleton.
    bool isStarGenerator = fun->isStarGenerator();
    Rooted<GlobalObject*> global(cx, &fun->global());
    RootedObject objProto(cx);
    if (isStarGenerator)
        objProto = GlobalObject::getOrCreateStarGeneratorObjectPrototype(cx, global);
    else
        objProto = fun->global().getOrCreateObjectPrototype(cx);
    if (!objProto)
        return false;

    RootedPlainObject proto(cx, NewObjectWithGivenProto<PlainObject>(cx, objProto,
                                                                     SingletonObject));
    if (!proto)
        return false;

    // Per ES5 13.2 the prototype's .constructor property is configurable,
    // non-enumerable, and writable.  However, per the 15 July 2013 ES6 draft,
    // section 15.19.3, the .prototype of a generator function does not link
    // back with a .constructor.
    if (!isStarGenerator) {
        RootedValue objVal(cx, ObjectValue(*fun));
        if (!DefineProperty(cx, proto, cx->names().constructor, objVal, nullptr, nullptr, 0))
            return false;
    }

    // Per ES5 15.3.5.2 a user-defined function's .prototype property is
    // initially non-configurable, non-enumerable, and writable.
    RootedValue protoVal(cx, ObjectValue(*proto));
    return DefineProperty(cx, fun, id, protoVal, nullptr, nullptr,
                          JSPROP_PERMANENT | JSPROP_RESOLVING);
}

static bool
fun_mayResolve(const JSAtomState& names, jsid id, JSObject*)
{
    if (!JSID_IS_ATOM(id))
        return false;

    JSAtom* atom = JSID_TO_ATOM(id);
    return atom == names.prototype || atom == names.length || atom == names.name;
}

static bool
fun_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    if (!JSID_IS_ATOM(id))
        return true;

    RootedFunction fun(cx, &obj->as<JSFunction>());

    if (JSID_IS_ATOM(id, cx->names().prototype)) {
        /*
         * Built-in functions do not have a .prototype property per ECMA-262,
         * or (Object.prototype, Function.prototype, etc.) have that property
         * created eagerly.
         *
         * ES5 15.3.4.5: bound functions don't have a prototype property. The
         * isBuiltin() test covers this case because bound functions are native
         * (and thus built-in) functions by definition/construction.
         *
         * ES6 9.2.8 MakeConstructor defines the .prototype property on constructors.
         * Generators are not constructors, but they have a .prototype property anyway,
         * according to errata to ES6. See bug 1191486.
         *
         * Thus all of the following don't get a .prototype property:
         * - Methods (that are not class-constructors or generators)
         * - Arrow functions
         * - Function.prototype
         */
        if (fun->isBuiltin() || (!fun->isConstructor() && !fun->isGenerator()))
            return true;

        if (!ResolveInterpretedFunctionPrototype(cx, fun, id))
            return false;

        *resolvedp = true;
        return true;
    }

    bool isLength = JSID_IS_ATOM(id, cx->names().length);
    if (isLength || JSID_IS_ATOM(id, cx->names().name)) {
        MOZ_ASSERT(!IsInternalFunctionObject(*obj));

        RootedValue v(cx);

        // Since f.length and f.name are configurable, they could be resolved
        // and then deleted:
        //     function f(x) {}
        //     assertEq(f.length, 1);
        //     delete f.length;
        //     assertEq(f.name, "f");
        //     delete f.name;
        // Afterwards, asking for f.length or f.name again will cause this
        // resolve hook to run again. Defining the property again the second
        // time through would be a bug.
        //     assertEq(f.length, 0);  // gets Function.prototype.length!
        //     assertEq(f.name, "");  // gets Function.prototype.name!
        // We use the RESOLVED_LENGTH and RESOLVED_NAME flags as a hack to prevent this
        // bug.
        if (isLength) {
            if (fun->hasResolvedLength())
                return true;

            if (!fun->getUnresolvedLength(cx, &v))
                return false;
        } else {
            if (fun->hasResolvedName())
                return true;

            // Don't define an own .name property for unnamed functions.
            JSAtom* name = fun->getUnresolvedName(cx);
            if (name == nullptr)
                return true;

            v.setString(name);
        }

        if (!NativeDefineProperty(cx, fun, id, v, nullptr, nullptr,
                                  JSPROP_READONLY | JSPROP_RESOLVING))
        {
            return false;
        }

        if (isLength)
            fun->setResolvedLength();
        else
            fun->setResolvedName();

        *resolvedp = true;
        return true;
    }

    return true;
}

template<XDRMode mode>
bool
js::XDRInterpretedFunction(XDRState<mode>* xdr, HandleScope enclosingScope,
                           HandleScript enclosingScript, MutableHandleFunction objp)
{
    enum FirstWordFlag {
        HasAtom             = 0x1,
        IsStarGenerator     = 0x2,
        IsLazy              = 0x4,
        HasSingletonType    = 0x8
    };

    /* NB: Keep this in sync with CloneInnerInterpretedFunction. */
    RootedAtom atom(xdr->cx());
    uint32_t firstword = 0;        /* bitmask of FirstWordFlag */
    uint32_t flagsword = 0;        /* word for argument count and fun->flags */

    JSContext* cx = xdr->cx();
    RootedFunction fun(cx);
    RootedScript script(cx);
    Rooted<LazyScript*> lazy(cx);

    if (mode == XDR_ENCODE) {
        fun = objp;
        if (!fun->isInterpreted()) {
            JSAutoByteString funNameBytes;
            if (const char* name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr,
                                           JSMSG_NOT_SCRIPTED_FUNCTION, name);
            }
            return false;
        }

        if (fun->name() || fun->hasGuessedAtom())
            firstword |= HasAtom;

        if (fun->isStarGenerator())
            firstword |= IsStarGenerator;

        if (fun->isInterpretedLazy()) {
            // Encode a lazy script.
            firstword |= IsLazy;
            lazy = fun->lazyScript();
        } else {
            // Encode the script.
            script = fun->nonLazyScript();
        }

        if (fun->isSingleton())
            firstword |= HasSingletonType;

        atom = fun->displayAtom();
        flagsword = (fun->nargs() << 16) |
                    (fun->flags() & ~JSFunction::NO_XDR_FLAGS);

        // The environment of any function which is not reused will always be
        // null, it is later defined when a function is cloned or reused to
        // mirror the scope chain.
        MOZ_ASSERT_IF(fun->isSingleton() &&
                      !((lazy && lazy->hasBeenCloned()) || (script && script->hasBeenCloned())),
                      fun->environment() == nullptr);
    }

    if (!xdr->codeUint32(&firstword))
        return false;

    if ((firstword & HasAtom) && !XDRAtom(xdr, &atom))
        return false;
    if (!xdr->codeUint32(&flagsword))
        return false;

    if (mode == XDR_DECODE) {
        RootedObject proto(cx);
        if (firstword & IsStarGenerator) {
            proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, cx->global());
            if (!proto)
                return false;
        }

        gc::AllocKind allocKind = gc::AllocKind::FUNCTION;
        if (uint16_t(flagsword) & JSFunction::EXTENDED)
            allocKind = gc::AllocKind::FUNCTION_EXTENDED;
        fun = NewFunctionWithProto(cx, nullptr, 0, JSFunction::INTERPRETED,
                                   /* enclosingDynamicScope = */ nullptr, nullptr, proto,
                                   allocKind, TenuredObject);
        if (!fun)
            return false;
        script = nullptr;
    }

    if (firstword & IsLazy) {
        if (!XDRLazyScript(xdr, enclosingScope, enclosingScript, fun, &lazy))
            return false;
    } else {
        if (!XDRScript(xdr, enclosingScope, enclosingScript, fun, &script))
            return false;
    }

    if (mode == XDR_DECODE) {
        fun->setArgCount(flagsword >> 16);
        fun->setFlags(uint16_t(flagsword));
        fun->initAtom(atom);
        if (firstword & IsLazy) {
            MOZ_ASSERT(fun->lazyScript() == lazy);
        } else {
            MOZ_ASSERT(fun->nonLazyScript() == script);
            MOZ_ASSERT(fun->nargs() == script->numArgs());
        }

        bool singleton = firstword & HasSingletonType;
        if (!JSFunction::setTypeForScriptedFunction(cx, fun, singleton))
            return false;
        objp.set(fun);
    }

    return true;
}

template bool
js::XDRInterpretedFunction(XDRState<XDR_ENCODE>*, HandleScope, HandleScript, MutableHandleFunction);

template bool
js::XDRInterpretedFunction(XDRState<XDR_DECODE>*, HandleScope, HandleScript, MutableHandleFunction);

/* ES6 (04-25-16) 19.2.3.6 Function.prototype [ @@hasInstance ] */
bool
js::fun_symbolHasInstance(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (args.length() < 1) {
        args.rval().setBoolean(false);
        return true;
    }

    /* Step 1. */
    HandleValue func = args.thisv();

    // Primitives are non-callable and will always return false from
    // OrdinaryHasInstance.
    if (!func.isObject()) {
        args.rval().setBoolean(false);
        return true;
    }

    RootedObject obj(cx, &func.toObject());

    /* Step 2. */
    bool result;
    if (!OrdinaryHasInstance(cx, obj, args[0], &result))
        return false;

    args.rval().setBoolean(result);
    return true;
}

/*
 * ES6 (4-25-16) 7.3.19 OrdinaryHasInstance
 */
bool
JS::OrdinaryHasInstance(JSContext* cx, HandleObject objArg, HandleValue v, bool* bp)
{
    RootedObject obj(cx, objArg);

    /* Step 1. */
    if (!obj->isCallable()) {
        *bp = false;
        return true;
    }

    /* Step 2. */
    if (obj->is<JSFunction>() && obj->isBoundFunction()) {
        /* Steps 2a-b. */
        obj = obj->as<JSFunction>().getBoundFunctionTarget();
        return InstanceOfOperator(cx, obj, v, bp);
    }

    /* Step 3. */
    if (!v.isObject()) {
        *bp = false;
        return true;
    }

    /* Step 4. */
    RootedValue pval(cx);
    if (!GetProperty(cx, obj, obj, cx->names().prototype, &pval))
        return false;

    /* Step 5. */
    if (pval.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        RootedValue val(cx, ObjectValue(*obj));
        ReportValueError(cx, JSMSG_BAD_PROTOTYPE, -1, val, nullptr);
        return false;
    }

    /* Step 6. */
    RootedObject pobj(cx, &pval.toObject());
    bool isDelegate;
    if (!IsDelegate(cx, pobj, v, &isDelegate))
        return false;
    *bp = isDelegate;
    return true;
}

inline void
JSFunction::trace(JSTracer* trc)
{
    if (isExtended()) {
        TraceRange(trc, ArrayLength(toExtended()->extendedSlots),
                   (GCPtrValue*)toExtended()->extendedSlots, "nativeReserved");
    }

    TraceNullableEdge(trc, &atom_, "atom");

    if (isInterpreted()) {
        // Functions can be be marked as interpreted despite having no script
        // yet at some points when parsing, and can be lazy with no lazy script
        // for self-hosted code.
        if (hasScript() && !hasUncompiledScript())
            TraceManuallyBarrieredEdge(trc, &u.i.s.script_, "script");
        else if (isInterpretedLazy() && u.i.s.lazy_)
            TraceManuallyBarrieredEdge(trc, &u.i.s.lazy_, "lazyScript");

        if (u.i.env_)
            TraceManuallyBarrieredEdge(trc, &u.i.env_, "fun_environment");
    }
}

static void
fun_trace(JSTracer* trc, JSObject* obj)
{
    obj->as<JSFunction>().trace(trc);
}

static bool
ThrowTypeError(JSContext* cx, unsigned argc, Value* vp)
{
    ThrowTypeErrorBehavior(cx);
    return false;
}

static JSObject*
CreateFunctionConstructor(JSContext* cx, JSProtoKey key)
{
    Rooted<GlobalObject*> global(cx, cx->global());
    RootedObject functionProto(cx, &global->getPrototype(JSProto_Function).toObject());

    RootedObject functionCtor(cx,
      NewFunctionWithProto(cx, Function, 1, JSFunction::NATIVE_CTOR,
                           nullptr, HandlePropertyName(cx->names().Function),
                           functionProto, AllocKind::FUNCTION, SingletonObject));
    if (!functionCtor)
        return nullptr;

    return functionCtor;

}

static JSObject*
CreateFunctionPrototype(JSContext* cx, JSProtoKey key)
{
    Rooted<GlobalObject*> self(cx, cx->global());

    RootedObject objectProto(cx, &self->getPrototype(JSProto_Object).toObject());
    /*
     * Bizarrely, |Function.prototype| must be an interpreted function, so
     * give it the guts to be one.
     */
    RootedObject enclosingEnv(cx, &self->lexicalEnvironment());
    JSObject* functionProto_ =
        NewFunctionWithProto(cx, nullptr, 0, JSFunction::INTERPRETED,
                             enclosingEnv, nullptr, objectProto, AllocKind::FUNCTION,
                             SingletonObject);
    if (!functionProto_)
        return nullptr;

    RootedFunction functionProto(cx, &functionProto_->as<JSFunction>());

    const char* rawSource = "() {\n}";
    size_t sourceLen = strlen(rawSource);
    mozilla::UniquePtr<char16_t[], JS::FreePolicy> source(InflateString(cx, rawSource, &sourceLen));
    if (!source)
        return nullptr;

    ScriptSource* ss = cx->new_<ScriptSource>();
    if (!ss)
        return nullptr;
    ScriptSourceHolder ssHolder(ss);
    if (!ss->setSource(cx, mozilla::Move(source), sourceLen))
        return nullptr;

    CompileOptions options(cx);
    options.setNoScriptRval(true)
           .setVersion(JSVERSION_DEFAULT);
    RootedScriptSource sourceObject(cx, ScriptSourceObject::create(cx, ss));
    if (!sourceObject || !ScriptSourceObject::initFromOptions(cx, sourceObject, options))
        return nullptr;

    RootedScript script(cx, JSScript::Create(cx,
                                             options,
                                             sourceObject,
                                             0,
                                             ss->length()));
    if (!script || !JSScript::initFunctionPrototype(cx, script, functionProto))
        return nullptr;

    functionProto->initScript(script);
    ObjectGroup* protoGroup = functionProto->getGroup(cx);
    if (!protoGroup)
        return nullptr;

    protoGroup->setInterpretedFunction(functionProto);

    /*
     * The default 'new' group of Function.prototype is required by type
     * inference to have unknown properties, to simplify handling of e.g.
     * NewFunctionClone.
     */
    if (!JSObject::setNewGroupUnknown(cx, &JSFunction::class_, functionProto))
        return nullptr;

    // Set the prototype before we call NewFunctionWithProto below. This
    // ensures EmptyShape::getInitialShape can share function shapes.
    self->setPrototype(key, ObjectValue(*functionProto));

    // Construct the unique [[%ThrowTypeError%]] function object, used only for
    // "callee" and "caller" accessors on strict mode arguments objects.  (The
    // spec also uses this for "arguments" and "caller" on various functions,
    // but we're experimenting with implementing them using accessors on
    // |Function.prototype| right now.)
    //
    // Note that we can't use NewFunction here, even though we want the normal
    // Function.prototype for our proto, because we're still in the middle of
    // creating that as far as the world is concerned, so things will get all
    // confused.
    RootedFunction throwTypeError(cx,
      NewFunctionWithProto(cx, ThrowTypeError, 0, JSFunction::NATIVE_FUN,
                           nullptr, nullptr, functionProto, AllocKind::FUNCTION,
                           SingletonObject));
    if (!throwTypeError || !PreventExtensions(cx, throwTypeError))
        return nullptr;

    // The "length" property of %ThrowTypeError% is non-configurable, adjust
    // the default property attributes accordingly.
    Rooted<PropertyDescriptor> nonConfigurableDesc(cx);
    nonConfigurableDesc.setAttributes(JSPROP_PERMANENT | JSPROP_IGNORE_READONLY |
                                      JSPROP_IGNORE_ENUMERATE | JSPROP_IGNORE_VALUE);

    RootedId lengthId(cx, NameToId(cx->names().length));
    ObjectOpResult lengthResult;
    if (!NativeDefineProperty(cx, throwTypeError, lengthId, nonConfigurableDesc, lengthResult))
        return nullptr;
    MOZ_ASSERT(lengthResult);

    // Non-standard: Also change "name" to non-configurable. ECMAScript defines
    // %ThrowTypeError% as an anonymous function, i.e. it shouldn't actually
    // get an own "name" property. To be consistent with other built-in,
    // anonymous functions, we don't delete %ThrowTypeError%'s "name" property.
    RootedId nameId(cx, NameToId(cx->names().name));
    ObjectOpResult nameResult;
    if (!NativeDefineProperty(cx, throwTypeError, nameId, nonConfigurableDesc, nameResult))
        return nullptr;
    MOZ_ASSERT(nameResult);

    self->setThrowTypeError(throwTypeError);

    return functionProto;
}

static const ClassOps JSFunctionClassOps = {
    nullptr,                 /* addProperty */
    nullptr,                 /* delProperty */
    nullptr,                 /* getProperty */
    nullptr,                 /* setProperty */
    fun_enumerate,
    fun_resolve,
    fun_mayResolve,
    nullptr,                 /* finalize    */
    nullptr,                 /* call        */
    nullptr,
    nullptr,                 /* construct   */
    fun_trace,
};

static const ClassSpec JSFunctionClassSpec = {
    CreateFunctionConstructor,
    CreateFunctionPrototype,
    nullptr,
    nullptr,
    function_methods,
    function_properties
};

const Class JSFunction::class_ = {
    js_Function_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    &JSFunctionClassOps,
    &JSFunctionClassSpec
};

const Class* const js::FunctionClassPtr = &JSFunction::class_;

JSString*
js::FunctionToString(JSContext* cx, HandleFunction fun, bool prettyPrint)
{
    if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
        return nullptr;

    if (IsAsmJSModule(fun))
        return AsmJSModuleToString(cx, fun, !prettyPrint);
    if (IsAsmJSFunction(fun))
        return AsmJSFunctionToString(cx, fun);

    if (IsWrappedAsyncFunction(fun)) {
        RootedFunction unwrapped(cx, GetUnwrappedAsyncFunction(fun));
        return FunctionToString(cx, unwrapped, prettyPrint);
    }

    StringBuffer out(cx);
    RootedScript script(cx);

    if (fun->hasScript()) {
        script = fun->nonLazyScript();
        if (script->isGeneratorExp()) {
            if (!out.append("function genexp() {") ||
                !out.append("\n    [generator expression]\n") ||
                !out.append("}"))
            {
                return nullptr;
            }
            return out.finishString();
        }
    }

    if (fun->isAsync()) {
        if (!out.append("async "))
            return nullptr;
    }

    bool funIsMethodOrNonArrowLambda = (fun->isLambda() && !fun->isArrow()) || fun->isMethod() ||
                                        fun->isGetter() || fun->isSetter();
    bool haveSource = fun->isInterpreted() && !fun->isSelfHostedBuiltin();

    // If we're not in pretty mode, put parentheses around lambda functions and methods.
    if (haveSource && !prettyPrint && funIsMethodOrNonArrowLambda) {
        if (!out.append("("))
            return nullptr;
    }
    if (!fun->isArrow()) {
        bool ok;
        if (fun->isStarGenerator() && !fun->isAsync())
            ok = out.append("function* ");
        else
            ok = out.append("function ");
        if (!ok)
            return nullptr;
    }
    if (fun->name()) {
        if (!out.append(fun->name()))
            return nullptr;
    }

    if (haveSource && !script->scriptSource()->hasSourceData() &&
        !JSScript::loadSource(cx, script->scriptSource(), &haveSource))
    {
        return nullptr;
    }
    if (haveSource) {
        Rooted<JSFlatString*> src(cx, script->sourceData(cx));
        if (!src)
            return nullptr;

        if (!out.append(src))
            return nullptr;

        if (!prettyPrint && funIsMethodOrNonArrowLambda) {
            if (!out.append(")"))
                return nullptr;
        }
    } else if (fun->isInterpreted() && !fun->isSelfHostedBuiltin()) {
        if (!out.append("() {\n    ") ||
            !out.append("[sourceless code]") ||
            !out.append("\n}"))
        {
            return nullptr;
        }
    } else {
        MOZ_ASSERT(!fun->isExprBody());

        bool derived = fun->infallibleIsDefaultClassConstructor(cx);
        if (derived && fun->isDerivedClassConstructor()) {
            if (!out.append("(...args) {\n    ") ||
                !out.append("super(...args);\n}"))
            {
                return nullptr;
            }
        } else {
            if (!out.append("() {\n    "))
                return nullptr;

            if (!derived) {
                if (!out.append("[native code]"))
                    return nullptr;
            }

            if (!out.append("\n}"))
                return nullptr;
        }
    }
    return out.finishString();
}

JSString*
fun_toStringHelper(JSContext* cx, HandleObject obj, unsigned indent)
{
    if (!obj->is<JSFunction>()) {
        if (JSFunToStringOp op = obj->getOpsFunToString())
            return op(cx, obj, indent);

        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_INCOMPATIBLE_PROTO,
                                  js_Function_str, js_toString_str, "object");
        return nullptr;
    }

    RootedFunction fun(cx, &obj->as<JSFunction>());
    return FunctionToString(cx, fun, indent != JS_DONT_PRETTY_PRINT);
}

bool
js::FunctionHasDefaultHasInstance(JSFunction* fun, const WellKnownSymbols& symbols)
{
    jsid id = SYMBOL_TO_JSID(symbols.hasInstance);
    Shape* shape = fun->lookupPure(id);
    if (shape) {
        if (!shape->hasSlot() || !shape->hasDefaultGetter())
            return false;
        const Value hasInstance = fun->as<NativeObject>().getSlot(shape->slot());
        return IsNativeFunction(hasInstance, js::fun_symbolHasInstance);
    }
    return true;
}

bool
js::fun_toString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(IsFunctionObject(args.calleev()));

    uint32_t indent = 0;

    if (args.length() != 0 && !ToUint32(cx, args[0], &indent))
        return false;

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedString str(cx, fun_toStringHelper(cx, obj, indent));
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

#if JS_HAS_TOSOURCE
static bool
fun_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(IsFunctionObject(args.calleev()));

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedString str(cx);
    if (obj->isCallable())
        str = fun_toStringHelper(cx, obj, JS_DONT_PRETTY_PRINT);
    else
        str = ObjectToSource(cx, obj);

    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}
#endif

bool
js::fun_call(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    HandleValue func = args.thisv();

    // We don't need to do this -- Call would do it for us -- but the error
    // message is *much* better if we do this here.  (Without this,
    // JSDVG_SEARCH_STACK tries to decompile |func| as if it were |this| in
    // the scripted caller's frame -- so for example
    //
    //   Function.prototype.call.call({});
    //
    // would identify |{}| as |this| as being the result of evaluating
    // |Function.prototype.call| and would conclude, "Function.prototype.call
    // is not a function".  Grotesque.)
    if (!IsCallable(func)) {
        ReportIncompatibleMethod(cx, args, &JSFunction::class_);
        return false;
    }

    size_t argCount = args.length();
    if (argCount > 0)
        argCount--; // strip off provided |this|

    InvokeArgs iargs(cx);
    if (!iargs.init(cx, argCount))
        return false;

    for (size_t i = 0; i < argCount; i++)
        iargs[i].set(args[i + 1]);

    return Call(cx, func, args.get(0), iargs, args.rval());
}

// ES5 15.3.4.3
bool
js::fun_apply(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    //
    // Note that we must check callability here, not at actual call time,
    // because extracting argument values from the provided arraylike might
    // have side effects or throw an exception.
    HandleValue fval = args.thisv();
    if (!IsCallable(fval)) {
        ReportIncompatibleMethod(cx, args, &JSFunction::class_);
        return false;
    }

    // Step 2.
    if (args.length() < 2 || args[1].isNullOrUndefined())
        return fun_call(cx, (args.length() > 0) ? 1 : 0, vp);

    InvokeArgs args2(cx);

    // A JS_OPTIMIZED_ARGUMENTS magic value means that 'arguments' flows into
    // this apply call from a scripted caller and, as an optimization, we've
    // avoided creating it since apply can simply pull the argument values from
    // the calling frame (which we must do now).
    if (args[1].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        // Step 3-6.
        ScriptFrameIter iter(cx);
        MOZ_ASSERT(iter.numActualArgs() <= ARGS_LENGTH_MAX);
        if (!args2.init(cx, iter.numActualArgs()))
            return false;

        // Steps 7-8.
        iter.unaliasedForEachActual(cx, CopyTo(args2.array()));
    } else {
        // Step 3.
        if (!args[1].isObject()) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_BAD_APPLY_ARGS, js_apply_str);
            return false;
        }

        // Steps 4-5 (note erratum removing steps originally numbered 5 and 7 in
        // original version of ES5).
        RootedObject aobj(cx, &args[1].toObject());
        uint32_t length;
        if (!GetLengthProperty(cx, aobj, &length))
            return false;

        // Step 6.
        if (!args2.init(cx, length))
            return false;

        MOZ_ASSERT(length <= ARGS_LENGTH_MAX);

        // Steps 7-8.
        if (!GetElements(cx, aobj, length, args2.array()))
            return false;
    }

    // Step 9.
    return Call(cx, fval, args[0], args2, args.rval());
}

bool
JSFunction::infallibleIsDefaultClassConstructor(JSContext* cx) const
{
    if (!isSelfHostedBuiltin())
        return false;

    bool isDefault = false;
    if (isInterpretedLazy()) {
        JSAtom* name = &getExtendedSlot(LAZY_FUNCTION_NAME_SLOT).toString()->asAtom();
        isDefault = name == cx->names().DefaultDerivedClassConstructor ||
                    name == cx->names().DefaultBaseClassConstructor;
    } else {
        isDefault = nonLazyScript()->isDefaultClassConstructor();
    }

    MOZ_ASSERT_IF(isDefault, isConstructor());
    MOZ_ASSERT_IF(isDefault, isClassConstructor());
    return isDefault;
}

bool
JSFunction::isDerivedClassConstructor()
{
    bool derived;
    if (isInterpretedLazy()) {
        // There is only one plausible lazy self-hosted derived
        // constructor.
        if (isSelfHostedBuiltin()) {
            JSAtom* name = &getExtendedSlot(LAZY_FUNCTION_NAME_SLOT).toString()->asAtom();

            // This function is called from places without access to a
            // JSContext. Trace some plumbing to get what we want.
            derived = name == compartment()->runtimeFromAnyThread()->
                              commonNames->DefaultDerivedClassConstructor;
        } else {
            derived = lazyScript()->isDerivedClassConstructor();
        }
    } else {
        derived = nonLazyScript()->isDerivedClassConstructor();
    }
    MOZ_ASSERT_IF(derived, isClassConstructor());
    return derived;
}

bool
JSFunction::getLength(JSContext* cx, uint16_t* length)
{
    JS::RootedFunction self(cx, this);
    MOZ_ASSERT(!self->isBoundFunction());
    if (self->isInterpretedLazy() && !self->getOrCreateScript(cx))
        return false;

    *length = self->hasScript() ? self->nonLazyScript()->funLength()
                                : (self->nargs() - self->hasRest());
    return true;
}

bool
JSFunction::getUnresolvedLength(JSContext* cx, MutableHandleValue v)
{
    MOZ_ASSERT(!IsInternalFunctionObject(*this));
    MOZ_ASSERT(!hasResolvedLength());

    // Bound functions' length can have values up to MAX_SAFE_INTEGER, so
    // they're handled differently from other functions.
    if (isBoundFunction()) {
        MOZ_ASSERT(getExtendedSlot(BOUND_FUN_LENGTH_SLOT).isNumber());
        v.set(getExtendedSlot(BOUND_FUN_LENGTH_SLOT));
        return true;
    }

    uint16_t length;
    if (!getLength(cx, &length))
        return false;

    v.setInt32(length);
    return true;
}

JSAtom*
JSFunction::getUnresolvedName(JSContext* cx)
{
    MOZ_ASSERT(!IsInternalFunctionObject(*this));
    MOZ_ASSERT(!hasResolvedName());

    if (isClassConstructor()) {
        // It's impossible to have an empty named class expression. We use
        // empty as a sentinel when creating default class constructors.
        MOZ_ASSERT(name() != cx->names().empty);

        // Unnamed class expressions should not get a .name property at all.
        return name();
    }

    // Returns the empty string for unnamed functions (FIXME: bug 883377).
    return name() != nullptr ? name() : cx->names().empty;
}

static const js::Value&
BoundFunctionEnvironmentSlotValue(const JSFunction* fun, uint32_t slotIndex)
{
    MOZ_ASSERT(fun->isBoundFunction());
    MOZ_ASSERT(fun->environment()->is<CallObject>());
    CallObject* callObject = &fun->environment()->as<CallObject>();
    return callObject->getSlot(slotIndex);
}

JSObject*
JSFunction::getBoundFunctionTarget() const
{
    js::Value targetVal = BoundFunctionEnvironmentSlotValue(this, JSSLOT_BOUND_FUNCTION_TARGET);
    MOZ_ASSERT(IsCallable(targetVal));
    return &targetVal.toObject();
}

const js::Value&
JSFunction::getBoundFunctionThis() const
{
    return BoundFunctionEnvironmentSlotValue(this, JSSLOT_BOUND_FUNCTION_THIS);
}

static ArrayObject*
GetBoundFunctionArguments(const JSFunction* boundFun)
{
    js::Value argsVal = BoundFunctionEnvironmentSlotValue(boundFun, JSSLOT_BOUND_FUNCTION_ARGS);
    return &argsVal.toObject().as<ArrayObject>();
}

const js::Value&
JSFunction::getBoundFunctionArgument(JSContext* cx, unsigned which) const
{
    MOZ_ASSERT(which < getBoundFunctionArgumentCount());

    RootedArrayObject boundArgs(cx, GetBoundFunctionArguments(this));
    RootedValue res(cx);
    return boundArgs->getDenseElement(which);
}

size_t
JSFunction::getBoundFunctionArgumentCount() const
{
    return GetBoundFunctionArguments(this)->length();
}

/* static */ bool
JSFunction::createScriptForLazilyInterpretedFunction(JSContext* cx, HandleFunction fun)
{
    MOZ_ASSERT(fun->isInterpretedLazy());

    Rooted<LazyScript*> lazy(cx, fun->lazyScriptOrNull());
    if (lazy) {
        RootedScript script(cx, lazy->maybeScript());

        // Only functions without inner functions or direct eval are
        // re-lazified. Functions with either of those are on the static scope
        // chain of their inner functions, or in the case of eval, possibly
        // eval'd inner functions. This prohibits re-lazification as
        // StaticScopeIter queries needsCallObject of those functions, which
        // requires a non-lazy script.  Note that if this ever changes,
        // XDRRelazificationInfo will have to be fixed.
        bool canRelazify = !lazy->numInnerFunctions() && !lazy->hasDirectEval();

        if (script) {
            fun->setUnlazifiedScript(script);
            // Remember the lazy script on the compiled script, so it can be
            // stored on the function again in case of re-lazification.
            if (canRelazify)
                script->setLazyScript(lazy);
            return true;
        }

        if (fun != lazy->functionNonDelazifying()) {
            if (!lazy->functionDelazifying(cx))
                return false;
            script = lazy->functionNonDelazifying()->nonLazyScript();
            if (!script)
                return false;

            fun->setUnlazifiedScript(script);
            return true;
        }

        // Lazy script caching is only supported for leaf functions. If a
        // script with inner functions was returned by the cache, those inner
        // functions would be delazified when deep cloning the script, even if
        // they have never executed.
        //
        // Additionally, the lazy script cache is not used during incremental
        // GCs, to avoid resurrecting dead scripts after incremental sweeping
        // has started.
        if (canRelazify && !JS::IsIncrementalGCInProgress(cx)) {
            LazyScriptCache::Lookup lookup(cx, lazy);
            cx->caches.lazyScriptCache.lookup(lookup, script.address());
        }

        if (script) {
            RootedScope enclosingScope(cx, lazy->enclosingScope());
            RootedScript clonedScript(cx, CloneScriptIntoFunction(cx, enclosingScope, fun, script));
            if (!clonedScript)
                return false;

            clonedScript->setSourceObject(lazy->sourceObject());

            fun->initAtom(script->functionNonDelazifying()->displayAtom());

            if (!lazy->maybeScript())
                lazy->initScript(clonedScript);
            return true;
        }

        MOZ_ASSERT(lazy->scriptSource()->hasSourceData());

        // Parse and compile the script from source.
        size_t lazyLength = lazy->end() - lazy->begin();
        UncompressedSourceCache::AutoHoldEntry holder;
        const char16_t* chars = lazy->scriptSource()->chars(cx, holder, lazy->begin(), lazyLength);
        if (!chars)
            return false;

        if (!frontend::CompileLazyFunction(cx, lazy, chars, lazyLength)) {
            // The frontend may have linked the function and the non-lazy
            // script together during bytecode compilation. Reset it now on
            // error.
            fun->initLazyScript(lazy);
            if (lazy->hasScript())
                lazy->resetScript();
            return false;
        }

        script = fun->nonLazyScript();

        // Remember the compiled script on the lazy script itself, in case
        // there are clones of the function still pointing to the lazy script.
        if (!lazy->maybeScript())
            lazy->initScript(script);

        // Try to insert the newly compiled script into the lazy script cache.
        if (canRelazify) {
            // A script's starting column isn't set by the bytecode emitter, so
            // specify this from the lazy script so that if an identical lazy
            // script is encountered later a match can be determined.
            script->setColumn(lazy->column());

            LazyScriptCache::Lookup lookup(cx, lazy);
            cx->caches.lazyScriptCache.insert(lookup, script);

            // Remember the lazy script on the compiled script, so it can be
            // stored on the function again in case of re-lazification.
            // Only functions without inner functions are re-lazified.
            script->setLazyScript(lazy);
        }
        return true;
    }

    /* Lazily cloned self-hosted script. */
    MOZ_ASSERT(fun->isSelfHostedBuiltin());
    RootedAtom funAtom(cx, &fun->getExtendedSlot(LAZY_FUNCTION_NAME_SLOT).toString()->asAtom());
    if (!funAtom)
        return false;
    Rooted<PropertyName*> funName(cx, funAtom->asPropertyName());
    return cx->runtime()->cloneSelfHostedFunctionScript(cx, funName, fun);
}

void
JSFunction::maybeRelazify(JSRuntime* rt)
{
    // Try to relazify functions with a non-lazy script. Note: functions can be
    // marked as interpreted despite having no script yet at some points when
    // parsing.
    if (!hasScript() || !u.i.s.script_)
        return;

    // Don't relazify functions in compartments that are active.
    JSCompartment* comp = compartment();
    if (comp->hasBeenEntered() && !rt->allowRelazificationForTesting)
        return;

    // The caller should have checked we're not in the self-hosting zone (it's
    // shared with worker runtimes so relazifying functions in it will race).
    MOZ_ASSERT(!comp->isSelfHosting);

    // Don't relazify if the compartment is being debugged.
    if (comp->isDebuggee())
        return;

    // Don't relazify if the compartment and/or runtime is instrumented to
    // collect code coverage for analysis.
    if (comp->collectCoverageForDebug())
        return;

    // Don't relazify functions with JIT code.
    if (!u.i.s.script_->isRelazifiable())
        return;

    // To delazify self-hosted builtins we need the name of the function
    // to clone. This name is stored in the first extended slot. Since
    // that slot is sometimes also used for other purposes, make sure it
    // contains a string.
    if (isSelfHostedBuiltin() &&
        (!isExtended() || !getExtendedSlot(LAZY_FUNCTION_NAME_SLOT).isString()))
    {
        return;
    }

    JSScript* script = nonLazyScript();

    flags_ &= ~INTERPRETED;
    flags_ |= INTERPRETED_LAZY;
    LazyScript* lazy = script->maybeLazyScript();
    u.i.s.lazy_ = lazy;
    if (lazy) {
        MOZ_ASSERT(!isSelfHostedBuiltin());
    } else {
        MOZ_ASSERT(isSelfHostedBuiltin());
        MOZ_ASSERT(isExtended());
        MOZ_ASSERT(getExtendedSlot(LAZY_FUNCTION_NAME_SLOT).toString()->isAtom());
    }

    comp->scheduleDelazificationForDebugger();
}

static bool
fun_isGenerator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSFunction* fun;
    if (!IsFunctionObject(args.thisv(), &fun)) {
        args.rval().setBoolean(false);
        return true;
    }

    args.rval().setBoolean(fun->isGenerator());
    return true;
}

const JSFunctionSpec js::function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      fun_apply,      2,0),
    JS_FN(js_call_str,       fun_call,       1,0),
    JS_FN("isGenerator",     fun_isGenerator,0,0),
    JS_SELF_HOSTED_FN("bind", "FunctionBind", 2, JSFUN_HAS_REST),
    JS_SYM_FN(hasInstance, fun_symbolHasInstance, 1, JSPROP_READONLY | JSPROP_PERMANENT),
    JS_FS_END
};

// ES 2017 draft rev 0f10dba4ad18de92d47d421f378233a2eae8f077 19.2.1.1.1.
static bool
FunctionConstructor(JSContext* cx, const CallArgs& args, GeneratorKind generatorKind,
                    FunctionAsyncKind asyncKind)
{
    // Block this call if security callbacks forbid it.
    Rooted<GlobalObject*> global(cx, &args.callee().global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, global)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CSP_BLOCKED_FUNCTION);
        return false;
    }

    bool isStarGenerator = generatorKind == StarGenerator;
    bool isAsync = asyncKind == AsyncFunction;
    MOZ_ASSERT(generatorKind != LegacyGenerator);
    MOZ_ASSERT_IF(isAsync, isStarGenerator);
    MOZ_ASSERT_IF(!isStarGenerator, !isAsync);

    RootedScript maybeScript(cx);
    const char* filename;
    unsigned lineno;
    bool mutedErrors;
    uint32_t pcOffset;
    DescribeScriptedCallerForCompilation(cx, &maybeScript, &filename, &lineno, &pcOffset,
                                         &mutedErrors);

    const char* introductionType = "Function";
    if (isAsync)
        introductionType = "AsyncFunction";
    else if (generatorKind != NotGenerator)
        introductionType = "GeneratorFunction";

    const char* introducerFilename = filename;
    if (maybeScript && maybeScript->scriptSource()->introducerFilename())
        introducerFilename = maybeScript->scriptSource()->introducerFilename();

    CompileOptions options(cx);
    // Use line 0 to make the function body starts from line 1.
    options.setMutedErrors(mutedErrors)
           .setFileAndLine(filename, 0)
           .setNoScriptRval(false)
           .setIntroductionInfo(introducerFilename, introductionType, lineno, maybeScript, pcOffset);

    StringBuffer sb(cx);

    if (!sb.append('('))
        return false;

    if (args.length() > 1) {
        RootedString str(cx);

        // Steps 5-6, 9.
        unsigned n = args.length() - 1;

        for (unsigned i = 0; i < n; i++) {
            // Steps 9.a-b, 9.d.i-ii.
            str = ToString<CanGC>(cx, args[i]);
            if (!str)
                return false;

            // Steps 9.b, 9.d.iii.
            if (!sb.append(str))
                 return false;

            if (i < args.length() - 2) {
                // Step 9.d.iii.
                if (!sb.append(", "))
                    return false;
            }
        }
    }

    // Remember the position of ")".
    Maybe<uint32_t> parameterListEnd = Some(uint32_t(sb.length()));
    MOZ_ASSERT(FunctionConstructorMedialSigils[0] == ')');

    if (!sb.append(FunctionConstructorMedialSigils))
        return false;

    if (args.length() > 0) {
        // Steps 7-8, 10.
        RootedString body(cx, ToString<CanGC>(cx, args[args.length() - 1]));
        if (!body || !sb.append(body))
             return false;
     }

    if (!sb.append(FunctionConstructorFinalBrace))
        return false;

    // The parser only accepts two byte strings.
    if (!sb.ensureTwoByteChars())
        return false;

    RootedString functionText(cx, sb.finishString());
    if (!functionText)
        return false;

    /*
     * NB: (new Function) is not lexically closed by its caller, it's just an
     * anonymous function in the top-level scope that its constructor inhabits.
     * Thus 'var x = 42; f = new Function("return x"); print(f())' prints 42,
     * and so would a call to f from another top-level's script or function.
     */
    RootedAtom anonymousAtom(cx, cx->names().anonymous);

    // Step 24.
    RootedObject proto(cx);
    if (!isAsync) {
        if (!GetPrototypeFromCallableConstructor(cx, args, &proto))
            return false;
    }

    // Step 4.d, use %Generator% as the fallback prototype.
    // Also use %Generator% for the unwrapped function of async functions.
    if (!proto && isStarGenerator) {
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, global);
        if (!proto)
            return false;
    }

    // Step 25-32 (reordered).
    RootedObject globalLexical(cx, &global->lexicalEnvironment());
    AllocKind allocKind = isAsync ? AllocKind::FUNCTION_EXTENDED : AllocKind::FUNCTION;
    RootedFunction fun(cx, NewFunctionWithProto(cx, nullptr, 0,
                                                JSFunction::INTERPRETED_LAMBDA, globalLexical,
                                                anonymousAtom, proto,
                                                allocKind, TenuredObject));
    if (!fun)
        return false;

    if (!JSFunction::setTypeForScriptedFunction(cx, fun))
        return false;

    // Steps 2.a-b, 3.a-b, 4.a-b, 11-23.
    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, functionText))
        return false;

    mozilla::Range<const char16_t> chars = stableChars.twoByteRange();
    SourceBufferHolder::Ownership ownership = stableChars.maybeGiveOwnershipToCaller()
                                              ? SourceBufferHolder::GiveOwnership
                                              : SourceBufferHolder::NoOwnership;
    bool ok;
    SourceBufferHolder srcBuf(chars.begin().get(), chars.length(), ownership);
    if (isAsync)
        ok = frontend::CompileStandaloneAsyncFunction(cx, &fun, options, srcBuf, parameterListEnd);
    else if (isStarGenerator)
        ok = frontend::CompileStandaloneGenerator(cx, &fun, options, srcBuf, parameterListEnd);
    else
        ok = frontend::CompileStandaloneFunction(cx, &fun, options, srcBuf, parameterListEnd);

    // Step 33.
    args.rval().setObject(*fun);
    return ok;
}

bool
js::Function(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return FunctionConstructor(cx, args, NotGenerator, SyncFunction);
}

bool
js::Generator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return FunctionConstructor(cx, args, StarGenerator, SyncFunction);
}

bool
js::AsyncFunctionConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Save the callee before its reset in FunctionConstructor().
    RootedObject newTarget(cx);
    if (args.isConstructing())
        newTarget = &args.newTarget().toObject();
    else
        newTarget = &args.callee();

    if (!FunctionConstructor(cx, args, StarGenerator, AsyncFunction))
        return false;

    // ES2017, draft rev 0f10dba4ad18de92d47d421f378233a2eae8f077
    // 19.2.1.1.1 Runtime Semantics: CreateDynamicFunction, step 24.
    RootedObject proto(cx);
    if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
        return false;

    // 19.2.1.1.1, step 4.d, use %AsyncFunctionPrototype% as the fallback.
    if (!proto) {
        proto = GlobalObject::getOrCreateAsyncFunctionPrototype(cx, cx->global());
        if (!proto)
            return false;
    }

    RootedFunction unwrapped(cx, &args.rval().toObject().as<JSFunction>());
    RootedObject wrapped(cx, WrapAsyncFunctionWithProto(cx, unwrapped, proto));
    if (!wrapped)
        return false;

    args.rval().setObject(*wrapped);
    return true;
}

bool
JSFunction::isBuiltinFunctionConstructor()
{
    return maybeNative() == Function || maybeNative() == Generator;
}

bool
JSFunction::needsExtraBodyVarEnvironment() const
{
    MOZ_ASSERT(!isInterpretedLazy());

    if (isNative())
        return false;

    if (!nonLazyScript()->functionHasExtraBodyVarScope())
        return false;

    return nonLazyScript()->functionExtraBodyVarScope()->hasEnvironment();
}

bool
JSFunction::needsNamedLambdaEnvironment() const
{
    MOZ_ASSERT(!isInterpretedLazy());

    if (!isNamedLambda())
        return false;

    LexicalScope* scope = nonLazyScript()->maybeNamedLambdaScope();
    if (!scope)
        return false;

    return scope->hasEnvironment();
}

JSFunction*
js::NewNativeFunction(ExclusiveContext* cx, Native native, unsigned nargs, HandleAtom atom,
                      gc::AllocKind allocKind /* = AllocKind::FUNCTION */,
                      NewObjectKind newKind /* = SingletonObject */)
{
    MOZ_ASSERT(native);
    return NewFunctionWithProto(cx, native, nargs, JSFunction::NATIVE_FUN,
                                nullptr, atom, nullptr, allocKind, newKind);
}

JSFunction*
js::NewNativeConstructor(ExclusiveContext* cx, Native native, unsigned nargs, HandleAtom atom,
                         gc::AllocKind allocKind /* = AllocKind::FUNCTION */,
                         NewObjectKind newKind /* = SingletonObject */,
                         JSFunction::Flags flags /* = JSFunction::NATIVE_CTOR */)
{
    MOZ_ASSERT(native);
    MOZ_ASSERT(flags & JSFunction::NATIVE_CTOR);
    return NewFunctionWithProto(cx, native, nargs, flags, nullptr, atom,
                                nullptr, allocKind, newKind);
}

JSFunction*
js::NewScriptedFunction(ExclusiveContext* cx, unsigned nargs,
                        JSFunction::Flags flags, HandleAtom atom,
                        HandleObject proto /* = nullptr */,
                        gc::AllocKind allocKind /* = AllocKind::FUNCTION */,
                        NewObjectKind newKind /* = GenericObject */,
                        HandleObject enclosingEnvArg /* = nullptr */)
{
    RootedObject enclosingEnv(cx, enclosingEnvArg);
    if (!enclosingEnv)
        enclosingEnv = &cx->global()->lexicalEnvironment();
    return NewFunctionWithProto(cx, nullptr, nargs, flags, enclosingEnv,
                                atom, proto, allocKind, newKind);
}

#ifdef DEBUG
static bool
NewFunctionEnvironmentIsWellFormed(ExclusiveContext* cx, HandleObject env)
{
    // Assert that the terminating environment is null, global, or a debug
    // scope proxy. All other cases of polluting global scope behavior are
    // handled by EnvironmentObjects (viz. non-syntactic DynamicWithObject and
    // NonSyntacticVariablesObject).
    RootedObject terminatingEnv(cx, SkipEnvironmentObjects(env));
    return !terminatingEnv || terminatingEnv == cx->global() ||
           terminatingEnv->is<DebugEnvironmentProxy>();
}
#endif

JSFunction*
js::NewFunctionWithProto(ExclusiveContext* cx, Native native,
                         unsigned nargs, JSFunction::Flags flags, HandleObject enclosingEnv,
                         HandleAtom atom, HandleObject proto,
                         gc::AllocKind allocKind /* = AllocKind::FUNCTION */,
                         NewObjectKind newKind /* = GenericObject */,
                         NewFunctionProtoHandling protoHandling /* = NewFunctionClassProto */)
{
    MOZ_ASSERT(allocKind == AllocKind::FUNCTION || allocKind == AllocKind::FUNCTION_EXTENDED);
    MOZ_ASSERT_IF(native, !enclosingEnv);
    MOZ_ASSERT(NewFunctionEnvironmentIsWellFormed(cx, enclosingEnv));

    RootedObject funobj(cx);
    if (protoHandling == NewFunctionClassProto) {
        funobj = NewObjectWithClassProto(cx, &JSFunction::class_, proto, allocKind,
                                         newKind);
    } else {
        funobj = NewObjectWithGivenTaggedProto(cx, &JSFunction::class_, AsTaggedProto(proto),
                                               allocKind, newKind);
    }
    if (!funobj)
        return nullptr;

    RootedFunction fun(cx, &funobj->as<JSFunction>());

    if (allocKind == AllocKind::FUNCTION_EXTENDED)
        flags = JSFunction::Flags(flags | JSFunction::EXTENDED);

    /* Initialize all function members. */
    fun->setArgCount(uint16_t(nargs));
    fun->setFlags(flags);
    if (fun->isInterpreted()) {
        MOZ_ASSERT(!native);
        if (fun->isInterpretedLazy())
            fun->initLazyScript(nullptr);
        else
            fun->initScript(nullptr);
        fun->initEnvironment(enclosingEnv);
    } else {
        MOZ_ASSERT(fun->isNative());
        MOZ_ASSERT(native);
        fun->initNative(native, nullptr);
    }
    if (allocKind == AllocKind::FUNCTION_EXTENDED)
        fun->initializeExtended();
    fun->initAtom(atom);

    return fun;
}

bool
js::CanReuseScriptForClone(JSCompartment* compartment, HandleFunction fun,
                           HandleObject newParent)
{
    if (compartment != fun->compartment() ||
        fun->isSingleton() ||
        ObjectGroup::useSingletonForClone(fun))
    {
        return false;
    }

    if (newParent->is<GlobalObject>())
        return true;

    // Don't need to clone the script if newParent is a syntactic scope, since
    // in that case we have some actual scope objects on our scope chain and
    // whatnot; whoever put them there should be responsible for setting our
    // script's flags appropriately.  We hit this case for JSOP_LAMBDA, for
    // example.
    if (IsSyntacticEnvironment(newParent))
        return true;

    // We need to clone the script if we're interpreted and not already marked
    // as having a non-syntactic scope. If we're lazy, go ahead and clone the
    // script; see the big comment at the end of CopyScriptInternal for the
    // explanation of what's going on there.
    return !fun->isInterpreted() ||
           (fun->hasScript() && fun->nonLazyScript()->hasNonSyntacticScope());
}

static inline JSFunction*
NewFunctionClone(JSContext* cx, HandleFunction fun, NewObjectKind newKind,
                 gc::AllocKind allocKind, HandleObject proto)
{
    RootedObject cloneProto(cx, proto);
    if (!proto && fun->isStarGenerator()) {
        cloneProto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, cx->global());
        if (!cloneProto)
            return nullptr;
    }

    JSObject* cloneobj = NewObjectWithClassProto(cx, &JSFunction::class_, cloneProto,
                                                 allocKind, newKind);
    if (!cloneobj)
        return nullptr;
    RootedFunction clone(cx, &cloneobj->as<JSFunction>());

    uint16_t flags = fun->flags() & ~JSFunction::EXTENDED;
    if (allocKind == AllocKind::FUNCTION_EXTENDED)
        flags |= JSFunction::EXTENDED;

    clone->setArgCount(fun->nargs());
    clone->setFlags(flags);
    clone->initAtom(fun->displayAtom());

    if (allocKind == AllocKind::FUNCTION_EXTENDED) {
        if (fun->isExtended() && fun->compartment() == cx->compartment()) {
            for (unsigned i = 0; i < FunctionExtended::NUM_EXTENDED_SLOTS; i++)
                clone->initExtendedSlot(i, fun->getExtendedSlot(i));
        } else {
            clone->initializeExtended();
        }
    }

    return clone;
}

JSFunction*
js::CloneFunctionReuseScript(JSContext* cx, HandleFunction fun, HandleObject enclosingEnv,
                             gc::AllocKind allocKind /* = FUNCTION */ ,
                             NewObjectKind newKind /* = GenericObject */,
                             HandleObject proto /* = nullptr */)
{
    MOZ_ASSERT(NewFunctionEnvironmentIsWellFormed(cx, enclosingEnv));
    MOZ_ASSERT(!fun->isBoundFunction());
    MOZ_ASSERT(CanReuseScriptForClone(cx->compartment(), fun, enclosingEnv));

    RootedFunction clone(cx, NewFunctionClone(cx, fun, newKind, allocKind, proto));
    if (!clone)
        return nullptr;

    if (fun->hasScript()) {
        clone->initScript(fun->nonLazyScript());
        clone->initEnvironment(enclosingEnv);
    } else if (fun->isInterpretedLazy()) {
        MOZ_ASSERT(fun->compartment() == clone->compartment());
        LazyScript* lazy = fun->lazyScriptOrNull();
        clone->initLazyScript(lazy);
        clone->initEnvironment(enclosingEnv);
    } else {
        clone->initNative(fun->native(), fun->jitInfo());
    }

    /*
     * Clone the function, reusing its script. We can use the same group as
     * the original function provided that its prototype is correct.
     */
    if (fun->staticPrototype() == clone->staticPrototype())
        clone->setGroup(fun->group());
    return clone;
}

JSFunction*
js::CloneFunctionAndScript(JSContext* cx, HandleFunction fun, HandleObject enclosingEnv,
                           HandleScope newScope, gc::AllocKind allocKind /* = FUNCTION */,
                           HandleObject proto /* = nullptr */)
{
    MOZ_ASSERT(NewFunctionEnvironmentIsWellFormed(cx, enclosingEnv));
    MOZ_ASSERT(!fun->isBoundFunction());

    JSScript::AutoDelazify funScript(cx);
    if (fun->isInterpreted()) {
        funScript = fun;
        if (!funScript)
            return nullptr;
    }

    RootedFunction clone(cx, NewFunctionClone(cx, fun, SingletonObject, allocKind, proto));
    if (!clone)
        return nullptr;

    if (fun->hasScript()) {
        clone->initScript(nullptr);
        clone->initEnvironment(enclosingEnv);
    } else {
        clone->initNative(fun->native(), fun->jitInfo());
    }

    /*
     * Across compartments or if we have to introduce a non-syntactic scope we
     * have to clone the script for interpreted functions. Cross-compartment
     * cloning only happens via JSAPI (JS::CloneFunctionObject) which
     * dynamically ensures that 'script' has no enclosing lexical scope (only
     * the global scope or other non-lexical scope).
     */
#ifdef DEBUG
    RootedObject terminatingEnv(cx, enclosingEnv);
    while (IsSyntacticEnvironment(terminatingEnv))
        terminatingEnv = terminatingEnv->enclosingEnvironment();
    MOZ_ASSERT_IF(!terminatingEnv->is<GlobalObject>(),
                  newScope->hasOnChain(ScopeKind::NonSyntactic));
#endif

    if (clone->isInterpreted()) {
        RootedScript script(cx, fun->nonLazyScript());
        MOZ_ASSERT(script->compartment() == fun->compartment());
        MOZ_ASSERT(cx->compartment() == clone->compartment(),
                   "Otherwise we could relazify clone below!");

        RootedScript clonedScript(cx, CloneScriptIntoFunction(cx, newScope, clone, script));
        if (!clonedScript)
            return nullptr;
        Debugger::onNewScript(cx, clonedScript);
    }

    return clone;
}

/*
 * Return an atom for use as the name of a builtin method with the given
 * property id.
 *
 * Function names are always strings. If id is the well-known @@iterator
 * symbol, this returns "[Symbol.iterator]".  If a prefix is supplied the final
 * name is |prefix + " " + name|. A prefix cannot be supplied if id is a
 * symbol value.
 *
 * Implements steps 3-5 of 9.2.11 SetFunctionName in ES2016.
 */
JSAtom*
js::IdToFunctionName(JSContext* cx, HandleId id, const char* prefix /* = nullptr */)
{
    if (JSID_IS_ATOM(id) && !prefix)
        return JSID_TO_ATOM(id);

    // Step 3.
    MOZ_ASSERT_IF(prefix, !JSID_IS_SYMBOL(id));

    // Step 4.
    if (JSID_IS_SYMBOL(id)) {
        RootedAtom desc(cx, JSID_TO_SYMBOL(id)->description());
        StringBuffer sb(cx);
        if (!sb.append('[') || !sb.append(desc) || !sb.append(']'))
            return nullptr;
        return sb.finishAtom();
    }

    // Step 5.
    RootedValue idv(cx, IdToValue(id));
    if (!prefix)
        return ToAtom<CanGC>(cx, idv);

    StringBuffer sb(cx);
    if (!sb.append(prefix, strlen(prefix)) || !sb.append(' ') || !sb.append(ToAtom<CanGC>(cx, idv)))
        return nullptr;
    return sb.finishAtom();
}

JSFunction*
js::DefineFunction(JSContext* cx, HandleObject obj, HandleId id, Native native,
                   unsigned nargs, unsigned flags, AllocKind allocKind /* = AllocKind::FUNCTION */)
{
    GetterOp gop;
    SetterOp sop;
    if (flags & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        flags &= ~JSFUN_STUB_GSOPS;
        gop = nullptr;
        sop = nullptr;
    } else {
        gop = obj->getClass()->getGetProperty();
        sop = obj->getClass()->getSetProperty();
        MOZ_ASSERT(gop != JS_PropertyStub);
        MOZ_ASSERT(sop != JS_StrictPropertyStub);
    }

    RootedAtom atom(cx, IdToFunctionName(cx, id));
    if (!atom)
        return nullptr;

    RootedFunction fun(cx);
    if (!native)
        fun = NewScriptedFunction(cx, nargs,
                                  JSFunction::INTERPRETED_LAZY, atom,
                                  /* proto = */ nullptr,
                                  allocKind, GenericObject, obj);
    else if (flags & JSFUN_CONSTRUCTOR)
        fun = NewNativeConstructor(cx, native, nargs, atom, allocKind);
    else
        fun = NewNativeFunction(cx, native, nargs, atom, allocKind);

    if (!fun)
        return nullptr;

    RootedValue funVal(cx, ObjectValue(*fun));
    if (!DefineProperty(cx, obj, id, funVal, gop, sop, flags & ~JSFUN_FLAGS_MASK))
        return nullptr;

    return fun;
}

void
js::ReportIncompatibleMethod(JSContext* cx, const CallArgs& args, const Class* clasp)
{
    RootedValue thisv(cx, args.thisv());

#ifdef DEBUG
    if (thisv.isObject()) {
        MOZ_ASSERT(thisv.toObject().getClass() != clasp ||
                   !thisv.toObject().isNative() ||
                   !thisv.toObject().staticPrototype() ||
                   thisv.toObject().staticPrototype()->getClass() != clasp);
    } else if (thisv.isString()) {
        MOZ_ASSERT(clasp != &StringObject::class_);
    } else if (thisv.isNumber()) {
        MOZ_ASSERT(clasp != &NumberObject::class_);
    } else if (thisv.isBoolean()) {
        MOZ_ASSERT(clasp != &BooleanObject::class_);
    } else if (thisv.isSymbol()) {
        MOZ_ASSERT(clasp != &SymbolObject::class_);
    } else {
        MOZ_ASSERT(thisv.isUndefined() || thisv.isNull());
    }
#endif

    if (JSFunction* fun = ReportIfNotFunction(cx, args.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char* funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                                       clasp->name, funName, InformalValueTypeName(thisv));
        }
    }
}

void
js::ReportIncompatible(JSContext* cx, const CallArgs& args)
{
    if (JSFunction* fun = ReportIfNotFunction(cx, args.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char* funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumberLatin1(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_METHOD,
                                       funName, "method", InformalValueTypeName(args.thisv()));
        }
    }
}

namespace JS {
namespace detail {

JS_PUBLIC_API(void)
CheckIsValidConstructible(const Value& calleev)
{
    JSObject* callee = &calleev.toObject();
    if (callee->is<JSFunction>())
        MOZ_ASSERT(callee->as<JSFunction>().isConstructor());
    else
        MOZ_ASSERT(callee->constructHook() != nullptr);
}

} // namespace detail
} // namespace JS
