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
#include "mozilla/PodOperations.h"

#include <string.h>

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstypes.h"
#include "jswrapper.h"

#include "builtin/Eval.h"
#include "builtin/Object.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/TokenStream.h"
#include "gc/Marking.h"
#ifdef JS_ION
#include "jit/Ion.h"
#include "jit/IonFrameIterator.h"
#endif
#include "vm/Interpreter.h"
#include "vm/Shape.h"
#include "vm/StringBuffer.h"
#include "vm/WrapperObject.h"
#include "vm/Xdr.h"

#include "jsscriptinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;
using namespace js::frontend;

using mozilla::ArrayLength;
using mozilla::PodCopy;

static bool
fun_getProperty(JSContext *cx, HandleObject obj_, HandleId id, MutableHandleValue vp)
{
    RootedObject obj(cx, obj_);
    while (!obj->is<JSFunction>()) {
        if (!JSObject::getProto(cx, obj, &obj))
            return false;
        if (!obj)
            return true;
    }
    RootedFunction fun(cx, &obj->as<JSFunction>());

    /* Set to early to null in case of error */
    vp.setNull();

    /* Find fun's top-most activation record. */
    NonBuiltinScriptFrameIter iter(cx);
    for (; !iter.done(); ++iter) {
        if (!iter.isFunctionFrame() || iter.isEvalFrame())
            continue;
        if (iter.callee() == fun)
            break;
    }
    if (iter.done())
        return true;

    if (JSID_IS_ATOM(id, cx->names().arguments)) {
        if (fun->hasRest()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                 JSMSG_FUNCTION_ARGUMENTS_AND_REST);
            return false;
        }
        /* Warn if strict about f.arguments or equivalent unqualified uses. */
        if (!JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING | JSREPORT_STRICT, js_GetErrorMessage,
                                          nullptr, JSMSG_DEPRECATED_USAGE, js_arguments_str)) {
            return false;
        }

        ArgumentsObject *argsobj = ArgumentsObject::createUnexpected(cx, iter);
        if (!argsobj)
            return false;

#ifdef JS_ION
        // Disabling compiling of this script in IonMonkey.
        // IonMonkey does not guarantee |f.arguments| can be
        // fully recovered, so we try to mitigate observing this behavior by
        // detecting its use early.
        JSScript *script = iter.script();
        jit::ForbidCompilation(cx, script);
#endif

        vp.setObject(*argsobj);
        return true;
    }

    if (JSID_IS_ATOM(id, cx->names().caller)) {
        ++iter;
        if (iter.done() || !iter.isFunctionFrame()) {
            JS_ASSERT(vp.isNull());
            return true;
        }

        /* Callsite clones should never escape to script. */
        JSObject &maybeClone = iter.calleev().toObject();
        if (maybeClone.is<JSFunction>())
            vp.setObject(*maybeClone.as<JSFunction>().originalFunction());
        else
            vp.set(iter.calleev());

        if (!cx->compartment()->wrap(cx, vp))
            return false;

        /*
         * Censor the caller if we don't have full access to it.
         */
        RootedObject caller(cx, &vp.toObject());
        if (caller->is<WrapperObject>() && Wrapper::wrapperHandler(caller)->hasSecurityPolicy()) {
            vp.setNull();
        } else if (caller->is<JSFunction>()) {
            JSFunction *callerFun = &caller->as<JSFunction>();
            if (callerFun->isInterpreted() && callerFun->strict()) {
                JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, nullptr,
                                             JSMSG_CALLER_IS_STRICT);
                return false;
            }
        }

        return true;
    }

    MOZ_ASSUME_UNREACHABLE("fun_getProperty");
}



/* NB: no sentinels at ends -- use ArrayLength to bound loops.
 * Properties censored into [[ThrowTypeError]] in strict mode. */
static const uint16_t poisonPillProps[] = {
    NAME_OFFSET(arguments),
    NAME_OFFSET(caller),
};

static bool
fun_enumerate(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<JSFunction>());

    RootedId id(cx);
    bool found;

    if (!obj->isBoundFunction()) {
        id = NameToId(cx->names().prototype);
        if (!JSObject::hasProperty(cx, obj, id, &found, 0))
            return false;
    }

    id = NameToId(cx->names().length);
    if (!JSObject::hasProperty(cx, obj, id, &found, 0))
        return false;

    id = NameToId(cx->names().name);
    if (!JSObject::hasProperty(cx, obj, id, &found, 0))
        return false;

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];
        id = NameToId(AtomStateOffsetToName(cx->names(), offset));
        if (!JSObject::hasProperty(cx, obj, id, &found, 0))
            return false;
    }

    return true;
}

static JSObject *
ResolveInterpretedFunctionPrototype(JSContext *cx, HandleObject obj)
{
#ifdef DEBUG
    JSFunction *fun = &obj->as<JSFunction>();
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!fun->isFunctionPrototype());
#endif

    // Assert that fun is not a compiler-created function object, which
    // must never leak to script or embedding code and then be mutated.
    // Also assert that obj is not bound, per the ES5 15.3.4.5 ref above.
    JS_ASSERT(!IsInternalFunctionObject(obj));
    JS_ASSERT(!obj->isBoundFunction());

    // Make the prototype object an instance of Object with the same parent as
    // the function object itself, unless the function is an ES6 generator.  In
    // that case, per the 15 July 2013 ES6 draft, section 15.19.3, its parent is
    // the GeneratorObjectPrototype singleton.
    bool isStarGenerator = obj->as<JSFunction>().isStarGenerator();
    Rooted<GlobalObject*> global(cx, &obj->global());
    JSObject *objProto;
    if (isStarGenerator)
        objProto = GlobalObject::getOrCreateStarGeneratorObjectPrototype(cx, global);
    else
        objProto = obj->global().getOrCreateObjectPrototype(cx);
    if (!objProto)
        return nullptr;
    const Class *clasp = &JSObject::class_;

    RootedObject proto(cx, NewObjectWithGivenProto(cx, clasp, objProto, nullptr, SingletonObject));
    if (!proto)
        return nullptr;

    // Per ES5 15.3.5.2 a user-defined function's .prototype property is
    // initially non-configurable, non-enumerable, and writable.
    RootedValue protoVal(cx, ObjectValue(*proto));
    if (!JSObject::defineProperty(cx, obj, cx->names().prototype,
                                  protoVal, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT))
    {
        return nullptr;
    }

    // Per ES5 13.2 the prototype's .constructor property is configurable,
    // non-enumerable, and writable.  However, per the 15 July 2013 ES6 draft,
    // section 15.19.3, the .prototype of a generator function does not link
    // back with a .constructor.
    if (!isStarGenerator) {
        RootedValue objVal(cx, ObjectValue(*obj));
        if (!JSObject::defineProperty(cx, proto, cx->names().constructor,
                                      objVal, JS_PropertyStub, JS_StrictPropertyStub, 0))
        {
            return nullptr;
        }
    }

    return proto;
}

bool
js::FunctionHasResolveHook(const JSAtomState &atomState, PropertyName *name)
{
    if (name == atomState.prototype || name == atomState.length || name == atomState.name)
        return true;

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];

        if (name == AtomStateOffsetToName(atomState, offset))
            return true;
    }

    return false;
}

bool
js::fun_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
                MutableHandleObject objp)
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
         * ES5 15.3.4: the non-native function object named Function.prototype
         * does not have a .prototype property.
         *
         * ES5 15.3.4.5: bound functions don't have a prototype property. The
         * isBuiltin() test covers this case because bound functions are native
         * (and thus built-in) functions by definition/construction.
         */
        if (fun->isBuiltin() || fun->isFunctionPrototype())
            return true;

        if (!ResolveInterpretedFunctionPrototype(cx, fun))
            return false;
        objp.set(fun);
        return true;
    }

    if (JSID_IS_ATOM(id, cx->names().length) || JSID_IS_ATOM(id, cx->names().name)) {
        JS_ASSERT(!IsInternalFunctionObject(obj));

        RootedValue v(cx);
        if (JSID_IS_ATOM(id, cx->names().length)) {
            if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
                return false;
            uint16_t length = fun->hasScript() ? fun->nonLazyScript()->funLength() :
                fun->nargs() - fun->hasRest();
            v.setInt32(length);
        } else {
            v.setString(fun->atom() == nullptr ? cx->runtime()->emptyString : fun->atom());
        }

        if (!DefineNativeProperty(cx, fun, id, v, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0)) {
            return false;
        }
        objp.set(fun);
        return true;
    }

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];

        if (JSID_IS_ATOM(id, AtomStateOffsetToName(cx->names(), offset))) {
            JS_ASSERT(!IsInternalFunctionObject(fun));

            PropertyOp getter;
            StrictPropertyOp setter;
            unsigned attrs = JSPROP_PERMANENT | JSPROP_SHARED;
            if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
                return false;
            if (fun->isInterpreted() ? fun->strict() : fun->isBoundFunction()) {
                JSObject *throwTypeError = fun->global().getThrowTypeError();

                getter = CastAsPropertyOp(throwTypeError);
                setter = CastAsStrictPropertyOp(throwTypeError);
                attrs |= JSPROP_GETTER | JSPROP_SETTER;
            } else {
                getter = fun_getProperty;
                setter = JS_StrictPropertyStub;
            }

            if (!DefineNativeProperty(cx, fun, id, UndefinedHandleValue, getter, setter, attrs, 0))
                return false;
            objp.set(fun);
            return true;
        }
    }

    return true;
}

template<XDRMode mode>
bool
js::XDRInterpretedFunction(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript enclosingScript,
                           MutableHandleObject objp)
{
    enum FirstWordFlag {
        HasAtom             = 0x1,
        IsStarGenerator     = 0x2,
        IsLazy              = 0x4,
        HasSingletonType    = 0x8
    };

    /* NB: Keep this in sync with CloneFunctionAndScript. */
    RootedAtom atom(xdr->cx());
    uint32_t firstword = 0;        /* bitmask of FirstWordFlag */
    uint32_t flagsword = 0;        /* word for argument count and fun->flags */

    JSContext *cx = xdr->cx();
    RootedFunction fun(cx);
    RootedScript script(cx);
    Rooted<LazyScript *> lazy(cx);

    if (mode == XDR_ENCODE) {
        fun = &objp->as<JSFunction>();
        if (!fun->isInterpreted()) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                                     JSMSG_NOT_SCRIPTED_FUNCTION, name);
            }
            return false;
        }

        if (fun->atom() || fun->hasGuessedAtom())
            firstword |= HasAtom;

        if (fun->isStarGenerator())
            firstword |= IsStarGenerator;

        if (fun->isInterpretedLazy()) {
            // This can only happen for re-lazified cloned functions, so this
            // does not apply to any JSFunction produced by the parser, only to
            // JSFunction created by the runtime.
            JS_ASSERT(!fun->lazyScript()->maybeScript());

            // Encode a lazy script.
            firstword |= IsLazy;
            lazy = fun->lazyScript();
        } else {
            // Encode the script.
            script = fun->nonLazyScript();
        }

        if (fun->hasSingletonType())
            firstword |= HasSingletonType;

        atom = fun->displayAtom();
        flagsword = (fun->nargs() << 16) | fun->flags();

        // The environment of any function which is not reused will always be
        // null, it is later defined when a function is cloned or reused to
        // mirror the scope chain.
        JS_ASSERT_IF(fun->hasSingletonType() &&
                     !((lazy && lazy->hasBeenCloned()) || (script && script->hasBeenCloned())),
                     fun->environment() == nullptr);
    }

    if (!xdr->codeUint32(&firstword))
        return false;

    if (mode == XDR_DECODE) {
        JSObject *proto = nullptr;
        if (firstword & IsStarGenerator) {
            proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, cx->global());
            if (!proto)
                return false;
        }

        fun = NewFunctionWithProto(cx, NullPtr(), nullptr, 0, JSFunction::INTERPRETED,
                                   /* parent = */ NullPtr(), NullPtr(), proto,
                                   JSFunction::FinalizeKind, TenuredObject);
        if (!fun)
            return false;
        atom = nullptr;
        script = nullptr;
    }

    if ((firstword & HasAtom) && !XDRAtom(xdr, &atom))
        return false;
    if (!xdr->codeUint32(&flagsword))
        return false;

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
            fun->initLazyScript(lazy);
        } else {
            fun->initScript(script);
            script->setFunction(fun);
            JS_ASSERT(fun->nargs() == script->bindings.numArgs());
        }

        bool singleton = firstword & HasSingletonType;
        if (!JSFunction::setTypeForScriptedFunction(cx, fun, singleton))
            return false;
        objp.set(fun);
    }

    return true;
}

template bool
js::XDRInterpretedFunction(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, MutableHandleObject);

template bool
js::XDRInterpretedFunction(XDRState<XDR_DECODE> *, HandleObject, HandleScript, MutableHandleObject);

JSObject *
js::CloneFunctionAndScript(JSContext *cx, HandleObject enclosingScope, HandleFunction srcFun)
{
    /* NB: Keep this in sync with XDRInterpretedFunction. */
    JSObject *cloneProto = nullptr;
    if (srcFun->isStarGenerator()) {
        cloneProto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, cx->global());
        if (!cloneProto)
            return nullptr;
    }
    RootedFunction clone(cx, NewFunctionWithProto(cx, NullPtr(), nullptr, 0,
                                                  JSFunction::INTERPRETED, NullPtr(), NullPtr(),
                                                  cloneProto, JSFunction::FinalizeKind,
                                                  TenuredObject));
    if (!clone)
        return nullptr;

    RootedScript srcScript(cx, srcFun->getOrCreateScript(cx));
    if (!srcScript)
        return nullptr;
    RootedScript clonedScript(cx, CloneScript(cx, enclosingScope, clone, srcScript));
    if (!clonedScript)
        return nullptr;

    clone->setArgCount(srcFun->nargs());
    clone->setFlags(srcFun->flags());
    clone->initAtom(srcFun->displayAtom());
    clone->initScript(clonedScript);
    clonedScript->setFunction(clone);
    if (!JSFunction::setTypeForScriptedFunction(cx, clone))
        return nullptr;

    RootedScript cloneScript(cx, clone->nonLazyScript());
    CallNewScriptHook(cx, cloneScript, clone);
    return clone;
}

/*
 * [[HasInstance]] internal method for Function objects: fetch the .prototype
 * property of its 'this' parameter, and walks the prototype chain of v (only
 * if v is an object) returning true if .prototype is found.
 */
static bool
fun_hasInstance(JSContext *cx, HandleObject objArg, MutableHandleValue v, bool *bp)
{
    RootedObject obj(cx, objArg);

    while (obj->is<JSFunction>() && obj->isBoundFunction())
        obj = obj->as<JSFunction>().getBoundFunctionTarget();

    RootedValue pval(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().prototype, &pval))
        return false;

    if (pval.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        RootedValue val(cx, ObjectValue(*obj));
        js_ReportValueError(cx, JSMSG_BAD_PROTOTYPE, -1, val, js::NullPtr());
        return false;
    }

    RootedObject pobj(cx, &pval.toObject());
    bool isDelegate;
    if (!IsDelegate(cx, pobj, v, &isDelegate))
        return false;
    *bp = isDelegate;
    return true;
}

inline void
JSFunction::trace(JSTracer *trc)
{
    if (isExtended()) {
        MarkValueRange(trc, ArrayLength(toExtended()->extendedSlots),
                       toExtended()->extendedSlots, "nativeReserved");
    }

    if (atom_)
        MarkString(trc, &atom_, "atom");

    if (isInterpreted()) {
        // Functions can be be marked as interpreted despite having no script
        // yet at some points when parsing, and can be lazy with no lazy script
        // for self-hosted code.
        if (hasScript() && u.i.s.script_) {
            // Functions can be relazified under the following conditions:
            // - their compartment isn't currently executing scripts or being
            //   debugged
            // - they are not in the self-hosting compartment
            // - they aren't generators
            // - they don't have JIT code attached
            // - they don't have child functions
            // - they have information for un-lazifying them again later
            // This information can either be a LazyScript, or the name of a
            // self-hosted function which can be cloned over again. The latter
            // is stored in the first extended slot.
            if (IS_GC_MARKING_TRACER(trc) && !compartment()->hasBeenEntered() &&
                !compartment()->debugMode() && !compartment()->isSelfHosting &&
                u.i.s.script_->isRelazifiable() && (!isSelfHostedBuiltin() || isExtended()))
            {
                relazify(trc);
            } else {
                MarkScriptUnbarriered(trc, &u.i.s.script_, "script");
            }
        } else if (isInterpretedLazy() && u.i.s.lazy_) {
            MarkLazyScriptUnbarriered(trc, &u.i.s.lazy_, "lazyScript");
        }
        if (u.i.env_)
            MarkObjectUnbarriered(trc, &u.i.env_, "fun_callscope");
    }
}

static void
fun_trace(JSTracer *trc, JSObject *obj)
{
    obj->as<JSFunction>().trace(trc);
}

const Class JSFunction::class_ = {
    js_Function_str,
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    fun_enumerate,
    (JSResolveOp)js::fun_resolve,
    JS_ConvertStub,
    nullptr,                 /* finalize    */
    nullptr,                 /* call        */
    fun_hasInstance,
    nullptr,                 /* construct   */
    fun_trace
};

const Class* const js::FunctionClassPtr = &JSFunction::class_;

/* Find the body of a function (not including braces). */
static bool
FindBody(JSContext *cx, HandleFunction fun, ConstTwoByteChars chars, size_t length,
         size_t *bodyStart, size_t *bodyEnd)
{
    // We don't need principals, since those are only used for error reporting.
    CompileOptions options(cx);
    options.setFileAndLine("internal-findBody", 0)
           .setVersion(fun->nonLazyScript()->getVersion());
    AutoKeepAtoms keepAtoms(cx->perThreadData);
    TokenStream ts(cx, options, chars.get(), length, nullptr);
    int nest = 0;
    bool onward = true;
    // Skip arguments list.
    do {
        switch (ts.getToken()) {
          case TOK_NAME:
          case TOK_YIELD:
            if (nest == 0)
                onward = false;
            break;
          case TOK_LP:
            nest++;
            break;
          case TOK_RP:
            if (--nest == 0)
                onward = false;
            break;
          case TOK_ERROR:
            // Must be memory.
            return false;
          default:
            break;
        }
    } while (onward);
    TokenKind tt = ts.getToken();
    if (tt == TOK_ARROW)
        tt = ts.getToken();
    if (tt == TOK_ERROR)
        return false;
    bool braced = tt == TOK_LC;
    JS_ASSERT_IF(fun->isExprClosure(), !braced);
    *bodyStart = ts.currentToken().pos.begin;
    if (braced)
        *bodyStart += 1;
    ConstTwoByteChars end(chars.get() + length, chars.get(), length);
    if (end[-1] == '}') {
        end--;
    } else {
        JS_ASSERT(!braced);
        for (; unicode::IsSpaceOrBOM2(end[-1]); end--)
            ;
    }
    *bodyEnd = end - chars;
    JS_ASSERT(*bodyStart <= *bodyEnd);
    return true;
}

JSString *
js::FunctionToString(JSContext *cx, HandleFunction fun, bool bodyOnly, bool lambdaParen)
{
    if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
        return nullptr;

    // If the object is an automatically-bound arrow function, get the source
    // of the pre-binding target.
    if (fun->isArrow() && fun->isBoundFunction()) {
        JSObject *target = fun->getBoundFunctionTarget();
        RootedFunction targetFun(cx, &target->as<JSFunction>());
        JS_ASSERT(targetFun->isArrow());
        return FunctionToString(cx, targetFun, bodyOnly, lambdaParen);
    }

    StringBuffer out(cx);
    RootedScript script(cx);

    if (fun->hasScript()) {
        script = fun->nonLazyScript();
        if (script->isGeneratorExp()) {
            if ((!bodyOnly && !out.append("function genexp() {")) ||
                !out.append("\n    [generator expression]\n") ||
                (!bodyOnly && !out.append("}")))
            {
                return nullptr;
            }
            return out.finishString();
        }
    }
    if (!bodyOnly) {
        // If we're not in pretty mode, put parentheses around lambda functions.
        if (fun->isInterpreted() && !lambdaParen && fun->isLambda() && !fun->isArrow()) {
            if (!out.append("("))
                return nullptr;
        }
        if (!fun->isArrow()) {
            if (!(fun->isStarGenerator() ? out.append("function* ") : out.append("function ")))
                return nullptr;
        }
        if (fun->atom()) {
            if (!out.append(fun->atom()))
                return nullptr;
        }
    }
    bool haveSource = fun->isInterpreted() && !fun->isSelfHostedBuiltin();
    if (haveSource && !script->scriptSource()->hasSourceData() &&
        !JSScript::loadSource(cx, script->scriptSource(), &haveSource))
    {
        return nullptr;
    }
    if (haveSource) {
        RootedString srcStr(cx, script->sourceData(cx));
        if (!srcStr)
            return nullptr;
        Rooted<JSFlatString *> src(cx, srcStr->ensureFlat(cx));
        if (!src)
            return nullptr;

        ConstTwoByteChars chars(src->chars(), src->length());
        bool exprBody = fun->isExprClosure();

        // The source data for functions created by calling the Function
        // constructor is only the function's body.  This depends on the fact,
        // asserted below, that in Function("function f() {}"), the inner
        // function's sourceStart points to the '(', not the 'f'.
        bool funCon = !fun->isArrow() &&
                      script->sourceStart() == 0 &&
                      script->sourceEnd() == script->scriptSource()->length() &&
                      script->scriptSource()->argumentsNotIncluded();

        // Functions created with the constructor can't be arrow functions or
        // expression closures.
        JS_ASSERT_IF(funCon, !fun->isArrow());
        JS_ASSERT_IF(funCon, !exprBody);
        JS_ASSERT_IF(!funCon && !fun->isArrow(), src->length() > 0 && chars[0] == '(');

        // If a function inherits strict mode by having scopes above it that
        // have "use strict", we insert "use strict" into the body of the
        // function. This ensures that if the result of toString is evaled, the
        // resulting function will have the same semantics.
        bool addUseStrict = script->strict() && !script->explicitUseStrict() && !fun->isArrow();

        bool buildBody = funCon && !bodyOnly;
        if (buildBody) {
            // This function was created with the Function constructor. We don't
            // have source for the arguments, so we have to generate that. Part
            // of bug 755821 should be cobbling the arguments passed into the
            // Function constructor into the source string.
            if (!out.append("("))
                return nullptr;

            // Fish out the argument names.
            BindingVector *localNames = cx->new_<BindingVector>(cx);
            ScopedJSDeletePtr<BindingVector> freeNames(localNames);
            if (!FillBindingVector(script, localNames))
                return nullptr;
            for (unsigned i = 0; i < fun->nargs(); i++) {
                if ((i && !out.append(", ")) ||
                    (i == unsigned(fun->nargs() - 1) && fun->hasRest() && !out.append("...")) ||
                    !out.append((*localNames)[i].name())) {
                    return nullptr;
                }
            }
            if (!out.append(") {\n"))
                return nullptr;
        }
        if ((bodyOnly && !funCon) || addUseStrict) {
            // We need to get at the body either because we're only supposed to
            // return the body or we need to insert "use strict" into the body.
            size_t bodyStart = 0, bodyEnd;

            // If the function is defined in the Function constructor, we
            // already have a body.
            if (!funCon) {
                JS_ASSERT(!buildBody);
                if (!FindBody(cx, fun, chars, src->length(), &bodyStart, &bodyEnd))
                    return nullptr;
            } else {
                bodyEnd = src->length();
            }

            if (addUseStrict) {
                // Output source up to beginning of body.
                if (!out.append(chars, bodyStart))
                    return nullptr;
                if (exprBody) {
                    // We can't insert a statement into a function with an
                    // expression body. Do what the decompiler did, and insert a
                    // comment.
                    if (!out.append("/* use strict */ "))
                        return nullptr;
                } else {
                    if (!out.append("\n\"use strict\";\n"))
                        return nullptr;
                }
            }

            // Output just the body (for bodyOnly) or the body and possibly
            // closing braces (for addUseStrict).
            size_t dependentEnd = bodyOnly ? bodyEnd : src->length();
            if (!out.append(chars + bodyStart, dependentEnd - bodyStart))
                return nullptr;
        } else {
            if (!out.append(src))
                return nullptr;
        }
        if (buildBody) {
            if (!out.append("\n}"))
                return nullptr;
        }
        if (bodyOnly) {
            // Slap a semicolon on the end of functions with an expression body.
            if (exprBody && !out.append(";"))
                return nullptr;
        } else if (!lambdaParen && fun->isLambda() && !fun->isArrow()) {
            if (!out.append(")"))
                return nullptr;
        }
    } else if (fun->isInterpreted() && !fun->isSelfHostedBuiltin()) {
        if ((!bodyOnly && !out.append("() {\n    ")) ||
            !out.append("[sourceless code]") ||
            (!bodyOnly && !out.append("\n}")))
            return nullptr;
        if (!lambdaParen && fun->isLambda() && !fun->isArrow() && !out.append(")"))
            return nullptr;
    } else {
        JS_ASSERT(!fun->isExprClosure());
        if ((!bodyOnly && !out.append("() {\n    ")) ||
            !out.append("[native code]") ||
            (!bodyOnly && !out.append("\n}")))
            return nullptr;
    }
    return out.finishString();
}

JSString *
fun_toStringHelper(JSContext *cx, HandleObject obj, unsigned indent)
{
    if (!obj->is<JSFunction>()) {
        if (obj->is<ProxyObject>())
            return Proxy::fun_toString(cx, obj, indent);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return nullptr;
    }

    RootedFunction fun(cx, &obj->as<JSFunction>());
    return FunctionToString(cx, fun, false, indent != JS_DONT_PRETTY_PRINT);
}

static bool
fun_toString(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(IsFunctionObject(args.calleev()));

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
fun_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(IsFunctionObject(args.calleev()));

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
js_fun_call(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    HandleValue fval = args.thisv();
    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, args, &JSFunction::class_);
        return false;
    }

    args.setCallee(fval);
    args.setThis(args.get(0));

    if (args.length() > 0) {
        for (size_t i = 0; i < args.length() - 1; i++)
            args[i].set(args[i + 1]);
        args = CallArgsFromVp(args.length() - 1, vp);
    }

    return Invoke(cx, args);
}

// ES5 15.3.4.3
bool
js_fun_apply(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    HandleValue fval = args.thisv();
    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, args, &JSFunction::class_);
        return false;
    }

    // Step 2.
    if (args.length() < 2 || args[1].isNullOrUndefined())
        return js_fun_call(cx, (argc > 0) ? 1 : 0, vp);

    InvokeArgs args2(cx);

    // A JS_OPTIMIZED_ARGUMENTS magic value means that 'arguments' flows into
    // this apply call from a scripted caller and, as an optimization, we've
    // avoided creating it since apply can simply pull the argument values from
    // the calling frame (which we must do now).
    if (args[1].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        // Step 3-6.
        ScriptFrameIter iter(cx);
        JS_ASSERT(iter.numActualArgs() <= ARGS_LENGTH_MAX);
        if (!args2.init(iter.numActualArgs()))
            return false;

        args2.setCallee(fval);
        args2.setThis(args[0]);

        // Steps 7-8.
        iter.unaliasedForEachActual(cx, CopyTo(args2.array()));
    } else {
        // Step 3.
        if (!args[1].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
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
        if (length > ARGS_LENGTH_MAX) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_TOO_MANY_FUN_APPLY_ARGS);
            return false;
        }

        if (!args2.init(length))
            return false;

        // Push fval, obj, and aobj's elements as args.
        args2.setCallee(fval);
        args2.setThis(args[0]);

        // Steps 7-8.
        if (!GetElements(cx, aobj, length, args2.array()))
            return false;
    }

    // Step 9.
    if (!Invoke(cx, args2))
        return false;

    args.rval().set(args2.rval());
    return true;
}

static const uint32_t JSSLOT_BOUND_FUNCTION_THIS       = 0;
static const uint32_t JSSLOT_BOUND_FUNCTION_ARGS_COUNT = 1;

static const uint32_t BOUND_FUNCTION_RESERVED_SLOTS = 2;

inline bool
JSFunction::initBoundFunction(JSContext *cx, HandleValue thisArg,
                              const Value *args, unsigned argslen)
{
    RootedFunction self(cx, this);

    /*
     * Convert to a dictionary to set the BOUND_FUNCTION flag and increase
     * the slot span to cover the arguments and additional slots for the 'this'
     * value and arguments count.
     */
    if (!self->toDictionaryMode(cx))
        return false;

    if (!self->setFlag(cx, BaseShape::BOUND_FUNCTION))
        return false;

    if (!JSObject::setSlotSpan(cx, self, BOUND_FUNCTION_RESERVED_SLOTS + argslen))
        return false;

    self->setSlot(JSSLOT_BOUND_FUNCTION_THIS, thisArg);
    self->setSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT, PrivateUint32Value(argslen));

    self->initSlotRange(BOUND_FUNCTION_RESERVED_SLOTS, args, argslen);

    return true;
}

inline const js::Value &
JSFunction::getBoundFunctionThis() const
{
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_THIS);
}

inline const js::Value &
JSFunction::getBoundFunctionArgument(unsigned which) const
{
    JS_ASSERT(isBoundFunction());
    JS_ASSERT(which < getBoundFunctionArgumentCount());

    return getSlot(BOUND_FUNCTION_RESERVED_SLOTS + which);
}

inline size_t
JSFunction::getBoundFunctionArgumentCount() const
{
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT).toPrivateUint32();
}

/* static */ bool
JSFunction::createScriptForLazilyInterpretedFunction(JSContext *cx, HandleFunction fun)
{
    JS_ASSERT(fun->isInterpretedLazy());

    Rooted<LazyScript*> lazy(cx, fun->lazyScriptOrNull());
    if (lazy) {
        // Trigger a pre barrier on the lazy script being overwritten.
        if (cx->zone()->needsBarrier())
            LazyScript::writeBarrierPre(lazy);

        // Suppress GC for now although we should be able to remove this by
        // making 'lazy' a Rooted<LazyScript*> (which requires adding a
        // THING_ROOT_LAZY_SCRIPT).
        AutoSuppressGC suppressGC(cx);

        RootedScript script(cx, lazy->maybeScript());

        if (script) {
            fun->setUnlazifiedScript(script);
            // Remember the lazy script on the compiled script, so it can be
            // stored on the function again in case of re-lazification.
            // Only functions without inner functions are re-lazified.
            if (!lazy->numInnerFunctions())
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
        if (!lazy->numInnerFunctions() && !JS::IsIncrementalGCInProgress(cx->runtime())) {
            LazyScriptCache::Lookup lookup(cx, lazy);
            cx->runtime()->lazyScriptCache.lookup(lookup, script.address());
        }

        if (script) {
            RootedObject enclosingScope(cx, lazy->enclosingScope());
            RootedScript clonedScript(cx, CloneScript(cx, enclosingScope, fun, script));
            if (!clonedScript)
                return false;

            clonedScript->setSourceObject(lazy->sourceObject());

            fun->initAtom(script->functionNonDelazifying()->displayAtom());
            clonedScript->setFunction(fun);

            fun->setUnlazifiedScript(clonedScript);

            CallNewScriptHook(cx, clonedScript, fun);

            if (!lazy->maybeScript())
                lazy->initScript(clonedScript);
            return true;
        }

        JS_ASSERT(lazy->source()->hasSourceData());

        // Parse and compile the script from source.
        SourceDataCache::AutoSuppressPurge asp(cx);
        const jschar *chars = lazy->source()->chars(cx, asp);
        if (!chars)
            return false;

        const jschar *lazyStart = chars + lazy->begin();
        size_t lazyLength = lazy->end() - lazy->begin();

        if (!frontend::CompileLazyFunction(cx, lazy, lazyStart, lazyLength))
            return false;

        script = fun->nonLazyScript();

        // Remember the compiled script on the lazy script itself, in case
        // there are clones of the function still pointing to the lazy script.
        if (!lazy->maybeScript())
            lazy->initScript(script);

        // Try to insert the newly compiled script into the lazy script cache.
        if (!lazy->numInnerFunctions()) {
            // A script's starting column isn't set by the bytecode emitter, so
            // specify this from the lazy script so that if an identical lazy
            // script is encountered later a match can be determined.
            script->setColumn(lazy->column());

            LazyScriptCache::Lookup lookup(cx, lazy);
            cx->runtime()->lazyScriptCache.insert(lookup, script);

            // Remember the lazy script on the compiled script, so it can be
            // stored on the function again in case of re-lazification.
            // Only functions without inner functions are re-lazified.
            script->setLazyScript(lazy);
        }
        return true;
    }

    /* Lazily cloned self-hosted script. */
    JS_ASSERT(fun->isSelfHostedBuiltin());
    RootedAtom funAtom(cx, &fun->getExtendedSlot(0).toString()->asAtom());
    if (!funAtom)
        return false;
    Rooted<PropertyName *> funName(cx, funAtom->asPropertyName());
    return cx->runtime()->cloneSelfHostedFunctionScript(cx, funName, fun);
}

void
JSFunction::relazify(JSTracer *trc)
{
    JSScript *script = nonLazyScript();
    JS_ASSERT(script->isRelazifiable());
    JS_ASSERT(!compartment()->hasBeenEntered());
    JS_ASSERT(!compartment()->debugMode());

    // If the script's canonical function isn't lazy, we have to mark the
    // script. Otherwise, the following scenario would leave it unmarked
    // and cause it to be swept while a function is still expecting it to be
    // valid:
    // 1. an incremental GC slice causes the canonical function to relazify
    // 2. a clone is used and delazifies the canonical function
    // 3. another GC slice causes the clone to relazify
    // The result is that no function marks the script, but the canonical
    // function expects it to be valid.
    if (script->functionNonDelazifying()->hasScript())
        MarkScriptUnbarriered(trc, &u.i.s.script_, "script");

    flags_ &= ~INTERPRETED;
    flags_ |= INTERPRETED_LAZY;
    LazyScript *lazy = script->maybeLazyScript();
    u.i.s.lazy_ = lazy;
    if (lazy) {
        JS_ASSERT(!isSelfHostedBuiltin());
        // If this is the script stored in the lazy script to be cloned
        // for un-lazifying other functions, reset it so the script can
        // be freed.
        if (lazy->maybeScript() == script)
            lazy->resetScript();
        MarkLazyScriptUnbarriered(trc, &u.i.s.lazy_, "lazyScript");
    } else {
        JS_ASSERT(isSelfHostedBuiltin());
        JS_ASSERT(isExtended());
        JS_ASSERT(getExtendedSlot(0).toString()->isAtom());
    }
}

/* ES5 15.3.4.5.1 and 15.3.4.5.2. */
bool
js::CallOrConstructBoundFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedFunction fun(cx, &args.callee().as<JSFunction>());
    JS_ASSERT(fun->isBoundFunction());

    bool constructing = args.isConstructing();
    if (constructing && fun->isArrow()) {
        /*
         * Per spec, arrow functions do not even have a [[Construct]] method.
         * So before anything else, if we are an arrow function, make sure we
         * don't even get here. You never saw me. Burn this comment.
         */
        RootedValue v(cx, ObjectValue(*fun));
        return ReportIsNotFunction(cx, v, -1, CONSTRUCT);
    }

    /* 15.3.4.5.1 step 1, 15.3.4.5.2 step 3. */
    unsigned argslen = fun->getBoundFunctionArgumentCount();

    if (argc + argslen > ARGS_LENGTH_MAX) {
        js_ReportAllocationOverflow(cx);
        return false;
    }

    /* 15.3.4.5.1 step 3, 15.3.4.5.2 step 1. */
    RootedObject target(cx, fun->getBoundFunctionTarget());

    /* 15.3.4.5.1 step 2. */
    const Value &boundThis = fun->getBoundFunctionThis();

    InvokeArgs invokeArgs(cx);
    if (!invokeArgs.init(argc + argslen))
        return false;

    /* 15.3.4.5.1, 15.3.4.5.2 step 4. */
    for (unsigned i = 0; i < argslen; i++)
        invokeArgs[i].set(fun->getBoundFunctionArgument(i));
    PodCopy(invokeArgs.array() + argslen, vp + 2, argc);

    /* 15.3.4.5.1, 15.3.4.5.2 step 5. */
    invokeArgs.setCallee(ObjectValue(*target));

    if (!constructing)
        invokeArgs.setThis(boundThis);

    if (constructing ? !InvokeConstructor(cx, invokeArgs) : !Invoke(cx, invokeArgs))
        return false;

    *vp = invokeArgs.rval();
    return true;
}

static bool
fun_isGenerator(JSContext *cx, unsigned argc, Value *vp)
{
    JSFunction *fun;
    if (!IsFunctionObject(vp[1], &fun)) {
        JS_SET_RVAL(cx, vp, BooleanValue(false));
        return true;
    }

    JS_SET_RVAL(cx, vp, BooleanValue(fun->isGenerator()));
    return true;
}

/* ES5 15.3.4.5. */
static bool
fun_bind(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    Value thisv = args.thisv();

    /* Step 2. */
    if (!js_IsCallable(thisv)) {
        ReportIncompatibleMethod(cx, args, &JSFunction::class_);
        return false;
    }

    /* Step 3. */
    Value *boundArgs = nullptr;
    unsigned argslen = 0;
    if (args.length() > 1) {
        boundArgs = args.array() + 1;
        argslen = args.length() - 1;
    }

    /* Steps 7-9. */
    RootedValue thisArg(cx, args.length() >= 1 ? args[0] : UndefinedValue());
    RootedObject target(cx, &thisv.toObject());
    JSObject *boundFunction = js_fun_bind(cx, target, thisArg, boundArgs, argslen);
    if (!boundFunction)
        return false;

    /* Step 22. */
    args.rval().setObject(*boundFunction);
    return true;
}

JSObject*
js_fun_bind(JSContext *cx, HandleObject target, HandleValue thisArg,
            Value *boundArgs, unsigned argslen)
{
    /* Steps 15-16. */
    unsigned length = 0;
    if (target->is<JSFunction>()) {
        unsigned nargs = target->as<JSFunction>().nargs();
        if (nargs > argslen)
            length = nargs - argslen;
    }

    /* Step 4-6, 10-11. */
    RootedAtom name(cx, target->is<JSFunction>() ? target->as<JSFunction>().atom() : nullptr);

    RootedObject funobj(cx, NewFunction(cx, js::NullPtr(), CallOrConstructBoundFunction, length,
                                        JSFunction::NATIVE_CTOR, target, name));
    if (!funobj)
        return nullptr;

    /* NB: Bound functions abuse |parent| to store their target. */
    if (!JSObject::setParent(cx, funobj, target))
        return nullptr;

    if (!funobj->as<JSFunction>().initBoundFunction(cx, thisArg, boundArgs, argslen))
        return nullptr;

    /* Steps 17, 19-21 are handled by fun_resolve. */
    /* Step 18 is the default for new functions. */
    return funobj;
}

/*
 * Report "malformed formal parameter" iff no illegal char or similar scanner
 * error was already reported.
 */
static bool
OnBadFormal(JSContext *cx, TokenKind tt)
{
    if (tt != TOK_ERROR)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_FORMAL);
    else
        JS_ASSERT(cx->isExceptionPending());
    return false;
}

const JSFunctionSpec js::function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      js_fun_apply,   2,0),
    JS_FN(js_call_str,       js_fun_call,    1,0),
    JS_FN("bind",            fun_bind,       1,0),
    JS_FN("isGenerator",     fun_isGenerator,0,0),
    JS_FS_END
};

static bool
FunctionConstructor(JSContext *cx, unsigned argc, Value *vp, GeneratorKind generatorKind)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedString arg(cx);   // used multiple times below

    /* Block this call if security callbacks forbid it. */
    Rooted<GlobalObject*> global(cx, &args.callee().global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, global)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CSP_BLOCKED_FUNCTION);
        return false;
    }

    AutoKeepAtoms keepAtoms(cx->perThreadData);
    AutoNameVector formals(cx);

    bool hasRest = false;

    bool isStarGenerator = generatorKind == StarGenerator;
    JS_ASSERT(generatorKind != LegacyGenerator);

    JSScript *maybeScript = nullptr;
    const char *filename;
    unsigned lineno;
    JSPrincipals *originPrincipals;
    uint32_t pcOffset;
    DescribeScriptedCallerForCompilation(cx, &maybeScript, &filename, &lineno, &pcOffset,
                                         &originPrincipals);

    const char *introductionType = "Function";
    if (generatorKind != NotGenerator)
        introductionType = "GeneratorFunction";

    const char *introducerFilename = filename;
    if (maybeScript && maybeScript->scriptSource()->introducerFilename())
        introducerFilename = maybeScript->scriptSource()->introducerFilename();

    CompileOptions options(cx);
    options.setOriginPrincipals(originPrincipals)
           .setFileAndLine(filename, 1)
           .setNoScriptRval(false)
           .setCompileAndGo(true)
           .setIntroductionInfo(introducerFilename, introductionType, lineno, maybeScript, pcOffset);

    unsigned n = args.length() ? args.length() - 1 : 0;
    if (n > 0) {
        /*
         * Collect the function-argument arguments into one string, separated
         * by commas, then make a tokenstream from that string, and scan it to
         * get the arguments.  We need to throw the full scanner at the
         * problem, because the argument string can legitimately contain
         * comments and linefeeds.  XXX It might be better to concatenate
         * everything up into a function definition and pass it to the
         * compiler, but doing it this way is less of a delta from the old
         * code.  See ECMA 15.3.2.1.
         */
        size_t args_length = 0;
        for (unsigned i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            arg = ToString<CanGC>(cx, args[i]);
            if (!arg)
                return false;
            args[i].setString(arg);

            /*
             * Check for overflow.  The < test works because the maximum
             * JSString length fits in 2 fewer bits than size_t has.
             */
            size_t old_args_length = args_length;
            args_length = old_args_length + arg->length();
            if (args_length < old_args_length) {
                js_ReportAllocationOverflow(cx);
                return false;
            }
        }

        /* Add 1 for each joining comma and check for overflow (two ways). */
        size_t old_args_length = args_length;
        args_length = old_args_length + n - 1;
        if (args_length < old_args_length ||
            args_length >= ~(size_t)0 / sizeof(jschar)) {
            js_ReportAllocationOverflow(cx);
            return false;
        }

        /*
         * Allocate a string to hold the concatenated arguments, including room
         * for a terminating 0. Mark cx->tempLifeAlloc for later release, to
         * free collected_args and its tokenstream in one swoop.
         */
        LifoAllocScope las(&cx->tempLifoAlloc());
        jschar *cp = cx->tempLifoAlloc().newArray<jschar>(args_length + 1);
        if (!cp) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        ConstTwoByteChars collected_args(cp, args_length + 1);

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (unsigned i = 0; i < n; i++) {
            arg = args[i].toString();
            size_t arg_length = arg->length();
            const jschar *arg_chars = arg->getChars(cx);
            if (!arg_chars)
                return false;
            (void) js_strncpy(cp, arg_chars, arg_length);
            cp += arg_length;

            /* Add separating comma or terminating 0. */
            *cp++ = (i + 1 < n) ? ',' : 0;
        }

        /*
         * Initialize a tokenstream that reads from the given string.  No
         * StrictModeGetter is needed because this TokenStream won't report any
         * strict mode errors.  Any strict mode errors which might be reported
         * here (duplicate argument names, etc.) will be detected when we
         * compile the function body.
         */
        TokenStream ts(cx, options, collected_args.get(), args_length,
                       /* strictModeGetter = */ nullptr);
        bool yieldIsValidName = ts.versionNumber() < JSVERSION_1_7 && !isStarGenerator;

        /* The argument string may be empty or contain no tokens. */
        TokenKind tt = ts.getToken();
        if (tt != TOK_EOF) {
            for (;;) {
                /*
                 * Check that it's a name.  This also implicitly guards against
                 * TOK_ERROR, which was already reported.
                 */
                if (hasRest) {
                    ts.reportError(JSMSG_PARAMETER_AFTER_REST);
                    return false;
                }

                if (tt == TOK_YIELD && yieldIsValidName)
                    tt = TOK_NAME;

                if (tt != TOK_NAME) {
                    if (tt == TOK_TRIPLEDOT) {
                        hasRest = true;
                        tt = ts.getToken();
                        if (tt == TOK_YIELD && yieldIsValidName)
                            tt = TOK_NAME;
                        if (tt != TOK_NAME) {
                            if (tt != TOK_ERROR)
                                ts.reportError(JSMSG_NO_REST_NAME);
                            return false;
                        }
                    } else {
                        return OnBadFormal(cx, tt);
                    }
                }

                if (!formals.append(ts.currentName()))
                    return false;

                /*
                 * Get the next token.  Stop on end of stream.  Otherwise
                 * insist on a comma, get another name, and iterate.
                 */
                tt = ts.getToken();
                if (tt == TOK_EOF)
                    break;
                if (tt != TOK_COMMA)
                    return OnBadFormal(cx, tt);
                tt = ts.getToken();
            }
        }
    }

#ifdef DEBUG
    for (unsigned i = 0; i < formals.length(); ++i) {
        JSString *str = formals[i];
        JS_ASSERT(str->asAtom().asPropertyName() == formals[i]);
    }
#endif

    RootedString str(cx);
    if (!args.length())
        str = cx->runtime()->emptyString;
    else
        str = ToString<CanGC>(cx, args[args.length() - 1]);
    if (!str)
        return false;
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    JS::Anchor<JSString *> strAnchor(str);
    const jschar *chars = linear->chars();
    size_t length = linear->length();

    /* Protect inlined chars from root analysis poisoning. */
    SkipRoot skip(cx, &chars);

    /*
     * NB: (new Function) is not lexically closed by its caller, it's just an
     * anonymous function in the top-level scope that its constructor inhabits.
     * Thus 'var x = 42; f = new Function("return x"); print(f())' prints 42,
     * and so would a call to f from another top-level's script or function.
     */
    RootedAtom anonymousAtom(cx, cx->names().anonymous);
    JSObject *proto = nullptr;
    if (isStarGenerator) {
        proto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, global);
        if (!proto)
            return false;
    }
    RootedFunction fun(cx, NewFunctionWithProto(cx, js::NullPtr(), nullptr, 0,
                                                JSFunction::INTERPRETED_LAMBDA, global,
                                                anonymousAtom, proto,
                                                JSFunction::FinalizeKind, TenuredObject));
    if (!fun)
        return false;

    if (!JSFunction::setTypeForScriptedFunction(cx, fun))
        return false;

    if (hasRest)
        fun->setHasRest();

    bool ok;
    if (isStarGenerator)
        ok = frontend::CompileStarGeneratorBody(cx, &fun, options, formals, chars, length);
    else
        ok = frontend::CompileFunctionBody(cx, &fun, options, formals, chars, length);
    args.rval().setObject(*fun);
    return ok;
}

bool
js::Function(JSContext *cx, unsigned argc, Value *vp)
{
    return FunctionConstructor(cx, argc, vp, NotGenerator);
}

bool
js::Generator(JSContext *cx, unsigned argc, Value *vp)
{
    return FunctionConstructor(cx, argc, vp, StarGenerator);
}

bool
JSFunction::isBuiltinFunctionConstructor()
{
    return maybeNative() == Function || maybeNative() == Generator;
}

JSFunction *
js::NewFunction(ExclusiveContext *cx, HandleObject funobjArg, Native native, unsigned nargs,
                JSFunction::Flags flags, HandleObject parent, HandleAtom atom,
                gc::AllocKind allocKind /* = JSFunction::FinalizeKind */,
                NewObjectKind newKind /* = GenericObject */)
{
    return NewFunctionWithProto(cx, funobjArg, native, nargs, flags, parent, atom, nullptr,
                                allocKind, newKind);
}

JSFunction *
js::NewFunctionWithProto(ExclusiveContext *cx, HandleObject funobjArg, Native native,
                         unsigned nargs, JSFunction::Flags flags, HandleObject parent,
                         HandleAtom atom, JSObject *proto,
                         gc::AllocKind allocKind /* = JSFunction::FinalizeKind */,
                         NewObjectKind newKind /* = GenericObject */)
{
    JS_ASSERT(allocKind == JSFunction::FinalizeKind || allocKind == JSFunction::ExtendedFinalizeKind);
    JS_ASSERT(sizeof(JSFunction) <= gc::Arena::thingSize(JSFunction::FinalizeKind));
    JS_ASSERT(sizeof(FunctionExtended) <= gc::Arena::thingSize(JSFunction::ExtendedFinalizeKind));

    RootedObject funobj(cx, funobjArg);
    if (funobj) {
        JS_ASSERT(funobj->is<JSFunction>());
        JS_ASSERT(funobj->getParent() == parent);
        JS_ASSERT_IF(native, funobj->hasSingletonType());
    } else {
        // Don't give asm.js module functions a singleton type since they
        // are cloned (via CloneFunctionObjectIfNotSingleton) which assumes
        // that hasSingletonType implies isInterpreted.
        if (native && !IsAsmJSModuleNative(native))
            newKind = SingletonObject;
        funobj = NewObjectWithClassProto(cx, &JSFunction::class_, proto,
                                         SkipScopeParent(parent), allocKind, newKind);
        if (!funobj)
            return nullptr;
    }
    RootedFunction fun(cx, &funobj->as<JSFunction>());

    if (allocKind == JSFunction::ExtendedFinalizeKind)
        flags = JSFunction::Flags(flags | JSFunction::EXTENDED);

    /* Initialize all function members. */
    fun->setArgCount(uint16_t(nargs));
    fun->setFlags(flags);
    if (fun->isInterpreted()) {
        JS_ASSERT(!native);
        fun->mutableScript().init(nullptr);
        fun->initEnvironment(parent);
    } else {
        JS_ASSERT(fun->isNative());
        JS_ASSERT(native);
        fun->initNative(native, nullptr);
    }
    if (allocKind == JSFunction::ExtendedFinalizeKind)
        fun->initializeExtended();
    fun->initAtom(atom);

    return fun;
}

JSFunction *
js::CloneFunctionObject(JSContext *cx, HandleFunction fun, HandleObject parent, gc::AllocKind allocKind,
                        NewObjectKind newKindArg /* = GenericObject */)
{
    JS_ASSERT(parent);
    JS_ASSERT(!fun->isBoundFunction());

    bool useSameScript = cx->compartment() == fun->compartment() &&
                         !fun->hasSingletonType() &&
                         !types::UseNewTypeForClone(fun);

    if (!useSameScript && fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
        return nullptr;

    NewObjectKind newKind = useSameScript ? newKindArg : SingletonObject;
    JSObject *cloneProto = nullptr;
    if (fun->isStarGenerator()) {
        cloneProto = GlobalObject::getOrCreateStarGeneratorFunctionPrototype(cx, cx->global());
        if (!cloneProto)
            return nullptr;
    }
    JSObject *cloneobj = NewObjectWithClassProto(cx, &JSFunction::class_, cloneProto,
                                                 SkipScopeParent(parent), allocKind, newKind);
    if (!cloneobj)
        return nullptr;
    RootedFunction clone(cx, &cloneobj->as<JSFunction>());

    uint16_t flags = fun->flags() & ~JSFunction::EXTENDED;
    if (allocKind == JSFunction::ExtendedFinalizeKind)
        flags |= JSFunction::EXTENDED;

    clone->setArgCount(fun->nargs());
    clone->setFlags(flags);
    if (fun->hasScript()) {
        clone->initScript(fun->nonLazyScript());
        clone->initEnvironment(parent);
    } else if (fun->isInterpretedLazy()) {
        LazyScript *lazy = fun->lazyScriptOrNull();
        clone->initLazyScript(lazy);
        clone->initEnvironment(parent);
    } else {
        clone->initNative(fun->native(), fun->jitInfo());
    }
    clone->initAtom(fun->displayAtom());

    if (allocKind == JSFunction::ExtendedFinalizeKind) {
        if (fun->isExtended() && fun->compartment() == cx->compartment()) {
            for (unsigned i = 0; i < FunctionExtended::NUM_EXTENDED_SLOTS; i++)
                clone->initExtendedSlot(i, fun->getExtendedSlot(i));
        } else {
            clone->initializeExtended();
        }
    }

    if (useSameScript) {
        /*
         * Clone the function, reusing its script. We can use the same type as
         * the original function provided that its prototype is correct.
         */
        if (fun->getProto() == clone->getProto())
            clone->setType(fun->type());
        return clone;
    }

    RootedFunction cloneRoot(cx, clone);

    /*
     * Across compartments we have to clone the script for interpreted
     * functions. Cross-compartment cloning only happens via JSAPI
     * (JS_CloneFunctionObject) which dynamically ensures that 'script' has
     * no enclosing lexical scope (only the global scope).
     */
    if (cloneRoot->isInterpreted() && !CloneFunctionScript(cx, fun, cloneRoot, newKindArg))
        return nullptr;

    return cloneRoot;
}

JSFunction *
js::DefineFunction(JSContext *cx, HandleObject obj, HandleId id, Native native,
                   unsigned nargs, unsigned flags, AllocKind allocKind /* = FinalizeKind */,
                   NewObjectKind newKind /* = GenericObject */)
{
    PropertyOp gop;
    StrictPropertyOp sop;

    RootedFunction fun(cx);

    if (flags & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        flags &= ~JSFUN_STUB_GSOPS;
        gop = JS_PropertyStub;
        sop = JS_StrictPropertyStub;
    } else {
        gop = nullptr;
        sop = nullptr;
    }

    JSFunction::Flags funFlags;
    if (!native)
        funFlags = JSFunction::INTERPRETED_LAZY;
    else
        funFlags = JSAPIToJSFunctionFlags(flags);
    RootedAtom atom(cx, JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : nullptr);
    fun = NewFunction(cx, NullPtr(), native, nargs, funFlags, obj, atom, allocKind, newKind);
    if (!fun)
        return nullptr;

    RootedValue funVal(cx, ObjectValue(*fun));
    if (!JSObject::defineGeneric(cx, obj, id, funVal, gop, sop, flags & ~JSFUN_FLAGS_MASK))
        return nullptr;

    return fun;
}

bool
js::IsConstructor(const Value &v)
{
    // Step 2.
    if (!v.isObject())
        return false;

    // Step 3-4, a bit complex for us, since we have several flavors of
    // [[Construct]] internal method.
    JSObject &obj = v.toObject();
    if (obj.is<JSFunction>()) {
        JSFunction &fun = obj.as<JSFunction>();
        return fun.isNativeConstructor() || fun.isInterpretedConstructor();
    }
    return obj.getClass()->construct != nullptr;
}

void
js::ReportIncompatibleMethod(JSContext *cx, CallReceiver call, const Class *clasp)
{
    RootedValue thisv(cx, call.thisv());

#ifdef DEBUG
    if (thisv.isObject()) {
        JS_ASSERT(thisv.toObject().getClass() != clasp ||
                  !thisv.toObject().isNative() ||
                  !thisv.toObject().getProto() ||
                  thisv.toObject().getProto()->getClass() != clasp);
    } else if (thisv.isString()) {
        JS_ASSERT(clasp != &StringObject::class_);
    } else if (thisv.isNumber()) {
        JS_ASSERT(clasp != &NumberObject::class_);
    } else if (thisv.isBoolean()) {
        JS_ASSERT(clasp != &BooleanObject::class_);
    } else {
        JS_ASSERT(thisv.isUndefined() || thisv.isNull());
    }
#endif

    if (JSFunction *fun = ReportIfNotFunction(cx, call.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                                 clasp->name, funName, InformalValueTypeName(thisv));
        }
    }
}

void
js::ReportIncompatible(JSContext *cx, CallReceiver call)
{
    if (JSFunction *fun = ReportIfNotFunction(cx, call.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_METHOD,
                                 funName, "method", InformalValueTypeName(call.thisv()));
        }
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

    MOZ_ASSUME_UNREACHABLE("Should not get here");
}

namespace JS {
namespace detail {

JS_PUBLIC_API(void)
CheckIsValidConstructible(Value calleev)
{
    JSObject *callee = &calleev.toObject();
    if (callee->is<JSFunction>())
        JS_ASSERT(callee->as<JSFunction>().isNativeConstructor());
    else
        JS_ASSERT(callee->getClass()->construct != nullptr);
}

} // namespace detail
} // namespace JS
