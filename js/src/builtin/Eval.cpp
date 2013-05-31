/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jsonparser.h"

#include "builtin/Eval.h"
#include "frontend/BytecodeCompiler.h"
#include "mozilla/HashFunctions.h"
#include "vm/GlobalObject.h"

#include "jsinterpinlines.h"

using namespace js;

using mozilla::AddToHash;
using mozilla::HashString;

// We should be able to assert this for *any* fp->scopeChain().
static void
AssertInnerizedScopeChain(JSContext *cx, JSObject &scopeobj)
{
#ifdef DEBUG
    RootedObject obj(cx);
    for (obj = &scopeobj; obj; obj = obj->enclosingScope()) {
        if (JSObjectOp op = obj->getClass()->ext.innerObject) {
            JS_ASSERT(op(cx, obj) == obj);
        }
    }
#endif
}

static bool
IsEvalCacheCandidate(JSScript *script)
{
    // Make sure there are no inner objects which might use the wrong parent
    // and/or call scope by reusing the previous eval's script. Skip the
    // script's first object, which entrains the eval's scope.
    return script->savedCallerFun &&
           !script->hasSingletons &&
           script->objects()->length == 1 &&
           !script->hasRegexps();
}

/* static */ HashNumber
EvalCacheHashPolicy::hash(const EvalCacheLookup &l)
{
    return AddToHash(HashString(l.str->chars(), l.str->length()),
                     l.callerScript.get(),
                     l.version,
                     l.pc);
}

/* static */ bool
EvalCacheHashPolicy::match(const EvalCacheEntry &cacheEntry, const EvalCacheLookup &l)
{
    JSScript *script = cacheEntry.script;

    JS_ASSERT(IsEvalCacheCandidate(script));

    // Get the source string passed for safekeeping in the atom map
    // by the prior eval to frontend::CompileScript.
    JSAtom *keyStr = script->atoms[0];

    return EqualStrings(keyStr, l.str) &&
           cacheEntry.callerScript == l.callerScript &&
           script->getVersion() == l.version &&
           cacheEntry.pc == l.pc;
}

// There are two things we want to do with each script executed in EvalKernel:
//  1. notify jsdbgapi about script creation/destruction
//  2. add the script to the eval cache when EvalKernel is finished
//
// NB: Although the eval cache keeps a script alive wrt to the JS engine, from
// a jsdbgapi user's perspective, we want each eval() to create and destroy a
// script. This hides implementation details and means we don't have to deal
// with calls to JS_GetScriptObject for scripts in the eval cache.
class EvalScriptGuard
{
    JSContext *cx_;
    Rooted<JSScript*> script_;

    /* These fields are only valid if lookup_.str is non-NULL. */
    EvalCacheLookup lookup_;
    EvalCache::AddPtr p_;

    Rooted<JSLinearString*> lookupStr_;

  public:
    EvalScriptGuard(JSContext *cx)
        : cx_(cx), script_(cx), lookup_(cx), lookupStr_(cx) {}

    ~EvalScriptGuard() {
        if (script_) {
            CallDestroyScriptHook(cx_->runtime->defaultFreeOp(), script_);
            script_->isActiveEval = false;
            script_->isCachedEval = true;
            EvalCacheEntry cacheEntry = {script_, lookup_.callerScript, lookup_.pc};
            lookup_.str = lookupStr_;
            if (lookup_.str && IsEvalCacheCandidate(script_))
                cx_->runtime->evalCache.relookupOrAdd(p_, lookup_, cacheEntry);
        }
    }

    void lookupInEvalCache(JSLinearString *str, JSScript *callerScript, jsbytecode *pc)
    {
        lookupStr_ = str;
        lookup_.str = str;
        lookup_.callerScript = callerScript;
        lookup_.version = cx_->findVersion();
        lookup_.pc = pc;
        p_ = cx_->runtime->evalCache.lookupForAdd(lookup_);
        if (p_) {
            script_ = p_->script;
            cx_->runtime->evalCache.remove(p_);
            CallNewScriptHook(cx_, script_, NullPtr());
            script_->isCachedEval = false;
            script_->isActiveEval = true;
        }
    }

    void setNewScript(JSScript *script) {
        // JSScript::initFromEmitter has already called js_CallNewScriptHook.
        JS_ASSERT(!script_ && script);
        script_ = script;
        script_->isActiveEval = true;
    }

    bool foundScript() {
        return !!script_;
    }

    HandleScript script() {
        JS_ASSERT(script_);
        return script_;
    }
};

enum EvalJSONResult {
    EvalJSON_Failure,
    EvalJSON_Success,
    EvalJSON_NotJSON
};

static EvalJSONResult
TryEvalJSON(JSContext *cx, JSScript *callerScript,
            StableCharPtr chars, size_t length, MutableHandleValue rval)
{
    // If the eval string starts with '(' or '[' and ends with ')' or ']', it may be JSON.
    // Try the JSON parser first because it's much faster.  If the eval string
    // isn't JSON, JSON parsing will probably fail quickly, so little time
    // will be lost.
    //
    // Don't use the JSON parser if the caller is strict mode code, because in
    // strict mode object literals must not have repeated properties, and the
    // JSON parser cheerfully (and correctly) accepts them.  If you're parsing
    // JSON with eval and using strict mode, you deserve to be slow.
    if (length > 2 &&
        ((chars[0] == '[' && chars[length - 1] == ']') ||
        (chars[0] == '(' && chars[length - 1] == ')')) &&
         (!callerScript || !callerScript->strict))
    {
        // Remarkably, JavaScript syntax is not a superset of JSON syntax:
        // strings in JavaScript cannot contain the Unicode line and paragraph
        // terminator characters U+2028 and U+2029, but strings in JSON can.
        // Rather than force the JSON parser to handle this quirk when used by
        // eval, we simply don't use the JSON parser when either character
        // appears in the provided string.  See bug 657367.
        for (const jschar *cp = &chars[1], *end = &chars[length - 2]; ; cp++) {
            if (*cp == 0x2028 || *cp == 0x2029)
                break;

            if (cp == end) {
                bool isArray = (chars[0] == '[');
                JSONParser parser(cx, isArray ? chars : chars + 1U, isArray ? length : length - 2,
                                  JSONParser::NoError);
                RootedValue tmp(cx);
                if (!parser.parse(&tmp))
                    return EvalJSON_Failure;
                if (tmp.isUndefined())
                    return EvalJSON_NotJSON;
                rval.set(tmp);
                return EvalJSON_Success;
            }
        }
    }
    return EvalJSON_NotJSON;
}

static void
MarkFunctionsWithinEvalScript(JSScript *script)
{
    // Mark top level functions in an eval script as being within an eval and,
    // if applicable, inside a with statement.

    if (!script->hasObjects())
        return;

    ObjectArray *objects = script->objects();
    size_t start = script->innerObjectsStart();

    for (size_t i = start; i < objects->length; i++) {
        JSObject *obj = objects->vector[i];
        if (obj->isFunction()) {
            JSFunction *fun = obj->toFunction();
            if (fun->hasScript())
                fun->nonLazyScript()->directlyInsideEval = true;
            else if (fun->isInterpretedLazy())
                fun->lazyScript()->setDirectlyInsideEval();
        }
    }
}

// Define subset of ExecuteType so that casting performs the injection.
enum EvalType { DIRECT_EVAL = EXECUTE_DIRECT_EVAL, INDIRECT_EVAL = EXECUTE_INDIRECT_EVAL };

// Common code implementing direct and indirect eval.
//
// Evaluate call.argv[2], if it is a string, in the context of the given calling
// frame, with the provided scope chain, with the semantics of either a direct
// or indirect eval (see ES5 10.4.2).  If this is an indirect eval, scopeobj
// must be a global object.
//
// On success, store the completion value in call.rval and return true.
static bool
EvalKernel(JSContext *cx, const CallArgs &args, EvalType evalType, AbstractFramePtr caller,
           HandleObject scopeobj, jsbytecode *pc)
{
    JS_ASSERT((evalType == INDIRECT_EVAL) == !caller);
    JS_ASSERT((evalType == INDIRECT_EVAL) == !pc);
    JS_ASSERT_IF(evalType == INDIRECT_EVAL, scopeobj->isGlobal());
    AssertInnerizedScopeChain(cx, *scopeobj);

    Rooted<GlobalObject*> scopeObjGlobal(cx, &scopeobj->global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, scopeObjGlobal)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_EVAL);
        return false;
    }

    // ES5 15.1.2.1 step 1.
    if (args.length() < 1) {
        args.rval().setUndefined();
        return true;
    }
    if (!args[0].isString()) {
        args.rval().set(args[0]);
        return true;
    }
    RootedString str(cx, args[0].toString());

    // ES5 15.1.2.1 steps 2-8.

    // Per ES5, indirect eval runs in the global scope. (eval is specified this
    // way so that the compiler can make assumptions about what bindings may or
    // may not exist in the current frame if it doesn't see 'eval'.)
    unsigned staticLevel;
    RootedValue thisv(cx);
    if (evalType == DIRECT_EVAL) {
        JS_ASSERT_IF(caller.isStackFrame(), !caller.asStackFrame()->runningInIon());
        staticLevel = caller.script()->staticLevel + 1;

        // Direct calls to eval are supposed to see the caller's |this|. If we
        // haven't wrapped that yet, do so now, before we make a copy of it for
        // the eval code to use.
        if (!ComputeThis(cx, caller))
            return false;
        thisv = caller.thisValue();
    } else {
        JS_ASSERT(args.callee().global() == *scopeobj);
        staticLevel = 0;

        // Use the global as 'this', modulo outerization.
        JSObject *thisobj = JSObject::thisObject(cx, scopeobj);
        if (!thisobj)
            return false;
        thisv = ObjectValue(*thisobj);
    }

    Rooted<JSStableString*> stableStr(cx, str->ensureStable(cx));
    if (!stableStr)
        return false;

    StableCharPtr chars = stableStr->chars();
    size_t length = stableStr->length();

    JSPrincipals *principals = PrincipalsForCompiledCode(args, cx);

    RootedScript callerScript(cx, caller ? caller.script() : NULL);
    EvalJSONResult ejr = TryEvalJSON(cx, callerScript, chars, length, args.rval());
    if (ejr != EvalJSON_NotJSON)
        return ejr == EvalJSON_Success;

    EvalScriptGuard esg(cx);

    if (evalType == DIRECT_EVAL && caller.isNonEvalFunctionFrame())
        esg.lookupInEvalCache(stableStr, callerScript, pc);

    if (!esg.foundScript()) {
        unsigned lineno;
        const char *filename;
        JSPrincipals *originPrincipals;
        CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals,
                                    evalType == DIRECT_EVAL ? CALLED_FROM_JSOP_EVAL
                                                            : NOT_CALLED_FROM_JSOP_EVAL);

        CompileOptions options(cx);
        options.setFileAndLine(filename, lineno)
               .setCompileAndGo(true)
               .setForEval(true)
               .setNoScriptRval(false)
               .setPrincipals(principals)
               .setOriginPrincipals(originPrincipals);
        JSScript *compiled = frontend::CompileScript(cx, scopeobj, callerScript, options,
                                                     chars.get(), length, stableStr, staticLevel);
        if (!compiled)
            return false;

        MarkFunctionsWithinEvalScript(compiled);

        esg.setNewScript(compiled);
    }

    return ExecuteKernel(cx, esg.script(), *scopeobj, thisv, ExecuteType(evalType),
                         NullFramePtr() /* evalInFrame */, args.rval().address());
}

bool
js::DirectEvalFromIon(JSContext *cx,
                      HandleObject scopeobj, HandleScript callerScript,
                      HandleValue thisValue, HandleString str,
                      jsbytecode *pc, MutableHandleValue vp)
{
    AssertInnerizedScopeChain(cx, *scopeobj);

    Rooted<GlobalObject*> scopeObjGlobal(cx, &scopeobj->global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, scopeObjGlobal)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_EVAL);
        return false;
    }

    // ES5 15.1.2.1 steps 2-8.

    unsigned staticLevel = callerScript->staticLevel + 1;

    Rooted<JSStableString*> stableStr(cx, str->ensureStable(cx));
    if (!stableStr)
        return false;

    StableCharPtr chars = stableStr->chars();
    size_t length = stableStr->length();

    EvalJSONResult ejr = TryEvalJSON(cx, callerScript, chars, length, vp);
    if (ejr != EvalJSON_NotJSON)
        return ejr == EvalJSON_Success;

    EvalScriptGuard esg(cx);

    // Ion will not perform cross compartment direct eval calls.
    JSPrincipals *principals = cx->compartment->principals;

    esg.lookupInEvalCache(stableStr, callerScript, pc);

    if (!esg.foundScript()) {
        unsigned lineno;
        const char *filename;
        JSPrincipals *originPrincipals;
        CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals,
                                    CALLED_FROM_JSOP_EVAL);

        CompileOptions options(cx);
        options.setFileAndLine(filename, lineno)
               .setCompileAndGo(true)
               .setForEval(true)
               .setNoScriptRval(false)
               .setPrincipals(principals)
               .setOriginPrincipals(originPrincipals);
        JSScript *compiled = frontend::CompileScript(cx, scopeobj, callerScript, options,
                                                     chars.get(), length, stableStr, staticLevel);
        if (!compiled)
            return false;

        MarkFunctionsWithinEvalScript(compiled);

        esg.setNewScript(compiled);
    }

    // Primitive 'this' values should have been filtered out by Ion. If boxed,
    // the calling frame cannot be updated to store the new object.
    JS_ASSERT(thisValue.isObject() || thisValue.isUndefined() || thisValue.isNull());

    return ExecuteKernel(cx, esg.script(), *scopeobj, thisValue, ExecuteType(DIRECT_EVAL),
                         NullFramePtr() /* evalInFrame */, vp.address());
}

// We once supported a second argument to eval to use as the scope chain
// when evaluating the code string.  Warn when such uses are seen so that
// authors will know that support for eval(s, o) has been removed.
static inline bool
WarnOnTooManyArgs(JSContext *cx, const CallArgs &args)
{
    if (args.length() > 1) {
        Rooted<JSScript*> script(cx, cx->stack.currentScript());
        if (script && !script->warnedAboutTwoArgumentEval) {
            static const char TWO_ARGUMENT_WARNING[] =
                "Support for eval(code, scopeObject) has been removed. "
                "Use |with (scopeObject) eval(code);| instead.";
            if (!JS_ReportWarning(cx, TWO_ARGUMENT_WARNING))
                return false;
            script->warnedAboutTwoArgumentEval = true;
        } else {
            // In the case of an indirect call without a caller frame, avoid a
            // potential warning-flood by doing nothing.
        }
    }

    return true;
}

JSBool
js::IndirectEval(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!WarnOnTooManyArgs(cx, args))
        return false;

    Rooted<GlobalObject*> global(cx, &args.callee().global());
    return EvalKernel(cx, args, INDIRECT_EVAL, NullFramePtr(), global, NULL);
}

bool
js::DirectEval(JSContext *cx, const CallArgs &args)
{
    // Direct eval can assume it was called from an interpreted or baseline frame.
    ScriptFrameIter iter(cx);
    AbstractFramePtr caller = iter.abstractFramePtr();

    JS_ASSERT(IsBuiltinEvalForScope(caller.scopeChain(), args.calleev()));
    JS_ASSERT(JSOp(*iter.pc()) == JSOP_EVAL);
    JS_ASSERT_IF(caller.isFunctionFrame(),
                 caller.compartment() == caller.callee()->compartment());

    if (!WarnOnTooManyArgs(cx, args))
        return false;

    RootedObject scopeChain(cx, caller.scopeChain());
    return EvalKernel(cx, args, DIRECT_EVAL, caller, scopeChain, iter.pc());
}

bool
js::IsBuiltinEvalForScope(JSObject *scopeChain, const Value &v)
{
    return scopeChain->global().getOriginalEval() == v;
}

bool
js::IsAnyBuiltinEval(JSFunction *fun)
{
    return fun->maybeNative() == IndirectEval;
}

JSPrincipals *
js::PrincipalsForCompiledCode(const CallReceiver &call, JSContext *cx)
{
    JS_ASSERT(IsAnyBuiltinEval(call.callee().toFunction()) ||
              IsBuiltinFunctionConstructor(call.callee().toFunction()));

    // To compute the principals of the compiled eval/Function code, we simply
    // use the callee's principals. To see why the caller's principals are
    // ignored, consider first that, in the capability-model we assume, the
    // high-privileged eval/Function should never have escaped to the
    // low-privileged caller. (For the Mozilla embedding, this is brute-enforced
    // by explicit filtering by wrappers.) Thus, the caller's privileges should
    // subsume the callee's.
    //
    // In the converse situation, where the callee has lower privileges than the
    // caller, we might initially guess that the caller would want to retain
    // their higher privileges in the generated code. However, since the
    // compiled code will be run with the callee's scope chain, this would make
    // fp->script()->compartment() != fp->compartment().

    return call.callee().compartment()->principals;
}
