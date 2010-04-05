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
#include "jsutil.h" /* Added by JSIFY */
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

#include "jsatominlines.h"
#include "jsobjinlines.h"

using namespace js;

static inline void
SetOverriddenArgsLength(JSObject *obj)
{
    JS_ASSERT(obj->isArguments());

    jsval v = obj->fslots[JSSLOT_ARGS_LENGTH];
    v = INT_TO_JSVAL(JSVAL_TO_INT(v) | 1);
    JS_ASSERT(JSVAL_IS_INT(v));
    obj->fslots[JSSLOT_ARGS_LENGTH] = v;
}

static inline void
InitArgsLengthSlot(JSObject *obj, uint32 argc)
{
    JS_ASSERT(obj->isArguments());
    JS_ASSERT(argc <= JS_ARGS_LENGTH_MAX);
    JS_ASSERT(obj->fslots[JSSLOT_ARGS_LENGTH] == JSVAL_VOID);
    obj->fslots[JSSLOT_ARGS_LENGTH] = INT_TO_JSVAL(argc << 1);
    JS_ASSERT(!IsOverriddenArgsLength(obj));
}

static inline void
SetArgsPrivateNative(JSObject *argsobj, ArgsPrivateNative *apn)
{
    JS_ASSERT(argsobj->isArguments());
    uintptr_t p = (uintptr_t) apn;
    argsobj->setPrivate((void*) (p | 2));
}

JSBool
js_GetArgsValue(JSContext *cx, JSStackFrame *fp, jsval *vp)
{
    JSObject *argsobj;

    if (fp->flags & JSFRAME_OVERRIDE_ARGS) {
        JS_ASSERT(fp->callobj);
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
        return fp->callobj->getProperty(cx, id, vp);
    }
    argsobj = js_GetArgsObject(cx, fp);
    if (!argsobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(argsobj);
    return JS_TRUE;
}

JSBool
js_GetArgsProperty(JSContext *cx, JSStackFrame *fp, jsid id, jsval *vp)
{
    if (fp->flags & JSFRAME_OVERRIDE_ARGS) {
        JS_ASSERT(fp->callobj);

        jsid argumentsid = ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
        jsval v;
        if (!fp->callobj->getProperty(cx, argumentsid, &v))
            return false;

        JSObject *obj;
        if (JSVAL_IS_PRIMITIVE(v)) {
            obj = js_ValueToNonNullObject(cx, v);
            if (!obj)
                return false;
        } else {
            obj = JSVAL_TO_OBJECT(v);
        }
        return obj->getProperty(cx, id, vp);
    }

    *vp = JSVAL_VOID;
    if (JSID_IS_INT(id)) {
        uint32 arg = uint32(JSID_TO_INT(id));
        JSObject *argsobj = JSVAL_TO_OBJECT(fp->argsobj);
        if (arg < fp->argc) {
            if (argsobj) {
                jsval v = GetArgsSlot(argsobj, arg);
                if (v == JSVAL_HOLE)
                    return argsobj->getProperty(cx, id, vp);
            }
            *vp = fp->argv[arg];
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
    } else if (id == ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)) {
        JSObject *argsobj = JSVAL_TO_OBJECT(fp->argsobj);
        if (argsobj && IsOverriddenArgsLength(argsobj))
            return argsobj->getProperty(cx, id, vp);
        *vp = INT_TO_JSVAL(jsint(fp->argc));
    }
    return true;
}

static JSObject *
NewArguments(JSContext *cx, JSObject *parent, uint32 argc, JSObject *callee)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, parent, JSProto_Object, &proto))
        return NULL;

    JSObject *argsobj = js_NewGCObject(cx);
    if (!argsobj)
        return NULL;

    /* Init immediately to avoid GC seeing a half-init'ed object. */
    argsobj->init(&js_ArgumentsClass, proto, parent, JSVAL_NULL);
    argsobj->fslots[JSSLOT_ARGS_CALLEE] = OBJECT_TO_JSVAL(callee);
    InitArgsLengthSlot(argsobj, argc);

    argsobj->map = cx->runtime->emptyArgumentsScope;
    cx->runtime->emptyArgumentsScope->hold();

    /* This must come after argsobj->map has been set. */
    if (!js_EnsureReservedSlots(cx, argsobj, argc))
        return NULL;
    return argsobj;
}

static void
PutArguments(JSContext *cx, JSObject *argsobj, jsval *args)
{
    uint32 argc = GetArgsLength(argsobj);
    for (uint32 i = 0; i != argc; ++i) {
        jsval v = argsobj->dslots[i];
        if (v != JSVAL_HOLE)
            argsobj->dslots[i] = args[i];
    }
}

JSObject *
js_GetArgsObject(JSContext *cx, JSStackFrame *fp)
{
    /*
     * We must be in a function activation; the function must be lightweight
     * or else fp must have a variable object.
     */
    JS_ASSERT(fp->fun);
    JS_ASSERT_IF(fp->fun->flags & JSFUN_HEAVYWEIGHT,
                 fp->varobj(cx->containingCallStack(fp)));

    /* Skip eval and debugger frames. */
    while (fp->flags & JSFRAME_SPECIAL)
        fp = fp->down;

    /* Create an arguments object for fp only if it lacks one. */
    JSObject *argsobj = JSVAL_TO_OBJECT(fp->argsobj);
    if (argsobj)
        return argsobj;

    /*
     * Give arguments an intrinsic scope chain link to fp's global object.
     * Since the arguments object lacks a prototype because js_ArgumentsClass
     * is not initialized, js_NewObject won't assign a default parent to it.
     *
     * Therefore if arguments is used as the head of an eval scope chain (via
     * a direct or indirect call to eval(program, arguments)), any reference
     * to a standard class object in the program will fail to resolve due to
     * js_GetClassPrototype not being able to find a global object containing
     * the standard prototype by starting from arguments and following parent.
     */
    JSObject *global = fp->scopeChain;
    while (JSObject *parent = global->getParent())
        global = parent;

    JS_ASSERT(fp->argv);
    argsobj = NewArguments(cx, global, fp->argc, JSVAL_TO_OBJECT(fp->argv[-2]));
    if (!argsobj)
        return argsobj;

    /* Link the new object to fp so it can get actual argument values. */
    argsobj->setPrivate(fp);
    fp->argsobj = OBJECT_TO_JSVAL(argsobj);
    return argsobj;
}

void
js_PutArgsObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *argsobj = JSVAL_TO_OBJECT(fp->argsobj);
    JS_ASSERT(argsobj->getPrivate() == fp);
    PutArguments(cx, argsobj, fp->argv);
    argsobj->setPrivate(NULL);
    fp->argsobj = JSVAL_NULL;
}

/*
 * Traced versions of js_GetArgsObject and js_PutArgsObject.
 */

#ifdef JS_TRACER
JSObject * JS_FASTCALL
js_Arguments(JSContext *cx, JSObject *parent, uint32 argc, JSObject *callee,
             double *argv, ArgsPrivateNative *apn)
{
    JSObject *argsobj = NewArguments(cx, parent, argc, callee);
    if (!argsobj)
        return NULL;
    apn->argv = argv;
    SetArgsPrivateNative(argsobj, apn);
    return argsobj;
}
#endif

JS_DEFINE_CALLINFO_6(extern, OBJECT, js_Arguments, CONTEXT, OBJECT, UINT32, OBJECT,
                     DOUBLEPTR, APNPTR, 0, nanojit::ACC_STORE_ANY)

/* FIXME change the return type to void. */
JSBool JS_FASTCALL
js_PutArguments(JSContext *cx, JSObject *argsobj, jsval *args)
{
    JS_ASSERT(GetArgsPrivateNative(argsobj));
    PutArguments(cx, argsobj, args);
    argsobj->setPrivate(NULL);
    return true;
}

JS_DEFINE_CALLINFO_3(extern, BOOL, js_PutArguments, CONTEXT, OBJECT, JSVALPTR, 0,
                     nanojit::ACC_STORE_ANY)

static JSBool
args_delProperty(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    JS_ASSERT(obj->isArguments());

    if (JSVAL_IS_INT(idval)) {
        uintN arg = uintN(JSVAL_TO_INT(idval));
        if (arg < GetArgsLength(obj))
            SetArgsSlot(obj, arg, JSVAL_HOLE);
    } else if (idval == ATOM_KEY(cx->runtime->atomState.lengthAtom)) {
        SetOverriddenArgsLength(obj);
    } else if (idval == ATOM_KEY(cx->runtime->atomState.calleeAtom)) {
        obj->fslots[JSSLOT_ARGS_CALLEE] = JSVAL_HOLE;
    }
    return true;
}

static JS_REQUIRES_STACK JSObject *
WrapEscapingClosure(JSContext *cx, JSStackFrame *fp, JSObject *funobj, JSFunction *fun)
{
    JS_ASSERT(GET_FUNCTION_PRIVATE(cx, funobj) == fun);
    JS_ASSERT(fun->optimizedClosure());
    JS_ASSERT(!fun->u.i.wrapper);

    /*
     * We do not attempt to reify Call and Block objects on demand for outer
     * scopes. This could be done (see the "v8" patch in bug 494235) but it is
     * fragile in the face of ongoing compile-time optimization. Instead, the
     * _DBG* opcodes used by wrappers created here must cope with unresolved
     * upvars and throw them as reference errors. Caveat debuggers!
     */
    JSObject *scopeChain = js_GetScopeChain(cx, fp);
    if (!scopeChain)
        return NULL;

    JSObject *wfunobj = js_NewObjectWithGivenProto(cx, &js_FunctionClass,
                                                   funobj, scopeChain);
    if (!wfunobj)
        return NULL;
    AutoValueRooter tvr(cx, wfunobj);

    JSFunction *wfun = (JSFunction *) wfunobj;
    wfunobj->setPrivate(wfun);
    wfun->nargs = 0;
    wfun->flags = fun->flags | JSFUN_HEAVYWEIGHT;
    wfun->u.i.nvars = 0;
    wfun->u.i.nupvars = 0;
    wfun->u.i.skipmin = fun->u.i.skipmin;
    wfun->u.i.wrapper = true;
    wfun->u.i.script = NULL;
    wfun->u.i.names.taggedAtom = NULL;
    wfun->atom = fun->atom;

    if (fun->hasLocalNames()) {
        void *mark = JS_ARENA_MARK(&cx->tempPool);
        jsuword *names = js_GetLocalNameArray(cx, fun, &cx->tempPool);
        if (!names)
            return NULL;

        JSBool ok = true;
        for (uintN i = 0, n = fun->countLocalNames(); i != n; i++) {
            jsuword name = names[i];
            JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(name);
            JSLocalKind localKind = (i < fun->nargs)
                                    ? JSLOCAL_ARG
                                    : (i < fun->countArgsAndVars())
                                    ? (JS_LOCAL_NAME_IS_CONST(name)
                                       ? JSLOCAL_CONST
                                       : JSLOCAL_VAR)
                                    : JSLOCAL_UPVAR;

            ok = js_AddLocal(cx, wfun, atom, localKind);
            if (!ok)
                break;
        }

        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (!ok)
            return NULL;
        JS_ASSERT(wfun->nargs == fun->nargs);
        JS_ASSERT(wfun->u.i.nvars == fun->u.i.nvars);
        JS_ASSERT(wfun->u.i.nupvars == fun->u.i.nupvars);
        js_FreezeLocalNames(cx, wfun);
    }

    JSScript *script = fun->u.i.script;
    jssrcnote *snbase = script->notes();
    jssrcnote *sn = snbase;
    while (!SN_IS_TERMINATOR(sn))
        sn = SN_NEXT(sn);
    uintN nsrcnotes = (sn - snbase) + 1;

    /* NB: GC must not occur before wscript is homed in wfun->u.i.script. */
    JSScript *wscript = js_NewScript(cx, script->length, nsrcnotes,
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
                                     : 0);
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

    if (wfun->u.i.nupvars != 0) {
        JS_ASSERT(wfun->u.i.nupvars == wscript->upvars()->length);
        memcpy(wscript->upvars()->vector, script->upvars()->vector,
               wfun->u.i.nupvars * sizeof(uint32));
    }

    jsbytecode *pc = wscript->code;
    while (*pc != JSOP_STOP) {
        /* XYZZYbe should copy JSOP_TRAP? */
        JSOp op = js_GetOpcode(cx, wscript, pc);
        const JSCodeSpec *cs = &js_CodeSpec[op];
        ptrdiff_t oplen = cs->length;
        if (oplen < 0)
            oplen = js_GetVariableBytecodeLength(pc);

        /*
         * Rewrite JSOP_{GET,CALL}DSLOT as JSOP_{GET,CALL}UPVAR_DBG for the
         * case where fun is an escaping flat closure. This works because the
         * UPVAR and DSLOT ops by design have the same format: an upvar index
         * immediate operand.
         */
        switch (op) {
          case JSOP_GETUPVAR:       *pc = JSOP_GETUPVAR_DBG; break;
          case JSOP_CALLUPVAR:      *pc = JSOP_CALLUPVAR_DBG; break;
          case JSOP_GETDSLOT:       *pc = JSOP_GETUPVAR_DBG; break;
          case JSOP_CALLDSLOT:      *pc = JSOP_CALLUPVAR_DBG; break;
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
    wscript->noScriptRval = script->noScriptRval;
    wscript->savedCallerFun = script->savedCallerFun;
    wscript->hasSharps = script->hasSharps;
    wscript->strictModeCode = script->strictModeCode;
    wscript->version = script->version;
    wscript->nfixed = script->nfixed;
    wscript->filename = script->filename;
    wscript->lineno = script->lineno;
    wscript->nslots = script->nslots;
    wscript->staticLevel = script->staticLevel;
    wscript->principals = script->principals;
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
ArgGetter(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
{
    if (!JS_InstanceOf(cx, obj, &js_ArgumentsClass, NULL))
        return true;

    if (JSVAL_IS_INT(idval)) {
        /*
         * arg can exceed the number of arguments if a script changed the
         * prototype to point to another Arguments object with a bigger argc.
         */
        uintN arg = uintN(JSVAL_TO_INT(idval));
        if (arg < GetArgsLength(obj)) {
#ifdef JS_TRACER
            ArgsPrivateNative *argp = GetArgsPrivateNative(obj);
            if (argp) {
                if (NativeToValue(cx, *vp, argp->typemap()[arg], &argp->argv[arg]))
                    return true;
                LeaveTrace(cx);
                return false;
            }
#endif

            JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
            if (fp) {
                *vp = fp->argv[arg];
            } else {
                jsval v = GetArgsSlot(obj, arg);
                if (v != JSVAL_HOLE)
                    *vp = v;
            }
        }
    } else if (idval == ATOM_KEY(cx->runtime->atomState.lengthAtom)) {
        if (!IsOverriddenArgsLength(obj))
            *vp = INT_TO_JSVAL(GetArgsLength(obj));
    } else {
        JS_ASSERT(idval == ATOM_KEY(cx->runtime->atomState.calleeAtom));
        jsval v = obj->fslots[JSSLOT_ARGS_CALLEE];
        if (v != JSVAL_HOLE) {
            /*
             * If this function or one in it needs upvars that reach above it
             * in the scope chain, it must not be a null closure (it could be a
             * flat closure, or an unoptimized closure -- the latter itself not
             * necessarily heavyweight). Rather than wrap here, we simply throw
             * to reduce code size and tell debugger users the truth instead of
             * passing off a fibbing wrapper.
             */
            if (GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(v))->needsWrapper()) {
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
ArgSetter(JSContext *cx, JSObject *obj, jsval idval, jsval *vp)
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

    if (!JS_InstanceOf(cx, obj, &js_ArgumentsClass, NULL))
        return true;

    if (JSVAL_IS_INT(idval)) {
        uintN arg = uintN(JSVAL_TO_INT(idval));
        if (arg < GetArgsLength(obj)) {
            JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
            if (fp) {
                fp->argv[arg] = *vp;
                return true;
            }
        }
    } else {
        JS_ASSERT(idval == ATOM_KEY(cx->runtime->atomState.lengthAtom) ||
                  idval == ATOM_KEY(cx->runtime->atomState.calleeAtom));
    }

    /*
     * For simplicity we use delete/set to replace the property with one
     * backed by the default Object getter and setter. Note the we rely on
     * args_delete to clear the corresponding reserved slot so the GC can
     * collect its value.
     */
    jsid id;
    if (!JS_ValueToId(cx, idval, &id))
        return false;

    AutoValueRooter tvr(cx);
    return js_DeleteProperty(cx, obj, id, tvr.addr()) &&
           js_SetProperty(cx, obj, id, vp);
}

static JSBool
args_resolve(JSContext *cx, JSObject *obj, jsval idval, uintN flags,
             JSObject **objp)
{
    JS_ASSERT(obj->isArguments());

    *objp = NULL;
    jsid id = 0;
    if (JSVAL_IS_INT(idval)) {
        uint32 arg = uint32(JSVAL_TO_INT(idval));
        if (arg < GetArgsLength(obj) && GetArgsSlot(obj, arg) != JSVAL_HOLE)
            id = INT_JSVAL_TO_JSID(idval);
    } else if (idval == ATOM_KEY(cx->runtime->atomState.lengthAtom)) {
        if (!IsOverriddenArgsLength(obj))
            id = ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
    } else if (idval == ATOM_KEY(cx->runtime->atomState.calleeAtom)) {
        if (obj->fslots[JSSLOT_ARGS_CALLEE] != JSVAL_HOLE)
            id = ATOM_TO_JSID(cx->runtime->atomState.calleeAtom);
    }

    if (id != 0) {
        /*
         * XXX ECMA specs DontEnum even for indexed properties, contrary to
         * other array-like objects.
         */
        if (!js_DefineProperty(cx, obj, id, JSVAL_VOID, ArgGetter, ArgSetter, JSPROP_SHARED))
            return JS_FALSE;
        *objp = obj;
    }
    return true;
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isArguments());

    /*
     * Trigger reflection in args_resolve using a series of js_LookupProperty
     * calls.
     */
    int argc = int(GetArgsLength(obj));
    for (int i = -2; i != argc; i++) {
        jsid id = (i == -2)
                  ? ATOM_TO_JSID(cx->runtime->atomState.lengthAtom)
                  : (i == -1)
                  ? ATOM_TO_JSID(cx->runtime->atomState.calleeAtom)
                  : INT_JSVAL_TO_JSID(INT_TO_JSVAL(i));

        JSObject *pobj;
        JSProperty *prop;
        if (!js_LookupProperty(cx, obj, id, &pobj, &prop))
            return false;

        /* prop is null when the property was deleted. */
        if (prop)
            pobj->dropProperty(cx, prop);
    }
    return true;
}

#if JS_HAS_GENERATORS
/*
 * If a generator-iterator's arguments or call object escapes, it needs to
 * mark its generator object.
 */
static void
args_or_call_trace(JSTracer *trc, JSObject *obj)
{
    if (obj->isArguments()) {
        if (GetArgsPrivateNative(obj))
            return;
    } else {
        JS_ASSERT(obj->getClass() == &js_CallClass);
    }

    JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
    if (fp && (fp->flags & JSFRAME_GENERATOR)) {
        JS_CALL_OBJECT_TRACER(trc, FRAME_TO_GENERATOR(fp)->obj,
                              "FRAME_TO_GENERATOR(fp)->obj");
    }
}
#else
# define args_or_call_trace NULL
#endif

static uint32
args_reserveSlots(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isArguments());
    return GetArgsLength(obj);
}

/*
 * The Arguments class is not initialized via JS_InitClass, and must not be,
 * because its name is "Object".  Per ECMA, that causes instances of it to
 * delegate to the object named by Object.prototype.  It also ensures that
 * arguments.toString() returns "[object Object]".
 *
 * The JSClass functions below collaborate to lazily reflect and synchronize
 * actual argument values, argument count, and callee function object stored
 * in a JSStackFrame with their corresponding property values in the frame's
 * arguments object.
 */
JSClass js_ArgumentsClass = {
    js_Object_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_RESERVED_SLOTS(ARGS_FIXED_RESERVED_SLOTS) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,    args_delProperty,
    JS_PropertyStub,    JS_PropertyStub,
    args_enumerate,     (JSResolveOp) args_resolve,
    JS_ConvertStub,     NULL,
    NULL,               NULL,
    NULL,               NULL,
    NULL,               NULL,
    JS_CLASS_TRACE(args_or_call_trace), args_reserveSlots
};

const uint32 JSSLOT_CALLEE =                    JSSLOT_PRIVATE + 1;
const uint32 JSSLOT_CALL_ARGUMENTS =            JSSLOT_PRIVATE + 2;
const uint32 CALL_CLASS_FIXED_RESERVED_SLOTS =  2;

/*
 * A Declarative Environment object stores its active JSStackFrame pointer in
 * its private slot, just as Call and Arguments objects do.
 */
JSClass js_DeclEnvClass = {
    js_Object_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSBool
CheckForEscapingClosure(JSContext *cx, JSObject *obj, jsval *vp)
{
    JS_ASSERT(obj->getClass() == &js_CallClass ||
              obj->getClass() == &js_DeclEnvClass);

    jsval v = *vp;

    if (VALUE_IS_FUNCTION(cx, v)) {
        JSObject *funobj = JSVAL_TO_OBJECT(v);
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
                JSObject *wrapper = WrapEscapingClosure(cx, fp, funobj, fun);
                if (!wrapper)
                    return false;
                *vp = OBJECT_TO_JSVAL(wrapper);
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
CalleeGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return CheckForEscapingClosure(cx, obj, vp);
}

static JSObject *
NewCallObject(JSContext *cx, JSFunction *fun, JSObject *scopeChain)
{
    JSObject *callobj = js_NewObjectWithGivenProto(cx, &js_CallClass, NULL, scopeChain);
    if (!callobj ||
        !js_EnsureReservedSlots(cx, callobj, fun->countArgsAndVars())) {
        return NULL;
    }
    return callobj;
}

JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *callobj;

    /* Create a call object for fp only if it lacks one. */
    JS_ASSERT(fp->fun);
    callobj = fp->callobj;
    if (callobj)
        return callobj;

#ifdef DEBUG
    /* A call object should be a frame's outermost scope chain element.  */
    JSClass *classp = OBJ_GET_CLASS(cx, fp->scopeChain);
    if (classp == &js_WithClass || classp == &js_BlockClass || classp == &js_CallClass)
        JS_ASSERT(fp->scopeChain->getPrivate() != fp);
#endif

    /*
     * Create the call object, using the frame's enclosing scope as its
     * parent, and link the call to its stack frame. For a named function
     * expression Call's parent points to an environment object holding
     * function's name.
     */
    JSAtom *lambdaName = (fp->fun->flags & JSFUN_LAMBDA) ? fp->fun->atom : NULL;
    if (lambdaName) {
        JSObject *env = js_NewObjectWithGivenProto(cx, &js_DeclEnvClass, NULL,
                                                   fp->scopeChain);
        if (!env)
            return NULL;
        env->setPrivate(fp);

        /* Root env before js_DefineNativeProperty (-> JSClass.addProperty). */
        fp->scopeChain = env;
        JS_ASSERT(fp->argv);
        if (!js_DefineNativeProperty(cx, fp->scopeChain, ATOM_TO_JSID(lambdaName),
                                     fp->calleeValue(),
                                     CalleeGetter, NULL,
                                     JSPROP_PERMANENT | JSPROP_READONLY,
                                     0, 0, NULL)) {
            return NULL;
        }
    }

    callobj = NewCallObject(cx, fp->fun, fp->scopeChain);
    if (!callobj)
        return NULL;

    callobj->setPrivate(fp);
    JS_ASSERT(fp->argv);
    JS_ASSERT(fp->fun == GET_FUNCTION_PRIVATE(cx, fp->calleeObject()));
    callobj->setSlot(JSSLOT_CALLEE, fp->calleeValue());
    fp->callobj = callobj;

    /*
     * Push callobj on the top of the scope chain, and make it the
     * variables object.
     */
    fp->scopeChain = callobj;
    return callobj;
}

JSObject * JS_FASTCALL
js_CreateCallObjectOnTrace(JSContext *cx, JSFunction *fun, JSObject *callee, JSObject *scopeChain)
{
    JS_ASSERT(!js_IsNamedLambda(fun));
    JSObject *callobj = NewCallObject(cx, fun, scopeChain);
    if (!callobj)
        return NULL;
    callobj->setSlot(JSSLOT_CALLEE, OBJECT_TO_JSVAL(callee));
    return callobj;
}

JS_DEFINE_CALLINFO_4(extern, OBJECT, js_CreateCallObjectOnTrace, CONTEXT, FUNCTION, OBJECT, OBJECT,
                     0, nanojit::ACC_STORE_ANY)

JSFunction *
js_GetCallObjectFunction(JSObject *obj)
{
    jsval v;

    JS_ASSERT(obj->getClass() == &js_CallClass);
    v = obj->getSlot(JSSLOT_CALLEE);
    if (JSVAL_IS_VOID(v)) {
        /* Newborn or prototype object. */
        return NULL;
    }
    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
    return GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(v));
}

inline static void
CopyValuesToCallObject(JSObject *callobj, int nargs, jsval *argv, int nvars, jsval *slots)
{
    memcpy(callobj->dslots, argv, nargs * sizeof(jsval));
    memcpy(callobj->dslots + nargs, slots, nvars * sizeof(jsval));
}

void
js_PutCallObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *callobj = fp->callobj;
    JS_ASSERT(callobj);

    /* Get the arguments object to snapshot fp's actual argument values. */
    if (fp->argsobj) {
        if (!(fp->flags & JSFRAME_OVERRIDE_ARGS))
            callobj->setSlot(JSSLOT_CALL_ARGUMENTS, fp->argsobj);
        js_PutArgsObject(cx, fp);
    }

    JSFunction *fun = fp->fun;
    JS_ASSERT(fun == js_GetCallObjectFunction(callobj));
    uintN n = fun->countArgsAndVars();

    /*
     * Since for a call object all fixed slots happen to be taken, we can copy
     * arguments and variables straight into JSObject.dslots.
     */
    JS_STATIC_ASSERT(JS_INITIAL_NSLOTS - JSSLOT_PRIVATE ==
                     1 + CALL_CLASS_FIXED_RESERVED_SLOTS);
    if (n != 0) {
        JS_ASSERT(callobj->numSlots() >= JS_INITIAL_NSLOTS + n);
        n += JS_INITIAL_NSLOTS;
        CopyValuesToCallObject(callobj, fun->nargs, fp->argv, fun->u.i.nvars, fp->slots);
    }

    /* Clear private pointers to fp, which is about to go away (js_Invoke). */
    if (js_IsNamedLambda(fun)) {
        JSObject *env = callobj->getParent();

        JS_ASSERT(env->getClass() == &js_DeclEnvClass);
        JS_ASSERT(env->getPrivate() == fp);
        env->setPrivate(NULL);
    }

    callobj->setPrivate(NULL);
    fp->callobj = NULL;
}

JSBool JS_FASTCALL
js_PutCallObjectOnTrace(JSContext *cx, JSObject *scopeChain, uint32 nargs, jsval *argv,
                        uint32 nvars, jsval *slots)
{
    JS_ASSERT(scopeChain->hasClass(&js_CallClass));
    JS_ASSERT(!scopeChain->getPrivate());

    uintN n = nargs + nvars;
    if (n != 0)
        CopyValuesToCallObject(scopeChain, nargs, argv, nvars, slots);

    return true;
}

JS_DEFINE_CALLINFO_6(extern, BOOL, js_PutCallObjectOnTrace, CONTEXT, OBJECT, UINT32, JSVALPTR,
                     UINT32, JSVALPTR, 0, nanojit::ACC_STORE_ANY)

static JSBool
call_enumerate(JSContext *cx, JSObject *obj)
{
    JSFunction *fun;
    uintN n, i;
    void *mark;
    jsuword *names;
    JSBool ok;
    JSAtom *name;
    JSObject *pobj;
    JSProperty *prop;

    fun = js_GetCallObjectFunction(obj);
    n = fun ? fun->countArgsAndVars() : 0;
    if (n == 0)
        return JS_TRUE;

    mark = JS_ARENA_MARK(&cx->tempPool);

    MUST_FLOW_THROUGH("out");
    names = js_GetLocalNameArray(cx, fun, &cx->tempPool);
    if (!names) {
        ok = JS_FALSE;
        goto out;
    }

    for (i = 0; i != n; ++i) {
        name = JS_LOCAL_NAME_TO_ATOM(names[i]);
        if (!name)
            continue;

        /*
         * Trigger reflection by looking up the name of the argument or
         * variable.
         */
        ok = js_LookupProperty(cx, obj, ATOM_TO_JSID(name), &pobj, &prop);
        if (!ok)
            goto out;

        /*
         * The call object will always have a property corresponding to the
         * argument or variable name because call_resolve creates the property
         * using JSPROP_PERMANENT.
         */
        JS_ASSERT(prop);
        JS_ASSERT(pobj == obj);
        pobj->dropProperty(cx, prop);
    }
    ok = JS_TRUE;

  out:
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    return ok;
}

enum JSCallPropertyKind {
    JSCPK_ARGUMENTS,
    JSCPK_ARG,
    JSCPK_VAR,
    JSCPK_UPVAR
};

static JSBool
CallPropertyOp(JSContext *cx, JSObject *obj, jsid id, jsval *vp,
               JSCallPropertyKind kind, JSBool setter = false)
{
    JS_ASSERT(obj->getClass() == &js_CallClass);

    uintN i = 0;
    if (kind != JSCPK_ARGUMENTS) {
        JS_ASSERT((int16) JSVAL_TO_INT(id) == JSVAL_TO_INT(id));
        i = (uint16) JSVAL_TO_INT(id);
    }

    jsval *array;
    if (kind == JSCPK_UPVAR) {
        JSObject *callee = JSVAL_TO_OBJECT(obj->getSlot(JSSLOT_CALLEE));

#ifdef DEBUG
        JSFunction *callee_fun = (JSFunction *) callee->getPrivate();
        JS_ASSERT(FUN_FLAT_CLOSURE(callee_fun));
        JS_ASSERT(i < callee_fun->u.i.nupvars);
#endif

        array = callee->dslots;
    } else {
        JSFunction *fun = js_GetCallObjectFunction(obj);
        JS_ASSERT_IF(kind == JSCPK_ARG, i < fun->nargs);
        JS_ASSERT_IF(kind == JSCPK_VAR, i < fun->u.i.nvars);

        JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();

        if (kind == JSCPK_ARGUMENTS) {
            if (setter) {
                if (fp)
                    fp->flags |= JSFRAME_OVERRIDE_ARGS;
                obj->setSlot(JSSLOT_CALL_ARGUMENTS, *vp);
            } else {
                if (fp && !(fp->flags & JSFRAME_OVERRIDE_ARGS)) {
                    JSObject *argsobj;

                    argsobj = js_GetArgsObject(cx, fp);
                    if (!argsobj)
                        return false;
                    *vp = OBJECT_TO_JSVAL(argsobj);
                } else {
                    *vp = obj->getSlot(JSSLOT_CALL_ARGUMENTS);
                }
            }
            return true;
        }

        if (!fp) {
            i += CALL_CLASS_FIXED_RESERVED_SLOTS;
            if (kind == JSCPK_VAR)
                i += fun->nargs;
            else
                JS_ASSERT(kind == JSCPK_ARG);
            return setter
                   ? JS_SetReservedSlot(cx, obj, i, *vp)
                   : JS_GetReservedSlot(cx, obj, i, vp);
        }

        if (kind == JSCPK_ARG) {
            array = fp->argv;
        } else {
            JS_ASSERT(kind == JSCPK_VAR);
            array = fp->slots;
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

static JSBool
GetCallArguments(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARGUMENTS);
}

static JSBool
SetCallArguments(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARGUMENTS, true);
}

JSBool
js_GetCallArg(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARG);
}

JSBool
SetCallArg(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_ARG, true);
}

JSBool
GetFlatUpvar(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_UPVAR);
}

JSBool
SetFlatUpvar(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_UPVAR, true);
}

JSBool
js_GetCallVar(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_VAR);
}

JSBool
js_GetCallVarChecked(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    if (!CallPropertyOp(cx, obj, id, vp, JSCPK_VAR))
        return false;

    return CheckForEscapingClosure(cx, obj, vp);
}

JSBool
SetCallVar(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return CallPropertyOp(cx, obj, id, vp, JSCPK_VAR, true);
}

JSBool JS_FASTCALL
js_SetCallArg(JSContext *cx, JSObject *obj, jsid id, jsval v)
{
    return CallPropertyOp(cx, obj, id, &v, JSCPK_ARG, true);
}

JSBool JS_FASTCALL
js_SetCallVar(JSContext *cx, JSObject *obj, jsid id, jsval v)
{
    return CallPropertyOp(cx, obj, id, &v, JSCPK_VAR, true);
}

JS_DEFINE_CALLINFO_4(extern, BOOL, js_SetCallArg, CONTEXT, OBJECT, JSID, JSVAL, 0,
                     nanojit::ACC_STORE_ANY)
JS_DEFINE_CALLINFO_4(extern, BOOL, js_SetCallVar, CONTEXT, OBJECT, JSID, JSVAL, 0,
                     nanojit::ACC_STORE_ANY)

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsval idval, uintN flags,
             JSObject **objp)
{
    jsval callee;
    JSFunction *fun;
    jsid id;
    JSLocalKind localKind;
    JSPropertyOp getter, setter;
    uintN slot, attrs;

    JS_ASSERT(obj->getClass() == &js_CallClass);
    JS_ASSERT(!obj->getProto());

    if (!JSVAL_IS_STRING(idval))
        return JS_TRUE;

    callee = obj->getSlot(JSSLOT_CALLEE);
    if (JSVAL_IS_VOID(callee))
        return JS_TRUE;
    fun = GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(callee));

    if (!js_ValueToStringId(cx, idval, &id))
        return JS_FALSE;

    /*
     * Check whether the id refers to a formal parameter, local variable or
     * the arguments special name.
     *
     * We define all such names using JSDNP_DONT_PURGE to avoid an expensive
     * shape invalidation in js_DefineNativeProperty. If such an id happens to
     * shadow a global or upvar of the same name, any inner functions can
     * never access the outer binding. Thus it cannot invalidate any property
     * cache entries or derived trace guards for the outer binding. See also
     * comments in js_PurgeScopeChainHelper from jsobj.cpp.
     */
    localKind = js_LookupLocal(cx, fun, JSID_TO_ATOM(id), &slot);
    if (localKind != JSLOCAL_NONE) {
        JS_ASSERT((uint16) slot == slot);

        /*
         * We follow 10.2.3 of ECMA 262 v3 and make argument and variable
         * properties of the Call objects enumerable.
         */
        attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_SHARED;
        if (localKind == JSLOCAL_ARG) {
            JS_ASSERT(slot < fun->nargs);
            getter = js_GetCallArg;
            setter = SetCallArg;
        } else {
            JSCallPropertyKind cpkind;
            if (localKind == JSLOCAL_UPVAR) {
                if (!FUN_FLAT_CLOSURE(fun))
                    return JS_TRUE;
                getter = GetFlatUpvar;
                setter = SetFlatUpvar;
                cpkind = JSCPK_UPVAR;
            } else {
                JS_ASSERT(localKind == JSLOCAL_VAR || localKind == JSLOCAL_CONST);
                JS_ASSERT(slot < fun->u.i.nvars);
                getter = js_GetCallVar;
                setter = SetCallVar;
                cpkind = JSCPK_VAR;
                if (localKind == JSLOCAL_CONST)
                    attrs |= JSPROP_READONLY;
            }

            /*
             * Use js_GetCallVarChecked if the local's value is a null closure.
             * This way we penalize performance only slightly on first use of a
             * null closure, not on every use.
             */
            jsval v;
            if (!CallPropertyOp(cx, obj, INT_TO_JSID((int16)slot), &v, cpkind))
                return JS_FALSE;
            if (VALUE_IS_FUNCTION(cx, v) &&
                GET_FUNCTION_PRIVATE(cx, JSVAL_TO_OBJECT(v))->needsWrapper()) {
                getter = js_GetCallVarChecked;
            }
        }
        if (!js_DefineNativeProperty(cx, obj, id, JSVAL_VOID, getter, setter,
                                     attrs, JSScopeProperty::HAS_SHORTID, (int16) slot,
                                     NULL, JSDNP_DONT_PURGE)) {
            return JS_FALSE;
        }
        *objp = obj;
        return JS_TRUE;
    }

    /*
     * Resolve arguments so that we never store a particular Call object's
     * arguments object reference in a Call prototype's |arguments| slot.
     */
    if (id == ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom)) {
        if (!js_DefineNativeProperty(cx, obj, id, JSVAL_VOID,
                                     GetCallArguments, SetCallArguments,
                                     JSPROP_PERMANENT | JSPROP_SHARED,
                                     0, 0, NULL, JSDNP_DONT_PURGE)) {
            return JS_FALSE;
        }
        *objp = obj;
        return JS_TRUE;
    }

    /* Control flow reaches here only if id was not resolved. */
    return JS_TRUE;
}

static JSBool
call_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    if (type == JSTYPE_FUNCTION) {
        JSStackFrame *fp = (JSStackFrame *) obj->getPrivate();
        if (fp) {
            JS_ASSERT(fp->fun);
            JS_ASSERT(fp->argv);
            *vp = fp->calleeValue();
        }
    }
    return JS_TRUE;
}

static uint32
call_reserveSlots(JSContext *cx, JSObject *obj)
{
    JSFunction *fun;

    fun = js_GetCallObjectFunction(obj);
    return fun->countArgsAndVars();
}

JS_FRIEND_DATA(JSClass) js_CallClass = {
    "Call",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_RESERVED_SLOTS(CALL_CLASS_FIXED_RESERVED_SLOTS) |
    JSCLASS_NEW_RESOLVE | JSCLASS_IS_ANONYMOUS | JSCLASS_MARK_IS_TRACE,
    JS_PropertyStub,    JS_PropertyStub,
    JS_PropertyStub,    JS_PropertyStub,
    call_enumerate,     (JSResolveOp)call_resolve,
    call_convert,       NULL,
    NULL,               NULL,
    NULL,               NULL,
    NULL,               NULL,
    JS_CLASS_TRACE(args_or_call_trace), call_reserveSlots
};

/* Generic function tinyids. */
enum {
    FUN_ARGUMENTS   = -1,       /* predefined arguments local variable */
    FUN_LENGTH      = -2,       /* number of actual args, arity if inactive */
    FUN_ARITY       = -3,       /* number of formal parameters; desired argc */
    FUN_NAME        = -4,       /* function name, "" if anonymous */
    FUN_CALLER      = -5        /* Function.prototype.caller, backward compat */
};

static JSBool
fun_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSFunction *fun;
    JSStackFrame *fp;
    JSSecurityCallbacks *callbacks;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    slot = JSVAL_TO_INT(id);

    /*
     * Loop because getter and setter can be delegated from another class,
     * but loop only for FUN_LENGTH because we must pretend that f.length
     * is in each function instance f, per ECMA-262, instead of only in the
     * Function.prototype object (we use JSPROP_PERMANENT with JSPROP_SHARED
     * to make it appear so).
     *
     * This code couples tightly to the attributes for the function_props[]
     * initializers above, and to js_SetProperty and js_HasOwnProperty.
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
    while (!(fun = (JSFunction *)
                   JS_GetInstancePrivate(cx, obj, &js_FunctionClass, NULL))) {
        if (slot != FUN_LENGTH)
            return JS_TRUE;
        obj = obj->getProto();
        if (!obj)
            return JS_TRUE;
    }

    /* Find fun's top-most activation record. */
    for (fp = js_GetTopStackFrame(cx);
         fp && (fp->fun != fun || (fp->flags & JSFRAME_SPECIAL));
         fp = fp->down) {
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
            return JS_FALSE;
        }
        if (fp) {
            if (!js_GetArgsValue(cx, fp, vp))
                return JS_FALSE;
        } else {
            *vp = JSVAL_NULL;
        }
        break;

      case FUN_LENGTH:
      case FUN_ARITY:
            *vp = INT_TO_JSVAL((jsint)fun->nargs);
        break;

      case FUN_NAME:
        *vp = fun->atom
              ? ATOM_KEY(fun->atom)
              : STRING_TO_JSVAL(cx->runtime->emptyString);
        break;

      case FUN_CALLER:
        if (fp && fp->down && fp->down->fun) {
            JSFunction *caller = fp->down->fun;
            /*
             * See equivalent condition in args_getProperty for ARGS_CALLEE,
             * but here we do not want to throw, since this escape can happen
             * via foo.caller alone, without any debugger or indirect eval. And
             * it seems foo.caller is still used on the Web.
             */
            if (caller->needsWrapper()) {
                JSObject *wrapper = WrapEscapingClosure(cx, fp->down, FUN_OBJECT(caller), caller);
                if (!wrapper)
                    return JS_FALSE;
                *vp = OBJECT_TO_JSVAL(wrapper);
                return JS_TRUE;
            }

            JS_ASSERT(fp->down->argv);
            *vp = fp->down->calleeValue();
        } else {
            *vp = JSVAL_NULL;
        }
        if (!JSVAL_IS_PRIMITIVE(*vp)) {
            callbacks = JS_GetSecurityCallbacks(cx);
            if (callbacks && callbacks->checkObjectAccess) {
                id = ATOM_KEY(cx->runtime->atomState.callerAtom);
                if (!callbacks->checkObjectAccess(cx, obj, id, JSACC_READ, vp))
                    return JS_FALSE;
            }
        }
        break;

      default:
        /* XXX fun[0] and fun.arguments[0] are equivalent. */
        if (fp && fp->fun && (uintN)slot < fp->fun->nargs)
            *vp = fp->argv[slot];
        break;
    }

    return JS_TRUE;
}

/*
 * ECMA-262 specifies that length is a property of function object instances,
 * but we can avoid that space cost by delegating to a prototype property that
 * is JSPROP_PERMANENT and JSPROP_SHARED.  Each fun_getProperty call computes
 * a fresh length value based on the arity of the individual function object's
 * private data.
 *
 * The extensions below other than length, i.e., the ones not in ECMA-262,
 * are neither JSPROP_READONLY nor JSPROP_SHARED, because for compatibility
 * with ECMA we must allow a delegating object to override them. Therefore to
 * avoid entraining garbage in Function.prototype slots, they must be resolved
 * in non-prototype function objects, wherefore the lazy_function_props table
 * and fun_resolve's use of it.
 */
#define LENGTH_PROP_ATTRS (JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_SHARED)

static JSPropertySpec function_props[] = {
    {js_length_str,    FUN_LENGTH,    LENGTH_PROP_ATTRS, fun_getProperty, JS_PropertyStub},
    {0,0,0,0,0}
};

typedef struct LazyFunctionProp {
    uint16      atomOffset;
    int8        tinyid;
    uint8       attrs;
} LazyFunctionProp;

/* NB: no sentinel at the end -- use JS_ARRAY_LENGTH to bound loops. */
static LazyFunctionProp lazy_function_props[] = {
    {ATOM_OFFSET(arguments), FUN_ARGUMENTS, JSPROP_PERMANENT},
    {ATOM_OFFSET(arity),     FUN_ARITY,      JSPROP_PERMANENT},
    {ATOM_OFFSET(caller),    FUN_CALLER,     JSPROP_PERMANENT},
    {ATOM_OFFSET(name),      FUN_NAME,       JSPROP_PERMANENT},
};

static JSBool
fun_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
            JSObject **objp)
{
    JSFunction *fun;
    JSAtom *atom;
    uintN i;

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;

    fun = GET_FUNCTION_PRIVATE(cx, obj);

    /*
     * No need to reflect fun.prototype in 'fun.prototype = ... '. Assert that
     * fun is not a compiler-created function object, which must never leak to
     * script or embedding code and then be mutated.
     */
    if (flags & JSRESOLVE_ASSIGNING) {
        JS_ASSERT(!IsInternalFunctionObject(obj));
        return JS_TRUE;
    }

    /*
     * Ok, check whether id is 'prototype' and bootstrap the function object's
     * prototype property.
     */
    atom = cx->runtime->atomState.classPrototypeAtom;
    if (id == ATOM_KEY(atom)) {
        JS_ASSERT(!IsInternalFunctionObject(obj));

        /*
         * Beware of the wacky case of a user function named Object -- trying
         * to find a prototype for that will recur back here _ad perniciem_.
         */
        if (fun->atom == CLASS_ATOM(cx, Object))
            return JS_TRUE;

        /*
         * Make the prototype object to have the same parent as the function
         * object itself.
         */
        JSObject *proto =
            js_NewObject(cx, &js_ObjectClass, NULL, obj->getParent());
        if (!proto)
            return JS_FALSE;

        /*
         * ECMA (15.3.5.2) says that constructor.prototype is DontDelete for
         * user-defined functions, but DontEnum | ReadOnly | DontDelete for
         * native "system" constructors such as Object or Function.  So lazily
         * set the former here in fun_resolve, but eagerly define the latter
         * in JS_InitClass, with the right attributes.
         */
        if (!js_SetClassPrototype(cx, obj, proto, JSPROP_PERMANENT))
            return JS_FALSE;

        *objp = obj;
        return JS_TRUE;
    }

    for (i = 0; i < JS_ARRAY_LENGTH(lazy_function_props); i++) {
        LazyFunctionProp *lfp = &lazy_function_props[i];

        atom = OFFSET_TO_ATOM(cx->runtime, lfp->atomOffset);
        if (id == ATOM_KEY(atom)) {
            JS_ASSERT(!IsInternalFunctionObject(obj));

            if (!js_DefineNativeProperty(cx, obj,
                                         ATOM_TO_JSID(atom), JSVAL_VOID,
                                         fun_getProperty, JS_PropertyStub,
                                         lfp->attrs, JSScopeProperty::HAS_SHORTID,
                                         lfp->tinyid, NULL)) {
                return JS_FALSE;
            }
            *objp = obj;
            return JS_TRUE;
        }
    }

    return JS_TRUE;
}

static JSBool
fun_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    switch (type) {
      case JSTYPE_FUNCTION:
        *vp = OBJECT_TO_JSVAL(obj);
        return JS_TRUE;
      default:
        return js_TryValueOf(cx, obj, type, vp);
    }
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
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_NOT_SCRIPTED_FUNCTION,
                                 JS_GetFunctionName(fun));
            return false;
        }
        if (fun->u.i.wrapper) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_XDR_CLOSURE_WRAPPER,
                                 JS_GetFunctionName(fun));
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

    AutoValueRooter tvr(cx, FUN_OBJECT(fun));

    if (!JS_XDRUint32(xdr, &firstword))
        return false;
    if ((firstword & 1U) && !js_XDRStringAtom(xdr, &fun->atom))
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
            names = js_GetLocalNameArray(xdr->cx, fun, &xdr->cx->tempPool);
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
                    ok = !!js_AddLocal(xdr->cx, fun, NULL, JSLOCAL_ARG);
                    if (!ok)
                        goto release_mark;
                } else {
                    JS_ASSERT(!JS_LOCAL_NAME_TO_ATOM(names[i]));
                }
                continue;
            }
            if (xdr->mode == JSXDR_ENCODE)
                name = JS_LOCAL_NAME_TO_ATOM(names[i]);
            ok = !!js_XDRStringAtom(xdr, &name);
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
                ok = !!js_AddLocal(xdr->cx, fun, name, localKind);
                if (!ok)
                    goto release_mark;
            }
        }

      release_mark:
        JS_ARENA_RELEASE(&xdr->cx->tempPool, mark);
        if (!ok)
            return false;

        if (xdr->mode == JSXDR_DECODE)
            js_FreezeLocalNames(cx, fun);
    }

    if (!js_XDRScript(xdr, &fun->u.i.script, false, NULL))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        *objp = FUN_OBJECT(fun);
        if (fun->u.i.script != JSScript::emptyScript()) {
#ifdef CHECK_SCRIPT_OWNER
            fun->u.i.script->owner = NULL;
#endif
            js_CallNewScriptHook(cx, fun->u.i.script, fun);
        }
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
fun_hasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    jsval pval;
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!obj->getProperty(cx, id, &pval))
        return JS_FALSE;

    if (JSVAL_IS_PRIMITIVE(pval)) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        js_ReportValueError(cx, JSMSG_BAD_PROTOTYPE,
                            -1, OBJECT_TO_JSVAL(obj), NULL);
        return JS_FALSE;
    }

    return js_IsDelegate(cx, JSVAL_TO_OBJECT(pval), v, bp);
}

static void
TraceLocalNames(JSTracer *trc, JSFunction *fun);

static void
DestroyLocalNames(JSContext *cx, JSFunction *fun);

static void
fun_trace(JSTracer *trc, JSObject *obj)
{
    /* A newborn function object may have a not yet initialized private slot. */
    JSFunction *fun = (JSFunction *) obj->getPrivate();
    if (!fun)
        return;

    if (FUN_OBJECT(fun) != obj) {
        /* obj is cloned function object, trace the original. */
        JS_CALL_TRACER(trc, FUN_OBJECT(fun), JSTRACE_OBJECT, "private");
        return;
    }
    if (fun->atom)
        JS_CALL_STRING_TRACER(trc, ATOM_TO_STRING(fun->atom), "atom");
    if (FUN_INTERPRETED(fun)) {
        if (fun->u.i.script)
            js_TraceScript(trc, fun->u.i.script);
        TraceLocalNames(trc, fun);
    }
}

static void
fun_finalize(JSContext *cx, JSObject *obj)
{
    /* Ignore newborn and cloned function objects. */
    JSFunction *fun = (JSFunction *) obj->getPrivate();
    if (!fun || FUN_OBJECT(fun) != obj)
        return;

    /*
     * Null-check of u.i.script is required since the parser sets interpreted
     * very early.
     */
    if (FUN_INTERPRETED(fun)) {
        if (fun->u.i.script)
            js_DestroyScript(cx, fun->u.i.script);
        DestroyLocalNames(cx, fun);
    }
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
            js_LookupLocal(cx, this, name, &index);
        JS_ASSERT(kind == JSLOCAL_VAR);
        return int(index);
    }
#endif
    return -1;
}

uint32
JSFunction::countInterpretedReservedSlots() const
{
    JS_ASSERT(FUN_INTERPRETED(this));

    return (u.i.nupvars == 0) ? 0 : u.i.script->upvars()->length;
}

static uint32
fun_reserveSlots(JSContext *cx, JSObject *obj)
{
    /*
     * We use getPrivate and not GET_FUNCTION_PRIVATE because during
     * js_InitFunctionClass invocation the function is called before the
     * private slot of the function object is set.
     */
    JSFunction *fun = (JSFunction *) obj->getPrivate();
    return (fun && FUN_INTERPRETED(fun))
           ? fun->countInterpretedReservedSlots()
           : 0;
}

/*
 * Reserve two slots in all function objects for XPConnect.  Note that this
 * does not bloat every instance, only those on which reserved slots are set,
 * and those on which ad-hoc properties are defined.
 */
JS_FRIEND_DATA(JSClass) js_FunctionClass = {
    js_Function_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(2) |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, (JSResolveOp)fun_resolve,
    fun_convert,      fun_finalize,
    NULL,             NULL,
    NULL,             NULL,
    js_XDRFunctionObject, fun_hasInstance,
    JS_CLASS_TRACE(fun_trace), fun_reserveSlots
};

static JSBool
fun_toStringHelper(JSContext *cx, uint32 indent, uintN argc, jsval *vp)
{
    jsval fval;
    JSObject *obj;
    JSFunction *fun;
    JSString *str;

    fval = JS_THIS(cx, vp);
    if (JSVAL_IS_NULL(fval))
        return JS_FALSE;

    if (!VALUE_IS_FUNCTION(cx, fval)) {
        /*
         * If we don't have a function to start off with, try converting the
         * object to a function. If that doesn't work, complain.
         */
        if (!JSVAL_IS_PRIMITIVE(fval)) {
            obj = JSVAL_TO_OBJECT(fval);
            if (!OBJ_GET_CLASS(cx, obj)->convert(cx, obj, JSTYPE_FUNCTION,
                                                 &fval)) {
                return JS_FALSE;
            }
            vp[1] = fval;
        }
        if (!VALUE_IS_FUNCTION(cx, fval)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_INCOMPATIBLE_PROTO,
                                 js_Function_str, js_toString_str,
                                 JS_GetTypeName(cx, JS_TypeOfValue(cx, fval)));
            return JS_FALSE;
        }
    }

    obj = JSVAL_TO_OBJECT(fval);
    if (argc != 0) {
        indent = js_ValueToECMAUint32(cx, &vp[2]);
        if (JSVAL_IS_NULL(vp[2]))
            return JS_FALSE;
    }

    JS_ASSERT(JS_ObjectIsFunction(cx, obj));
    fun = GET_FUNCTION_PRIVATE(cx, obj);
    if (!fun)
        return JS_TRUE;
    str = JS_DecompileFunction(cx, fun, (uintN)indent);
    if (!str)
        return JS_FALSE;
    *vp = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
fun_toString(JSContext *cx, uintN argc, jsval *vp)
{
    return fun_toStringHelper(cx, 0, argc,  vp);
}

#if JS_HAS_TOSOURCE
static JSBool
fun_toSource(JSContext *cx, uintN argc, jsval *vp)
{
    return fun_toStringHelper(cx, JS_DONT_PRETTY_PRINT, argc, vp);
}
#endif

JSBool
js_fun_call(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;
    jsval fval, *argv, *invokevp;
    JSString *str;
    void *mark;
    JSBool ok;

    LeaveTrace(cx);

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !obj->defaultValue(cx, JSTYPE_FUNCTION, &vp[1]))
        return JS_FALSE;
    fval = vp[1];

    if (!js_IsCallable(fval)) {
        str = JS_ValueToString(cx, fval);
        if (str) {
            const char *bytes = js_GetStringBytes(cx, str);

            if (bytes) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INCOMPATIBLE_PROTO,
                                     js_Function_str, js_call_str,
                                     bytes);
            }
        }
        return JS_FALSE;
    }

    argv = vp + 2;
    if (argc == 0) {
        /* Call fun with its global object as the 'this' param if no args. */
        obj = NULL;
    } else {
        /* Otherwise convert the first arg to 'this' and skip over it. */
        if (!JSVAL_IS_PRIMITIVE(argv[0]))
            obj = JSVAL_TO_OBJECT(argv[0]);
        else if (!js_ValueToObject(cx, argv[0], &obj))
            return JS_FALSE;
        argc--;
        argv++;
    }

    /* Allocate stack space for fval, obj, and the args. */
    invokevp = js_AllocStack(cx, 2 + argc, &mark);
    if (!invokevp)
        return JS_FALSE;

    /* Push fval, obj, and the args. */
    invokevp[0] = fval;
    invokevp[1] = OBJECT_TO_JSVAL(obj);
    memcpy(invokevp + 2, argv, argc * sizeof *argv);

    ok = js_Invoke(cx, argc, invokevp, 0);
    *vp = *invokevp;
    js_FreeStack(cx, mark);
    return ok;
}

JSBool
js_fun_apply(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj, *aobj;
    jsval fval, *invokevp, *sp;
    JSString *str;
    jsuint length;
    JSBool arraylike, ok;
    void *mark;
    uintN i;

    if (argc == 0) {
        /* Will get globalObject as 'this' and no other arguments. */
        return js_fun_call(cx, argc, vp);
    }

    LeaveTrace(cx);

    obj = JS_THIS_OBJECT(cx, vp);
    if (!obj || !obj->defaultValue(cx, JSTYPE_FUNCTION, &vp[1]))
        return JS_FALSE;
    fval = vp[1];

    if (!js_IsCallable(fval)) {
        str = JS_ValueToString(cx, fval);
        if (str) {
            const char *bytes = js_GetStringBytes(cx, str);

            if (bytes) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INCOMPATIBLE_PROTO,
                                     js_Function_str, js_apply_str,
                                     bytes);
            }
        }
        return JS_FALSE;
    }

    /* Quell GCC overwarnings. */
    aobj = NULL;
    length = 0;

    if (argc >= 2) {
        /* If the 2nd arg is null or void, call the function with 0 args. */
        if (JSVAL_IS_NULL(vp[3]) || JSVAL_IS_VOID(vp[3])) {
            argc = 0;
        } else {
            /* The second arg must be an array (or arguments object). */
            arraylike = JS_FALSE;
            if (!JSVAL_IS_PRIMITIVE(vp[3])) {
                aobj = JSVAL_TO_OBJECT(vp[3]);
                if (!js_IsArrayLike(cx, aobj, &arraylike, &length))
                    return JS_FALSE;
            }
            if (!arraylike) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_APPLY_ARGS, js_apply_str);
                return JS_FALSE;
            }
        }
    }

    /* Convert the first arg to 'this' and skip over it. */
    if (!JSVAL_IS_PRIMITIVE(vp[2]))
        obj = JSVAL_TO_OBJECT(vp[2]);
    else if (!js_ValueToObject(cx, vp[2], &obj))
        return JS_FALSE;

    /* Allocate stack space for fval, obj, and the args. */
    argc = (uintN)JS_MIN(length, JS_ARGS_LENGTH_MAX);
    invokevp = js_AllocStack(cx, 2 + argc, &mark);
    if (!invokevp)
        return JS_FALSE;

    /* Push fval, obj, and aobj's elements as args. */
    sp = invokevp;
    *sp++ = fval;
    *sp++ = OBJECT_TO_JSVAL(obj);
    if (aobj && aobj->isArguments()) {
        /*
         * Two cases, two loops: note how in the case of an active stack frame
         * backing aobj, even though we copy from fp->argv, we still must check
         * aobj->dslots[i] for a hole, to handle a delete on the corresponding
         * arguments element. See args_delProperty.
         */
        JSStackFrame *fp = (JSStackFrame *) aobj->getPrivate();
        if (fp) {
            memcpy(sp, fp->argv, argc * sizeof(jsval));
            for (i = 0; i < argc; i++) {
                if (aobj->dslots[i] == JSVAL_HOLE) // suppress deleted element
                    sp[i] = JSVAL_VOID;
            }
        } else {
            memcpy(sp, aobj->dslots, argc * sizeof(jsval));
            for (i = 0; i < argc; i++) {
                if (sp[i] == JSVAL_HOLE)
                    sp[i] = JSVAL_VOID;
            }
        }
    } else {
        for (i = 0; i < argc; i++) {
            ok = aobj->getProperty(cx, INT_TO_JSID(jsint(i)), sp);
            if (!ok)
                goto out;
            sp++;
        }
    }

    ok = js_Invoke(cx, argc, invokevp, 0);
    *vp = *invokevp;
out:
    js_FreeStack(cx, mark);
    return ok;
}

#ifdef NARCISSUS
static JS_REQUIRES_STACK JSBool
fun_applyConstructor(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *aobj;
    uintN length, i;
    void *mark;
    jsval *invokevp, *sp;
    JSBool ok;

    if (JSVAL_IS_PRIMITIVE(vp[2]) ||
        (aobj = JSVAL_TO_OBJECT(vp[2]),
         !aobj->isArray() &&
         !aobj->isArguments())) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_APPLY_ARGS, "__applyConstruct__");
        return JS_FALSE;
    }

    if (!js_GetLengthProperty(cx, aobj, &length))
        return JS_FALSE;

    if (length > JS_ARGS_LENGTH_MAX)
        length = JS_ARGS_LENGTH_MAX;
    invokevp = js_AllocStack(cx, 2 + length, &mark);
    if (!invokevp)
        return JS_FALSE;

    sp = invokevp;
    *sp++ = vp[1];
    *sp++ = JSVAL_NULL; /* this is filled automagically */
    for (i = 0; i < length; i++) {
        ok = aobj->getProperty(cx, INT_TO_JSID(jsint(i)), sp);
        if (!ok)
            goto out;
        sp++;
    }

    ok = js_InvokeConstructor(cx, length, JS_TRUE, invokevp);
    *vp = *invokevp;
out:
    js_FreeStack(cx, mark);
    return ok;
}
#endif

static JSFunctionSpec function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      js_fun_apply,   2,0),
    JS_FN(js_call_str,       js_fun_call,    1,0),
#ifdef NARCISSUS
    JS_FN("__applyConstructor__", fun_applyConstructor, 1,0),
#endif
    JS_FS_END
};

static JSBool
Function(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    JSObject *parent;
    JSStackFrame *fp, *caller;
    uintN i, n, lineno;
    JSAtom *atom;
    const char *filename;
    JSBool ok;
    JSString *str, *arg;
    TokenStream ts(cx);
    JSPrincipals *principals;
    jschar *collected_args, *cp;
    void *mark;
    size_t arg_length, args_length, old_args_length;
    TokenKind tt;

    if (!JS_IsConstructing(cx)) {
        obj = js_NewObject(cx, &js_FunctionClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    } else {
        /*
         * The constructor is called before the private slot is initialized so
         * we must use getPrivate, not GET_FUNCTION_PRIVATE here.
         */
        if (obj->getPrivate())
            return JS_TRUE;
    }

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
    parent = JSVAL_TO_OBJECT(argv[-2])->getParent();

    fun = js_NewFunction(cx, obj, NULL, 0, JSFUN_LAMBDA | JSFUN_INTERPRETED,
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
    fp = js_GetTopStackFrame(cx);
    JS_ASSERT(!fp->script && fp->fun && fp->fun->u.n.native == Function);
    caller = js_GetScriptedCaller(cx, fp);
    if (caller) {
        principals = JS_EvalFramePrincipals(cx, fp, caller);
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
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, 
                             JSMSG_CSP_BLOCKED_FUNCTION);
        return JS_FALSE;
    }

    n = argc ? argc - 1 : 0;
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
        args_length = 0;
        for (i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            arg = js_ValueToString(cx, argv[i]);
            if (!arg)
                return JS_FALSE;
            argv[i] = STRING_TO_JSVAL(arg);

            /*
             * Check for overflow.  The < test works because the maximum
             * JSString length fits in 2 fewer bits than size_t has.
             */
            old_args_length = args_length;
            args_length = old_args_length + arg->length();
            if (args_length < old_args_length) {
                js_ReportAllocationOverflow(cx);
                return JS_FALSE;
            }
        }

        /* Add 1 for each joining comma and check for overflow (two ways). */
        old_args_length = args_length;
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
        mark = JS_ARENA_MARK(&cx->tempPool);
        JS_ARENA_ALLOCATE_CAST(cp, jschar *, &cx->tempPool,
                               (args_length+1) * sizeof(jschar));
        if (!cp) {
            js_ReportOutOfScriptQuota(cx);
            return JS_FALSE;
        }
        collected_args = cp;

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (i = 0; i < n; i++) {
            arg = JSVAL_TO_STRING(argv[i]);
            arg_length = arg->length();
            (void) js_strncpy(cp, arg->chars(), arg_length);
            cp += arg_length;

            /* Add separating comma or terminating 0. */
            *cp++ = (i + 1 < n) ? ',' : 0;
        }

        /* Initialize a tokenstream that reads from the given string. */
        if (!ts.init(collected_args, args_length, NULL, filename, lineno)) {
            JS_ARENA_RELEASE(&cx->tempPool, mark);
            return JS_FALSE;
        }

        /* The argument string may be empty or contain no tokens. */
        tt = ts.getToken();
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
                atom = ts.currentToken().t_atom;

                /* Check for a duplicate parameter name. */
                if (js_LookupLocal(cx, fun, atom, NULL) != JSLOCAL_NONE) {
                    const char *name;

                    name = js_AtomToPrintableString(cx, atom);
                    ok = name && ReportCompileErrorNumber(cx, &ts, NULL,
                                                          JSREPORT_WARNING | JSREPORT_STRICT,
                                                          JSMSG_DUPLICATE_FORMAL, name);
                    if (!ok)
                        goto after_args;
                }
                if (!js_AddLocal(cx, fun, atom, JSLOCAL_ARG))
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
        if (state == BAD_FORMAL && !(ts.flags & TSF_ERROR)) {
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

    if (argc) {
        str = js_ValueToString(cx, argv[argc-1]);
        if (!str)
            return JS_FALSE;
        argv[argc-1] = STRING_TO_JSVAL(str);
    } else {
        str = cx->runtime->emptyString;
    }

    return JSCompiler::compileFunctionBody(cx, fun, principals,
                                           str->chars(), str->length(),
                                           filename, lineno);
}

JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;
    JSFunction *fun;

    proto = JS_InitClass(cx, obj, NULL, &js_FunctionClass, Function, 1,
                         function_props, function_methods, NULL, NULL);
    if (!proto)
        return NULL;
    fun = js_NewFunction(cx, proto, NULL, 0, JSFUN_INTERPRETED, obj, NULL);
    if (!fun)
        return NULL;
    fun->u.i.script = JSScript::emptyScript();
    return proto;
}

JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, JSNative native, uintN nargs,
               uintN flags, JSObject *parent, JSAtom *atom)
{
    JSFunction *fun;

    if (funobj) {
        JS_ASSERT(funobj->isFunction());
        funobj->setParent(parent);
    } else {
        funobj = js_NewObject(cx, &js_FunctionClass, NULL, parent);
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
#ifdef DEBUG
        fun->u.i.names.taggedAtom = 0;
#endif
    } else {
        fun->u.n.extra = 0;
        fun->u.n.spare = 0;
        fun->u.n.clasp = NULL;
        if (flags & JSFUN_TRCINFO) {
#ifdef JS_TRACER
            JSNativeTraceInfo *trcinfo =
                JS_FUNC_TO_DATA_PTR(JSNativeTraceInfo *, native);
            fun->u.n.native = (JSNative) trcinfo->native;
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

    /*
     * The cloned function object does not need the extra JSFunction members
     * beyond JSObject as it points to fun via the private slot.
     */
    JSObject *clone = js_NewObjectWithGivenProto(cx, &js_FunctionClass, proto,
                                                 parent, sizeof(JSObject));
    if (!clone)
        return NULL;
    clone->setPrivate(fun);
    return clone;
}

#ifdef JS_TRACER
JS_DEFINE_CALLINFO_4(extern, OBJECT, js_CloneFunctionObject, CONTEXT, FUNCTION, OBJECT, OBJECT, 0,
                     nanojit::ACC_STORE_ANY)
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

    uint32 nslots = fun->countInterpretedReservedSlots();
    if (!nslots)
        return closure;
    if (!js_EnsureReservedSlots(cx, closure, nslots))
        return NULL;

    return closure;
}

JS_DEFINE_CALLINFO_3(extern, OBJECT, js_AllocFlatClosure,
                     CONTEXT, FUNCTION, OBJECT, 0, nanojit::ACC_STORE_ANY)

JS_REQUIRES_STACK JSObject *
js_NewFlatClosure(JSContext *cx, JSFunction *fun)
{
    /*
     * Flat closures can be partial, they may need to search enclosing scope
     * objects via JSOP_NAME, etc.
     */
    JSObject *scopeChain = js_GetScopeChain(cx, cx->fp);
    if (!scopeChain)
        return NULL;

    JSObject *closure = js_AllocFlatClosure(cx, fun, scopeChain);
    if (!closure || fun->u.i.nupvars == 0)
        return closure;

    JSUpvarArray *uva = fun->u.i.script->upvars();
    JS_ASSERT(uva->length <= size_t(closure->dslots[-1]));

    uintN level = fun->u.i.script->staticLevel;
    for (uint32 i = 0, n = uva->length; i < n; i++)
        closure->dslots[i] = js_GetUpvar(cx, level, uva->vector[i]);

    return closure;
}

JSObject *
js_NewDebuggableFlatClosure(JSContext *cx, JSFunction *fun)
{
    JS_ASSERT(cx->fp->fun->flags & JSFUN_HEAVYWEIGHT);
    JS_ASSERT(!cx->fp->fun->optimizedClosure());
    JS_ASSERT(FUN_FLAT_CLOSURE(fun));

    return WrapEscapingClosure(cx, cx->fp, FUN_OBJECT(fun), fun);
}

JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, JSAtom *atom, JSNative native,
                  uintN nargs, uintN attrs)
{
    JSPropertyOp gsop;
    JSFunction *fun;

    if (attrs & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        attrs &= ~JSFUN_STUB_GSOPS;
        gsop = JS_PropertyStub;
    } else {
        gsop = NULL;
    }
    fun = js_NewFunction(cx, NULL, native, nargs, attrs, obj, atom);
    if (!fun)
        return NULL;
    if (!obj->defineProperty(cx, ATOM_TO_JSID(atom), OBJECT_TO_JSVAL(FUN_OBJECT(fun)),
                             gsop, gsop, attrs & ~JSFUN_FLAGS_MASK)) {
        return NULL;
    }
    return fun;
}

#if (JSV2F_CONSTRUCT & JSV2F_SEARCH_STACK)
# error "JSINVOKE_CONSTRUCT and JSV2F_SEARCH_STACK are not disjoint!"
#endif

JSFunction *
js_ValueToFunction(JSContext *cx, jsval *vp, uintN flags)
{
    jsval v;
    JSObject *obj;

    v = *vp;
    obj = NULL;
    if (JSVAL_IS_OBJECT(v)) {
        obj = JSVAL_TO_OBJECT(v);
        if (obj && OBJ_GET_CLASS(cx, obj) != &js_FunctionClass) {
            if (!obj->defaultValue(cx, JSTYPE_FUNCTION, &v))
                return NULL;
            obj = VALUE_IS_FUNCTION(cx, v) ? JSVAL_TO_OBJECT(v) : NULL;
        }
    }
    if (!obj) {
        js_ReportIsNotFunction(cx, vp, flags);
        return NULL;
    }
    return GET_FUNCTION_PRIVATE(cx, obj);
}

JSObject *
js_ValueToFunctionObject(JSContext *cx, jsval *vp, uintN flags)
{
    JSFunction *fun;
    JSStackFrame *caller;
    JSPrincipals *principals;

    if (VALUE_IS_FUNCTION(cx, *vp))
        return JSVAL_TO_OBJECT(*vp);

    fun = js_ValueToFunction(cx, vp, flags);
    if (!fun)
        return NULL;
    *vp = OBJECT_TO_JSVAL(FUN_OBJECT(fun));

    caller = js_GetScriptedCaller(cx, NULL);
    if (caller) {
        principals = JS_StackFramePrincipals(cx, caller);
    } else {
        /* No scripted caller, don't allow access. */
        principals = NULL;
    }

    if (!js_CheckPrincipalsAccess(cx, FUN_OBJECT(fun), principals,
                                  fun->atom
                                  ? fun->atom
                                  : cx->runtime->atomState.anonymousAtom)) {
        return NULL;
    }
    return FUN_OBJECT(fun);
}

JSObject *
js_ValueToCallableObject(JSContext *cx, jsval *vp, uintN flags)
{
    JSObject *callable = JSVAL_IS_OBJECT(*vp) ? JSVAL_TO_OBJECT(*vp) : NULL;

    if (callable && callable->isCallable()) {
        *vp = OBJECT_TO_JSVAL(callable);
        return callable;
    }
    return js_ValueToFunctionObject(cx, vp, flags);
}

void
js_ReportIsNotFunction(JSContext *cx, jsval *vp, uintN flags)
{
    JSStackFrame *fp;
    uintN error;
    const char *name, *source;

    for (fp = js_GetTopStackFrame(cx); fp && !fp->regs; fp = fp->down)
        continue;
    name = source = NULL;

    AutoValueRooter tvr(cx);
    if (flags & JSV2F_ITERATOR) {
        error = JSMSG_BAD_ITERATOR;
        name = js_iterator_str;
        JSString *src = js_ValueToSource(cx, *vp);
        if (!src)
            return;
        tvr.setString(src);
        JSString *qsrc = js_QuoteString(cx, src, 0);
        if (!qsrc)
            return;
        tvr.setString(qsrc);
        source = js_GetStringBytes(cx, qsrc);
        if (!source)
            return;
    } else if (flags & JSV2F_CONSTRUCT) {
        error = JSMSG_NOT_CONSTRUCTOR;
    } else {
        error = JSMSG_NOT_FUNCTION;
    }

    js_ReportValueError3(cx, error,
                         (fp && fp->regs &&
                          StackBase(fp) <= vp && vp < fp->regs->sp)
                         ? vp - fp->regs->sp
                         : (flags & JSV2F_SEARCH_STACK)
                         ? JSDVG_SEARCH_STACK
                         : JSDVG_IGNORE_STACK,
                         *vp, NULL,
                         name, source);
}

/*
 * When a function has between 2 and MAX_ARRAY_LOCALS arguments and variables,
 * their name are stored as the JSLocalNames.array.
 */
#define MAX_ARRAY_LOCALS 8

JS_STATIC_ASSERT(2 <= MAX_ARRAY_LOCALS);
JS_STATIC_ASSERT(MAX_ARRAY_LOCALS < JS_BITMASK(16));

/*
 * We use the lowest bit of the string atom to distinguish const from var
 * name when there is only single name or when names are stored as an array.
 */
JS_STATIC_ASSERT((JSVAL_STRING & 1) == 0);

/*
 * When we use a hash table to store the local names, we use a singly linked
 * list to record the indexes of duplicated parameter names to preserve the
 * duplicates for the decompiler.
 */
typedef struct JSNameIndexPair JSNameIndexPair;

struct JSNameIndexPair {
    JSAtom          *name;
    uint16          index;
    JSNameIndexPair *link;
};

struct JSLocalNameMap {
    JSDHashTable    names;
    JSNameIndexPair *lastdup;
};

typedef struct JSLocalNameHashEntry {
    JSDHashEntryHdr hdr;
    JSAtom          *name;
    uint16          index;
    uint8           localKind;
} JSLocalNameHashEntry;

static void
FreeLocalNameHash(JSContext *cx, JSLocalNameMap *map)
{
    JSNameIndexPair *dup, *next;

    for (dup = map->lastdup; dup; dup = next) {
        next = dup->link;
        cx->free(dup);
    }
    JS_DHashTableFinish(&map->names);
    cx->free(map);
}

static JSBool
HashLocalName(JSContext *cx, JSLocalNameMap *map, JSAtom *name,
              JSLocalKind localKind, uintN index)
{
    JSLocalNameHashEntry *entry;
    JSNameIndexPair *dup;

    JS_ASSERT(index <= JS_BITMASK(16));
#if JS_HAS_DESTRUCTURING
    if (!name) {
        /* A destructuring pattern does not need a hash entry. */
        JS_ASSERT(localKind == JSLOCAL_ARG);
        return JS_TRUE;
    }
#endif
    JS_ASSERT(ATOM_IS_STRING(name));
    entry = (JSLocalNameHashEntry *)
            JS_DHashTableOperate(&map->names, name, JS_DHASH_ADD);
    if (!entry) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    if (entry->name) {
        JS_ASSERT(entry->name == name);
        JS_ASSERT(entry->localKind == JSLOCAL_ARG && localKind == JSLOCAL_ARG);
        dup = (JSNameIndexPair *) cx->malloc(sizeof *dup);
        if (!dup)
            return JS_FALSE;
        dup->name = entry->name;
        dup->index = entry->index;
        dup->link = map->lastdup;
        map->lastdup = dup;
    }
    entry->name = name;
    entry->index = (uint16) index;
    entry->localKind = (uint8) localKind;
    return JS_TRUE;
}

JSBool
js_AddLocal(JSContext *cx, JSFunction *fun, JSAtom *atom, JSLocalKind kind)
{
    jsuword taggedAtom;
    uint16 *indexp;
    uintN n, i;
    jsuword *array;
    JSLocalNameMap *map;

    JS_ASSERT(FUN_INTERPRETED(fun));
    JS_ASSERT(!fun->u.i.script);
    JS_ASSERT(((jsuword) atom & 1) == 0);
    taggedAtom = (jsuword) atom;
    if (kind == JSLOCAL_ARG) {
        indexp = &fun->nargs;
    } else if (kind == JSLOCAL_UPVAR) {
        indexp = &fun->u.i.nupvars;
    } else {
        indexp = &fun->u.i.nvars;
        if (kind == JSLOCAL_CONST)
            taggedAtom |= 1;
        else
            JS_ASSERT(kind == JSLOCAL_VAR);
    }
    n = fun->countLocalNames();
    if (n == 0) {
        JS_ASSERT(fun->u.i.names.taggedAtom == 0);
        fun->u.i.names.taggedAtom = taggedAtom;
    } else if (n < MAX_ARRAY_LOCALS) {
        if (n > 1) {
            array = fun->u.i.names.array;
        } else {
            array = (jsuword *) cx->malloc(MAX_ARRAY_LOCALS * sizeof *array);
            if (!array)
                return JS_FALSE;
            array[0] = fun->u.i.names.taggedAtom;
            fun->u.i.names.array = array;
        }
        if (kind == JSLOCAL_ARG) {
            /*
             * A destructuring argument pattern adds variables, not arguments,
             * so for the following arguments nvars != 0.
             */
#if JS_HAS_DESTRUCTURING
            if (fun->u.i.nvars != 0) {
                memmove(array + fun->nargs + 1, array + fun->nargs,
                        fun->u.i.nvars * sizeof *array);
            }
#else
            JS_ASSERT(fun->u.i.nvars == 0);
#endif
            array[fun->nargs] = taggedAtom;
        } else {
            array[n] = taggedAtom;
        }
    } else if (n == MAX_ARRAY_LOCALS) {
        array = fun->u.i.names.array;
        map = (JSLocalNameMap *) cx->malloc(sizeof *map);
        if (!map)
            return JS_FALSE;
        if (!JS_DHashTableInit(&map->names, JS_DHashGetStubOps(),
                               NULL, sizeof(JSLocalNameHashEntry),
                               JS_DHASH_DEFAULT_CAPACITY(MAX_ARRAY_LOCALS
                                                         * 2))) {
            JS_ReportOutOfMemory(cx);
            cx->free(map);
            return JS_FALSE;
        }

        map->lastdup = NULL;
        for (i = 0; i != MAX_ARRAY_LOCALS; ++i) {
            taggedAtom = array[i];
            uintN j = i;
            JSLocalKind k = JSLOCAL_ARG;
            if (j >= fun->nargs) {
                j -= fun->nargs;
                if (j < fun->u.i.nvars) {
                    k = (taggedAtom & 1) ? JSLOCAL_CONST : JSLOCAL_VAR;
                } else {
                    j -= fun->u.i.nvars;
                    k = JSLOCAL_UPVAR;
                }
            }
            if (!HashLocalName(cx, map, (JSAtom *) (taggedAtom & ~1), k, j)) {
                FreeLocalNameHash(cx, map);
                return JS_FALSE;
            }
        }
        if (!HashLocalName(cx, map, atom, kind, *indexp)) {
            FreeLocalNameHash(cx, map);
            return JS_FALSE;
        }

        /*
         * At this point the entry is added and we cannot fail. It is time
         * to replace fun->u.i.names with the built map.
         */
        fun->u.i.names.map = map;
        cx->free(array);
    } else {
        if (*indexp == JS_BITMASK(16)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 (kind == JSLOCAL_ARG)
                                 ? JSMSG_TOO_MANY_FUN_ARGS
                                 : JSMSG_TOO_MANY_LOCALS);
            return JS_FALSE;
        }
        if (!HashLocalName(cx, fun->u.i.names.map, atom, kind, *indexp))
            return JS_FALSE;
    }

    /* Update the argument or variable counter. */
    ++*indexp;
    return JS_TRUE;
}

JSLocalKind
js_LookupLocal(JSContext *cx, JSFunction *fun, JSAtom *atom, uintN *indexp)
{
    uintN n, i, upvar_base;
    jsuword *array;
    JSLocalNameHashEntry *entry;

    JS_ASSERT(FUN_INTERPRETED(fun));
    n = fun->countLocalNames();
    if (n == 0)
        return JSLOCAL_NONE;
    if (n <= MAX_ARRAY_LOCALS) {
        array = (n == 1) ? &fun->u.i.names.taggedAtom : fun->u.i.names.array;

        /* Search from the tail to pick up the last duplicated name. */
        i = n;
        upvar_base = fun->countArgsAndVars();
        do {
            --i;
            if (atom == JS_LOCAL_NAME_TO_ATOM(array[i])) {
                if (i < fun->nargs) {
                    if (indexp)
                        *indexp = i;
                    return JSLOCAL_ARG;
                }
                if (i >= upvar_base) {
                    if (indexp)
                        *indexp = i - upvar_base;
                    return JSLOCAL_UPVAR;
                }
                if (indexp)
                    *indexp = i - fun->nargs;
                return JS_LOCAL_NAME_IS_CONST(array[i])
                       ? JSLOCAL_CONST
                       : JSLOCAL_VAR;
            }
        } while (i != 0);
    } else {
        entry = (JSLocalNameHashEntry *)
                JS_DHashTableOperate(&fun->u.i.names.map->names, atom,
                                     JS_DHASH_LOOKUP);
        if (JS_DHASH_ENTRY_IS_BUSY(&entry->hdr)) {
            JS_ASSERT(entry->localKind != JSLOCAL_NONE);
            if (indexp)
                *indexp = entry->index;
            return (JSLocalKind) entry->localKind;
        }
    }
    return JSLOCAL_NONE;
}

typedef struct JSLocalNameEnumeratorArgs {
    JSFunction      *fun;
    jsuword         *names;
#ifdef DEBUG
    uintN           nCopiedArgs;
    uintN           nCopiedVars;
#endif
} JSLocalNameEnumeratorArgs;

static JSDHashOperator
get_local_names_enumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                           uint32 number, void *arg)
{
    JSLocalNameHashEntry *entry;
    JSLocalNameEnumeratorArgs *args;
    uint i;
    jsuword constFlag;

    entry = (JSLocalNameHashEntry *) hdr;
    args = (JSLocalNameEnumeratorArgs *) arg;
    JS_ASSERT(entry->name);
    if (entry->localKind == JSLOCAL_ARG) {
        JS_ASSERT(entry->index < args->fun->nargs);
        JS_ASSERT(args->nCopiedArgs++ < args->fun->nargs);
        i = entry->index;
        constFlag = 0;
    } else {
        JS_ASSERT(entry->localKind == JSLOCAL_VAR ||
                  entry->localKind == JSLOCAL_CONST ||
                  entry->localKind == JSLOCAL_UPVAR);
        JS_ASSERT(entry->index < args->fun->u.i.nvars + args->fun->u.i.nupvars);
        JS_ASSERT(args->nCopiedVars++ < unsigned(args->fun->u.i.nvars + args->fun->u.i.nupvars));
        i = args->fun->nargs;
        if (entry->localKind == JSLOCAL_UPVAR)
           i += args->fun->u.i.nvars;
        i += entry->index;
        constFlag = (entry->localKind == JSLOCAL_CONST);
    }
    args->names[i] = (jsuword) entry->name | constFlag;
    return JS_DHASH_NEXT;
}

JS_FRIEND_API(jsuword *)
js_GetLocalNameArray(JSContext *cx, JSFunction *fun, JSArenaPool *pool)
{
    uintN n;
    jsuword *names;
    JSLocalNameMap *map;
    JSLocalNameEnumeratorArgs args;
    JSNameIndexPair *dup;

    JS_ASSERT(fun->hasLocalNames());
    n = fun->countLocalNames();

    if (n <= MAX_ARRAY_LOCALS)
        return (n == 1) ? &fun->u.i.names.taggedAtom : fun->u.i.names.array;

    /*
     * No need to check for overflow of the allocation size as we are making a
     * copy of already allocated data. As such it must fit size_t.
     */
    JS_ARENA_ALLOCATE_CAST(names, jsuword *, pool, (size_t) n * sizeof *names);
    if (!names) {
        js_ReportOutOfScriptQuota(cx);
        return NULL;
    }

#if JS_HAS_DESTRUCTURING
    /* Some parameter names can be NULL due to destructuring patterns. */
    PodZero(names, fun->nargs);
#endif
    map = fun->u.i.names.map;
    args.fun = fun;
    args.names = names;
#ifdef DEBUG
    args.nCopiedArgs = 0;
    args.nCopiedVars = 0;
#endif
    JS_DHashTableEnumerate(&map->names, get_local_names_enumerator, &args);
    for (dup = map->lastdup; dup; dup = dup->link) {
        JS_ASSERT(dup->index < fun->nargs);
        JS_ASSERT(args.nCopiedArgs++ < fun->nargs);
        names[dup->index] = (jsuword) dup->name;
    }
#if !JS_HAS_DESTRUCTURING
    JS_ASSERT(args.nCopiedArgs == fun->nargs);
#endif
    JS_ASSERT(args.nCopiedVars == fun->u.i.nvars + fun->u.i.nupvars);

    return names;
}

static JSDHashOperator
trace_local_names_enumerator(JSDHashTable *table, JSDHashEntryHdr *hdr,
                             uint32 number, void *arg)
{
    JSLocalNameHashEntry *entry;
    JSTracer *trc;

    entry = (JSLocalNameHashEntry *) hdr;
    JS_ASSERT(entry->name);
    trc = (JSTracer *) arg;
    JS_SET_TRACING_INDEX(trc,
                         entry->localKind == JSLOCAL_ARG ? "arg" : "var",
                         entry->index);
    js_CallGCMarker(trc, ATOM_TO_STRING(entry->name), JSTRACE_STRING);
    return JS_DHASH_NEXT;
}

static void
TraceLocalNames(JSTracer *trc, JSFunction *fun)
{
    uintN n, i;
    JSAtom *atom;
    jsuword *array;

    JS_ASSERT(FUN_INTERPRETED(fun));
    n = fun->countLocalNames();
    if (n == 0)
        return;
    if (n <= MAX_ARRAY_LOCALS) {
        array = (n == 1) ? &fun->u.i.names.taggedAtom : fun->u.i.names.array;
        i = n;
        do {
            --i;
            atom = (JSAtom *) (array[i] & ~1);
            if (atom) {
                JS_SET_TRACING_INDEX(trc,
                                     i < fun->nargs ? "arg" : "var",
                                     i < fun->nargs ? i : i - fun->nargs);
                js_CallGCMarker(trc, ATOM_TO_STRING(atom), JSTRACE_STRING);
            }
        } while (i != 0);
    } else {
        JS_DHashTableEnumerate(&fun->u.i.names.map->names,
                               trace_local_names_enumerator, trc);

        /*
         * No need to trace the list of duplicates in map->lastdup as the
         * names there are traced when enumerating the hash table.
         */
    }
}

void
DestroyLocalNames(JSContext *cx, JSFunction *fun)
{
    uintN n;

    n = fun->countLocalNames();
    if (n <= 1)
        return;
    if (n <= MAX_ARRAY_LOCALS)
        cx->free(fun->u.i.names.array);
    else
        FreeLocalNameHash(cx, fun->u.i.names.map);
}

void
js_FreezeLocalNames(JSContext *cx, JSFunction *fun)
{
    uintN n;
    jsuword *array;

    JS_ASSERT(FUN_INTERPRETED(fun));
    JS_ASSERT(!fun->u.i.script);
    n = fun->nargs + fun->u.i.nvars + fun->u.i.nupvars;
    if (2 <= n && n < MAX_ARRAY_LOCALS) {
        /* Shrink over-allocated array ignoring realloc failures. */
        array = (jsuword *) cx->realloc(fun->u.i.names.array,
                                        n * sizeof *array);
        if (array)
            fun->u.i.names.array = array;
    }
#ifdef DEBUG
    if (n > MAX_ARRAY_LOCALS)
        JS_DHashMarkTableImmutable(&fun->u.i.names.map->names);
#endif
}

JSAtom *
JSFunction::findDuplicateFormal() const
{
    if (nargs <= 1)
        return NULL;

    /* Function with two to MAX_ARRAY_LOCALS parameters use an aray. */
    unsigned n = nargs + u.i.nvars + u.i.nupvars;
    if (n <= MAX_ARRAY_LOCALS) {
        jsuword *array = u.i.names.array;

        /* Quadratic, but MAX_ARRAY_LOCALS is 8, so at most 28 comparisons. */
        for (unsigned i = 0; i < nargs; i++) {
            for (unsigned j = i + 1; j < nargs; j++) {
                if (array[i] == array[j])
                    return JS_LOCAL_NAME_TO_ATOM(array[i]);
            }
        }
        return NULL;
    }

    /*
     * Functions with more than MAX_ARRAY_LOCALS parameters use a hash
     * table. Hashed local name maps have already made a list of any
     * duplicate argument names for us.
     */
    JSNameIndexPair *dup = u.i.names.map->lastdup;
    return dup ? dup->name : NULL;
}
