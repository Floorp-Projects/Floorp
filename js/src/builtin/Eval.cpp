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
#include "mozilla/HashFunctions.h"
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
                     l.caller,
                     l.staticLevel,
                     l.version,
                     l.compartment);
}

/* static */ bool
EvalCacheHashPolicy::match(JSScript *script, const EvalCacheLookup &l)
{
    JS_ASSERT(IsEvalCacheCandidate(script));

    // Get the source string passed for safekeeping in the atom map
    // by the prior eval to frontend::CompileScript.
    JSAtom *keyStr = script->atoms[0];

    return EqualStrings(keyStr, l.str) &&
           script->getCallerFunction() == l.caller &&
           script->staticLevel == l.staticLevel &&
           script->getVersion() == l.version &&
           script->compartment() == l.compartment;
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
    JSContext *context;
    Rooted<JSScript*> script_;

    /* Components of the EvalCacheLookup for this eval. */
    Rooted<JSLinearString*> str;
    RootedFunction caller;
    unsigned staticLevel;

  public:
    EvalScriptGuard(JSContext *cx)
      : context(cx), script_(cx),
        str(cx), caller(cx), staticLevel(0)
    {}

    ~EvalScriptGuard() {
        if (script_) {
            CallDestroyScriptHook(context->runtime->defaultFreeOp(), script_);
            script_->isActiveEval = false;
            script_->isCachedEval = true;
            if (str && IsEvalCacheCandidate(script_)) {
                EvalCacheLookup key(str, caller, staticLevel,
                                    context->findVersion(), context->compartment);
                EvalCache::AddPtr p = context->runtime->evalCache.lookupForAdd(key);
                if (p)
                    context->runtime->evalCache.add(p, script_);
            }
        }
    }

    void lookupInEvalCache(JSLinearString *str, JSFunction *caller, unsigned staticLevel)
    {
        this->str = str;
        this->caller = caller;
        this->staticLevel = staticLevel;
        EvalCacheLookup key(str, caller, staticLevel,
                            context->findVersion(), context->compartment);
        EvalCache::Ptr p = context->runtime->evalCache.lookupForAdd(key);
        if (p) {
            script_ = *p;
            context->runtime->evalCache.remove(p);
            js_CallNewScriptHook(context, script_, NULL);
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

    EvalScriptGuard esg(cx);

    JSPrincipals *principals = PrincipalsForCompiledCode(args, cx);

    if (evalType == DIRECT_EVAL && caller->isNonEvalFunctionFrame())
        esg.lookupInEvalCache(linearStr, caller->fun(), staticLevel);

    if (!esg.foundScript()) {
        unsigned lineno;
        const char *filename;
        JSPrincipals *originPrincipals;
        CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals,
                                    evalType == DIRECT_EVAL ? CALLED_FROM_JSOP_EVAL
                                                            : NOT_CALLED_FROM_JSOP_EVAL);

        bool compileAndGo = true;
        bool noScriptRval = false;
        JSScript *compiled = frontend::CompileScript(cx, scopeobj, caller,
                                                     principals, originPrincipals,
                                                     compileAndGo, noScriptRval,
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
