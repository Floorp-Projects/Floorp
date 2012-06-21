/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jsonparser.h"

#include "builtin/Eval.h"
#include "frontend/BytecodeCompiler.h"
#include "vm/GlobalObject.h"

#include "jsinterpinlines.h"

using namespace js;

// We should be able to assert this for *any* fp->scopeChain().
static void
AssertInnerizedScopeChain(JSContext *cx, JSObject &scopeobj)
{
#ifdef DEBUG
    for (JSObject *o = &scopeobj; o; o = o->enclosingScope()) {
        if (JSObjectOp op = o->getClass()->ext.innerObject) {
            Rooted<JSObject*> obj(cx, o);
            JS_ASSERT(op(cx, obj) == o);
        }
    }
#endif
}

void
EvalCache::purge()
{
    // Purge all scripts from the eval cache. In addition to removing them from
    // table_, null out the evalHashLink field of any script removed. Since
    // evalHashLink is in a union with globalObject, this allows the GC to
    // indiscriminately use the union as a nullable globalObject pointer.
    for (size_t i = 0; i < ArrayLength(table_); ++i) {
        for (JSScript **listHeadp = &table_[i]; *listHeadp; ) {
            JSScript *script = *listHeadp;
            JS_ASSERT(GetGCThingTraceKind(script) == JSTRACE_SCRIPT);
            *listHeadp = script->evalHashLink();
            script->evalHashLink() = NULL;
        }
    }
}

JSScript **
EvalCache::bucket(JSLinearString *str)
{
    const jschar *s = str->chars();
    size_t n = str->length();

    if (n > 100)
        n = 100;
    uint32_t h;
    for (h = 0; n; s++, n--)
        h = JS_ROTATE_LEFT32(h, 4) ^ *s;

    h *= JS_GOLDEN_RATIO;
    h >>= 32 - SHIFT;
    JS_ASSERT(h < ArrayLength(table_));
    return &table_[h];
}

static JSScript *
EvalCacheLookup(JSContext *cx, JSLinearString *str, StackFrame *caller, unsigned staticLevel,
                JSPrincipals *principals, JSObject &scopeobj, JSScript **bucket)
{
    // Cache local eval scripts indexed by source qualified by scope.
    // 
    // An eval cache entry should never be considered a hit unless its
    // strictness matches that of the new eval code. The existing code takes
    // care of this, because hits are qualified by the function from which
    // eval was called, whose strictness doesn't change. (We don't cache evals
    // in eval code, so the calling function corresponds to the calling script,
    // and its strictness never varies.) Scripts produced by calls to eval from
    // global code aren't cached.
    // 
    // FIXME bug 620141: Qualify hits by calling script rather than function.
    // Then we wouldn't need the unintuitive !isEvalFrame() hack in EvalKernel
    // to avoid caching nested evals in functions (thus potentially mismatching
    // on strict mode), and we could cache evals in global code if desired.
    unsigned count = 0;
    JSScript **scriptp = bucket;

    JSVersion version = cx->findVersion();
    JSScript *script;
    JSSubsumePrincipalsOp subsume = cx->runtime->securityCallbacks->subsumePrincipals;
    while ((script = *scriptp) != NULL) {
        if (script->savedCallerFun &&
            script->staticLevel == staticLevel &&
            script->getVersion() == version &&
            !script->hasSingletons &&
            (!subsume || script->principals == principals ||
             (subsume(principals, script->principals) &&
              subsume(script->principals, principals))))
        {
            // Get the prior (cache-filling) eval's saved caller function.
            // See frontend::CompileScript.
            JSFunction *fun = script->getCallerFunction();

            if (fun == caller->fun()) {
                /*
                 * Get the source string passed for safekeeping in the atom map
                 * by the prior eval to frontend::CompileScript.
                 */
                JSAtom *src = script->atoms[0];

                if (src == str || EqualStrings(src, str)) {
                    // Source matches. Make sure there are no inner objects
                    // which might use the wrong parent and/or call scope by
                    // reusing the previous eval's script. Skip the script's
                    // first object, which entrains the eval's scope.
                    JS_ASSERT(script->objects()->length >= 1);
                    if (script->objects()->length == 1 &&
                        !script->hasRegexps()) {
                        JS_ASSERT(staticLevel == script->staticLevel);
                        *scriptp = script->evalHashLink();
                        script->evalHashLink() = NULL;
                        return script;
                    }
                }
            }
        }

        static const unsigned EVAL_CACHE_CHAIN_LIMIT = 4;
        if (++count == EVAL_CACHE_CHAIN_LIMIT)
            return NULL;
        scriptp = &script->evalHashLink();
    }
    return NULL;
}

// There are two things we want to do with each script executed in EvalKernel:
//  1. notify jsdbgapi about script creation/destruction
//  2. add the script to the eval cache when EvalKernel is finished
//
// NB: Although the eval cache keeps a script alive wrt to the JS engine, from
// a jsdbgapi user's perspective, we want each eval() to create and destroy a
// script. This hides implementation details and means we don't have to deal
// with calls to JS_GetScriptObject for scripts in the eval cache (currently,
// script->object aliases script->evalHashLink()).
class EvalScriptGuard
{
    JSContext *cx_;
    JSLinearString *str_;
    JSScript **bucket_;
    Rooted<JSScript*> script_;

  public:
    EvalScriptGuard(JSContext *cx, JSLinearString *str)
      : cx_(cx),
        str_(str),
        script_(cx) {
        bucket_ = cx->runtime->evalCache.bucket(str);
    }

    ~EvalScriptGuard() {
        if (script_) {
            CallDestroyScriptHook(cx_->runtime->defaultFreeOp(), script_);
            script_->isActiveEval = false;
            script_->isCachedEval = true;
            script_->evalHashLink() = *bucket_;
            *bucket_ = script_;
        }
    }

    void lookupInEvalCache(StackFrame *caller, unsigned staticLevel,
                           JSPrincipals *principals, JSObject &scopeobj) {
        if (JSScript *found = EvalCacheLookup(cx_, str_, caller, staticLevel,
                                              principals, scopeobj, bucket_)) {
            js_CallNewScriptHook(cx_, found, NULL);
            script_ = found;
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

    JSScript *script() const {
        JS_ASSERT(script_);
        return script_;
    }
};

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
EvalKernel(JSContext *cx, const CallArgs &args, EvalType evalType, StackFrame *caller,
           HandleObject scopeobj)
{
    JS_ASSERT((evalType == INDIRECT_EVAL) == (caller == NULL));
    JS_ASSERT_IF(evalType == INDIRECT_EVAL, scopeobj->isGlobal());
    AssertInnerizedScopeChain(cx, *scopeobj);

    if (!scopeobj->global().isRuntimeCodeGenEnabled(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_EVAL);
        return false;
    }

    // ES5 15.1.2.1 step 1.
    if (args.length() < 1) {
        args.rval().setUndefined();
        return true;
    }
    if (!args[0].isString()) {
        args.rval() = args[0];
        return true;
    }
    JSString *str = args[0].toString();

    // ES5 15.1.2.1 steps 2-8.

    // Per ES5, indirect eval runs in the global scope. (eval is specified this
    // way so that the compiler can make assumptions about what bindings may or
    // may not exist in the current frame if it doesn't see 'eval'.)
    unsigned staticLevel;
    RootedValue thisv(cx);
    if (evalType == DIRECT_EVAL) {
        staticLevel = caller->script()->staticLevel + 1;

        // Direct calls to eval are supposed to see the caller's |this|. If we
        // haven't wrapped that yet, do so now, before we make a copy of it for
        // the eval code to use.
        if (!ComputeThis(cx, caller))
            return false;
        thisv = caller->thisValue();

#ifdef DEBUG
        jsbytecode *callerPC = caller->pcQuadratic(cx);
        JS_ASSERT(callerPC && JSOp(*callerPC) == JSOP_EVAL);
#endif
    } else {
        JS_ASSERT(args.callee().global() == *scopeobj);
        staticLevel = 0;

        // Use the global as 'this', modulo outerization.
        JSObject *thisobj = scopeobj->thisObject(cx);
        if (!thisobj)
            return false;
        thisv = ObjectValue(*thisobj);
    }

    Rooted<JSLinearString*> linearStr(cx, str->ensureLinear(cx));
    if (!linearStr)
        return false;
    const jschar *chars = linearStr->chars();
    size_t length = linearStr->length();

    SkipRoot skip(cx, &chars);

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
         (!caller || !caller->script()->strictModeCode))
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
                JSONParser parser(cx, isArray ? chars : chars + 1, isArray ? length : length - 2,
                                  JSONParser::StrictJSON, JSONParser::NoError);
                Value tmp;
                if (!parser.parse(&tmp))
                    return false;
                if (tmp.isUndefined())
                    break;
                args.rval() = tmp;
                return true;
            }
        }
    }

    EvalScriptGuard esg(cx, linearStr);

    JSPrincipals *principals = PrincipalsForCompiledCode(args, cx);

    if (evalType == DIRECT_EVAL && caller->isNonEvalFunctionFrame())
        esg.lookupInEvalCache(caller, staticLevel, principals, *scopeobj);

    if (!esg.foundScript()) {
        unsigned lineno;
        const char *filename;
        JSPrincipals *originPrincipals;
        CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals,
                                    evalType == DIRECT_EVAL ? CALLED_FROM_JSOP_EVAL
                                                            : NOT_CALLED_FROM_JSOP_EVAL);

        bool compileAndGo = true;
        bool noScriptRval = false;
        bool needScriptGlobal = false;
        JSScript *compiled = frontend::CompileScript(cx, scopeobj, caller,
                                                     principals, originPrincipals,
                                                     compileAndGo, noScriptRval, needScriptGlobal,
                                                     chars, length, filename,
                                                     lineno, cx->findVersion(), linearStr,
                                                     staticLevel);
        if (!compiled)
            return false;

        esg.setNewScript(compiled);
    }

    return ExecuteKernel(cx, esg.script(), *scopeobj, thisv, ExecuteType(evalType),
                         NULL /* evalInFrame */, &args.rval());
}

// We once supported a second argument to eval to use as the scope chain
// when evaluating the code string.  Warn when such uses are seen so that
// authors will know that support for eval(s, o) has been removed.
static inline bool
WarnOnTooManyArgs(JSContext *cx, const CallArgs &args)
{
    if (args.length() > 1) {
        if (JSScript *script = cx->stack.currentScript()) {
            if (!script->warnedAboutTwoArgumentEval) {
                static const char TWO_ARGUMENT_WARNING[] =
                    "Support for eval(code, scopeObject) has been removed. "
                    "Use |with (scopeObject) eval(code);| instead.";
                if (!JS_ReportWarning(cx, TWO_ARGUMENT_WARNING))
                    return false;
                script->warnedAboutTwoArgumentEval = true;
            }
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
    return EvalKernel(cx, args, INDIRECT_EVAL, NULL, global);
}

bool
js::DirectEval(JSContext *cx, const CallArgs &args)
{
    // Direct eval can assume it was called from an interpreted frame.
    StackFrame *caller = cx->fp();
    JS_ASSERT(caller->isScriptFrame());
    JS_ASSERT(IsBuiltinEvalForScope(caller->scopeChain(), args.calleev()));
    JS_ASSERT(JSOp(*cx->regs().pc) == JSOP_EVAL);

    if (!WarnOnTooManyArgs(cx, args))
        return false;

    return EvalKernel(cx, args, DIRECT_EVAL, caller, caller->scopeChain());
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

    return call.callee().principals(cx);
}
