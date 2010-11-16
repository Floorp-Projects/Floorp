/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS function support.
 */
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdbgapi.h"
#include "jsemit.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jspropertytree.h"
#include "jsproxy.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsexn.h"
#include "jsstaticcheck.h"
#include "jstracer.h"

#if JS_HAS_GENERATORS
# include "jsiter.h"
#endif

#if JS_HAS_XDR
# include "jsxdrapi.h"
#endif

#ifdef JS_METHODJIT
#include "methodjit/MethodJIT.h"
#endif

#include "jsatominlines.h"
#include "jscntxtinlines.h"
#include "jsfuninlines.h"
#include "jsinterpinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;

inline JSObject *
JSObject::getThrowTypeError() const
{
    return &getGlobal()->getReservedSlot(JSRESERVED_GLOBAL_THROWTYPEERROR).toObject();
}

JSBool
js_GetArgsValue(JSContext *cx, JSStackFrame *fp, Value *vp)
{
    JSObject *argsobj;

    if (fp->hasOverriddenArgs()) {
        JS_ASSERT(fp->hasCallObj());
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
        return fp->callObj().getProperty(cx, id, vp);
    }
    argsobj = js_GetArgsObject(cx, fp);
    if (!argsobj)
        return JS_FALSE;
    vp->setObject(*argsobj);
    return JS_TRUE;
}

JSBool
js_GetArgsProperty(JSContext *cx, JSStackFrame *fp, jsid id, Value *vp)
{
    JS_ASSERT(fp->isFunctionFrame());

    if (fp->hasOverriddenArgs()) {
        JS_ASSERT(fp->hasCallObj());

        jsid argumentsid = ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
        Value v;
        if (!fp->callObj().getProperty(cx, argumentsid, &v))
            return false;

        JSObject *obj;
        if (v.isPrimitive()) {
            obj = js_ValueToNonNullObject(cx, v);
            if (!obj)
                return false;
        } else {
            obj = &v.toObject();
        }
        return obj->getProperty(cx, id, vp);
    }

    vp->setUndefined();
    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        JSObject *argsobj = fp->maybeArgsObj();
        if (arg < fp->numActualArgs()) {
            if (argsobj) {
                if (argsobj->getArgsElement(arg).isMagic(JS_ARGS_HOLE))
                    return argsobj->getProperty(cx, id, vp);
            }
            *vp = fp->canonicalActualArg(arg);
        } else {
            /*
             * Per ECMA-262 Ed. 3, 10.1.8, last bulleted item, do not share
             * storage between the formal parameter and arguments[k] for all
             * fp->argc <= k && k < fp->fun->nargs.  For example, in
             *
             *   function f(x) { x = 42; return arguments[0]; }
             *   f();
             *
             * the call to f should return undefined, not 42.  If fp->argsobj
             * is null at this point, as it would be in the example, return
             * undefined in *vp.
             */
            if (argsobj)
                return argsobj->getProperty(cx, id, vp);
        }
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        JSObject *argsobj = fp->maybeArgsObj();
        if (argsobj && argsobj->isArgsLengthOverridden())
            return argsobj->getProperty(cx, id, vp);
        vp->setInt32(fp->numActualArgs());
    }
    return true;
}

static JSObject *
NewArguments(JSContext *cx, JSObject *parent, uint32 argc, JSObject &callee)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
        return NULL;

    JS_STATIC_ASSERT(JSObject::ARGS_CLASS_RESERVED_SLOTS == 2);
    JSObject *argsobj = js_NewGCObject(cx, FINALIZE_OBJECT2);
    if (!argsobj)
        return NULL;

    ArgumentsData *data = (ArgumentsData *)
        cx->malloc(offsetof(ArgumentsData, slots) + argc * sizeof(Value));
    if (!data)
        return NULL;
    SetValueRangeToUndefined(data->slots, argc);

    /* Can't fail from here on, so initialize everything in argsobj. */
    argsobj->init(cx, callee.getFunctionPrivate()->inStrictMode()
                  ? &StrictArgumentsClass
                  : &js_ArgumentsClass,
                  proto, parent, NULL, false);

    argsobj->setMap(cx->runtime->emptyArgumentsShape);

    argsobj->setArgsLength(argc);
    argsobj->setArgsData(data);
    data->callee.setObject(callee);

    return argsobj;
}

struct STATIC_SKIP_INFERENCE PutArg
{
    PutArg(Value *dst) : dst(dst) {}
    Value *dst;
    void operator()(uintN, Value *src) {
        if (!dst->isMagic(JS_ARGS_HOLE))
            *dst = *src;
        ++dst;
    }
};

JSObject *
js_GetArgsObject(JSContext *cx, JSStackFrame *fp)
{
    /*
     * We must be in a function activation; the function must be lightweight
     * or else fp must have a variable object.
     */
    JS_ASSERT_IF(fp->fun()->isHeavyweight(), fp->hasCallObj());

    while (fp->isEvalOrDebuggerFrame())
        fp = fp->prev();

    /* Create an arguments object for fp only if it lacks one. */
    if (fp->hasArgsObj())
        return &fp->argsObj();

    /* Compute the arguments object's parent slot from fp's scope chain. */
    JSObject *global = fp->scopeChain().getGlobal();
    JSObject *argsobj = NewArguments(cx, global, fp->numActualArgs(), fp->callee());
    if (!argsobj)
        return argsobj;

    /*
     * Strict mode functions have arguments objects that copy the initial
     * actual parameter values.  It is the caller's responsibility to get the
     * arguments object before any parameters are modified!  (The emitter
     * ensures this by synthesizing an arguments access at the start of any
     * strict mode function that contains an assignment to a parameter, or
     * that calls eval.)  Non-strict mode arguments use the frame pointer to
     * retrieve up-to-date parameter values.
     */
    if (argsobj->isStrictArguments())
        fp->forEachCanonicalActualArg(PutArg(argsobj->getArgsData()->slots));
    else
        argsobj->setPrivate(fp);

    fp->setArgsObj(*argsobj);
    return argsobj;
}

void
js_PutArgsObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject &argsobj = fp->argsObj();
    if (argsobj.isNormalArguments()) {
        JS_ASSERT(argsobj.getPrivate() == fp);
        fp->forEachCanonicalActualArg(PutArg(argsobj.getArgsData()->slots));
        argsobj.setPrivate(NULL);
    } else {
        JS_ASSERT(!argsobj.getPrivate());
    }
    fp->clearArgsObj();
}

#ifdef JS_TRACER

/*
 * Traced versions of js_GetArgsObject and js_PutArgsObject.
 */
JSObject * JS_FASTCALL
js_NewArgumentsOnTrace(JSContext *cx, JSObject *parent, uint32 argc, JSObject *callee)
{
    JSObject *argsobj = NewArguments(cx, parent, argc, *callee);
    if (!argsobj)
        return NULL;

    if (argsobj->isStrictArguments()) {
        /*
         * Strict mode callers must copy arguments into the created arguments
         * object. The trace-JITting code is in TraceRecorder::newArguments.
         */
        JS_ASSERT(!argsobj->getPrivate());
    } else {
        argsobj->setPrivate(JS_ARGUMENTS_OBJECT_ON_TRACE);
    }

    return argsobj;
}
JS_DEFINE_CALLINFO_4(extern, OBJECT, js_NewArgumentsOnTrace, CONTEXT, OBJECT, UINT32, OBJECT,
                     0, nanojit::ACCSET_STORE_ANY)

/* FIXME change the return type to void. */
JSBool JS_FASTCALL
js_PutArgumentsOnTrace(JSContext *cx, JSObject *argsobj, Value *args)
{
    JS_ASSERT(argsobj->isNormalArguments());
    JS_ASSERT(argsobj->getPrivate() == JS_ARGUMENTS_OBJECT_ON_TRACE);

    /*
     * TraceRecorder::putActivationObjects builds a single, contiguous array of
     * the arguments, regardless of whether #actuals > #formals so there is no
     * need to worry about actual vs. formal arguments.
     */
    Value *srcend = args + argsobj->getArgsInitialLength();
    Value *dst = argsobj->getArgsData()->slots;
    for (Value *src = args; src != srcend; ++src, ++dst) {
        if (!dst->isMagic(JS_ARGS_HOLE))
            *dst = *src;
    }

    argsobj->setPrivate(NULL);
    return true;
}
JS_DEFINE_CALLINFO_3(extern, BOOL, js_PutArgumentsOnTrace, CONTEXT, OBJECT, VALUEPTR, 0,
                     nanojit::ACCSET_STORE_ANY)

#endif /* JS_TRACER */

static JSBool
args_delProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    JS_ASSERT(obj->isArguments());

    if (JSID_IS_INT(id)) {
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < obj->getArgsInitialLength())
            obj->setArgsElement(arg, MagicValue(JS_ARGS_HOLE));
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        obj->setArgsLengthOverridden();
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom)) {
        obj->setArgsCallee(MagicValue(JS_ARGS_HOLE));
    }
    return true;
}

static JS_REQUIRES_STACK JSObject *
WrapEscapingClosure(JSContext *cx, JSStackFrame *fp, JSFunction *fun)
{
    JS_ASSERT(fun->optimizedClosure());
    JS_ASSERT(!fun->u.i.wrapper);

    /*
     * We do not attempt to reify Call and Block objects on demand for outer
     * scopes. This could be done (see the "v8" patch in bug 494235) but it is
     * fragile in the face of ongoing compile-time optimization. Instead, the
     * _DBG* opcodes used by wrappers created here must cope with unresolved
     * upvars and throw them as reference errors. Caveat debuggers!
     */
    JSObject *scopeChain = GetScopeChain(cx, fp);
    if (!scopeChain)
        return NULL;

    JSObject *wfunobj = NewFunction(cx, scopeChain);
    if (!wfunobj)
        return NULL;
    AutoObjectRooter tvr(cx, wfunobj);

    JSFunction *wfun = (JSFunction *) wfunobj;
    wfunobj->setPrivate(wfun);
    wfun->nargs = fun->nargs;
    wfun->flags = fun->flags | JSFUN_HEAVYWEIGHT;
    wfun->u.i.nvars = fun->u.i.nvars;
    wfun->u.i.nupvars = fun->u.i.nupvars;
    wfun->u.i.skipmin = fun->u.i.skipmin;
    wfun->u.i.wrapper = true;
    wfun->u.i.script = NULL;
    wfun->u.i.names = fun->u.i.names;
    wfun->atom = fun->atom;

    JSScript *script = fun->u.i.script;
    jssrcnote *snbase = script->notes();
    jssrcnote *sn = snbase;
    while (!SN_IS_TERMINATOR(sn))
        sn = SN_NEXT(sn);
    uintN nsrcnotes = (sn - snbase) + 1;

    /* NB: GC must not occur before wscript is homed in wfun->u.i.script. */
    JSScript *wscript = JSScript::NewScript(cx, script->length, nsrcnotes,
                                            script->atomMap.length,
                                            (script->objectsOffset != 0)
                                            ? script->objects()->length
                                            : 0,
                                            fun->u.i.nupvars,
                                            (script->regexpsOffset != 0)
                                            ? script->regexps()->length
                                            : 0,
                                            (script->trynotesOffset != 0)
                                            ? script->trynotes()->length
                                            : 0,
                                            (script->constOffset != 0)
                                            ? script->consts()->length
                                            : 0,
                                            (script->globalsOffset != 0)
                                            ? script->globals()->length
                                            : 0,
                                            script->nClosedArgs,
                                            script->nClosedVars);
    if (!wscript)
        return NULL;

    memcpy(wscript->code, script->code, script->length);
    wscript->main = wscript->code + (script->main - script->code);

    memcpy(wscript->notes(), snbase, nsrcnotes * sizeof(jssrcnote));
    memcpy(wscript->atomMap.vector, script->atomMap.vector,
           wscript->atomMap.length * sizeof(JSAtom *));
    if (script->objectsOffset != 0) {
        memcpy(wscript->objects()->vector, script->objects()->vector,
               wscript->objects()->length * sizeof(JSObject *));
    }
    if (script->regexpsOffset != 0) {
        memcpy(wscript->regexps()->vector, script->regexps()->vector,
               wscript->regexps()->length * sizeof(JSObject *));
    }
    if (script->trynotesOffset != 0) {
        memcpy(wscript->trynotes()->vector, script->trynotes()->vector,
               wscript->trynotes()->length * sizeof(JSTryNote));
    }
    if (script->globalsOffset != 0) {
        memcpy(wscript->globals()->vector, script->globals()->vector,
               wscript->globals()->length * sizeof(GlobalSlotArray::Entry));
    }
    if (script->nClosedArgs + script->nClosedVars != 0)
        script->copyClosedSlotsTo(wscript);

    if (wfun->u.i.nupvars != 0) {
        JS_ASSERT(wfun->u.i.nupvars == wscript->upvars()->length);
        memcpy(wscript->upvars()->vector, script->upvars()->vector,
               wfun->u.i.nupvars * sizeof(uint32));
    }

    jsbytecode *pc = wscript->code;
    while (*pc != JSOP_STOP) {
        /* FIXME should copy JSOP_TRAP? */
        JSOp op = js_GetOpcode(cx, wscript, pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        ptrdiff_t oplen = cs->length;
        if (oplen < 0)
            oplen = js_GetVariableBytecodeLength(pc);

        /*
         * Rewrite JSOP_{GET,CALL}FCSLOT as JSOP_{GET,CALL}UPVAR_DBG for the
         * case where fun is an escaping flat closure. This works because the
         * UPVAR and FCSLOT ops by design have the same format: an upvar index
         * immediate operand.
         */
        switch (op) {
          case JSOP_GETUPVAR:       *pc = JSOP_GETUPVAR_DBG; break;
          case JSOP_CALLUPVAR:      *pc = JSOP_CALLUPVAR_DBG; break;
          case JSOP_GETFCSLOT:      *pc = JSOP_GETUPVAR_DBG; break;
          case JSOP_CALLFCSLOT:     *pc = JSOP_CALLUPVAR_DBG; break;
          case JSOP_DEFFUN_FC:      *pc = JSOP_DEFFUN_DBGFC; break;
          case JSOP_DEFLOCALFUN_FC: *pc = JSOP_DEFLOCALFUN_DBGFC; break;
          case JSOP_LAMBDA_FC:      *pc = JSOP_LAMBDA_DBGFC; break;
          default:;
        }
        pc += oplen;
    }

    /*
     * Fill in the rest of wscript. This means if you add members to JSScript
     * you must update this code. FIXME: factor into JSScript::clone method.
     */
    wscript->setVersion(script->getVersion());
    wscript->nfixed = script->nfixed;
    wscript->filename = script->filename;
    wscript->lineno = script->lineno;
    wscript->nslots = script->nslots;
    wscript->staticLevel = script->staticLevel;
    wscript->principals = script->principals;
    wscript->noScriptRval = script->noScriptRval;
    wscript->savedCallerFun = script->savedCallerFun;
    wscript->hasSharps = script->hasSharps;
    wscript->strictModeCode = script->strictModeCode;
    wscript->compileAndGo = script->compileAndGo;
    wscript->usesEval = script->usesEval;
    wscript->usesArguments = script->usesArguments;
    wscript->warnedAboutTwoArgumentEval = script->warnedAboutTwoArgumentEval;
    if (wscript->principals)
        JSPRINCIPALS_HOLD(cx, wscript->principals);
#ifdef CHECK_SCRIPT_OWNER
    wscript->owner = script->owner;
#endif

    /* Deoptimize wfun from FUN_{FLAT,NULL}_CLOSURE to FUN_INTERPRETED. */
    FUN_SET_KIND(wfun, JSFUN_INTERPRETED);
    wfun->u.i.script = wscript;
    return wfunobj;
}

static JSBool
ArgGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    LeaveTrace(cx);

    if (!InstanceOf(cx, obj, &js_ArgumentsClass, NULL))
        return true;

    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < obj->getArgsInitialLength()) {
            JS_ASSERT(!obj->getArgsElement(arg).isMagic(JS_ARGS_HOLE));
            if (JSStackFrame *fp = (JSStackFrame *) obj->getPrivate())
                *vp = fp->canonicalActualArg(arg);
            else
                *vp = obj->getArgsElement(arg);
        }
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (!obj->isArgsLengthOverridden())
            vp->setInt32(obj->getArgsInitialLength());
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom));
        const Value &v = obj->getArgsCallee();
        if (!v.isMagic(JS_ARGS_HOLE)) {
            /*
             * If this function or one in it needs upvars that reach above it
             * in the scope chain, it must not be a null closure (it could be a
             * flat closure, or an unoptimized closure -- the latter itself not
             * necessarily heavyweight). Rather than wrap here, we simply throw
             * to reduce code size and tell debugger users the truth instead of
             * passing off a fibbing wrapper.
             */
            if (GET_FUNCTION_PRIVATE(cx, &v.toObject())->needsWrapper()) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_OPTIMIZED_CLOSURE_LEAK);
                return false;
            }
            *vp = v;
        }
    }
    return true;
}

static JSBool
ArgSetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
#ifdef JS_TRACER
    // To be able to set a property here on trace, we would have to make
    // sure any updates also get written back to the trace native stack.
    // For simplicity, we just leave trace, since this is presumably not
    // a common operation.
    if (JS_ON_TRACE(cx)) {
        DeepBail(cx);
        return false;
    }
#endif

    if (!InstanceOf(cx, obj, &js_ArgumentsClass, NULL))
        return true;

    if (JSID_IS_INT(id)) {
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < obj->getArgsInitialLength()) {
            JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
            if (fp) {
                JSScript *script = fp->functionScript();
                if (script->usesArguments)
                    fp->canonicalActualArg(arg) = *vp;
                return true;
            }
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom) ||
                  JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom));
    }

    /*
     * For simplicity we use delete/set to replace the property with one
     * backed by the default Object getter and setter. Note that we rely on
     * args_delProperty to clear the corresponding reserved slot so the GC can
     * collect its value.
     */
    AutoValueRooter tvr(cx);
    return js_DeleteProperty(cx, obj, id, tvr.addr(), false) &&
           js_SetProperty(cx, obj, id, vp, false);
}

static JSBool
args_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
             JSObject **objp)
{
    JS_ASSERT(obj->isNormalArguments());

    *objp = NULL;
    bool valid = false;
    uintN attrs = JSPROP_SHARED;
    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        attrs = JSPROP_ENUMERATE | JSPROP_SHARED;
        if (arg < obj->getArgsInitialLength() && !obj->getArgsElement(arg).isMagic(JS_ARGS_HOLE))
            valid = true;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (!obj->isArgsLengthOverridden())
            valid = true;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom)) {
        if (!obj->getArgsCallee().isMagic(JS_ARGS_HOLE))
            valid = true;
    }

    if (valid) {
        Value tmp = UndefinedValue();
        if (!js_DefineProperty(cx, obj, id, &tmp, ArgGetter, ArgSetter, attrs))
            return JS_FALSE;
        *objp = obj;
    }
    return true;
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNormalArguments());

    /*
     * Trigger reflection in args_resolve using a series of js_LookupProperty
     * calls.
     */
    int argc = int(obj->getArgsInitialLength());
    for (int i = -2; i != argc; i++) {
        jsid id = (i == -2)
                  ? ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)
                  : (i == -1)
                  ? ATOM_TO_JSID(cx->runtime->atomState.calleeAtom)
                  : INT_TO_JSID(i);

        JSObject *pobj;
        JSProperty *prop;
        if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
            return false;
    }
    return true;
}

static JSBool
StrictArgGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    LeaveTrace(cx);

    if (!InstanceOf(cx, obj, &StrictArgumentsClass, NULL))
        return true;

    if (JSID_IS_INT(id)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < obj->getArgsInitialLength()) {
            const Value &v = obj->getArgsElement(arg);
            if (!v.isMagic(JS_ARGS_HOLE))
                *vp = v;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
        if (!obj->isArgsLengthOverridden())
            vp->setInt32(obj->getArgsInitialLength());
    }
    return true;
}

static JSBool
StrictArgSetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (!InstanceOf(cx, obj, &StrictArgumentsClass, NULL))
        return true;

    if (JSID_IS_INT(id)) {
        uintN arg = uintN(JSID_TO_INT(id));
        if (arg < obj->getArgsInitialLength()) {
            obj->setArgsElement(arg, *vp);
            return true;
        }
    } else {
        JS_ASSERT(JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom));
    }

    /*
     * For simplicity we use delete/set to replace the property with one
     * backed by the default Object getter and setter. Note that we rely on
     * args_delProperty to clear the corresponding reserved slot so the GC can
     * collect its value.
     */
    AutoValueRooter tvr(cx);
    return js_DeleteProperty(cx, obj, id, tvr.addr(), true) &&
           js_SetProperty(cx, obj, id, vp, true);
}

static JSBool
strictargs_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags, JSObject **objp)
{
    JS_ASSERT(obj->isStrictArguments());

    *objp = NULL;
    bool valid = false;
    uintN attrs = JSPROP_SHARED;
    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        attrs = JSPROP_SHARED | JSPROP_ENUMERATE;
        if (arg < obj->getArgsInitialLength() && !obj->getArgsElement(arg).isMagic(JS_ARGS_HOLE))
            valid = true;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        if (!obj->isArgsLengthOverridden())
            valid = true;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.calleeAtom)) {
        Value tmp = UndefinedValue();
        PropertyOp throwTypeError = CastAsPropertyOp(obj->getThrowTypeError());
        uintN attrs = JSPROP_PERMANENT | JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED;
        if (!js_DefineProperty(cx, obj, id, &tmp, throwTypeError, throwTypeError, attrs))
            return false;

        *objp = obj;
        return true;
    } else if (JSID_IS_ATOM(id, cx->runtime->atomState.callerAtom)) {
        /*
         * Strict mode arguments objects have an immutable poison-pill caller
         * property that throws a TypeError on getting or setting.
         */
        PropertyOp throwTypeError = CastAsPropertyOp(obj->getThrowTypeError());
        Value tmp = UndefinedValue();
        if (!js_DefineProperty(cx, obj, id, &tmp, throwTypeError, throwTypeError,
                               JSPROP_PERMANENT | JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED)) {
            return false;
        }

        *objp = obj;
        return true;
    }

    if (valid) {
        Value tmp = UndefinedValue();
        if (!js_DefineProperty(cx, obj, id, &tmp, StrictArgGetter, StrictArgSetter, attrs))
            return false;
        *objp = obj;
    }
    return true;
}

static JSBool
strictargs_enumerate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isStrictArguments());

    /*
     * Trigger reflection in strictargs_resolve using a series of
     * js_LookupProperty calls.
     */
    JSObject *pobj;
    JSProperty *prop;

    // length
    if (!js_LookupProperty(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom), &pobj, &prop))
        return false;

    // callee
    if (!js_LookupProperty(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.calleeAtom), &pobj, &prop))
        return false;

    // caller
    if (!js_LookupProperty(cx, obj, ATOM_TO_JSID(cx->runtime->atomState.callerAtom), &pobj, &prop))
        return false;

    for (uint32 i = 0, argc = obj->getArgsInitialLength(); i < argc; i++) {
        if (!js_LookupProperty(cx, obj, INT_TO_JSID(i), &pobj, &prop))
            return false;
    }

    return true;
}

static void
args_finalize(JSContext *cx, JSObject *obj)
{
    cx->free((void *) obj->getArgsData());
}

/*
 * If a generator's arguments or call object escapes, and the generator frame
 * is not executing, the generator object needs to be marked because it is not
 * otherwise reachable. An executing generator is rooted by its invocation.  To
 * distinguish the two cases (which imply different access paths to the
 * generator object), we use the JSFRAME_FLOATING_GENERATOR flag, which is only
 * set on the JSStackFrame kept in the generator object's JSGenerator.
 */
static inline void
MaybeMarkGenerator(JSTracer *trc, JSObject *obj)
{
#if JS_HAS_GENERATORS
    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp && fp->isFloatingGenerator()) {
        JSObject *genobj = js_FloatingFrameToGenerator(fp)->obj;
        MarkObject(trc, *genobj, "generator object");
    }
#endif
}

static void
args_trace(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->isArguments());
    if (obj->getPrivate() == JS_ARGUMENTS_OBJECT_ON_TRACE) {
        JS_ASSERT(!obj->isStrictArguments());
        return;
    }

    ArgumentsData *data = obj->getArgsData();
    if (data->callee.isObject())
        MarkObject(trc, data->callee.toObject(), js_callee_str);
    MarkValueRange(trc, obj->getArgsInitialLength(), data->slots, js_arguments_str);

    MaybeMarkGenerator(trc, obj);
}

/*
 * The Arguments classes aren't initialized via js_InitClass, because arguments
 * objects have the initial value of Object.prototype as their [[Prototype]].
 * However, Object.prototype.toString.call(arguments) === "[object Arguments]"
 * per ES5 (although not ES3), so the class name is "Arguments" rather than
 * "Object".
 *
 * The JSClass functions below collaborate to lazily reflect and synchronize
 * actual argument values, argument count, and callee function object stored
 * in a JSStackFrame with their corresponding property values in the frame's
 * arguments object.
 */
Class js_ArgumentsClass = {
    "Arguments",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::ARGS_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,   /* addProperty */
    args_delProperty,
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    args_enumerate,
    (JSResolveOp) args_resolve,
    ConvertStub,
    args_finalize,  /* finalize   */
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    JS_CLASS_TRACE(args_trace)
};

namespace js {

/*
 * Strict mode arguments is significantly less magical than non-strict mode
 * arguments, so it is represented by a different class while sharing some
 * functionality.
 */
Class StrictArgumentsClass = {
    "Arguments",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::ARGS_CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,   /* addProperty */
    args_delProperty,
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    strictargs_enumerate,
    reinterpret_cast<JSResolveOp>(strictargs_resolve),
    ConvertStub,
    args_finalize,  /* finalize   */
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    JS_CLASS_TRACE(args_trace)
};

}

/*
 * A Declarative Environment object stores its active JSStackFrame pointer in
 * its private slot, just as Call and Arguments objects do.
 */
Class js_DeclEnvClass = {
    js_Object_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub
};

static JSBool
CheckForEscapingClosure(JSContext *cx, JSObject *obj, Value *vp)
{
    JS_ASSERT(obj->isCall() || obj->getClass() == &js_DeclEnvClass);

    const Value &v = *vp;

    JSObject *funobj;
    if (IsFunctionObject(v, &funobj)) {
        JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);

        /*
         * Any escaping null or flat closure that reaches above itself or
         * contains nested functions that reach above it must be wrapped.
         * We can wrap only when this Call or Declarative Environment obj
         * still has an active stack frame associated with it.
         */
        if (fun->needsWrapper()) {
            LeaveTrace(cx);

            JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
            if (fp) {
                JSObject *wrapper = WrapEscapingClosure(cx, fp, fun);
                if (!wrapper)
                    return false;
                vp->setObject(*wrapper);
                return true;
            }

            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_OPTIMIZED_CLOSURE_LEAK);
            return false;
        }
    }
    return true;
}

static JSBool
CalleeGetter(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CheckForEscapingClosure(cx, obj, vp);
}

static JSObject *
NewCallObject(JSContext *cx, JSFunction *fun, JSObject &scopeChain, JSObject &callee)
{
    size_t vars = fun->countArgsAndVars();
    size_t slots = JSObject::CALL_RESERVED_SLOTS + vars;
    gc::FinalizeKind kind = gc::GetGCObjectKind(slots);

    JSObject *callobj = js_NewGCObject(cx, kind);
    if (!callobj)
        return NULL;

    /* Init immediately to avoid GC seeing a half-init'ed object. */
    callobj->init(cx, &js_CallClass, NULL, &scopeChain, NULL, false);
    callobj->setMap(fun->u.i.names);

    /* This must come after callobj->lastProp has been set. */
    if (!callobj->ensureInstanceReservedSlots(cx, vars))
        return NULL;

#ifdef DEBUG
    for (Shape::Range r = callobj->lastProp; !r.empty(); r.popFront()) {
        const Shape &s = r.front();
        if (s.slot != SHAPE_INVALID_SLOT) {
            JS_ASSERT(s.slot + 1 == callobj->slotSpan());
            break;
        }
    }
#endif

    callobj->setCallObjCallee(callee);
    return callobj;
}

static inline JSObject *
NewDeclEnvObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *envobj = js_NewGCObject(cx, FINALIZE_OBJECT2);
    if (!envobj)
        return NULL;

    envobj->init(cx, &js_DeclEnvClass, NULL, &fp->scopeChain(), fp, false);
    envobj->setMap(cx->runtime->emptyDeclEnvShape);
    return envobj;
}

JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp)
{
    /* Create a call object for fp only if it lacks one. */
    JS_ASSERT(fp->isFunctionFrame());
    if (fp->hasCallObj())
        return &fp->callObj();

#ifdef DEBUG
    /* A call object should be a frame's outermost scope chain element.  */
    Class *clasp = fp->scopeChain().getClass();
    if (clasp == &js_WithClass || clasp == &js_BlockClass)
        JS_ASSERT(fp->scopeChain().getPrivate() != js_FloatingFrameIfGenerator(cx, fp));
    else if (clasp == &js_CallClass)
        JS_ASSERT(fp->scopeChain().getPrivate() != fp);
#endif

    /*
     * Create the call object, using the frame's enclosing scope as its
     * parent, and link the call to its stack frame. For a named function
     * expression Call's parent points to an environment object holding
     * function's name.
     */
    JSAtom *lambdaName =
        (fp->fun()->flags & JSFUN_LAMBDA) ? fp->fun()->atom : NULL;
    if (lambdaName) {
        JSObject *envobj = NewDeclEnvObject(cx, fp);
        if (!envobj)
            return NULL;

        /* Root envobj before js_DefineNativeProperty (-> JSClass.addProperty). */
        fp->setScopeChainNoCallObj(*envobj);
        if (!js_DefineNativeProperty(cx, &fp->scopeChain(), ATOM_TO_JSID(lambdaName),
                                     ObjectValue(fp->callee()),
                                     CalleeGetter, NULL,
                                     JSPROP_PERMANENT | JSPROP_READONLY,
                                     0, 0, NULL)) {
            return NULL;
        }
    }

    JSObject *callobj = NewCallObject(cx, fp->fun(), fp->scopeChain(), fp->callee());
    if (!callobj)
        return NULL;

    callobj->setPrivate(fp);
    JS_ASSERT(fp->fun() == fp->callee().getFunctionPrivate());

    /*
     * Push callobj on the top of the scope chain, and make it the
     * variables object.
     */
    fp->setScopeChainAndCallObj(*callobj);
    return callobj;
}

JSObject * JS_FASTCALL
js_CreateCallObjectOnTrace(JSContext *cx, JSFunction *fun, JSObject *callee, JSObject *scopeChain)
{
    JS_ASSERT(!js_IsNamedLambda(fun));
    JS_ASSERT(scopeChain);
    return NewCallObject(cx, fun, *scopeChain, *callee);
}

JS_DEFINE_CALLINFO_4(extern, OBJECT, js_CreateCallObjectOnTrace, CONTEXT, FUNCTION, OBJECT, OBJECT,
                     0, nanojit::ACCSET_STORE_ANY)

inline static void
CopyValuesToCallObject(JSObject &callobj, uintN nargs, Value *argv, uintN nvars, Value *slots)
{
    JS_ASSERT(callobj.numSlots() >= JSObject::CALL_RESERVED_SLOTS + nargs + nvars);
    Value *base = callobj.getSlots() + JSObject::CALL_RESERVED_SLOTS;
    memcpy(base, argv, nargs * sizeof(Value));
    memcpy(base + nargs, slots, nvars * sizeof(Value));
}

void
js_PutCallObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject &callobj = fp->callObj();

    /* Get the arguments object to snapshot fp's actual argument values. */
    if (fp->hasArgsObj()) {
        if (!fp->hasOverriddenArgs())
            callobj.setCallObjArguments(ObjectValue(fp->argsObj()));
        js_PutArgsObject(cx, fp);
    }

    JSFunction *fun = fp->fun();
    JS_ASSERT(fun == callobj.getCallObjCalleeFunction());
    uintN n = fun->countArgsAndVars();

    if (n != 0) {
        JS_ASSERT(JSFunction::CLASS_RESERVED_SLOTS + n <= callobj.numSlots());

        uint32 nargs = fun->nargs;
        uint32 nvars = fun->u.i.nvars;

        JSScript *script = fun->u.i.script;
        if (script->usesEval
#ifdef JS_METHODJIT
            || script->debugMode
#endif
            ) {
            CopyValuesToCallObject(callobj, nargs, fp->formalArgs(), nvars, fp->slots());
        } else {
            /*
             * For each arg & var that is closed over, copy it from the stack
             * into the call object.
             */
            JSScript *script = fun->u.i.script;
            uint32 nclosed = script->nClosedArgs;
            for (uint32 i = 0; i < nclosed; i++) {
                uint32 e = script->getClosedArg(i);
                callobj.setSlot(JSObject::CALL_RESERVED_SLOTS + e, fp->formalArg(e));
            }

            nclosed = script->nClosedVars;
            for (uint32 i = 0; i < nclosed; i++) {
                uint32 e = script->getClosedVar(i);
                callobj.setSlot(JSObject::CALL_RESERVED_SLOTS + nargs + e, fp->slots()[e]);
            }
        }
    }

    /* Clear private pointers to fp, which is about to go away (js_Invoke). */
    if (js_IsNamedLambda(fun)) {
        JSObject *env = callobj.getParent();

        JS_ASSERT(env->getClass() == &js_DeclEnvClass);
        JS_ASSERT(env->getPrivate() == fp);
        env->setPrivate(NULL);
    }

    callobj.setPrivate(NULL);
    fp->clearCallObj();
}

JSBool JS_FASTCALL
js_PutCallObjectOnTrace(JSContext *cx, JSObject *callobj, uint32 nargs, Value *argv,
                        uint32 nvars, Value *slots)
{
    JS_ASSERT(callobj->isCall());
    JS_ASSERT(!callobj->getPrivate());

    uintN n = nargs + nvars;
    if (n != 0)
        CopyValuesToCallObject(*callobj, nargs, argv, nvars, slots);

    return true;
}

JS_DEFINE_CALLINFO_6(extern, BOOL, js_PutCallObjectOnTrace, CONTEXT, OBJECT, UINT32, VALUEPTR,
                     UINT32, VALUEPTR, 0, nanojit::ACCSET_STORE_ANY)

enum JSCallPropertyKind {
    JSCPK_ARGUMENTS,
    JSCPK_ARG,
    JSCPK_VAR,
    JSCPK_UPVAR
};

static JSBool
CallPropertyOp(JSContext *cx, JSObject *obj, jsid id, Value *vp,
               JSCallPropertyKind kind, JSBool setter = false)
{
    JS_ASSERT(obj->isCall());

    uintN i = 0;
    if (kind != JSCPK_ARGUMENTS) {
        JS_ASSERT((int16) JSID_TO_INT(id) == JSID_TO_INT(id));
        i = (uint16) JSID_TO_INT(id);
    }

    Value *array;
    if (kind == JSCPK_UPVAR) {
        JSObject &callee = obj->getCallObjCallee();

#ifdef DEBUG
        JSFunction *callee_fun = (JSFunction *) callee.getPrivate();
        JS_ASSERT(FUN_FLAT_CLOSURE(callee_fun));
        JS_ASSERT(i < callee_fun->u.i.nupvars);
#endif

        array = callee.getFlatClosureUpvars();
    } else {
        JSFunction *fun = obj->getCallObjCalleeFunction();
        JS_ASSERT_IF(kind == JSCPK_ARG, i < fun->nargs);
        JS_ASSERT_IF(kind == JSCPK_VAR, i < fun->u.i.nvars);

        JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();

        if (kind == JSCPK_ARGUMENTS) {
            if (setter) {
                if (fp)
                    fp->setOverriddenArgs();
                obj->setCallObjArguments(*vp);
            } else {
                if (fp && !fp->hasOverriddenArgs()) {
                    JSObject *argsobj;

                    argsobj = js_GetArgsObject(cx, fp);
                    if (!argsobj)
                        return false;
                    vp->setObject(*argsobj);
                } else {
                    *vp = obj->getCallObjArguments();
                }
            }
            return true;
        }

        if (!fp) {
            i += JSObject::CALL_RESERVED_SLOTS;
            if (kind == JSCPK_VAR)
                i += fun->nargs;
            else
                JS_ASSERT(kind == JSCPK_ARG);

            array = obj->getSlots();
        } else if (kind == JSCPK_ARG) {
            array = fp->formalArgs();
        } else {
            JS_ASSERT(kind == JSCPK_VAR);
            array = fp->slots();
        }
    }

    if (setter) {
        GC_POKE(cx, array[i]);
        array[i] = *vp;
    } else {
        *vp = array[i];
    }
    return true;
}

namespace js {

static JSBool
GetCallArguments(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARGUMENTS);
}

static JSBool
SetCallArguments(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARGUMENTS, true);
}

JSBool
GetCallArg(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARG);
}

JSBool
SetCallArg(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARG, true);
}

JSBool
GetFlatUpvar(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_UPVAR);
}

JSBool
SetFlatUpvar(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_UPVAR, true);
}

JSBool
GetCallVar(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_VAR);
}

JSBool
GetCallVarChecked(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (!CallPropertyOp(cx, obj, id, vp, JSCPK_VAR))
        return false;

    return CheckForEscapingClosure(cx, obj, vp);
}

JSBool
SetCallVar(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_VAR, true);
}

} // namespace js

#if JS_TRACER
JSBool JS_FASTCALL
js_SetCallArg(JSContext *cx, JSObject *obj, jsid slotid, ValueArgType arg)
{
    Value argcopy = ValueArgToConstRef(arg);
    return CallPropertyOp(cx, obj, slotid, &argcopy, JSCPK_ARG, true);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_SetCallArg, CONTEXT, OBJECT, JSID, VALUE, 0,
                     nanojit::ACCSET_STORE_ANY)

JSBool JS_FASTCALL
js_SetCallVar(JSContext *cx, JSObject *obj, jsid slotid, ValueArgType arg)
{
    Value argcopy = ValueArgToConstRef(arg);
    return CallPropertyOp(cx, obj, slotid, &argcopy, JSCPK_VAR, true);
}
JS_DEFINE_CALLINFO_4(extern, BOOL, js_SetCallVar, CONTEXT, OBJECT, JSID, VALUE, 0,
                     nanojit::ACCSET_STORE_ANY)
#endif

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
             JSObject **objp)
{
    JS_ASSERT(obj->isCall());
    JS_ASSERT(!obj->getProto());

    if (!JSID_IS_ATOM(id))
        return JS_TRUE;

#ifdef DEBUG
    JSFunction *fun = obj->getCallObjCalleeFunction();
    JS_ASSERT(fun->lookupLocal(cx, JSID_TO_ATOM(id), NULL) == JSLOCAL_NONE);
#endif

    /*
     * Resolve arguments so that we never store a particular Call object's
     * arguments object reference in a Call prototype's |arguments| slot.
     *
     * Include JSPROP_ENUMERATE for consistency with all other Call object
     * properties; see JSFunction::addLocal and js::Interpret's JSOP_DEFFUN
     * rebinding-Call-property logic.
     */
    if (JSID_IS_ATOM(id, cx->runtime->atomState.argumentsAtom)) {
        if (!js_DefineNativeProperty(cx, obj, id, UndefinedValue(),
                                     GetCallArguments, SetCallArguments,
                                     JSPROP_PERMANENT | JSPROP_SHARED | JSPROP_ENUMERATE,
                                     0, 0, NULL, JSDNP_DONT_PURGE)) {
            return JS_FALSE;
        }
        *objp = obj;
        return JS_TRUE;
    }

    /* Control flow reaches here only if id was not resolved. */
    return JS_TRUE;
}

static void
call_trace(JSTracer *trc, JSObject *obj)
{
    JS_ASSERT(obj->isCall());
    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp) {
        /*
         * FIXME: Hide copies of stack values rooted by fp from the Cycle
         * Collector, which currently lacks a non-stub Unlink implementation
         * for JS objects (including Call objects), so is unable to collect
         * cycles involving Call objects whose frames are active without this
         * hiding hack.
         */
        uintN first = JSObject::CALL_RESERVED_SLOTS;
        uintN count = fp->fun()->countArgsAndVars();

        JS_ASSERT(obj->numSlots() >= first + count);
        SetValueRangeToUndefined(obj->getSlots() + first, count);
    }

    MaybeMarkGenerator(trc, obj);
}

JS_PUBLIC_DATA(Class) js_CallClass = {
    "Call",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(JSObject::CALL_RESERVED_SLOTS) |
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_ANONYMOUS | JSCLASS_MARK_IS_TRACE,
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    JS_EnumerateStub,
    (JSResolveOp)call_resolve,
    NULL,           /* convert: Leave it NULL so we notice if calls ever escape */
    NULL,           /* finalize */
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    JS_CLASS_TRACE(call_trace)
};

bool
JSStackFrame::getValidCalleeObject(JSContext *cx, Value *vp)
{
    if (!isFunctionFrame()) {
        vp->setUndefined();
        return true;
    }

    JSFunction *fun = this->fun();

    /*
     * See the equivalent condition in ArgGetter for the 'callee' id case, but
     * note that here we do not want to throw, since this escape can happen via
     * a foo.caller reference alone, without any debugger or indirect eval. And
     * alas, it seems foo.caller is still used on the Web.
     */
    if (fun->needsWrapper()) {
        JSObject *wrapper = WrapEscapingClosure(cx, this, fun);
        if (!wrapper)
            return false;
        vp->setObject(*wrapper);
        return true;
    }

    JSObject &funobj = callee();
    vp->setObject(funobj);

    /*
     * Check for an escape attempt by a joined function object, which must go
     * through the frame's |this| object's method read barrier for the method
     * atom by which it was uniquely associated with a property.
     */
    const Value &thisv = functionThis();
    if (thisv.isObject()) {
        JS_ASSERT(funobj.getFunctionPrivate() == fun);

        if (&fun->compiledFunObj() == &funobj && fun->methodAtom()) {
            JSObject *thisp = &thisv.toObject();
            JS_ASSERT(thisp->canHaveMethodBarrier());

            if (thisp->hasMethodBarrier()) {
                const Shape *shape = thisp->nativeLookup(ATOM_TO_JSID(fun->methodAtom()));

                /*
                 * The method property might have been deleted while the method
                 * barrier flag stuck, so we must lookup and test here.
                 *
                 * Two cases follow: the method barrier was not crossed yet, so
                 * we cross it here; the method barrier *was* crossed, in which
                 * case we must fetch and validate the cloned (unjoined) funobj
                 * in the method property's slot.
                 *
                 * In either case we must allow for the method property to have
                 * been replaced, or its value to have been overwritten.
                 */
                if (shape) {
                    if (shape->isMethod() && &shape->methodObject() == &funobj) {
                        if (!thisp->methodReadBarrier(cx, *shape, vp))
                            return false;
                        calleeValue().setObject(vp->toObject());
                        return true;
                    }
                    if (shape->hasSlot()) {
                        Value v = thisp->getSlot(shape->slot);
                        JSObject *clone;

                        if (IsFunctionObject(v, &clone) &&
                            GET_FUNCTION_PRIVATE(cx, clone) == fun &&
                            clone->hasMethodObj(*thisp)) {
                            JS_ASSERT(clone != &funobj);
                            *vp = v;
                            calleeValue().setObject(*clone);
                            return true;
                        }
                    }
                }

                /*
                 * If control flows here, we can't find an already-existing
                 * clone (or force to exist a fresh clone) created via thisp's
                 * method read barrier, so we must clone fun and store it in
                 * fp's callee to avoid re-cloning upon repeated foo.caller
                 * access. It seems that there are no longer any properties
                 * referring to fun.
                 */
                JSObject *newfunobj = CloneFunctionObject(cx, fun, fun->getParent());
                if (!newfunobj)
                    return false;
                newfunobj->setMethodObj(*thisp);
                calleeValue().setObject(*newfunobj);
                return true;
            }
        }
    }

    return true;
}

/* Generic function tinyids. */
enum {
    FUN_ARGUMENTS   = -1,       /* predefined arguments local variable */
    FUN_LENGTH      = -2,       /* number of actual args, arity if inactive */
    FUN_ARITY       = -3,       /* number of formal parameters; desired argc */
    FUN_NAME        = -4,       /* function name, "" if anonymous */
    FUN_CALLER      = -5        /* Function.prototype.caller, backward compat */
};

static JSBool
fun_getProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    if (!JSID_IS_INT(id))
        return true;

    jsint slot = JSID_TO_INT(id);

    /*
     * Loop because getter and setter can be delegated from another class,
     * but loop only for FUN_LENGTH because we must pretend that f.length
     * is in each function instance f, per ECMA-262, instead of only in the
     * Function.prototype object (we use JSPROP_PERMANENT with JSPROP_SHARED
     * to make it appear so).
     *
     * This code couples tightly to the attributes for lazyFunctionDataProps[]
     * and poisonPillProps[] initializers below, and to js_SetProperty and
     * js_HasOwnProperty.
     *
     * It's important to allow delegating objects, even though they inherit
     * this getter (fun_getProperty), to override arguments, arity, caller,
     * and name.  If we didn't return early for slot != FUN_LENGTH, we would
     * clobber *vp with the native property value, instead of letting script
     * override that value in delegating objects.
     *
     * Note how that clobbering is what simulates JSPROP_READONLY for all of
     * the non-standard properties when the directly addressed object (obj)
     * is a function object (i.e., when this loop does not iterate).
     */
    JSFunction *fun;
    while (!(fun = (JSFunction *)
                   GetInstancePrivate(cx, obj, &js_FunctionClass, NULL))) {
        if (slot != FUN_LENGTH)
            return true;
        obj = obj->getProto();
        if (!obj)
            return true;
    }

    /* Find fun's top-most activation record. */
    JSStackFrame *fp;
    for (fp = js_GetTopStackFrame(cx);
         fp && (fp->maybeFun() != fun || fp->isEvalOrDebuggerFrame());
         fp = fp->prev()) {
        continue;
    }

    switch (slot) {
      case FUN_ARGUMENTS:
        /* Warn if strict about f.arguments or equivalent unqualified uses. */
        if (!JS_ReportErrorFlagsAndNumber(cx,
                                          JSREPORT_WARNING | JSREPORT_STRICT,
                                          js_GetErrorMessage, NULL,
                                          JSMSG_DEPRECATED_USAGE,
                                          js_arguments_str)) {
            return false;
        }
        if (fp) {
            if (!js_GetArgsValue(cx, fp, vp))
                return false;
        } else {
            vp->setNull();
        }
        break;

      case FUN_LENGTH:
      case FUN_ARITY:
        vp->setInt32(fun->nargs);
        break;

      case FUN_NAME:
        vp->setString(fun->atom ? ATOM_TO_STRING(fun->atom)
                                : cx->runtime->emptyString);
        break;

      case FUN_CALLER:
        vp->setNull();
        if (fp && fp->prev() && !fp->prev()->getValidCalleeObject(cx, vp))
            return false;

        if (vp->isObject()) {
            JSObject &caller = vp->toObject();

            /* Censor the caller if it is from another compartment. */
            if (caller.getCompartment() != cx->compartment) {
                vp->setNull();
            } else if (caller.isFunction() && caller.getFunctionPrivate()->inStrictMode()) {
                JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                             JSMSG_CALLER_IS_STRICT);
                return false;
            }
        }
        break;

      default:
        /* XXX fun[0] and fun.arguments[0] are equivalent. */
        if (fp && fp->isFunctionFrame() && uint16(slot) < fp->numFormalArgs())
            *vp = fp->formalArg(slot);
        break;
    }

    return true;
}

struct LazyFunctionDataProp {
    uint16      atomOffset;
    int8        tinyid;
    uint8       attrs;
};

struct PoisonPillProp {
    uint16       atomOffset;
    int8         tinyid;
};

/* NB: no sentinels at ends -- use JS_ARRAY_LENGTH to bound loops. */

static const LazyFunctionDataProp lazyFunctionDataProps[] = {
    {ATOM_OFFSET(arity),     FUN_ARITY,      JSPROP_PERMANENT|JSPROP_READONLY},
    {ATOM_OFFSET(name),      FUN_NAME,       JSPROP_PERMANENT|JSPROP_READONLY},
};

/* Properties censored into [[ThrowTypeError]] in strict mode. */
static const PoisonPillProp poisonPillProps[] = {
    {ATOM_OFFSET(arguments), FUN_ARGUMENTS },
    {ATOM_OFFSET(caller),    FUN_CALLER    },
};

static JSBool
fun_enumerate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isFunction());

    jsid id;
    bool found;

    if (!obj->isBoundFunction()) {
        id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
        return false;

    for (uintN i = 0; i < JS_ARRAY_LENGTH(lazyFunctionDataProps); i++) {
        const LazyFunctionDataProp &lfp = lazyFunctionDataProps[i];
        id = ATOM_TO_JSID(OFFSET_TO_ATOM(cx->runtime, lfp.atomOffset));
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    for (uintN i = 0; i < JS_ARRAY_LENGTH(poisonPillProps); i++) {
        const PoisonPillProp &p = poisonPillProps[i];
        id = ATOM_TO_JSID(OFFSET_TO_ATOM(cx->runtime, p.atomOffset));
        if (!obj->hasProperty(cx, id, &found, JSRESOLVE_QUALIFIED))
            return false;
    }

    return true;
}

static JSBool
fun_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
            JSObject **objp)
{
    if (!JSID_IS_ATOM(id))
        return true;

    JSFunction *fun = obj->getFunctionPrivate();

    if (JSID_IS_ATOM(id, cx->runtime->atomState.classPrototypeAtom)) {
        /*
         * Native or "built-in" functions do not have a .prototype property per
         * ECMA-262 (all editions). Built-in constructor functions, e.g. Object
         * and Function to name two conspicuous examples, do have a .prototype
         * property, but it is created eagerly by js_InitClass (jsobj.cpp).
         *
         * ES5 15.3.4: the non-native function object named Function.prototype
         * must not have a .prototype property.
         *
         * ES5 15.3.4.5: bound functions don't have a prototype property. The
         * isNative() test covers this case because bound functions are native
         * functions by definition/construction.
         */
        if (fun->isNative() || fun->isFunctionPrototype())
            return true;

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
        JSObject *parent = obj->getParent();
        JSObject *proto;
        if (!js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
            return false;
        proto = NewNativeClassInstance(cx, &js_ObjectClass, proto, parent);
        if (!proto)
            return false;

        /*
         * ECMA (15.3.5.2) says that constructor.prototype is DontDelete for
         * user-defined functions, but DontEnum | ReadOnly | DontDelete for
         * native "system" constructors such as Object or Function.  So lazily
         * set the former here in fun_resolve, but eagerly define the latter
         * in js_InitClass, with the right attributes.
         */
        if (!js_SetClassPrototype(cx, obj, proto, JSPROP_PERMANENT))
            return false;

        *objp = obj;
        return true;
    }

    if (JSID_IS_ATOM(id, cx->runtime->atomState.lengthAtom)) {
        JS_ASSERT(!IsInternalFunctionObject(obj));
        if (!js_DefineNativeProperty(cx, obj, id, Int32Value(fun->nargs),
                                     PropertyStub, PropertyStub,
                                     JSPROP_PERMANENT | JSPROP_READONLY, 0, 0, NULL)) {
            return false;
        }
        *objp = obj;
        return true;
    }

    for (uintN i = 0; i < JS_ARRAY_LENGTH(lazyFunctionDataProps); i++) {
        const LazyFunctionDataProp *lfp = &lazyFunctionDataProps[i];

        if (JSID_IS_ATOM(id, OFFSET_TO_ATOM(cx->runtime, lfp->atomOffset))) {
            JS_ASSERT(!IsInternalFunctionObject(obj));

            if (!js_DefineNativeProperty(cx, obj, id, UndefinedValue(),
                                         fun_getProperty, PropertyStub,
                                         lfp->attrs, Shape::HAS_SHORTID,
                                         lfp->tinyid, NULL)) {
                return false;
            }
            *objp = obj;
            return true;
        }
    }

    for (uintN i = 0; i < JS_ARRAY_LENGTH(poisonPillProps); i++) {
        const PoisonPillProp &p = poisonPillProps[i];

        if (JSID_IS_ATOM(id, OFFSET_TO_ATOM(cx->runtime, p.atomOffset))) {
            JS_ASSERT(!IsInternalFunctionObject(obj));

            PropertyOp getter, setter;
            uintN attrs = JSPROP_PERMANENT;
            if (fun->inStrictMode() || obj->isBoundFunction()) {
                JSObject *throwTypeError = obj->getThrowTypeError();

                getter = CastAsPropertyOp(throwTypeError);
                setter = CastAsPropertyOp(throwTypeError);
                attrs |= JSPROP_GETTER | JSPROP_SETTER;
            } else {
                getter = fun_getProperty;
                setter = PropertyStub;
            }

            if (!js_DefineNativeProperty(cx, obj, id, UndefinedValue(),
                                         getter, setter,
                                         attrs, Shape::HAS_SHORTID,
                                         p.tinyid, NULL)) {
                return false;
            }
            *objp = obj;
            return true;
        }
    }

    return true;
}

#if JS_HAS_XDR

/* XXX store parent and proto, if defined */
JSBool
js_XDRFunctionObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSFunction *fun;
    uint32 firstword;           /* flag telling whether fun->atom is non-null,
                                   plus for fun->u.i.skipmin, fun->u.i.wrapper,
                                   and 14 bits reserved for future use */
    uintN nargs, nvars, nupvars, n;
    uint32 localsword;          /* word for argument and variable counts */
    uint32 flagsword;           /* word for fun->u.i.nupvars and fun->flags */

    cx = xdr->cx;
    if (xdr->mode == JSXDR_ENCODE) {
        fun = GET_FUNCTION_PRIVATE(cx, *objp);
        if (!FUN_INTERPRETED(fun)) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_SCRIPTED_FUNCTION,
                                     name);
            }
            return false;
        }
        if (fun->u.i.wrapper) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes))
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_XDR_CLOSURE_WRAPPER, name);
            return false;
        }
        JS_ASSERT((fun->u.i.wrapper & ~1U) == 0);
        firstword = (fun->u.i.skipmin << 2) | (fun->u.i.wrapper << 1) | !!fun->atom;
        nargs = fun->nargs;
        nvars = fun->u.i.nvars;
        nupvars = fun->u.i.nupvars;
        localsword = (nargs << 16) | nvars;
        flagsword = (nupvars << 16) | fun->flags;
    } else {
        fun = js_NewFunction(cx, NULL, NULL, 0, JSFUN_INTERPRETED, NULL, NULL);
        if (!fun)
            return false;
        FUN_OBJECT(fun)->clearParent();
        FUN_OBJECT(fun)->clearProto();
#ifdef __GNUC__
        nvars = nargs = nupvars = 0;    /* quell GCC uninitialized warning */
#endif
    }

    AutoObjectRooter tvr(cx, FUN_OBJECT(fun));

    if (!JS_XDRUint32(xdr, &firstword))
        return false;
    if ((firstword & 1U) && !js_XDRAtom(xdr, &fun->atom))
        return false;
    if (!JS_XDRUint32(xdr, &localsword) ||
        !JS_XDRUint32(xdr, &flagsword)) {
        return false;
    }

    if (xdr->mode == JSXDR_DECODE) {
        nargs = localsword >> 16;
        nvars = uint16(localsword);
        JS_ASSERT((flagsword & JSFUN_KINDMASK) >= JSFUN_INTERPRETED);
        nupvars = flagsword >> 16;
        fun->flags = uint16(flagsword);
        fun->u.i.skipmin = uint16(firstword >> 2);
        fun->u.i.wrapper = JSPackedBool((firstword >> 1) & 1);
    }

    /* do arguments and local vars */
    n = nargs + nvars + nupvars;
    if (n != 0) {
        void *mark;
        uintN i;
        uintN bitmapLength;
        uint32 *bitmap;
        jsuword *names;
        JSAtom *name;
        JSLocalKind localKind;

        bool ok = true;
        mark = JS_ARENA_MARK(&xdr->cx->tempPool);

        /*
         * From this point the control must flow via the label release_mark.
         *
         * To xdr the names we prefix the names with a bitmap descriptor and
         * then xdr the names as strings. For argument names (indexes below
         * nargs) the corresponding bit in the bitmap is unset when the name
         * is null. Such null names are not encoded or decoded. For variable
         * names (indexes starting from nargs) bitmap's bit is set when the
         * name is declared as const, not as ordinary var.
         * */
        MUST_FLOW_THROUGH("release_mark");
        bitmapLength = JS_HOWMANY(n, JS_BITS_PER_UINT32);
        JS_ARENA_ALLOCATE_CAST(bitmap, uint32 *, &xdr->cx->tempPool,
                               bitmapLength * sizeof *bitmap);
        if (!bitmap) {
            js_ReportOutOfScriptQuota(xdr->cx);
            ok = false;
            goto release_mark;
        }
        if (xdr->mode == JSXDR_ENCODE) {
            names = fun->getLocalNameArray(xdr->cx, &xdr->cx->tempPool);
            if (!names) {
                ok = false;
                goto release_mark;
            }
            PodZero(bitmap, bitmapLength);
            for (i = 0; i != n; ++i) {
                if (i < fun->nargs
                    ? JS_LOCAL_NAME_TO_ATOM(names[i]) != NULL
                    : JS_LOCAL_NAME_IS_CONST(names[i])) {
                    bitmap[i >> JS_BITS_PER_UINT32_LOG2] |=
                        JS_BIT(i & (JS_BITS_PER_UINT32 - 1));
                }
            }
        }
#ifdef __GNUC__
        else {
            names = NULL;   /* quell GCC uninitialized warning */
        }
#endif
        for (i = 0; i != bitmapLength; ++i) {
            ok = !!JS_XDRUint32(xdr, &bitmap[i]);
            if (!ok)
                goto release_mark;
        }
        for (i = 0; i != n; ++i) {
            if (i < nargs &&
                !(bitmap[i >> JS_BITS_PER_UINT32_LOG2] &
                  JS_BIT(i & (JS_BITS_PER_UINT32 - 1)))) {
                if (xdr->mode == JSXDR_DECODE) {
                    ok = !!fun->addLocal(xdr->cx, NULL, JSLOCAL_ARG);
                    if (!ok)
                        goto release_mark;
                } else {
                    JS_ASSERT(!JS_LOCAL_NAME_TO_ATOM(names[i]));
                }
                continue;
            }
            if (xdr->mode == JSXDR_ENCODE)
                name = JS_LOCAL_NAME_TO_ATOM(names[i]);
            ok = !!js_XDRAtom(xdr, &name);
            if (!ok)
                goto release_mark;
            if (xdr->mode == JSXDR_DECODE) {
                localKind = (i < nargs)
                            ? JSLOCAL_ARG
                            : (i < nargs + nvars)
                            ? (bitmap[i >> JS_BITS_PER_UINT32_LOG2] &
                               JS_BIT(i & (JS_BITS_PER_UINT32 - 1))
                               ? JSLOCAL_CONST
                               : JSLOCAL_VAR)
                            : JSLOCAL_UPVAR;
                ok = !!fun->addLocal(xdr->cx, name, localKind);
                if (!ok)
                    goto release_mark;
            }
        }

      release_mark:
        JS_ARENA_RELEASE(&xdr->cx->tempPool, mark);
        if (!ok)
            return false;

        if (xdr->mode == JSXDR_DECODE)
            fun->freezeLocalNames(cx);
    }

    if (!js_XDRScript(xdr, &fun->u.i.script, NULL))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        *objp = FUN_OBJECT(fun);
#ifdef CHECK_SCRIPT_OWNER
        fun->script()->owner = NULL;
#endif
        js_CallNewScriptHook(cx, fun->script(), fun);
    }

    return true;
}

#else  /* !JS_HAS_XDR */

#define js_XDRFunctionObject NULL

#endif /* !JS_HAS_XDR */

/*
 * [[HasInstance]] internal method for Function objects: fetch the .prototype
 * property of its 'this' parameter, and walks the prototype chain of v (only
 * if v is an object) returning true if .prototype is found.
 */
static JSBool
fun_hasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    while (obj->isFunction()) {
        if (!obj->isBoundFunction())
            break;
        obj = obj->getBoundFunctionTarget();
    }

    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    Value pval;
    if (!obj->getProperty(cx, id, &pval))
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

static void
fun_trace(JSTracer *trc, JSObject *obj)
{
    /* A newborn function object may have a not yet initialized private slot. */
    JSFunction *fun = (JSFunction *) obj->getPrivate();
    if (!fun)
        return;

    if (fun != obj) {
        /* obj is a cloned function object, trace the clone-parent, fun. */
        MarkObject(trc, *fun, "private");

        /* The function could be a flat closure with upvar copies in the clone. */
        if (FUN_FLAT_CLOSURE(fun) && fun->u.i.nupvars)
            MarkValueRange(trc, fun->u.i.nupvars, obj->getFlatClosureUpvars(), "upvars");
        return;
    }

    if (fun->atom)
        MarkString(trc, ATOM_TO_STRING(fun->atom), "atom");

    if (FUN_INTERPRETED(fun)) {
        if (fun->u.i.script)
            js_TraceScript(trc, fun->u.i.script);
        for (const Shape *shape = fun->u.i.names; shape; shape = shape->previous())
            shape->trace(trc);
    }
}

static void
fun_finalize(JSContext *cx, JSObject *obj)
{
    /* Ignore newborn function objects. */
    JSFunction *fun = (JSFunction *) obj->getPrivate();
    if (!fun)
        return;

    /* Cloned function objects may be flat closures with upvars to free. */
    if (fun != obj) {
        if (FUN_FLAT_CLOSURE(fun) && fun->u.i.nupvars != 0)
            cx->free((void *) obj->getFlatClosureUpvars());
        return;
    }

    /*
     * Null-check of u.i.script is required since the parser sets interpreted
     * very early.
     */
    if (FUN_INTERPRETED(fun) && fun->u.i.script)
        js_DestroyScriptFromGC(cx, fun->u.i.script, NULL);
}

int
JSFunction::sharpSlotBase(JSContext *cx)
{
#if JS_HAS_SHARP_VARS
    JSAtom *name = js_Atomize(cx, "#array", 6, 0);
    if (name) {
        uintN index = uintN(-1);
#ifdef DEBUG
        JSLocalKind kind =
#endif
            lookupLocal(cx, name, &index);
        JS_ASSERT(kind == JSLOCAL_VAR);
        return int(index);
    }
#endif
    return -1;
}

uint32
JSFunction::countUpvarSlots() const
{
    JS_ASSERT(FUN_INTERPRETED(this));

    return (u.i.nupvars == 0) ? 0 : u.i.script->upvars()->length;
}

/*
 * Reserve two slots in all function objects for XPConnect.  Note that this
 * does not bloat every instance, only those on which reserved slots are set,
 * and those on which ad-hoc properties are defined.
 */
JS_PUBLIC_DATA(Class) js_FunctionClass = {
    js_Function_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(JSFunction::CLASS_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    PropertyStub,   /* addProperty */
    PropertyStub,   /* delProperty */
    PropertyStub,   /* getProperty */
    PropertyStub,   /* setProperty */
    fun_enumerate,
    (JSResolveOp)fun_resolve,
    ConvertStub,
    fun_finalize,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    js_XDRFunctionObject,
    fun_hasInstance,
    JS_CLASS_TRACE(fun_trace)
};

JSString *
fun_toStringHelper(JSContext *cx, JSObject *obj, uintN indent)
{
    if (!obj->isFunction()) {
        if (obj->isFunctionProxy())
            return JSProxy::fun_toString(cx, obj, indent);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return NULL;
    }

    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, obj);
    if (!fun)
        return NULL;
    return JS_DecompileFunction(cx, fun, indent);
}

static JSBool
fun_toString(JSContext *cx, uintN argc, Value *vp)
{
    JS_ASSERT(IsFunctionObject(vp[0]));
    uint32_t indent = 0;

    if (argc != 0 && !ValueToECMAUint32(cx, vp[2], &indent))
        return false;

    JSObject *obj = ComputeThisFromVp(cx, vp);
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
fun_toSource(JSContext *cx, uintN argc, Value *vp)
{
    JS_ASSERT(IsFunctionObject(vp[0]));

    JSObject *obj = ComputeThisFromVp(cx, vp);
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
js_fun_call(JSContext *cx, uintN argc, Value *vp)
{
    LeaveTrace(cx);

    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return JS_FALSE;
    Value fval = vp[1];

    if (!js_IsCallable(fval)) {
        JSString *str = js_ValueToString(cx, fval);
        if (str) {
            JSAutoByteString bytes(cx, str);
            if (!!bytes) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INCOMPATIBLE_PROTO,
                                     js_Function_str, js_call_str,
                                     bytes.ptr());
            }
        }
        return JS_FALSE;
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
    if (!cx->stack().pushInvokeArgs(cx, argc, &args))
        return JS_FALSE;

    /* Push fval, thisv, and the args. */
    args.callee() = fval;
    args.thisv() = thisv;
    memcpy(args.argv(), argv, argc * sizeof *argv);

    bool ok = Invoke(cx, args, 0);
    *vp = args.rval();
    return ok;
}

/* ES5 15.3.4.3 */
JSBool
js_fun_apply(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ComputeThisFromVp(cx, vp);
    if (!obj)
        return false;

    /* Step 1. */
    Value fval = vp[1];
    if (!js_IsCallable(fval)) {
        if (JSString *str = js_ValueToString(cx, fval)) {
            JSAutoByteString bytes(cx, str);
            if (!!bytes) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INCOMPATIBLE_PROTO,
                                     js_Function_str, js_apply_str,
                                     bytes.ptr());
            }
        }
        return false;
    }

    /* Step 2. */
    if (argc < 2 || vp[3].isNullOrUndefined())
        return js_fun_call(cx, (argc > 0) ? 1 : 0, vp);

    /* N.B. Changes need to be propagated to stubs::SplatApplyArgs. */

    /* Step 3. */
    if (!vp[3].isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
        return false;
    }

    /*
     * Steps 4-5 (note erratum removing steps originally numbered 5 and 7 in
     * original version of ES5).
     */
    JSObject *aobj = &vp[3].toObject();
    jsuint length;
    if (!js_GetLengthProperty(cx, aobj, &length))
        return false;

    LeaveTrace(cx);

    /* Step 6. */
    uintN n = uintN(JS_MIN(length, JS_ARGS_LENGTH_MAX));

    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, n, &args))
        return false;

    /* Push fval, obj, and aobj's elements as args. */
    args.callee() = fval;
    args.thisv() = vp[2];

    /* Steps 7-8. */
    if (!GetElements(cx, aobj, n, args.argv()))
        return false;

    /* Step 9. */
    if (!Invoke(cx, args, 0))
        return false;
    *vp = args.rval();
    return true;
}

static JSBool
CallOrConstructBoundFunction(JSContext *cx, uintN argc, Value *vp);

inline bool
JSObject::initBoundFunction(JSContext *cx, const Value &thisArg,
                            const Value *args, uintN argslen)
{
    JS_ASSERT(isFunction());

    flags |= JSObject::BOUND_FUNCTION;
    getSlotRef(JSSLOT_BOUND_FUNCTION_THIS) = thisArg;
    getSlotRef(JSSLOT_BOUND_FUNCTION_ARGS_COUNT).setPrivateUint32(argslen);
    if (argslen != 0) {
        /* FIXME? Burn memory on an empty scope whose shape covers the args slots. */
        EmptyShape *empty = EmptyShape::create(cx, clasp);
        if (!empty)
            return false;

        empty->slotSpan += argslen;
        map = empty;

        if (!ensureInstanceReservedSlots(cx, argslen))
            return false;

        JS_ASSERT(numSlots() >= argslen + FUN_CLASS_RESERVED_SLOTS);
        memcpy(getSlots() + FUN_CLASS_RESERVED_SLOTS, args, argslen * sizeof(Value));
    }
    return true;
}

inline JSObject *
JSObject::getBoundFunctionTarget() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    /* Bound functions abuse |parent| to store their target function. */
    return getParent();
}

inline const js::Value &
JSObject::getBoundFunctionThis() const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_THIS);
}

inline const js::Value *
JSObject::getBoundFunctionArguments(uintN &argslen) const
{
    JS_ASSERT(isFunction());
    JS_ASSERT(isBoundFunction());

    argslen = getSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT).toPrivateUint32();
    JS_ASSERT_IF(argslen > 0, numSlots() >= argslen);

    return getSlots() + FUN_CLASS_RESERVED_SLOTS;
}

/* ES5 15.3.4.5.1 and 15.3.4.5.2. */
static JSBool
CallOrConstructBoundFunction(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = &vp[0].toObject();
    JS_ASSERT(obj->isFunction());
    JS_ASSERT(obj->isBoundFunction());

    LeaveTrace(cx);

    bool constructing = IsConstructing(vp);

    /* 15.3.4.5.1 step 1, 15.3.4.5.2 step 3. */
    uintN argslen;
    const Value *boundArgs = obj->getBoundFunctionArguments(argslen);

    if (argc + argslen > JS_ARGS_LENGTH_MAX) {
        js_ReportAllocationOverflow(cx);
        return false;
    }

    /* 15.3.4.5.1 step 3, 15.3.4.5.2 step 1. */
    JSObject *target = obj->getBoundFunctionTarget();

    /* 15.3.4.5.1 step 2. */
    const Value &boundThis = obj->getBoundFunctionThis();

    InvokeArgsGuard args;
    if (!cx->stack().pushInvokeArgs(cx, argc + argslen, &args))
        return false;

    /* 15.3.4.5.1, 15.3.4.5.2 step 4. */
    memcpy(args.argv(), boundArgs, argslen * sizeof(Value));
    memcpy(args.argv() + argslen, vp + 2, argc * sizeof(Value));

    /* 15.3.4.5.1, 15.3.4.5.2 step 5. */
    args.callee().setObject(*target);

    if (!constructing)
        args.thisv() = boundThis;

    if (constructing ? !InvokeConstructor(cx, args) : !Invoke(cx, args, 0))
        return false;

    *vp = args.rval();
    return true;
}

/* ES5 15.3.4.5. */
static JSBool
fun_bind(JSContext *cx, uintN argc, Value *vp)
{
    /* Step 1. */
    JSObject *target = ComputeThisFromVp(cx, vp);
    if (!target)
        return false;

    /* Step 2. */
    if (!target->isCallable()) {
        if (JSString *str = js_ValueToString(cx, vp[1])) {
            JSAutoByteString bytes(cx, str);
            if (!!bytes) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INCOMPATIBLE_PROTO,
                                     js_Function_str, "bind", bytes.ptr());
            }
        }
        return false;
    }

    /* Step 3. */
    Value *args = NULL;
    uintN argslen = 0;
    if (argc > 1) {
        args = vp + 3;
        argslen = argc - 1;
    }

    /* Steps 15-16. */
    uintN length = 0;
    if (target->isFunction()) {
        uintN nargs = target->getFunctionPrivate()->nargs;
        if (nargs > argslen)
            length = nargs - argslen;
    }

    /* Step 4-6, 10-11. */
    JSAtom *name = target->isFunction() ? target->getFunctionPrivate()->atom : NULL;

    /* NB: Bound functions abuse |parent| to store their target. */
    JSObject *funobj =
        js_NewFunction(cx, NULL, CallOrConstructBoundFunction, length,
                       JSFUN_CONSTRUCTOR, target, name);
    if (!funobj)
        return false;

    /* Steps 7-9. */
    Value thisArg = argc >= 1 ? vp[2] : UndefinedValue();
    if (!funobj->initBoundFunction(cx, thisArg, args, argslen))
        return false;

    /* Steps 17, 19-21 are handled by fun_resolve. */
    /* Step 18 is the default for new functions. */

    /* Step 22. */
    vp->setObject(*funobj);
    return true;
}

static JSFunctionSpec function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      js_fun_apply,   2,0),
    JS_FN(js_call_str,       js_fun_call,    1,0),
    JS_FN("bind",            fun_bind,       1,0),
    JS_FS_END
};

static JSBool
Function(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = NewFunction(cx, NULL);
    if (!obj)
        return JS_FALSE;

    /* N.B. overwriting callee with return value */
    JSObject *parent = vp[0].toObject().getParent();
    vp[0].setObject(*obj);

    /*
     * NB: (new Function) is not lexically closed by its caller, it's just an
     * anonymous function in the top-level scope that its constructor inhabits.
     * Thus 'var x = 42; f = new Function("return x"); print(f())' prints 42,
     * and so would a call to f from another top-level's script or function.
     *
     * In older versions, before call objects, a new Function was adopted by
     * its running context's globalObject, which might be different from the
     * top-level reachable from scopeChain (in HTML frames, e.g.).
     */
    JSFunction *fun = js_NewFunction(cx, obj, NULL, 0, JSFUN_LAMBDA | JSFUN_INTERPRETED,
                                     parent, cx->runtime->atomState.anonymousAtom);
    if (!fun)
        return JS_FALSE;

    /*
     * Function is static and not called directly by other functions in this
     * file, therefore it is callable only as a native function by js_Invoke.
     * Find the scripted caller, possibly skipping other native frames such as
     * are built for Function.prototype.call or .apply activations that invoke
     * Function indirectly from a script.
     */
    JSStackFrame *caller = js_GetScriptedCaller(cx, NULL);
    uintN lineno;
    const char *filename;
    JSPrincipals *principals;
    if (caller) {
        JSObject *callee = &JS_CALLEE(cx, vp).toObject();
        principals = js_EvalFramePrincipals(cx, callee, caller);
        filename = js_ComputeFilename(cx, caller, principals, &lineno);
    } else {
        filename = NULL;
        lineno = 0;
        principals = NULL;
    }

    /* Belt-and-braces: check that the caller has access to parent. */
    if (!js_CheckPrincipalsAccess(cx, parent, principals,
                                  CLASS_ATOM(cx, Function))) {
        return JS_FALSE;
    }

    /*
     * CSP check: whether new Function() is allowed at all.
     * Report errors via CSP is done in the script security manager.
     * js_CheckContentSecurityPolicy is defined in jsobj.cpp
     */
    if (!js_CheckContentSecurityPolicy(cx)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_FUNCTION);
        return JS_FALSE;
    }

    Value *argv = vp + 2;
    uintN n = argc ? argc - 1 : 0;
    if (n > 0) {
        enum { OK, BAD, BAD_FORMAL } state;

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
        state = BAD_FORMAL;
        size_t args_length = 0;
        for (uintN i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            JSString *arg = js_ValueToString(cx, argv[i]);
            if (!arg)
                return JS_FALSE;
            argv[i].setString(arg);

            /*
             * Check for overflow.  The < test works because the maximum
             * JSString length fits in 2 fewer bits than size_t has.
             */
            size_t old_args_length = args_length;
            args_length = old_args_length + arg->length();
            if (args_length < old_args_length) {
                js_ReportAllocationOverflow(cx);
                return JS_FALSE;
            }
        }

        /* Add 1 for each joining comma and check for overflow (two ways). */
        size_t old_args_length = args_length;
        args_length = old_args_length + n - 1;
        if (args_length < old_args_length ||
            args_length >= ~(size_t)0 / sizeof(jschar)) {
            js_ReportAllocationOverflow(cx);
            return JS_FALSE;
        }

        /*
         * Allocate a string to hold the concatenated arguments, including room
         * for a terminating 0.  Mark cx->tempPool for later release, to free
         * collected_args and its tokenstream in one swoop.
         */
        void *mark = JS_ARENA_MARK(&cx->tempPool);
        jschar *cp;
        JS_ARENA_ALLOCATE_CAST(cp, jschar *, &cx->tempPool,
                               (args_length+1) * sizeof(jschar));
        if (!cp) {
            js_ReportOutOfScriptQuota(cx);
            return JS_FALSE;
        }
        jschar *collected_args = cp;

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (uintN i = 0; i < n; i++) {
            JSString *arg = argv[i].toString();
            size_t arg_length = arg->length();
            (void) js_strncpy(cp, arg->chars(), arg_length);
            cp += arg_length;

            /* Add separating comma or terminating 0. */
            *cp++ = (i + 1 < n) ? ',' : 0;
        }

        /* Initialize a tokenstream that reads from the given string. */
        TokenStream ts(cx);
        if (!ts.init(cx->findVersion(), collected_args, args_length, filename, lineno)) {
            JS_ARENA_RELEASE(&cx->tempPool, mark);
            return JS_FALSE;
        }

        /* The argument string may be empty or contain no tokens. */
        TokenKind tt = ts.getToken();
        if (tt != TOK_EOF) {
            for (;;) {
                /*
                 * Check that it's a name.  This also implicitly guards against
                 * TOK_ERROR, which was already reported.
                 */
                if (tt != TOK_NAME)
                    goto after_args;

                /*
                 * Get the atom corresponding to the name from the token
                 * stream; we're assured at this point that it's a valid
                 * identifier.
                 */
                JSAtom *atom = ts.currentToken().t_atom;

                /* Check for a duplicate parameter name. */
                if (fun->lookupLocal(cx, atom, NULL) != JSLOCAL_NONE) {
                    JSAutoByteString name;
                    if (!js_AtomToPrintableString(cx, atom, &name)) {
                        state = BAD;
                        goto after_args;
                    }
                    if (!ReportCompileErrorNumber(cx, &ts, NULL,
                                                  JSREPORT_WARNING | JSREPORT_STRICT,
                                                  JSMSG_DUPLICATE_FORMAL, name.ptr())) {
                        goto after_args;
                    }
                }
                if (!fun->addLocal(cx, atom, JSLOCAL_ARG))
                    goto after_args;

                /*
                 * Get the next token.  Stop on end of stream.  Otherwise
                 * insist on a comma, get another name, and iterate.
                 */
                tt = ts.getToken();
                if (tt == TOK_EOF)
                    break;
                if (tt != TOK_COMMA)
                    goto after_args;
                tt = ts.getToken();
            }
        }

        state = OK;
      after_args:
        if (state == BAD_FORMAL && !ts.isError()) {
            /*
             * Report "malformed formal parameter" iff no illegal char or
             * similar scanner error was already reported.
             */
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_FORMAL);
        }
        ts.close();
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (state != OK)
            return JS_FALSE;
    }

    JSString *str;
    if (argc) {
        str = js_ValueToString(cx, argv[argc-1]);
        if (!str)
            return JS_FALSE;
        argv[argc-1].setString(str);
    } else {
        str = cx->runtime->emptyString;
    }

    return Compiler::compileFunctionBody(cx, fun, principals,
                                         str->chars(), str->length(),
                                         filename, lineno);
}

static JSBool
ThrowTypeError(JSContext *cx, uintN argc, Value *vp)
{
    JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                 JSMSG_THROW_TYPE_ERROR);
    return false;
}

JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto = js_InitClass(cx, obj, NULL, &js_FunctionClass, Function, 1,
                                   NULL, function_methods, NULL, NULL);
    if (!proto)
        return NULL;

    JSFunction *fun = js_NewFunction(cx, proto, NULL, 0, JSFUN_INTERPRETED, obj, NULL);
    if (!fun)
        return NULL;
    fun->flags |= JSFUN_PROTOTYPE;

    JSScript *script = JSScript::NewScript(cx, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    if (!script)
        return NULL;
    script->setVersion(JSVERSION_DEFAULT);
    script->noScriptRval = true;
    script->code[0] = JSOP_STOP;
    script->code[1] = SRC_NULL;
#ifdef CHECK_SCRIPT_OWNER
    script->owner = NULL;
#endif
    fun->u.i.script = script;

    if (obj->getClass()->flags & JSCLASS_IS_GLOBAL) {
        /* ES5 13.2.3: Construct the unique [[ThrowTypeError]] function object. */
        JSObject *throwTypeError =
            js_NewFunction(cx, NULL, reinterpret_cast<Native>(ThrowTypeError), 0,
                           0, obj, NULL);
        if (!throwTypeError)
            return NULL;

        JS_ALWAYS_TRUE(js_SetReservedSlot(cx, obj, JSRESERVED_GLOBAL_THROWTYPEERROR,
                                          ObjectValue(*throwTypeError)));
    }

    return proto;
}

JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, Native native, uintN nargs,
               uintN flags, JSObject *parent, JSAtom *atom)
{
    JSFunction *fun;

    if (funobj) {
        JS_ASSERT(funobj->isFunction());
        funobj->setParent(parent);
    } else {
        funobj = NewFunction(cx, parent);
        if (!funobj)
            return NULL;
    }
    JS_ASSERT(!funobj->getPrivate());
    fun = (JSFunction *) funobj;

    /* Initialize all function members. */
    fun->nargs = uint16(nargs);
    fun->flags = flags & (JSFUN_FLAGS_MASK | JSFUN_KINDMASK | JSFUN_TRCINFO);
    if ((flags & JSFUN_KINDMASK) >= JSFUN_INTERPRETED) {
        JS_ASSERT(!native);
        JS_ASSERT(nargs == 0);
        fun->u.i.nvars = 0;
        fun->u.i.nupvars = 0;
        fun->u.i.skipmin = 0;
        fun->u.i.wrapper = false;
        fun->u.i.script = NULL;
        fun->u.i.names = cx->runtime->emptyCallShape;
    } else {
        fun->u.n.clasp = NULL;
        if (flags & JSFUN_TRCINFO) {
#ifdef JS_TRACER
            JSNativeTraceInfo *trcinfo =
                JS_FUNC_TO_DATA_PTR(JSNativeTraceInfo *, native);
            fun->u.n.native = (js::Native) trcinfo->native;
            fun->u.n.trcinfo = trcinfo;
#else
            fun->u.n.trcinfo = NULL;
#endif
        } else {
            fun->u.n.native = native;
            fun->u.n.trcinfo = NULL;
        }
        JS_ASSERT(fun->u.n.native);
    }
    fun->atom = atom;

    /* Set private to self to indicate non-cloned fully initialized function. */
    FUN_OBJECT(fun)->setPrivate(fun);
    return fun;
}

JSObject * JS_FASTCALL
js_CloneFunctionObject(JSContext *cx, JSFunction *fun, JSObject *parent,
                       JSObject *proto)
{
    JS_ASSERT(parent);
    JS_ASSERT(proto);

    JSObject *clone;
    if (cx->compartment == fun->compartment()) {
        /*
         * The cloned function object does not need the extra JSFunction members
         * beyond JSObject as it points to fun via the private slot.
         */
        clone = NewNativeClassInstance(cx, &js_FunctionClass, proto, parent);
        if (!clone)
            return NULL;
        clone->setPrivate(fun);
    } else {
        /*
         * Across compartments we have to deep copy JSFunction and clone the
         * script (for interpreted functions).
         */
        clone = NewFunction(cx, parent);
        if (!clone)
            return NULL;
        JSFunction *cfun = (JSFunction *) clone;
        cfun->nargs = fun->nargs;
        cfun->flags = fun->flags;
        cfun->u = fun->getFunctionPrivate()->u;
        cfun->atom = fun->atom;
        clone->setPrivate(cfun);
        if (cfun->isInterpreted()) {
            JSScript *script = cfun->u.i.script;
            JS_ASSERT(script);
            JS_ASSERT(script->compartment == fun->compartment());
            JS_ASSERT(script->compartment != cx->compartment);

            cfun->u.i.script = js_CloneScript(cx, script);
            if (!cfun->u.i.script)
                return NULL;
#ifdef CHECK_SCRIPT_OWNER
            cfun->script()->owner = NULL;
#endif
            js_CallNewScriptHook(cx, cfun->script(), cfun);
        }
    }
    return clone;
}

#ifdef JS_TRACER
JS_DEFINE_CALLINFO_4(extern, OBJECT, js_CloneFunctionObject, CONTEXT, FUNCTION, OBJECT, OBJECT, 0,
                     nanojit::ACCSET_STORE_ANY)
#endif

/*
 * Create a new flat closure, but don't initialize the imported upvar
 * values. The tracer calls this function and then initializes the upvar
 * slots on trace.
 */
JSObject * JS_FASTCALL
js_AllocFlatClosure(JSContext *cx, JSFunction *fun, JSObject *scopeChain)
{
    JS_ASSERT(FUN_FLAT_CLOSURE(fun));
    JS_ASSERT((fun->u.i.script->upvarsOffset
               ? fun->u.i.script->upvars()->length
               : 0) == fun->u.i.nupvars);

    JSObject *closure = CloneFunctionObject(cx, fun, scopeChain);
    if (!closure)
        return closure;

    uint32 nslots = fun->countUpvarSlots();
    if (nslots == 0)
        return closure;

    Value *upvars = (Value *) cx->malloc(nslots * sizeof(Value));
    if (!upvars)
        return NULL;

    closure->setFlatClosureUpvars(upvars);
    return closure;
}

JS_DEFINE_CALLINFO_3(extern, OBJECT, js_AllocFlatClosure,
                     CONTEXT, FUNCTION, OBJECT, 0, nanojit::ACCSET_STORE_ANY)

JS_REQUIRES_STACK JSObject *
js_NewFlatClosure(JSContext *cx, JSFunction *fun, JSOp op, size_t oplen)
{
    /*
     * Flat closures can be partial, they may need to search enclosing scope
     * objects via JSOP_NAME, etc.
     */
    JSObject *scopeChain = GetScopeChainFast(cx, cx->fp(), op, oplen);
    if (!scopeChain)
        return NULL;

    JSObject *closure = js_AllocFlatClosure(cx, fun, scopeChain);
    if (!closure || fun->u.i.nupvars == 0)
        return closure;

    Value *upvars = closure->getFlatClosureUpvars();
    uintN level = fun->u.i.script->staticLevel;
    JSUpvarArray *uva = fun->u.i.script->upvars();

    for (uint32 i = 0, n = uva->length; i < n; i++)
        upvars[i] = GetUpvar(cx, level, uva->vector[i]);

    return closure;
}

JSObject *
js_NewDebuggableFlatClosure(JSContext *cx, JSFunction *fun)
{
    JS_ASSERT(cx->fp()->fun()->flags & JSFUN_HEAVYWEIGHT);
    JS_ASSERT(!cx->fp()->fun()->optimizedClosure());
    JS_ASSERT(FUN_FLAT_CLOSURE(fun));

    return WrapEscapingClosure(cx, cx->fp(), fun);
}

JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, jsid id, Native native,
                  uintN nargs, uintN attrs)
{
    PropertyOp gsop;
    JSFunction *fun;

    if (attrs & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        attrs &= ~JSFUN_STUB_GSOPS;
        gsop = PropertyStub;
    } else {
        gsop = NULL;
    }

    /*
     * Historically, all objects have had a parent member as intrinsic scope
     * chain link. We want to move away from this universal parent, but JS
     * requires that function objects have something like parent (ES3 and ES5
     * call it the [[Scope]] internal property), to bake a particular static
     * scope environment into each function object.
     *
     * All function objects thus have parent, including all native functions.
     * All native functions defined by the JS_DefineFunction* APIs are created
     * via the call below to js_NewFunction, which passes obj as the parent
     * parameter, and so binds fun's parent to obj using JSObject::setParent,
     * under js_NewFunction (in JSObject::init, called from NewObject -- see
     * jsobjinlines.h).
     *
     * But JSObject::setParent sets the DELEGATE object flag on its receiver,
     * to mark the object as a proto or parent of another object. Such objects
     * may intervene in property lookups and scope chain searches, so require
     * special handling when caching lookup and search results (since such
     * intervening objects can in general grow shadowing properties later).
     *
     * Thus using setParent prematurely flags certain objects, notably class
     * prototypes, so that defining native methods on them, where the method's
     * name (e.g., toString) is already bound on Object.prototype, triggers
     * shadowingShapeChange events and gratuitous shape regeneration.
     *
     * To fix this longstanding bug, we set check whether obj is already a
     * delegate, and if not, then if js_NewFunction flagged obj as a delegate,
     * we clear the flag.
     *
     * We thus rely on the fact that native functions (including indirect eval)
     * do not use the property cache or equivalent JIT techniques that require
     * this bit to be set on their parent-linked scope chain objects.
     *
     * Note: we keep API compatibility by setting parent to obj for all native
     * function objects, even if obj->getGlobal() would suffice. This should be
     * revisited when parent is narrowed to exist only for function objects and
     * possibly a few prehistoric scope objects (e.g. event targets).
     *
     * FIXME: bug 611190.
     */
    bool wasDelegate = obj->isDelegate();

    fun = js_NewFunction(cx, NULL, native, nargs,
                         attrs & (JSFUN_FLAGS_MASK | JSFUN_TRCINFO),
                         obj,
                         JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : NULL);
    if (!fun)
        return NULL;

    if (!wasDelegate && obj->isDelegate())
        obj->clearDelegate();

    if (!obj->defineProperty(cx, id, ObjectValue(*fun), gsop, gsop, attrs & ~JSFUN_FLAGS_MASK))
        return NULL;
    return fun;
}

#if (JSV2F_CONSTRUCT & JSV2F_SEARCH_STACK)
# error "JSINVOKE_CONSTRUCT and JSV2F_SEARCH_STACK are not disjoint!"
#endif

JSFunction *
js_ValueToFunction(JSContext *cx, const Value *vp, uintN flags)
{
    JSObject *funobj;
    if (!IsFunctionObject(*vp, &funobj)) {
        js_ReportIsNotFunction(cx, vp, flags);
        return NULL;
    }
    return GET_FUNCTION_PRIVATE(cx, funobj);
}

JSObject *
js_ValueToFunctionObject(JSContext *cx, Value *vp, uintN flags)
{
    JSObject *funobj;
    if (!IsFunctionObject(*vp, &funobj)) {
        js_ReportIsNotFunction(cx, vp, flags);
        return NULL;
    }

    return funobj;
}

JSObject *
js_ValueToCallableObject(JSContext *cx, Value *vp, uintN flags)
{
    if (vp->isObject()) {
        JSObject *callable = &vp->toObject();
        if (callable->isCallable())
            return callable;
    }

    js_ReportIsNotFunction(cx, vp, flags);
    return NULL;
}

void
js_ReportIsNotFunction(JSContext *cx, const Value *vp, uintN flags)
{
    const char *name = NULL, *source = NULL;
    AutoValueRooter tvr(cx);
    uintN error = (flags & JSV2F_CONSTRUCT) ? JSMSG_NOT_CONSTRUCTOR : JSMSG_NOT_FUNCTION;
    LeaveTrace(cx);

    /*
     * We try to the print the code that produced vp if vp is a value in the
     * most recent interpreted stack frame. Note that additional values, not
     * directly produced by the script, may have been pushed onto the frame's
     * expression stack (e.g. by pushInvokeArgs) thereby incrementing sp past
     * the depth simulated by ReconstructPCStack. Since we must pass an offset
     * from the top of the simulated stack to js_ReportValueError3, it is
     * important to do bounds checking using the simulated, rather than actual,
     * stack depth.
     */
    ptrdiff_t spindex = 0;

    FrameRegsIter i(cx);
    while (!i.done() && !i.pc())
        ++i;

    if (!i.done()) {
        uintN depth = js_ReconstructStackDepth(cx, i.fp()->script(), i.pc());
        Value *simsp = i.fp()->base() + depth;
        JS_ASSERT(simsp <= i.sp());
        if (i.fp()->base() <= vp && vp < simsp)
            spindex = vp - simsp;
    }

    if (!spindex)
        spindex = ((flags & JSV2F_SEARCH_STACK) ? JSDVG_SEARCH_STACK : JSDVG_IGNORE_STACK);

    js_ReportValueError3(cx, error, spindex, *vp, NULL, name, source);
}

const Shape *
JSFunction::lastArg() const
{
    const Shape *shape = lastVar();
    if (u.i.nvars != 0) {
        while (shape->previous() && shape->getter() != GetCallArg)
            shape = shape->previous();
    }
    return shape;
}

const Shape *
JSFunction::lastVar() const
{
    const Shape *shape = u.i.names;
    if (u.i.nupvars != 0) {
        while (shape->getter() == GetFlatUpvar)
            shape = shape->previous();
    }
    return shape;
}

bool
JSFunction::addLocal(JSContext *cx, JSAtom *atom, JSLocalKind kind)
{
    JS_ASSERT(FUN_INTERPRETED(this));
    JS_ASSERT(!u.i.script);

    /*
     * We still follow 10.2.3 of ES3 and make argument and variable properties
     * of the Call objects enumerable. ES5 reformulated all of its Clause 10 to
     * avoid objects as activations, something we should do too.
     */
    uintN attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED;
    uint16 *indexp;
    PropertyOp getter, setter;
    uint32 slot = JSObject::CALL_RESERVED_SLOTS;

    if (kind == JSLOCAL_ARG) {
        JS_ASSERT(u.i.nupvars == 0);

        indexp = &nargs;
        getter = GetCallArg;
        setter = SetCallArg;
        slot += nargs;
    } else if (kind == JSLOCAL_UPVAR) {
        indexp = &u.i.nupvars;
        getter = GetFlatUpvar;
        setter = SetFlatUpvar;
        slot = SHAPE_INVALID_SLOT;
    } else {
        JS_ASSERT(u.i.nupvars == 0);

        indexp = &u.i.nvars;
        getter = GetCallVar;
        setter = SetCallVar;
        if (kind == JSLOCAL_CONST)
            attrs |= JSPROP_READONLY;
        else
            JS_ASSERT(kind == JSLOCAL_VAR);
        slot += nargs + u.i.nvars;
    }

    if (*indexp == JS_BITMASK(16)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             (kind == JSLOCAL_ARG)
                             ? JSMSG_TOO_MANY_FUN_ARGS
                             : JSMSG_TOO_MANY_LOCALS);
        return false;
    }

    Shape **listp = &u.i.names;
    Shape *parent = *listp;
    jsid id;

    /*
     * The destructuring formal parameter parser adds a null atom, which we
     * encode as an INT id. The parser adds such locals after adding vars for
     * the destructured-to parameter bindings -- those must be vars to avoid
     * aliasing arguments[i] for any i -- so we must switch u.i.names to a
     * dictionary list to cope with insertion "in the middle" of an index-named
     * shape for the object or array argument.
     */
    bool findArgInsertionPoint = false;
    if (!atom) {
        JS_ASSERT(kind == JSLOCAL_ARG);
        if (u.i.nvars != 0) {
            /*
             * A dictionary list needed only if the destructing pattern wasn't
             * empty, i.e., there were vars for its destructured-to bindings.
             */
            if (!parent->inDictionary() && !(parent = Shape::newDictionaryList(cx, listp)))
                return false;
            findArgInsertionPoint = true;
        }
        id = INT_TO_JSID(nargs);
    } else {
        if (kind == JSLOCAL_ARG && parent->inDictionary())
            findArgInsertionPoint = true;
        id = ATOM_TO_JSID(atom);
    }

    if (findArgInsertionPoint) {
        while (parent->parent && parent->getter() != GetCallArg) {
            ++parent->slot;
            JS_ASSERT(parent->slot == parent->slotSpan);
            ++parent->slotSpan;
            listp = &parent->parent;
            parent = *listp;
        }
    }

    Shape child(id, getter, setter, slot, attrs, Shape::HAS_SHORTID, *indexp);

    Shape *shape = parent->getChild(cx, child, listp);
    if (!shape)
        return false;

    JS_ASSERT_IF(!shape->inDictionary(), u.i.names == shape);
    ++*indexp;
    return true;
}

JSLocalKind
JSFunction::lookupLocal(JSContext *cx, JSAtom *atom, uintN *indexp)
{
    JS_ASSERT(FUN_INTERPRETED(this));

    Shape *shape = SHAPE_FETCH(Shape::search(&u.i.names, ATOM_TO_JSID(atom)));
    if (shape) {
        JSLocalKind localKind;

        if (shape->getter() == GetCallArg)
            localKind = JSLOCAL_ARG;
        else if (shape->getter() == GetFlatUpvar)
            localKind = JSLOCAL_UPVAR;
        else if (!shape->writable())
            localKind = JSLOCAL_CONST;
        else
            localKind = JSLOCAL_VAR;

        if (indexp)
            *indexp = shape->shortid;
        return localKind;
    }
    return JSLOCAL_NONE;
}

jsuword *
JSFunction::getLocalNameArray(JSContext *cx, JSArenaPool *pool)
{
    JS_ASSERT(hasLocalNames());

    uintN n = countLocalNames();
    jsuword *names;

    /*
     * No need to check for overflow of the allocation size as we are making a
     * copy of already allocated data. As such it must fit size_t.
     */
    JS_ARENA_ALLOCATE_CAST(names, jsuword *, pool, size_t(n) * sizeof *names);
    if (!names) {
        js_ReportOutOfScriptQuota(cx);
        return NULL;
    }

#ifdef DEBUG
    for (uintN i = 0; i != n; i++)
        names[i] = 0xdeadbeef;
#endif

    for (Shape::Range r = u.i.names; !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        uintN index = uint16(shape.shortid);
        jsuword constFlag = 0;

        if (shape.getter() == GetCallArg) {
            JS_ASSERT(index < nargs);
        } else if (shape.getter() == GetFlatUpvar) {
            JS_ASSERT(index < u.i.nupvars);
            index += nargs + u.i.nvars;
        } else {
            JS_ASSERT(index < u.i.nvars);
            index += nargs;
            if (!shape.writable())
                constFlag = 1;
        }

        JSAtom *atom;
        if (JSID_IS_ATOM(shape.id)) {
            atom = JSID_TO_ATOM(shape.id);
        } else {
            JS_ASSERT(JSID_IS_INT(shape.id));
            JS_ASSERT(shape.getter() == GetCallArg);
            atom = NULL;
        }

        names[index] = jsuword(atom);
    }

#ifdef DEBUG
    for (uintN i = 0; i != n; i++)
        JS_ASSERT(names[i] != 0xdeadbeef);
#endif
    return names;
}

void
JSFunction::freezeLocalNames(JSContext *cx)
{
    JS_ASSERT(FUN_INTERPRETED(this));

    Shape *shape = u.i.names;
    if (shape->inDictionary()) {
        do {
            JS_ASSERT(!shape->frozen());
            shape->setFrozen();
        } while ((shape = shape->parent) != NULL);
    }
}

/*
 * This method is called only if we parsed a duplicate formal. Let's use the
 * simplest possible algorithm, risking O(n^2) pain -- anyone dup'ing formals
 * is asking for it!
 */
JSAtom *
JSFunction::findDuplicateFormal() const
{
    JS_ASSERT(isInterpreted());

    if (nargs <= 1)
        return NULL;

    for (Shape::Range r = lastArg(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();
        for (Shape::Range r2 = shape.previous(); !r2.empty(); r2.popFront()) {
            if (r2.front().id == shape.id)
                return JSID_TO_ATOM(shape.id);
        }
    }
    return NULL;
}
