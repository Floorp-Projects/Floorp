/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Eval.h"

#include "mozilla/HashFunctions.h"

#include "jscntxt.h"
#include "jsonparser.h"

#include "frontend/BytecodeCompiler.h"
#include "vm/GlobalObject.h"

#include "vm/Interpreter-inl.h"

using namespace js;

using mozilla::AddToHash;
using mozilla::HashString;

// We should be able to assert this for *any* fp->scopeChain().
static void
AssertInnerizedScopeChain(JSContext *cx, JSObject &scopeobj)
{
#ifdef DEBUG
    RootedObject obj(cx);
    for (obj = &scopeobj; obj; obj = obj->enclosingScope())
        JS_ASSERT(GetInnerObject(obj) == obj);
#endif
}

static bool
IsEvalCacheCandidate(JSScript *script)
{
    // Make sure there are no inner objects which might use the wrong parent
    // and/or call scope by reusing the previous eval's script. Skip the
    // script's first object, which entrains the eval's scope.
    return script->savedCallerFun() &&
           !script->hasSingletons() &&
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
//  1. notify OldDebugAPI about script creation/destruction
//  2. add the script to the eval cache when EvalKernel is finished
//
// NB: Although the eval cache keeps a script alive wrt to the JS engine, from
// an OldDebugAPI  user's perspective, we want each eval() to create and
// destroy a script. This hides implementation details and means we don't have
// to deal with calls to JS_GetScriptObject for scripts in the eval cache.
class EvalScriptGuard
{
    JSContext *cx_;
    Rooted<JSScript*> script_;

    /* These fields are only valid if lookup_.str is non-nullptr. */
    EvalCacheLookup lookup_;
    EvalCache::AddPtr p_;

    Rooted<JSLinearString*> lookupStr_;

  public:
    EvalScriptGuard(JSContext *cx)
        : cx_(cx), script_(cx), lookup_(cx), lookupStr_(cx) {}

    ~EvalScriptGuard() {
        if (script_) {
            CallDestroyScriptHook(cx_->runtime()->defaultFreeOp(), script_);
            script_->cacheForEval();
            EvalCacheEntry cacheEntry = {script_, lookup_.callerScript, lookup_.pc};
            lookup_.str = lookupStr_;
            if (lookup_.str && IsEvalCacheCandidate(script_))
                cx_->runtime()->evalCache.relookupOrAdd(p_, lookup_, cacheEntry);
        }
    }

    void lookupInEvalCache(JSLinearString *str, JSScript *callerScript, jsbytecode *pc)
    {
        lookupStr_ = str;
        lookup_.str = str;
        lookup_.callerScript = callerScript;
        lookup_.version = cx_->findVersion();
        lookup_.pc = pc;
        p_ = cx_->runtime()->evalCache.lookupForAdd(lookup_);
        if (p_) {
            script_ = p_->script;
            cx_->runtime()->evalCache.remove(p_);
            CallNewScriptHook(cx_, script_, NullPtr());
            script_->uncacheForEval();
        }
    }

    void setNewScript(JSScript *script) {
        // JSScript::initFromEmitter has already called js_CallNewScriptHook.
        JS_ASSERT(!script_ && script);
        script_ = script;
        script_->setActiveEval();
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
            ConstTwoByteChars chars, size_t length, MutableHandleValue rval)
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
        (!callerScript || !callerScript->strict()))
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
    JS_ASSERT_IF(evalType == INDIRECT_EVAL, scopeobj->is<GlobalObject>());
    AssertInnerizedScopeChain(cx, *scopeobj);

    Rooted<GlobalObject*> scopeObjGlobal(cx, &scopeobj->global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, scopeObjGlobal)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CSP_BLOCKED_EVAL);
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
        JS_ASSERT_IF(caller.isInterpreterFrame(), !caller.asInterpreterFrame()->runningInJit());
        staticLevel = caller.script()->staticLevel() + 1;

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

    Rooted<JSFlatString*> flatStr(cx, str->ensureFlat(cx));
    if (!flatStr)
        return false;

    size_t length = flatStr->length();
    ConstTwoByteChars chars(flatStr->chars(), length);

    RootedScript callerScript(cx, caller ? caller.script() : nullptr);
    EvalJSONResult ejr = TryEvalJSON(cx, callerScript, chars, length, args.rval());
    if (ejr != EvalJSON_NotJSON)
        return ejr == EvalJSON_Success;

    EvalScriptGuard esg(cx);

    if (evalType == DIRECT_EVAL && caller.isNonEvalFunctionFrame())
        esg.lookupInEvalCache(flatStr, callerScript, pc);

    if (!esg.foundScript()) {
        JSScript *maybeScript;
        unsigned lineno;
        const char *filename;
        JSPrincipals *originPrincipals;
        uint32_t pcOffset;
        DescribeScriptedCallerForCompilation(cx, &maybeScript, &filename, &lineno, &pcOffset,
                                             &originPrincipals,
                                             evalType == DIRECT_EVAL
                                             ? CALLED_FROM_JSOP_EVAL
                                             : NOT_CALLED_FROM_JSOP_EVAL);

        const char *introducerFilename = filename;
        if (maybeScript && maybeScript->scriptSource()->introducerFilename())
            introducerFilename = maybeScript->scriptSource()->introducerFilename();

        CompileOptions options(cx);
        options.setFileAndLine(filename, 1)
               .setCompileAndGo(true)
               .setForEval(true)
               .setNoScriptRval(false)
               .setOriginPrincipals(originPrincipals)
               .setIntroductionInfo(introducerFilename, "eval", lineno, maybeScript, pcOffset);
        SourceBufferHolder srcBuf(chars.get(), length, SourceBufferHolder::NoOwnership);
        JSScript *compiled = frontend::CompileScript(cx, &cx->tempLifoAlloc(),
                                                     scopeobj, callerScript, options,
                                                     srcBuf, flatStr, staticLevel);
        if (!compiled)
            return false;

        esg.setNewScript(compiled);
    }

    return ExecuteKernel(cx, esg.script(), *scopeobj, thisv, ExecuteType(evalType),
                         NullFramePtr() /* evalInFrame */, args.rval().address());
}

bool
js::DirectEvalStringFromIon(JSContext *cx,
                            HandleObject scopeobj, HandleScript callerScript,
                            HandleValue thisValue, HandleString str,
                            jsbytecode *pc, MutableHandleValue vp)
{
    AssertInnerizedScopeChain(cx, *scopeobj);

    Rooted<GlobalObject*> scopeObjGlobal(cx, &scopeobj->global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, scopeObjGlobal)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_CSP_BLOCKED_EVAL);
        return false;
    }

    // ES5 15.1.2.1 steps 2-8.

    unsigned staticLevel = callerScript->staticLevel() + 1;

    Rooted<JSFlatString*> flatStr(cx, str->ensureFlat(cx));
    if (!flatStr)
        return false;

    size_t length = flatStr->length();
    ConstTwoByteChars chars(flatStr->chars(), length);

    EvalJSONResult ejr = TryEvalJSON(cx, callerScript, chars, length, vp);
    if (ejr != EvalJSON_NotJSON)
        return ejr == EvalJSON_Success;

    EvalScriptGuard esg(cx);

    esg.lookupInEvalCache(flatStr, callerScript, pc);

    if (!esg.foundScript()) {
        JSScript *maybeScript;
        const char *filename;
        unsigned lineno;
        JSPrincipals *originPrincipals;
        uint32_t pcOffset;
        DescribeScriptedCallerForCompilation(cx, &maybeScript, &filename, &lineno, &pcOffset,
                                              &originPrincipals, CALLED_FROM_JSOP_EVAL);

        const char *introducerFilename = filename;
        if (maybeScript && maybeScript->scriptSource()->introducerFilename())
            introducerFilename = maybeScript->scriptSource()->introducerFilename();

        CompileOptions options(cx);
        options.setFileAndLine(filename, 1)
               .setCompileAndGo(true)
               .setForEval(true)
               .setNoScriptRval(false)
               .setOriginPrincipals(originPrincipals)
               .setIntroductionInfo(introducerFilename, "eval", lineno, maybeScript, pcOffset);
        SourceBufferHolder srcBuf(chars.get(), length, SourceBufferHolder::NoOwnership);
        JSScript *compiled = frontend::CompileScript(cx, &cx->tempLifoAlloc(),
                                                     scopeobj, callerScript, options,
                                                     srcBuf, flatStr, staticLevel);
        if (!compiled)
            return false;

        esg.setNewScript(compiled);
    }

    // Primitive 'this' values should have been filtered out by Ion. If boxed,
    // the calling frame cannot be updated to store the new object.
    JS_ASSERT(thisValue.isObject() || thisValue.isUndefined() || thisValue.isNull());

    return ExecuteKernel(cx, esg.script(), *scopeobj, thisValue, ExecuteType(DIRECT_EVAL),
                         NullFramePtr() /* evalInFrame */, vp.address());
}

bool
js::DirectEvalValueFromIon(JSContext *cx,
                           HandleObject scopeobj, HandleScript callerScript,
                           HandleValue thisValue, HandleValue evalArg,
                           jsbytecode *pc, MutableHandleValue vp)
{
    // Act as identity on non-strings per ES5 15.1.2.1 step 1.
    if (!evalArg.isString()) {
        vp.set(evalArg);
        return true;
    }

    RootedString string(cx, evalArg.toString());
    return DirectEvalStringFromIon(cx, scopeobj, callerScript, thisValue, string, pc, vp);
}

bool
js::IndirectEval(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Rooted<GlobalObject*> global(cx, &args.callee().global());
    return EvalKernel(cx, args, INDIRECT_EVAL, NullFramePtr(), global, nullptr);
}

bool
js::DirectEval(JSContext *cx, const CallArgs &args)
{
    // Direct eval can assume it was called from an interpreted or baseline frame.
    ScriptFrameIter iter(cx);
    AbstractFramePtr caller = iter.abstractFramePtr();

    JS_ASSERT(caller.scopeChain()->global().valueIsEval(args.calleev()));
    JS_ASSERT(JSOp(*iter.pc()) == JSOP_EVAL ||
              JSOp(*iter.pc()) == JSOP_SPREADEVAL);
    JS_ASSERT_IF(caller.isFunctionFrame(),
                 caller.compartment() == caller.callee()->compartment());

    RootedObject scopeChain(cx, caller.scopeChain());
    return EvalKernel(cx, args, DIRECT_EVAL, caller, scopeChain, iter.pc());
}

bool
js::IsAnyBuiltinEval(JSFunction *fun)
{
    return fun->maybeNative() == IndirectEval;
}
