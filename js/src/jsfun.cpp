/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS function support.
 */
#include <string.h>

#include "mozilla/RangedPtr.h"
#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

#include "builtin/Eval.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/TokenStream.h"
#include "gc/Marking.h"
#include "vm/Debugger.h"
#include "vm/ScopeObject.h"
#include "vm/StringBuffer.h"
#include "vm/Xdr.h"

#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#endif

#include "jsatominlines.h"
#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/ArgumentsObject-inl.h"
#include "vm/ScopeObject-inl.h"
#include "vm/Stack-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;
using namespace js::types;
using namespace js::frontend;

static JSBool
fun_getProperty(JSContext *cx, HandleObject obj_, HandleId id, MutableHandleValue vp)
{
    JSObject *obj = obj_;
    while (!obj->isFunction()) {
        obj = obj->getProto();
        if (!obj)
            return true;
    }
    JSFunction *fun = obj->toFunction();

    /*
     * Mark the function's script as uninlineable, to expand any of its
     * frames on the stack before we go looking for them. This allows the
     * below walk to only check each explicit frame rather than needing to
     * check any calls that were inlined.
     */
    if (fun->isInterpreted()) {
        fun->script()->uninlineable = true;
        MarkTypeObjectFlags(cx, fun, OBJECT_FLAG_UNINLINEABLE);
    }

    /* Set to early to null in case of error */
    vp.setNull();

    /* Find fun's top-most activation record. */
    StackIter iter(cx);
    for (; !iter.done(); ++iter) {
        if (!iter.isFunctionFrame() || iter.isEvalFrame())
            continue;
        if (iter.callee() == fun)
            break;
    }
    if (iter.done())
        return true;

    StackFrame *fp = iter.fp();

    if (JSID_IS_ATOM(id, cx->runtime->atomState.argumentsAtom)) {
        if (fun->hasRest()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_FUNCTION_ARGUMENTS_AND_REST);
            return false;
        }
        /* Warn if strict about f.arguments or equivalent unqualified uses. */
        if (!JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING | JSREPORT_STRICT, js_GetErrorMessage,
                                          NULL, JSMSG_DEPRECATED_USAGE, js_arguments_str)) {
            return false;
        }

        ArgumentsObject *argsobj = ArgumentsObject::createUnexpected(cx, fp);
        if (!argsobj)
            return false;

        vp.setObject(*argsobj);
        return true;
    }

#ifdef JS_METHODJIT
    if (JSID_IS_ATOM(id, cx->runtime->atomState.callerAtom) && fp && fp->prev()) {
        /*
         * If the frame was called from within an inlined frame, mark the
         * innermost function as uninlineable to expand its frame and allow us
         * to recover its callee object.
         */
        InlinedSite *inlined;
        jsbytecode *prevpc = fp->prevpc(&inlined);
        if (inlined) {
            mjit::JITChunk *chunk = fp->prev()->jit()->chunk(prevpc);
            JSFunction *fun = chunk->inlineFrames()[inlined->inlineIndex].fun;
            fun->script()->uninlineable = true;
            MarkTypeObjectFlags(cx, fun, OBJECT_FLAG_UNINLINEABLE);
        }
    }
#endif

    if (JSID_IS_ATOM(id, cx->runtime->atomState.callerAtom)) {
        ++iter;
        if (iter.done() || !iter.isFunctionFrame()) {
            JS_ASSERT(vp.isNull());
            return true;
        }

        vp.set(iter.calleev());

        /* Censor the caller if it is from another compartment. */
        JSObject &caller = vp.toObject();
        if (caller.compartment() != cx->compartment) {
            vp.setNull();
        } else if (caller.isFunction()) {
            JSFunction *callerFun = caller.toFunction();
            if (callerFun->isInterpreted() && callerFun->inStrictMode()) {
                JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                             JSMSG_CALLER_IS_STRICT);
                return false;
            }
        }

        return true;
    }

    JS_NOT_REACHED("fun_getProperty");
    return false;
}



/* NB: no sentinels at ends -- use ArrayLength to bound loops.
 * Properties censored into [[ThrowTypeError]] in strict mode. */
static const uint16_t poisonPillProps[] = {
    NAME_OFFSET(arguments),
    NAME_OFFSET(caller),
};

static JSBool
fun_enumerate(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->isFunction());

    RootedId id(cx);
    bool found;

    if (!obj->isBoundFunction()) {
        id = NameToId(cx->runtime->atomState.classPrototypeAtom);
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    id = NameToId(cx->runtime->atomState.lengthAtom);
    if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
        return false;

    id = NameToId(cx->runtime->atomState.nameAtom);
    if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
        return false;

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];
        id = NameToId(OFFSET_TO_NAME(cx->runtime, offset));
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    return true;
}

static JSObject *
ResolveInterpretedFunctionPrototype(JSContext *cx, HandleObject obj)
{
#ifdef DEBUG
    JSFunction *fun = obj->toFunction();
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!fun->isFunctionPrototype());
#endif

    /*
     * Assert that fun is not a compiler-created function object, which
     * must never leak to script or embedding code and then be mutated.
     * Also assert that obj is not bound, per the ES5 15.3.4.5 ref above.
     */
    JS_ASSERT(!IsInternalFunctionObject(obj));
    JS_ASSERT(!obj->isBoundFunction());

    /*
     * Make the prototype object an instance of Object with the same parent
     * as the function object itself.
     */
    JSObject *objProto = obj->global().getOrCreateObjectPrototype(cx);
    if (!objProto)
        return NULL;
    RootedObject proto(cx, NewObjectWithGivenProto(cx, &ObjectClass, objProto, NULL));
    if (!proto || !proto->setSingletonType(cx))
        return NULL;

    /*
     * Per ES5 15.3.5.2 a user-defined function's .prototype property is
     * initially non-configurable, non-enumerable, and writable.  Per ES5 13.2
     * the prototype's .constructor property is configurable, non-enumerable,
     * and writable.
     */
    RootedValue protoVal(cx, ObjectValue(*proto));
    RootedValue objVal(cx, ObjectValue(*obj));
    if (!obj->defineProperty(cx, cx->runtime->atomState.classPrototypeAtom,
                             protoVal, JS_PropertyStub, JS_StrictPropertyStub,
                             JSPROP_PERMANENT) ||
        !proto->defineProperty(cx, cx->runtime->atomState.constructorAtom,
                               objVal, JS_PropertyStub, JS_StrictPropertyStub, 0))
    {
       return NULL;
    }

    return proto;
}

static JSBool
fun_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    if (!JSID_IS_ATOM(id))
        return true;

    RootedFunction fun(cx, obj->toFunction());

    if (JSID_IS_ATOM(id, cx->runtime->atomState.classPrototypeAtom)) {
        /*
         * Native or "built-in" functions do not have a .prototype property per
         * ECMA-262, or (Object.prototype, Function.prototype, etc.) have that
         * property created eagerly.
         *
         * ES5 15.3.4: the non-native function object named Function.prototype
         * does not have a .prototype property.
         *
         * ES5 15.3.4.5: bound functions don't have a prototype property. The
         * isNative() test covers this case because bound functions are native
         * functions by definition/construction.
         */
        if (fun->isNative() || fun->isFunctionPrototype())
            return true;

        if (!ResolveInterpretedFunctionPrototype(cx, fun))
            return false;
        objp.set(fun);
        return true;
    }

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
        JSID_IS_ATOM(id, cx->runtime->atomState.nameAtom)) {
        JS_ASSERT(!IsInternalFunctionObject(obj));

        RootedValue v(cx);
        if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom))
            v.setInt32(fun->nargs - fun->hasRest());
        else
            v.setString(fun->atom ? fun->atom : cx->runtime->emptyString);

        if (!DefineNativeProperty(cx, fun, id, v, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0)) {
            return false;
        }
        objp.set(fun);
        return true;
    }

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];

        if (JSID_IS_ATOM(id, OFFSET_TO_NAME(cx->runtime, offset))) {
            JS_ASSERT(!IsInternalFunctionObject(fun));

            PropertyOp getter;
            StrictPropertyOp setter;
            unsigned attrs = JSPROP_PERMANENT;
            if (fun->isInterpreted() ? fun->inStrictMode() : fun->isBoundFunction()) {
                JSObject *throwTypeError = fun->global().getThrowTypeError();

                getter = CastAsPropertyOp(throwTypeError);
                setter = CastAsStrictPropertyOp(throwTypeError);
                attrs |= JSPROP_GETTER | JSPROP_SETTER;
            } else {
                getter = fun_getProperty;
                setter = JS_StrictPropertyStub;
            }

            RootedValue value(cx, UndefinedValue());
            if (!DefineNativeProperty(cx, fun, id, value, getter, setter,
                                      attrs, 0, 0)) {
                return false;
            }
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
    /* NB: Keep this in sync with CloneInterpretedFunction. */
    Rooted<JSAtom*> atom(xdr->cx());
    uint32_t firstword;           /* flag telling whether fun->atom is non-null,
                                   plus for fun->u.i.skipmin, fun->u.i.wrapper,
                                   and 14 bits reserved for future use */
    uint32_t flagsword;           /* word for argument count and fun->flags */

    JSContext *cx = xdr->cx();
    RootedFunction fun(cx);
    JSScript *script;
    if (mode == XDR_ENCODE) {
        fun = objp->toFunction();
        if (!fun->isInterpreted()) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_SCRIPTED_FUNCTION,
                                     name);
            }
            return false;
        }
        firstword = !!fun->atom;
        flagsword = (fun->nargs << 16) | fun->flags;
        atom = fun->atom;
        script = fun->script();
    } else {
        RootedObject parent(cx, NULL);
        fun = js_NewFunction(cx, NULL, NULL, 0, JSFUN_INTERPRETED, parent, NULL);
        if (!fun)
            return false;
        if (!JSObject::clearParent(cx, fun))
            return false;
        if (!JSObject::clearType(cx, fun))
            return false;
        atom = NULL;
        script = NULL;
    }

    if (!xdr->codeUint32(&firstword))
        return false;
    if ((firstword & 1U) && !XDRAtom(xdr, atom.address()))
        return false;
    if (!xdr->codeUint32(&flagsword))
        return false;

    if (!XDRScript(xdr, enclosingScope, enclosingScript, fun, &script))
        return false;

    if (mode == XDR_DECODE) {
        fun->nargs = flagsword >> 16;
        fun->flags = uint16_t(flagsword);
        fun->atom.init(atom);
        fun->initScript(script);
        script->setFunction(fun);
        if (!fun->setTypeForScriptedFunction(cx))
            return false;
        JS_ASSERT(fun->nargs == fun->script()->bindings.numArgs());
        js_CallNewScriptHook(cx, fun->script(), fun);
        objp.set(fun);
    }

    return true;
}

template bool
js::XDRInterpretedFunction(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, MutableHandleObject);

template bool
js::XDRInterpretedFunction(XDRState<XDR_DECODE> *, HandleObject, HandleScript, MutableHandleObject);

JSObject *
js::CloneInterpretedFunction(JSContext *cx, HandleObject enclosingScope, HandleFunction srcFun)
{
    /* NB: Keep this in sync with XDRInterpretedFunction. */

    RootedObject parent(cx, NULL);
    RootedFunction clone(cx, js_NewFunction(cx, NULL, NULL, 0, JSFUN_INTERPRETED, parent, NULL));
    if (!clone)
        return NULL;
    if (!JSObject::clearParent(cx, clone))
        return NULL;
    if (!JSObject::clearType(cx, clone))
        return NULL;

    Rooted<JSScript*> srcScript(cx, srcFun->script());
    JSScript *clonedScript = CloneScript(cx, enclosingScope, clone, srcScript);
    if (!clonedScript)
        return NULL;

    clone->nargs = srcFun->nargs;
    clone->flags = srcFun->flags;
    clone->atom.init(srcFun->atom);
    clone->initScript(clonedScript);
    clonedScript->setFunction(clone);
    if (!clone->setTypeForScriptedFunction(cx))
        return NULL;

    js_CallNewScriptHook(cx, clone->script(), clone);
    return clone;
}

/*
 * [[HasInstance]] internal method for Function objects: fetch the .prototype
 * property of its 'this' parameter, and walks the prototype chain of v (only
 * if v is an object) returning true if .prototype is found.
 */
static JSBool
fun_hasInstance(JSContext *cx, HandleObject obj_, const Value *v, JSBool *bp)
{
    RootedObject obj(cx, obj_);

    while (obj->isFunction()) {
        if (!obj->isBoundFunction())
            break;
        obj = obj->toFunction()->getBoundFunctionTarget();
    }

    RootedValue pval(cx);
    if (!obj->getProperty(cx, cx->runtime->atomState.classPrototypeAtom, &pval))
        return JS_FALSE;

    if (pval.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        js_ReportValueError(cx, JSMSG_BAD_PROTOTYPE, -1, ObjectValue(*obj), NULL);
        return JS_FALSE;
    }

    *bp = js_IsDelegate(cx, &pval.toObject(), *v);
    return JS_TRUE;
}

inline void
JSFunction::trace(JSTracer *trc)
{
    if (isExtended()) {
        MarkValueRange(trc, ArrayLength(toExtended()->extendedSlots),
                       toExtended()->extendedSlots, "nativeReserved");
    }

    if (atom)
        MarkString(trc, &atom, "atom");

    if (isInterpreted()) {
        if (u.i.script_)
            MarkScriptUnbarriered(trc, &u.i.script_, "script");
        if (u.i.env_)
            MarkObjectUnbarriered(trc, &u.i.env_, "fun_callscope");
    }
}

static void
fun_trace(JSTracer *trc, JSObject *obj)
{
    obj->toFunction()->trace(trc);
}

/*
 * Reserve two slots in all function objects for XPConnect.  Note that this
 * does not bloat every instance, only those on which reserved slots are set,
 * and those on which ad-hoc properties are defined.
 */
JS_FRIEND_DATA(Class) js::FunctionClass = {
    js_Function_str,
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    fun_enumerate,
    (JSResolveOp)fun_resolve,
    JS_ConvertStub,
    NULL,                    /* finalize    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    fun_hasInstance,
    NULL,                    /* construct   */
    fun_trace
};


/* Find the body of a function (not including braces). */
static bool
FindBody(JSContext *cx, HandleFunction fun, const jschar *chars, size_t length,
         size_t *bodyStart, size_t *bodyEnd)
{
    // We don't need principals, since those are only used for error reporting.
    CompileOptions options(cx);
    options.setFileAndLine("internal-findBody", 0)
           .setVersion(fun->script()->getVersion());
    TokenStream ts(cx, options, chars, length, NULL);
    JS_ASSERT(chars[0] == '(');
    int nest = 0;
    bool onward = true;
    // Skip arguments list.
    do {
        switch (ts.getToken()) {
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
    if (tt == TOK_ERROR)
        return false;
    bool braced = tt == TOK_LC;
    JS_ASSERT(!!(fun->flags & JSFUN_EXPR_CLOSURE) ^ braced);
    *bodyStart = ts.offsetOfToken(ts.currentToken());
    if (braced)
        *bodyStart += 1;
    RangedPtr<const jschar> end(chars, length);
    end = chars + length;
    if (end[-1] == '}') {
        end--;
    } else {
        JS_ASSERT(!braced);
        for (; unicode::IsSpaceOrBOM2(end[-1]); end--)
            ;
    }
    *bodyEnd = end.get() - chars;
    JS_ASSERT(*bodyStart <= *bodyEnd);
    return true;
}

JSString *
js::FunctionToString(JSContext *cx, HandleFunction fun, bool bodyOnly, bool lambdaParen)
{
    StringBuffer out(cx);

    if (fun->isInterpreted() && fun->script()->isGeneratorExp) {
        if ((!bodyOnly && !out.append("function genexp() {")) ||
            !out.append("\n    [generator expression]\n") ||
            (!bodyOnly && !out.append("}"))) {
            return NULL;
        }
        return out.finishString();
    }
    if (!bodyOnly) {
        // If we're not in pretty mode, put parentheses around lambda functions.
        if (fun->isInterpreted() && !lambdaParen && (fun->flags & JSFUN_LAMBDA)) {
            if (!out.append("("))
                return NULL;
        }
        if (!out.append("function "))
            return NULL;
        if (fun->atom) {
            if (!out.append(fun->atom))
                return NULL;
        }
    }
    bool haveSource = fun->isInterpreted();
    if (haveSource && !fun->script()->scriptSource()->hasSourceData() &&
        !fun->script()->loadSource(cx, &haveSource))
    {
        return NULL;
    }
    if (haveSource) {
        RootedScript script(cx, fun->script());
        RootedString src(cx, fun->script()->sourceData(cx));
        if (!src)
            return NULL;
        const jschar *chars = src->getChars(cx);
        if (!chars)
            return NULL;
        bool exprBody = fun->flags & JSFUN_EXPR_CLOSURE;

        // The source data for functions created by calling the Function
        // constructor is only the function's body.
        bool funCon = script->sourceStart == 0 && script->scriptSource()->argumentsNotIncluded();

        // Functions created with the constructor should not be using the
        // expression body extension.
        JS_ASSERT_IF(funCon, !exprBody);
        JS_ASSERT_IF(!funCon, src->length() > 0 && chars[0] == '(');

        // If a function inherits strict mode by having scopes above it that
        // have "use strict", we insert "use strict" into the body of the
        // function. This ensures that if the result of toString is evaled, the
        // resulting function will have the same semantics.
        bool addUseStrict = script->strictModeCode && !script->explicitUseStrict;

        // Functions created with the constructor can't have inherited strict
        // mode.
        JS_ASSERT(!funCon || !addUseStrict);
        bool buildBody = funCon && !bodyOnly;
        if (buildBody) {
            // This function was created with the Function constructor. We don't
            // have source for the arguments, so we have to generate that. Part
            // of bug 755821 should be cobbling the arguments passed into the
            // Function constructor into the source string.
            if (!out.append("("))
                return NULL;

            // Fish out the argument names.
            BindingVector *localNames = cx->new_<BindingVector>(cx);
            js::ScopedDeletePtr<BindingVector> freeNames(localNames);
            if (!GetOrderedBindings(cx, script->bindings, localNames))
                return NULL;
            for (unsigned i = 0; i < fun->nargs; i++) {
                if ((i && !out.append(", ")) ||
                    (i == unsigned(fun->nargs - 1) && fun->hasRest() && !out.append("...")) ||
                    !out.append((*localNames)[i].maybeName)) {
                    return NULL;
                }
            }
            if (!out.append(") {\n"))
                return NULL;
        }
        if ((bodyOnly && !funCon) || addUseStrict) {
            // We need to get at the body either because we're only supposed to
            // return the body or we need to insert "use strict" into the body.
            JS_ASSERT(!buildBody);
            size_t bodyStart = 0, bodyEnd = 0;
            if (!FindBody(cx, fun, chars, src->length(), &bodyStart, &bodyEnd))
                return NULL;

            if (addUseStrict) {
                // Output source up to beginning of body.
                if (!out.append(chars, bodyStart))
                    return NULL;
                if (exprBody) {
                    // We can't insert a statement into a function with an
                    // expression body. Do what the decompiler did, and insert a
                    // comment.
                    if (!out.append("/* use strict */ "))
                        return NULL;
                } else {
                    if (!out.append("\n\"use strict\";\n"))
                        return NULL;
                }
            }

            // Output just the body (for bodyOnly) or the body and possibly
            // closing braces (for addUseStrict).
            size_t dependentEnd = (bodyOnly) ? bodyEnd : src->length();
            if (!out.append(chars + bodyStart, dependentEnd - bodyStart))
                return NULL;
        } else {
            if (!out.append(src))
                return NULL;
        }
        if (buildBody) {
            if (!out.append("\n}"))
                return NULL;
        }
        if (bodyOnly) {
            // Slap a semicolon on the end of functions with an expression body.
            if (exprBody && !out.append(";"))
                return NULL;
        } else if (!lambdaParen && (fun->flags & JSFUN_LAMBDA)) {
            if (!out.append(")"))
                return NULL;
        }
    } else if (fun->isInterpreted()) {
        if ((!bodyOnly && !out.append("() {\n    ")) ||
            !out.append("[sourceless code]") ||
            (!bodyOnly && !out.append("\n}")))
            return NULL;
        if (!lambdaParen && (fun->flags & JSFUN_LAMBDA) && (!out.append(")")))
            return NULL;
    } else {
        JS_ASSERT(!(fun->flags & JSFUN_EXPR_CLOSURE));
        if ((!bodyOnly && !out.append("() {\n    ")) ||
            !out.append("[native code]") ||
            (!bodyOnly && !out.append("\n}")))
            return NULL;
    }
    return out.finishString();
}

JSString *
fun_toStringHelper(JSContext *cx, JSObject *obj, unsigned indent)
{
    if (!obj->isFunction()) {
        if (IsFunctionProxy(obj))
            return Proxy::fun_toString(cx, obj, indent);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return NULL;
    }

    RootedFunction fun(cx, obj->toFunction());
    return FunctionToString(cx, fun, false, indent != JS_DONT_PRETTY_PRINT);
}

static JSBool
fun_toString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ASSERT(IsFunctionObject(vp[0]));
    uint32_t indent = 0;

    if (argc != 0 && !ToUint32(cx, vp[2], &indent))
        return false;

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    JSString *str = fun_toStringHelper(cx, obj, indent);
    if (!str)
        return false;

    vp->setString(str);
    return true;
}

#if JS_HAS_TOSOURCE
static JSBool
fun_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    JS_ASSERT(IsFunctionObject(vp[0]));

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;

    JSString *str = fun_toStringHelper(cx, obj, JS_DONT_PRETTY_PRINT);
    if (!str)
        return false;

    vp->setString(str);
    return true;
}
#endif

JSBool
js_fun_call(JSContext *cx, unsigned argc, Value *vp)
{
    Value fval = vp[1];

    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, CallReceiverFromVp(vp), &FunctionClass);
        return false;
    }

    Value *argv = vp + 2;
    Value thisv;
    if (argc == 0) {
        thisv.setUndefined();
    } else {
        thisv = argv[0];

        argc--;
        argv++;
    }

    /* Allocate stack space for fval, obj, and the args. */
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return JS_FALSE;

    /* Push fval, thisv, and the args. */
    args.calleev() = fval;
    args.thisv() = thisv;
    PodCopy(args.array(), argv, argc);

    bool ok = Invoke(cx, args);
    *vp = args.rval();
    return ok;
}

/* ES5 15.3.4.3 */
JSBool
js_fun_apply(JSContext *cx, unsigned argc, Value *vp)
{
    /* Step 1. */
    RootedValue fval(cx, vp[1]);
    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, CallReceiverFromVp(vp), &FunctionClass);
        return false;
    }

    /* Step 2. */
    if (argc < 2 || vp[3].isNullOrUndefined())
        return js_fun_call(cx, (argc > 0) ? 1 : 0, vp);

    InvokeArgsGuard args;

    /*
     * GuardFunApplyArgumentsOptimization already called IsOptimizedArguments,
     * so we don't need to here. This is not an optimization: we can't rely on
     * cx->fp (since natives can be called directly from JSAPI).
     */
    if (vp[3].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        /*
         * Pretend we have been passed the 'arguments' object for the current
         * function and read actuals out of the frame.
         *
         * N.B. Changes here need to be propagated to stubs::SplatApplyArgs.
         */
        /* Steps 4-6. */
        unsigned length = cx->fp()->numActualArgs();
        JS_ASSERT(length <= StackSpace::ARGS_LENGTH_MAX);

        if (!cx->stack.pushInvokeArgs(cx, length, &args))
            return false;

        /* Push fval, obj, and aobj's elements as args. */
        args.calleev() = fval;
        args.thisv() = vp[2];

        /* Steps 7-8. */
        cx->fp()->forEachUnaliasedActual(CopyTo(args.array()));
    } else {
        /* Step 3. */
        if (!vp[3].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
            return false;
        }

        /*
         * Steps 4-5 (note erratum removing steps originally numbered 5 and 7 in
         * original version of ES5).
         */
        RootedObject aobj(cx, &vp[3].toObject());
        uint32_t length;
        if (!js_GetLengthProperty(cx, aobj, &length))
            return false;

        /* Step 6. */
        if (length > StackSpace::ARGS_LENGTH_MAX) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TOO_MANY_FUN_APPLY_ARGS);
            return false;
        }

        if (!cx->stack.pushInvokeArgs(cx, length, &args))
            return false;

        /* Push fval, obj, and aobj's elements as args. */
        args.calleev() = fval;
        args.thisv() = vp[2];

        /* Steps 7-8. */
        if (!GetElements(cx, aobj, length, args.array()))
            return false;
    }

    /* Step 9. */
    if (!Invoke(cx, args))
        return false;

    *vp = args.rval();
    return true;
}

namespace js {

JSBool
CallOrConstructBoundFunction(JSContext *cx, unsigned argc, Value *vp);

}

static const uint32_t JSSLOT_BOUND_FUNCTION_THIS       = 0;
static const uint32_t JSSLOT_BOUND_FUNCTION_ARGS_COUNT = 1;

static const uint32_t BOUND_FUNCTION_RESERVED_SLOTS = 2;

inline bool
JSFunction::initBoundFunction(JSContext *cx, HandleValue thisArg,
                              const Value *args, unsigned argslen)
{
    JS_ASSERT(isFunction());

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

    if (!self->setSlotSpan(cx, BOUND_FUNCTION_RESERVED_SLOTS + argslen))
        return false;

    self->setSlot(JSSLOT_BOUND_FUNCTION_THIS, thisArg);
    self->setSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT, PrivateUint32Value(argslen));

    self->initSlotRange(BOUND_FUNCTION_RESERVED_SLOTS, args, argslen);

    return true;
}

inline JSObject *
JSFunction::getBoundFunctionTarget() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    /* Bound functions abuse |parent| to store their target function. */
    return getParent();
}

inline const js::Value &
JSFunction::getBoundFunctionThis() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_THIS);
}

inline const js::Value &
JSFunction::getBoundFunctionArgument(unsigned which) const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());
    JS_ASSERT(which < getBoundFunctionArgumentCount());

    return getSlot(BOUND_FUNCTION_RESERVED_SLOTS + which);
}

inline size_t
JSFunction::getBoundFunctionArgumentCount() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT).toPrivateUint32();
}

namespace js {

/* ES5 15.3.4.5.1 and 15.3.4.5.2. */
JSBool
CallOrConstructBoundFunction(JSContext *cx, unsigned argc, Value *vp)
{
    JSFunction *fun = vp[0].toObject().toFunction();
    JS_ASSERT(fun->isBoundFunction());

    bool constructing = IsConstructing(vp);

    /* 15.3.4.5.1 step 1, 15.3.4.5.2 step 3. */
    unsigned argslen = fun->getBoundFunctionArgumentCount();

    if (argc + argslen > StackSpace::ARGS_LENGTH_MAX) {
        js_ReportAllocationOverflow(cx);
        return false;
    }

    /* 15.3.4.5.1 step 3, 15.3.4.5.2 step 1. */
    JSObject *target = fun->getBoundFunctionTarget();

    /* 15.3.4.5.1 step 2. */
    const Value &boundThis = fun->getBoundFunctionThis();

    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc + argslen, &args))
        return false;

    /* 15.3.4.5.1, 15.3.4.5.2 step 4. */
    for (unsigned i = 0; i < argslen; i++)
        args[i] = fun->getBoundFunctionArgument(i);
    PodCopy(args.array() + argslen, vp + 2, argc);

    /* 15.3.4.5.1, 15.3.4.5.2 step 5. */
    args.calleev().setObject(*target);

    if (!constructing)
        args.thisv() = boundThis;

    if (constructing ? !InvokeConstructor(cx, args) : !Invoke(cx, args))
        return false;

    *vp = args.rval();
    return true;
}

}

#if JS_HAS_GENERATORS
static JSBool
fun_isGenerator(JSContext *cx, unsigned argc, Value *vp)
{
    JSFunction *fun;
    if (!IsFunctionObject(vp[1], &fun)) {
        JS_SET_RVAL(cx, vp, BooleanValue(false));
        return true;
    }

    bool result = false;
    if (fun->isInterpreted()) {
        JSScript *script = fun->script();
        JS_ASSERT(script->length != 0);
        result = script->isGenerator;
    }

    JS_SET_RVAL(cx, vp, BooleanValue(result));
    return true;
}
#endif

/* ES5 15.3.4.5. */
static JSBool
fun_bind(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    Value &thisv = args.thisv();

    /* Step 2. */
    if (!js_IsCallable(thisv)) {
        ReportIncompatibleMethod(cx, args, &FunctionClass);
        return false;
    }

    /* Step 3. */
    Value *boundArgs = NULL;
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
    if (target->isFunction()) {
        unsigned nargs = target->toFunction()->nargs;
        if (nargs > argslen)
            length = nargs - argslen;
    }

    /* Step 4-6, 10-11. */
    JSAtom *name = target->isFunction() ? target->toFunction()->atom.get() : NULL;

    RootedObject funobj(cx, js_NewFunction(cx, NULL, CallOrConstructBoundFunction, length,
                                           JSFUN_CONSTRUCTOR, target, name));
    if (!funobj)
        return NULL;

    /* NB: Bound functions abuse |parent| to store their target. */
    if (!JSObject::setParent(cx, funobj, target))
        return NULL;

    if (!funobj->toFunction()->initBoundFunction(cx, thisArg, boundArgs, argslen))
        return NULL;

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
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_FORMAL);
    else
        JS_ASSERT(cx->isExceptionPending());
    return false;
}

namespace js {

JSFunctionSpec function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      js_fun_apply,   2,0),
    JS_FN(js_call_str,       js_fun_call,    1,0),
    JS_FN("bind",            fun_bind,       1,0),
#if JS_HAS_GENERATORS
    JS_FN("isGenerator",     fun_isGenerator,0,0),
#endif
    JS_FS_END
};

JSBool
Function(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Block this call if security callbacks forbid it. */
    Rooted<GlobalObject*> global(cx, &args.callee().global());
    if (!global->isRuntimeCodeGenEnabled(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_FUNCTION);
        return false;
    }

    Bindings bindings;
    Bindings::AutoRooter bindingsRoot(cx, &bindings);

    bool hasRest = false;

    const char *filename;
    unsigned lineno;
    JSPrincipals *originPrincipals;
    CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals);
    JSPrincipals *principals = PrincipalsForCompiledCode(args, cx);

    CompileOptions options(cx);
    options.setPrincipals(principals)
           .setOriginPrincipals(originPrincipals)
           .setFileAndLine(filename, lineno);

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
            JSString *arg = ToString(cx, args[i]);
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
        jschar *collected_args = cp;

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (unsigned i = 0; i < n; i++) {
            JSString *arg = args[i].toString();
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
        TokenStream ts(cx, options, collected_args, args_length, /* strictModeGetter = */ NULL);

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

                if (tt != TOK_NAME) {
                    if (tt == TOK_TRIPLEDOT) {
                        hasRest = true;
                        tt = ts.getToken();
                        if (tt != TOK_NAME) {
                            if (tt != TOK_ERROR)
                                ts.reportError(JSMSG_NO_REST_NAME);
                            return false;
                        }
                    } else {
                        return OnBadFormal(cx, tt);
                    }
                }

                /* Check for a duplicate parameter name. */
                Rooted<PropertyName*> name(cx, ts.currentToken().name());
                if (bindings.hasBinding(cx, name)) {
                    JSAutoByteString bytes;
                    if (!js_AtomToPrintableString(cx, name, &bytes))
                        return false;
                    if (!ts.reportStrictWarning(JSMSG_DUPLICATE_FORMAL, bytes.ptr()))
                        return false;
                }

                uint16_t dummy;
                if (!bindings.addArgument(cx, name, &dummy))
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

    JS::Anchor<JSString *> strAnchor(NULL);
    const jschar *chars;
    size_t length;

    SkipRoot skip(cx, &chars);

    if (args.length()) {
        JSString *str = ToString(cx, args[args.length() - 1]);
        if (!str)
            return false;
        strAnchor.set(str);
        chars = str->getChars(cx);
        length = str->length();
    } else {
        chars = cx->runtime->emptyString->chars();
        length = 0;
    }

    /*
     * NB: (new Function) is not lexically closed by its caller, it's just an
     * anonymous function in the top-level scope that its constructor inhabits.
     * Thus 'var x = 42; f = new Function("return x"); print(f())' prints 42,
     * and so would a call to f from another top-level's script or function.
     */
    RootedFunction fun(cx, js_NewFunction(cx, NULL, NULL, 0, JSFUN_LAMBDA | JSFUN_INTERPRETED,
                                             global, cx->runtime->atomState.anonymousAtom));
    if (!fun)
        return false;

    if (hasRest)
        fun->setHasRest();

    bool ok = frontend::CompileFunctionBody(cx, fun, options, &bindings, chars, length);
    args.rval().setObject(*fun);
    return ok;
}

bool
IsBuiltinFunctionConstructor(JSFunction *fun)
{
    return fun->maybeNative() == Function;
}

} /* namespace js */

JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, Native native, unsigned nargs,
               unsigned flags, HandleObject parent, JSAtom *atom_, js::gc::AllocKind kind)
{
    JS_ASSERT(kind == JSFunction::FinalizeKind || kind == JSFunction::ExtendedFinalizeKind);
    JS_ASSERT(sizeof(JSFunction) <= gc::Arena::thingSize(JSFunction::FinalizeKind));
    JS_ASSERT(sizeof(FunctionExtended) <= gc::Arena::thingSize(JSFunction::ExtendedFinalizeKind));

    RootedAtom atom(cx, atom_);

    if (funobj) {
        JS_ASSERT(funobj->isFunction());
        JS_ASSERT(funobj->getParent() == parent);
    } else {
        funobj = NewObjectWithClassProto(cx, &FunctionClass, NULL, SkipScopeParent(parent), kind);
        if (!funobj)
            return NULL;
    }
    RootedFunction fun(cx, static_cast<JSFunction *>(funobj));

    /* Initialize all function members. */
    fun->nargs = uint16_t(nargs);
    fun->flags = flags & (JSFUN_FLAGS_MASK | JSFUN_INTERPRETED);
    if (flags & JSFUN_INTERPRETED) {
        JS_ASSERT(!native);
        fun->mutableScript().init(NULL);
        fun->initEnvironment(parent);
    } else {
        fun->u.native = native;
        JS_ASSERT(fun->u.native);
    }
    if (kind == JSFunction::ExtendedFinalizeKind) {
        fun->flags |= JSFUN_EXTENDED;
        fun->initializeExtended();
    }
    fun->atom.init(atom);

    if (native && !fun->setSingletonType(cx))
        return NULL;

    return fun;
}

JSFunction * JS_FASTCALL
js_CloneFunctionObject(JSContext *cx, HandleFunction fun, HandleObject parent,
                       HandleObject proto, gc::AllocKind kind)
{
    JS_ASSERT(parent);
    JS_ASSERT(proto);
    JS_ASSERT(!fun->isBoundFunction());

    JSObject *cloneobj = NewObjectWithClassProto(cx, &FunctionClass, NULL, SkipScopeParent(parent), kind);
    if (!cloneobj)
        return NULL;
    RootedFunction clone(cx, static_cast<JSFunction *>(cloneobj));

    clone->nargs = fun->nargs;
    clone->flags = fun->flags & ~JSFUN_EXTENDED;
    if (fun->isInterpreted()) {
        clone->initScript(fun->script());
        clone->initEnvironment(parent);
    } else {
        clone->u.native = fun->native();
    }
    clone->atom.init(fun->atom);

    if (kind == JSFunction::ExtendedFinalizeKind) {
        clone->flags |= JSFUN_EXTENDED;
        clone->initializeExtended();
    }

    if (cx->compartment == fun->compartment() && !types::UseNewTypeForClone(fun)) {
        /*
         * We can use the same type as the original function provided that (a)
         * its prototype is correct, and (b) its type is not a singleton. The
         * first case will hold in all compileAndGo code, and the second case
         * will have been caught by CloneFunctionObject coming from function
         * definitions or read barriers, so will not get here.
         */
        if (fun->getProto() == proto && !fun->hasSingletonType())
            clone->setType(fun->type());
    } else {
        if (!clone->setSingletonType(cx))
            return NULL;

        /*
         * Across compartments we have to clone the script for interpreted
         * functions. Cross-compartment cloning only happens via JSAPI
         * (JS_CloneFunctionObject) which dynamically ensures that 'script' has
         * no enclosing lexical scope (only the global scope).
         */
        if (clone->isInterpreted()) {
            RootedScript script(cx, clone->script());
            JS_ASSERT(script);
            JS_ASSERT(script->compartment() == fun->compartment());
            JS_ASSERT_IF(script->compartment() != cx->compartment,
                         !script->enclosingStaticScope() && !script->compileAndGo);

            RootedObject scope(cx, script->enclosingStaticScope());

            clone->mutableScript().init(NULL);

            JSScript *cscript = CloneScript(cx, scope, clone, script);
            if (!cscript)
                return NULL;

            clone->setScript(cscript);
            cscript->setFunction(clone);

            GlobalObject *global = script->compileAndGo ? &script->global() : NULL;

            js_CallNewScriptHook(cx, clone->script(), clone);
            Debugger::onNewScript(cx, clone->script(), global);
        }
    }
    return clone;
}

JSFunction *
js_DefineFunction(JSContext *cx, HandleObject obj, HandleId id, Native native,
                  unsigned nargs, unsigned attrs, AllocKind kind)
{
    PropertyOp gop;
    StrictPropertyOp sop;

    RootedFunction fun(cx);

    if (attrs & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        attrs &= ~JSFUN_STUB_GSOPS;
        gop = JS_PropertyStub;
        sop = JS_StrictPropertyStub;
    } else {
        gop = NULL;
        sop = NULL;
    }

    fun = js_NewFunction(cx, NULL, native, nargs,
                         attrs & (JSFUN_FLAGS_MASK),
                         obj,
                         JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : NULL,
                         kind);
    if (!fun)
        return NULL;

    RootedValue funVal(cx, ObjectValue(*fun));
    if (!obj->defineGeneric(cx, id, funVal, gop, sop, attrs & ~JSFUN_FLAGS_MASK))
        return NULL;

    return fun;
}

void
js::ReportIncompatibleMethod(JSContext *cx, CallReceiver call, Class *clasp)
{
    Value &thisv = call.thisv();

#ifdef DEBUG
    if (thisv.isObject()) {
        JS_ASSERT(thisv.toObject().getClass() != clasp ||
                  !thisv.toObject().getProto() ||
                  thisv.toObject().getProto()->getClass() != clasp);
    } else if (thisv.isString()) {
        JS_ASSERT(clasp != &StringClass);
    } else if (thisv.isNumber()) {
        JS_ASSERT(clasp != &NumberClass);
    } else if (thisv.isBoolean()) {
        JS_ASSERT(clasp != &BooleanClass);
    } else {
        JS_ASSERT(thisv.isUndefined() || thisv.isNull());
    }
#endif

    if (JSFunction *fun = ReportIfNotFunction(cx, call.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_METHOD,
                                 funName, "method", InformalValueTypeName(call.thisv()));
        }
    }
}
