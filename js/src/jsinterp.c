/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/*
 * JavaScript bytecode interpreter.
 */
#include "jsstddef.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "jstypes.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsdbgapi.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"

void
js_FlushPropertyCache(JSContext *cx)
{
    JSPropertyCache *cache;

    cache = &cx->runtime->propertyCache;
    if (cache->empty)
        return;
    memset(cache->table, 0, sizeof cache->table);
    cache->empty = JS_TRUE;
    cache->flushes++;
}

void
js_FlushPropertyCacheByProp(JSContext *cx, JSProperty *prop)
{
    JSPropertyCache *cache;
    JSBool empty;
    JSPropertyCacheEntry *end, *pce, entry;
    JSProperty *pce_prop;

    cache = &cx->runtime->propertyCache;
    if (cache->empty)
        return;

    empty = JS_TRUE;
    end = &cache->table[PROPERTY_CACHE_SIZE];
    for (pce = &cache->table[0]; pce < end; pce++) {
        PCE_LOAD(cache, pce, entry);
        pce_prop = PCE_PROPERTY(entry);
        if (pce_prop) {
            if (pce_prop == prop) {
                PCE_OBJECT(entry) = NULL;
                PCE_PROPERTY(entry) = NULL;
                PCE_STORE(cache, pce, entry);
            } else {
                empty = JS_FALSE;
            }
        }
    }
    cache->empty = empty;
}

void
js_FlushPropertyCacheNotFounds(JSContext *cx)
{
    JSPropertyCache *cache;
    JSBool empty;
    JSPropertyCacheEntry *end, *pce, entry;
    JSProperty *pce_prop;

    cache = &cx->runtime->propertyCache;
    if (cache->empty)
        return;

    empty = JS_TRUE;
    end = &cache->table[PROPERTY_CACHE_SIZE];
    for (pce = &cache->table[0]; pce < end; pce++) {
        PCE_LOAD(cache, pce, entry);
        pce_prop = PCE_PROPERTY(entry);
        if (pce_prop) {
            if (!PROP_FOUND(pce_prop)) {
                PCE_OBJECT(entry) = NULL;
                PCE_PROPERTY(entry) = NULL;
                PCE_STORE(cache, pce, entry);
            } else {
                empty = JS_FALSE;
            }
        }
    }
    cache->empty = empty;
}

/*
 * Class for for/in loop property iterator objects.
 */
#define JSSLOT_ITER_STATE   (JSSLOT_START)

static void
prop_iterator_finalize(JSContext *cx, JSObject *obj)
{
    jsval iter_state;
    jsval iteratee;

    /* Protect against stillborn iterators. */
    iter_state = obj->slots[JSSLOT_ITER_STATE];
    iteratee = obj->slots[JSSLOT_PARENT];
    if (iter_state != JSVAL_NULL && !JSVAL_IS_PRIMITIVE(iteratee)) {
        OBJ_ENUMERATE(cx, JSVAL_TO_OBJECT(iteratee), JSENUMERATE_DESTROY,
                      &iter_state, NULL);
    }
    js_RemoveRoot(cx->runtime, &obj->slots[JSSLOT_PARENT]);
}

static JSClass prop_iterator_class = {
    "PropertyIterator",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   prop_iterator_finalize,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

/*
 * Stack macros and functions.  These all use a local variable, jsval *sp, to
 * point to the next free stack slot.  SAVE_SP must be called before any call
 * to a function that may invoke the interpreter.  RESTORE_SP must be called
 * only after return from js_Invoke, because only js_Invoke changes fp->sp.
 */
#define PUSH(v)         (*sp++ = (v))
#define POP()           (*--sp)
#define SAVE_SP(fp)     ((fp)->sp = sp)
#define RESTORE_SP(fp)  (sp = (fp)->sp)

/*
 * Push the generating bytecode's pc onto the parallel pc stack that runs
 * depth slots below the operands.
 *
 * NB: PUSH_OPND uses sp, depth, and pc from its lexical environment.  See
 * Interpret for these local variables' declarations and uses.
 */
#define PUSH_OPND(v)    (sp[-depth] = (jsval)pc, PUSH(v))

/*
 * Push the jsdouble d using sp, depth, and pc from the lexical environment.
 * Try to convert d to a jsint that fits in a jsval, otherwise GC-alloc space
 * for it and push a reference.
 */
#define PUSH_NUMBER(cx, d)                                                    \
    JS_BEGIN_MACRO                                                            \
        jsint _i;                                                             \
        jsval _v;                                                             \
                                                                              \
        if (JSDOUBLE_IS_INT(d, _i) && INT_FITS_IN_JSVAL(_i)) {                \
            _v = INT_TO_JSVAL(_i);                                            \
        } else {                                                              \
            ok = js_NewDoubleValue(cx, d, &_v);                               \
            if (!ok)                                                          \
                goto out;                                                     \
        }                                                                     \
        PUSH_OPND(_v);                                                        \
    JS_END_MACRO

#define POP_NUMBER(cx, d)                                                     \
    JS_BEGIN_MACRO                                                            \
        jsval _v;                                                             \
                                                                              \
        _v = POP();                                                           \
        VALUE_TO_NUMBER(cx, _v, d);                                           \
    JS_END_MACRO

/*
 * This POP variant is called only for bitwise operators, so we don't bother
 * to inline it.  The calls in Interpret must therefore SAVE_SP first!
 */
static JSBool
PopInt(JSContext *cx, jsint *ip)
{
    JSStackFrame *fp;
    jsval *sp, v;

    fp = cx->fp;
    RESTORE_SP(fp);
    v = POP();
    SAVE_SP(fp);
    if (JSVAL_IS_INT(v)) {
        *ip = JSVAL_TO_INT(v);
        return JS_TRUE;
    }
    return js_ValueToECMAInt32(cx, v, (int32 *)ip);
}

static JSBool
PopUint(JSContext *cx, jsuint *ip)
{
    JSStackFrame *fp;
    jsval *sp, v;
    jsint i;

    fp = cx->fp;
    RESTORE_SP(fp);
    v = POP();
    SAVE_SP(fp);
    if (JSVAL_IS_INT(v) && (i = JSVAL_TO_INT(v)) >= 0) {
        *ip = i;
        return JS_TRUE;
    }
    return js_ValueToECMAUint32(cx, v, (uint32 *)ip);
}

/*
 * Optimized conversion macros that test for the desired type in v before
 * homing sp and calling a conversion function.
 */
#define VALUE_TO_NUMBER(cx, v, d)                                             \
    JS_BEGIN_MACRO                                                            \
        if (JSVAL_IS_INT(v)) {                                                \
            d = (jsdouble)JSVAL_TO_INT(v);                                    \
        } else if (JSVAL_IS_DOUBLE(v)) {                                      \
            d = *JSVAL_TO_DOUBLE(v);                                          \
        } else {                                                              \
            SAVE_SP(fp);                                                      \
            ok = js_ValueToNumber(cx, v, &d);                                 \
            if (!ok)                                                          \
                goto out;                                                     \
        }                                                                     \
    JS_END_MACRO

#define POP_BOOLEAN(cx, v, b)                                                 \
    JS_BEGIN_MACRO                                                            \
        v = POP();                                                            \
        if (v == JSVAL_NULL) {                                                \
            b = JS_FALSE;                                                     \
        } else if (JSVAL_IS_BOOLEAN(v)) {                                     \
            b = JSVAL_TO_BOOLEAN(v);                                          \
        } else {                                                              \
            SAVE_SP(fp);                                                      \
            ok = js_ValueToBoolean(cx, v, &b);                                \
            if (!ok)                                                          \
                goto out;                                                     \
        }                                                                     \
    JS_END_MACRO

#define VALUE_TO_OBJECT(cx, v, obj)                                           \
    JS_BEGIN_MACRO                                                            \
        if (JSVAL_IS_OBJECT(v) && v != JSVAL_NULL) {                          \
            obj = JSVAL_TO_OBJECT(v);                                         \
        } else {                                                              \
            SAVE_SP(fp);                                                      \
            obj = js_ValueToNonNullObject(cx, v);                             \
            if (!obj) {                                                       \
                ok = JS_FALSE;                                                \
                goto out;                                                     \
            }                                                                 \
        }                                                                     \
    JS_END_MACRO

#if JS_BUG_VOID_TOSTRING
#define CHECK_VOID_TOSTRING(cx, v)                                            \
    if (JSVAL_IS_VOID(v)) {                                                   \
        JSString *_str;                                                       \
        _str = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]); \
        v = STRING_TO_JSVAL(_str);                                            \
    }
#else
#define CHECK_VOID_TOSTRING(cx, v)  ((void)0)
#endif

#if JS_BUG_EAGER_TOSTRING
#define CHECK_EAGER_TOSTRING(hint)  (hint = JSTYPE_STRING)
#else
#define CHECK_EAGER_TOSTRING(hint)  ((void)0)
#endif

#define VALUE_TO_PRIMITIVE(cx, v, hint, vp)                                   \
    JS_BEGIN_MACRO                                                            \
        if (JSVAL_IS_PRIMITIVE(v)) {                                          \
            CHECK_VOID_TOSTRING(cx, v);                                       \
            *vp = v;                                                          \
        } else {                                                              \
            SAVE_SP(fp);                                                      \
            CHECK_EAGER_TOSTRING(hint);                                       \
            ok = OBJ_DEFAULT_VALUE(cx, JSVAL_TO_OBJECT(v), hint, vp);         \
            if (!ok)                                                          \
                goto out;                                                     \
        }                                                                     \
    JS_END_MACRO

JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp)
{
    jsval *sp;

    if (markp)
        *markp = JS_ARENA_MARK(&cx->stackPool);
    JS_ARENA_ALLOCATE(sp, &cx->stackPool, nslots * sizeof(jsval));
    if (!sp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_STACK_OVERFLOW,
                             (cx->fp && cx->fp->fun)
                             ? JS_GetFunctionName(cx->fp->fun)
                             : "script");
    }
    return sp;
}

JS_FRIEND_API(void)
js_FreeStack(JSContext *cx, void *mark)
{
    JS_ARENA_RELEASE(&cx->stackPool, mark);
}

JSBool
js_GetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
        /* Find most recent non-native function frame. */
        if (fp->fun && !fp->fun->native) {
            if (fp->fun == fun) {
                JS_ASSERT((uintN)JSVAL_TO_INT(id) < fp->fun->nargs);
                *vp = fp->argv[JSVAL_TO_INT(id)];
            }
            return JS_TRUE;
        }
    }
    return JS_TRUE;
}

JSBool
js_SetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
        /* Find most recent non-native function frame. */
        if (fp->fun && !fp->fun->native) {
            if (fp->fun == fun) {
                JS_ASSERT((uintN)JSVAL_TO_INT(id) < fp->fun->nargs);
                fp->argv[JSVAL_TO_INT(id)] = *vp;
            }
            return JS_TRUE;
        }
    }
    return JS_TRUE;
}

JSBool
js_GetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;
    jsint slot;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
        /* Find most recent non-native function frame. */
        if (fp->fun && !fp->fun->native) {
            if (fp->fun == fun) {
                slot = JSVAL_TO_INT(id);
                JS_ASSERT((uintN)slot < fp->fun->nvars);
                if ((uintN)slot < fp->nvars)
                    *vp = fp->vars[slot];
            }
            return JS_TRUE;
        }
    }
    return JS_TRUE;
}

JSBool
js_SetLocalVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;
    jsint slot;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
        /* Find most recent non-native function frame. */
        if (fp->fun && !fp->fun->native) {
            if (fp->fun == fun) {
                slot = JSVAL_TO_INT(id);
                JS_ASSERT((uintN)slot < fp->fun->nvars);
                if ((uintN)slot < fp->nvars)
                    fp->vars[slot] = *vp;
            }
            return JS_TRUE;
        }
    }
    return JS_TRUE;
}

/*
 * Compute the 'this' parameter and store it in frame as frame.thisp.
 * Activation objects ("Call" objects not created with "new Call()", i.e.,
 * "Call" objects that have private data) may not be referred to by 'this',
 * as dictated by ECMA.
 *
 * N.B.: fp->argv must be set, and fp->argv[-2] must be the callee object
 * reference, usually a function object.  Also, fp->constructing must be
 * set if we are preparing for a constructor call.
 */
static JSBool
ComputeThis(JSContext *cx, JSObject *thisp, JSStackFrame *fp)
{
    JSObject *parent;

    if (thisp &&
        !(OBJ_GET_CLASS(cx, thisp) == &js_CallClass &&
          JS_GetPrivate(cx, thisp) != NULL))
    {
        /* Some objects (e.g., With) delegate 'this' to another object. */
        thisp = OBJ_THIS_OBJECT(cx, thisp);
        if (!thisp)
            return JS_FALSE;

        /* Default return value for a constructor is the new object. */
        if (fp->constructing)
            fp->rval = OBJECT_TO_JSVAL(thisp);
    } else {
        /*
         * ECMA requires "the global object", but in the presence of multiple
         * top-level objects (windows, frames, or certain layers in the client
         * object model), we prefer fun's parent.  An example that causes this
         * code to run:
         *
         *   // in window w1
         *   function f() { return this }
         *   function g() { return f }
         *
         *   // in window w2
         *   var h = w1.g()
         *   alert(h() == w1)
         *
         * The alert should display "true".
         */
        JS_ASSERT(!fp->constructing);
        parent = OBJ_GET_PARENT(cx, JSVAL_TO_OBJECT(fp->argv[-2]));
        if (!parent) {
            thisp = cx->globalObject;
        } else {
            /* walk up to find the top-level object */
            thisp = parent;
            while ((parent = OBJ_GET_PARENT(cx, thisp)) != NULL)
                thisp = parent;
        }
    }
    fp->thisp = thisp;
    return JS_TRUE;
}

/*
 * Find a function reference and its 'this' object implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
JS_FRIEND_API(JSBool)
js_Invoke(JSContext *cx, uintN argc, uintN flags)
{
    JSStackFrame *fp, frame;
    jsval *sp, *newsp, *limit;
    jsval *vp, v;
    JSObject *funobj, *parent, *thisp;
    JSClass *clasp;
    JSObjectOps *ops;
    JSBool ok;
    JSNative native;
    JSFunction *fun;
    JSScript *script;
    uintN minargs, nvars;
    void *mark;
    intN nslots, nalloc, surplus;
    JSInterpreterHook hook;
    void *hookData;

    /* Reach under args and this to find the callee on the stack. */
    fp = cx->fp;
    sp = fp->sp;

    /*
     * Set vp to the callee value's stack slot (it's where rval goes).
     * Once vp is set, control must flow through label out2: to return.
     * Set frame.rval early so native class and object ops can throw and
     * return false, causing a goto out2 with ok set to false.  Also set
     * frame.constructing so we may test it anywhere below.
     */
    vp = sp - (2 + argc);
    v = *vp;
    frame.rval = JSVAL_VOID;
    frame.constructing = (JSPackedBool)(flags & JSINVOKE_CONSTRUCT);

    /* A callee must be an object reference. */
    if (JSVAL_IS_PRIMITIVE(v))
        goto bad;
    funobj = JSVAL_TO_OBJECT(v);

    /* Load callee parent and this parameter for later. */
    parent = OBJ_GET_PARENT(cx, funobj);
    thisp = JSVAL_TO_OBJECT(vp[1]);

    clasp = OBJ_GET_CLASS(cx, funobj);
    if (clasp != &js_FunctionClass) {
        /* Function is inlined, all other classes use object ops. */
        ops = funobj->map->ops;

        /*
         * XXX
         * Try converting to function, for closure and API compatibility.
         * We attempt the conversion under all circumstances for 1.2, but
         * only if there is a call op defined otherwise.
         */
        if (cx->version == JSVERSION_1_2 ||
            ((ops == &js_ObjectOps) ? clasp->call : ops->call)) {
            ok = clasp->convert(cx, funobj, JSTYPE_FUNCTION, &v);
            if (!ok)
                goto out2;

            if (JSVAL_IS_FUNCTION(cx, v)) {
                funobj = JSVAL_TO_OBJECT(v);
                parent = OBJ_GET_PARENT(cx, funobj);
                fun = (JSFunction *) JS_GetPrivate(cx, funobj);

                /* Make vp refer to funobj to keep it available as argv[-2]. */
                *vp = v;
                goto have_fun;
            }
        }
        fun = NULL;
        script = NULL;
        minargs = nvars = 0;

        /* Try a call or construct native object op, using fun as fallback. */
        native = frame.constructing ? ops->construct : ops->call;
        if (!native)
            goto bad;
    } else {
        /* Get private data and set derived locals from it. */
        fun = (JSFunction *) JS_GetPrivate(cx, funobj);
have_fun:
        native = fun->native;
        script = fun->script;
        minargs = fun->nargs + fun->extra;
        nvars = fun->nvars;

        /* Handle bound method special case. */
        if (fun->flags & JSFUN_BOUND_METHOD)
            thisp = parent;
    }

    /* Initialize most of frame, except for varobj, thisp, and scopeChain. */
    frame.varobj = NULL;
    frame.callobj = frame.argsobj = NULL;
    frame.script = script;
    frame.fun = fun;
    frame.argc = argc;
    frame.argv = sp - argc;
    frame.nvars = nvars;
    frame.vars = sp;
    frame.down = fp;
    frame.annotation = NULL;
    frame.scopeChain = NULL;    /* set below for real, after cx->fp is set */
    frame.pc = NULL;
    frame.sp = sp;
    frame.sharpDepth = 0;
    frame.sharpArray = NULL;
    frame.overrides = 0;
    frame.special = 0;
    frame.dormantNext = NULL;

    /* Compute the 'this' parameter and store it in frame as frame.thisp. */
    ok = ComputeThis(cx, thisp, &frame);
    if (!ok)
        goto out2;

    /* From here on, control must flow through label out: to return. */
    cx->fp = &frame;
    mark = JS_ARENA_MARK(&cx->stackPool);

    /* Init these now in case we goto out before first hook call. */
    hook = cx->runtime->callHook;
    hookData = NULL;

    /* Check for missing arguments expected by the function. */
    nslots = (intN)((argc < minargs) ? minargs - argc : 0);
    if (nslots) {
        /* All arguments must be contiguous, so we may have to copy actuals. */
        nalloc = nslots;
        limit = (jsval *) cx->stackPool.current->limit;
        if (sp + nslots > limit) {
            /* Hit end of arena: we have to copy argv[-2..(argc+nslots-1)]. */
            nalloc += 2 + argc;
        } else {
            /* Take advantage of surplus slots in the caller's frame depth. */
            surplus = (jsval *)mark - sp;
            JS_ASSERT(surplus >= 0);
            nalloc -= surplus;
        }

        /* Check whether we have enough space in the caller's frame. */
        if (nalloc > 0) {
            /* Need space for actuals plus missing formals minus surplus. */
            newsp = js_AllocStack(cx, (uintN)nalloc, NULL);
            if (!newsp) {
                ok = JS_FALSE;
                goto out;
            }

            /* If we couldn't allocate contiguous args, copy actuals now. */
            if (newsp != mark) {
                JS_ASSERT(sp + nslots > limit);
                JS_ASSERT(2 + argc + nslots == (uintN)nalloc);
                *newsp++ = vp[0];
                *newsp++ = vp[1];
                if (argc)
                    memcpy(newsp, frame.argv, argc * sizeof(jsval));
                frame.argv = newsp;
                frame.vars = frame.sp = newsp + argc;
                RESTORE_SP(&frame);
            }
        }

        /* Advance frame.vars to make room for the missing args. */
        frame.vars += nslots;

        /* Push void to initialize missing args. */
        while (--nslots >= 0)
            PUSH(JSVAL_VOID);
    }

    /* Now allocate stack space for local variables. */
    nslots = (intN)frame.nvars;
    if (nslots) {
        surplus = (intN)((jsval *)cx->stackPool.current->avail - frame.vars);
        if (surplus < nslots) {
            newsp = js_AllocStack(cx, (uintN)nslots, NULL);
            if (!newsp) {
                ok = JS_FALSE;
                goto out;
            }
            if (newsp != sp) {
                /* NB: Discontinuity between argv and vars. */
                frame.vars = frame.sp = newsp;
                RESTORE_SP(&frame);
            }
        }

        /* Push void to initialize local variables. */
        while (--nslots >= 0)
            PUSH(JSVAL_VOID);
    }

    /* Store the current sp in frame before calling fun. */
    SAVE_SP(&frame);

    /* call the hook if present */
    if (hook && (native || script))
        hookData = hook(cx, &frame, JS_TRUE, 0, cx->runtime->callHookData);

    /* Call the function, either a native method or an interpreted script. */
    if (native) {
        /* Set by JS_SetCallReturnValue2, used to return reference types. */
        cx->rval2set = JS_FALSE;

        /* If native, use caller varobj and scopeChain for eval. */
        frame.varobj = fp->varobj;
        frame.scopeChain = fp->scopeChain;
        ok = native(cx, frame.thisp, argc, frame.argv, &frame.rval);
    } else if (script) {
        /* Use parent scope so js_GetCallObject can find the right "Call". */
        frame.scopeChain = parent;
        if (fun->flags & JSFUN_HEAVYWEIGHT) {
#if JS_HAS_CALL_OBJECT
            /* Scope with a call object parented by the callee's parent. */
            if (!js_GetCallObject(cx, &frame, parent)) {
                ok = JS_FALSE;
                goto out;
            }
#else
            /* Bad old code used the function as a proxy for all calls to it. */
            frame.scopeChain = funobj;
#endif
        }
        ok = js_Interpret(cx, &v);
    } else {
        /* fun might be onerror trying to report a syntax error in itself. */
        frame.scopeChain = NULL;
        ok = JS_TRUE;
    }

out:
    if (hook && hookData)
        hook(cx, &frame, JS_FALSE, &ok, hookData);
#if JS_HAS_CALL_OBJECT
    /* If frame has a call object, sync values and clear back-pointer. */
    if (frame.callobj)
        ok &= js_PutCallObject(cx, &frame);
#endif
#if JS_HAS_ARGS_OBJECT
    /* If frame has an arguments object, sync values and clear back-pointer. */
    if (frame.argsobj)
        ok &= js_PutArgsObject(cx, &frame);
#endif

    /* Pop everything off the stack and restore cx->fp. */
    JS_ARENA_RELEASE(&cx->stackPool, mark);
    cx->fp = fp;

out2:
    /* Store the return value and restore sp just above it. */
    *vp = frame.rval;
    fp->sp = vp + 1;

    /*
     * Store the location of the JSOP_CALL or JSOP_EVAL that generated the
     * return value, but only if this is an external (compiled from script
     * source) call that has stack budget for the generating pc.
     */
    if (fp->script && !(flags & JSINVOKE_INTERNAL))
        vp[-(intN)fp->script->depth] = (jsval)fp->pc;
    return ok;

bad:
    js_ReportIsNotFunction(cx, vp, frame.constructing);
    ok = JS_FALSE;
    goto out2;
}

JSBool
js_InternalInvoke(JSContext *cx, JSObject *obj, jsval fval, uintN flags,
                  uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *fp, *oldfp, frame;
    jsval *oldsp, *sp;
    void *mark;
    uintN i;
    JSBool ok;

    fp = oldfp = cx->fp;
    if (!fp) {
        memset(&frame, 0, sizeof frame);
        cx->fp = fp = &frame;
    }
    oldsp = fp->sp;
    sp = js_AllocStack(cx, 2 + argc, &mark);
    if (!sp) {
        ok = JS_FALSE;
        goto out;
    }
    fp->sp = sp;

    PUSH(fval);
    PUSH(OBJECT_TO_JSVAL(obj));
    for (i = 0; i < argc; i++)
        PUSH(argv[i]);
    SAVE_SP(fp);
    ok = js_Invoke(cx, argc, flags | JSINVOKE_INTERNAL);
    if (ok) {
        RESTORE_SP(fp);
        *rval = POP();
    }

    js_FreeStack(cx, mark);
out:
    fp->sp = oldsp;
    if (oldfp != fp)
        cx->fp = oldfp;

    return ok;
}

JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script, JSFunction *fun,
           JSStackFrame *down, uintN special, jsval *result)
{
    JSStackFrame *oldfp, frame;
    JSBool ok;
    JSInterpreterHook hook;
    void *hookData;

    hook = cx->runtime->executeHook;
    hookData = NULL;
    oldfp = cx->fp;
    frame.callobj = frame.argsobj = NULL;
    frame.script = script;
    frame.fun = fun;
    if (down) {
        /* Propagate arg/var state for eval and the debugger API. */
        frame.varobj = down->varobj;
        frame.thisp = down->thisp;
        frame.argc = down->argc;
        frame.argv = down->argv;
        frame.nvars = down->nvars;
        frame.vars = down->vars;
        frame.annotation = down->annotation;
        frame.sharpArray = down->sharpArray;
    } else {
        frame.varobj = chain;
        frame.thisp = chain;
        frame.argc = frame.nvars = 0;
        frame.argv = frame.vars = NULL;
        frame.annotation = NULL;
        frame.sharpArray = NULL;
    }
    frame.rval = JSVAL_VOID;
    frame.down = down;
    frame.scopeChain = chain;
    frame.pc = NULL;
    frame.sp = oldfp ? oldfp->sp : NULL;
    frame.sharpDepth = 0;
    frame.constructing = JS_FALSE;
    frame.overrides = 0;
    frame.special = (uint8) special;
    frame.dormantNext = NULL;

    /*
     * Here we wrap the call to js_Interpret with code to (conditionally)
     * save and restore the old stack frame chain into a chain of 'dormant'
     * frame chains.  Since we are replacing cx->fp, we were running into
     * the problem that if GC was called under this frame, some of the GC
     * things associated with the old frame chain (available here only in
     * the C variable 'oldfp') were not rooted and were being collected.
     *
     * So, now we preserve the links to these 'dormant' frame chains in cx
     * before calling js_Interpret and cleanup afterwards.  The GC walks
     * these dormant chains and marks objects in the same way that it marks
     * objects in the primary cx->fp chain.
     */
    if (oldfp && oldfp != down) {
        JS_ASSERT(!oldfp->dormantNext);
        oldfp->dormantNext = cx->dormantFrameChain;
        cx->dormantFrameChain = oldfp;
    }

    cx->fp = &frame;
    if (hook)
        hookData = hook(cx, &frame, JS_TRUE, 0, cx->runtime->executeHookData);

    ok = js_Interpret(cx, result);

    if (hook && hookData)
        hook(cx, &frame, JS_FALSE, &ok, hookData);
    cx->fp = oldfp;

    if (oldfp && oldfp != down) {
        JS_ASSERT(cx->dormantFrameChain == oldfp);
        cx->dormantFrameChain = oldfp->dormantNext;
        oldfp->dormantNext = NULL;
    }

    return ok;
}

#if JS_HAS_EXPORT_IMPORT
/*
 * If id is JSVAL_VOID, import all exported properties from obj.
 */
static JSBool
ImportProperty(JSContext *cx, JSObject *obj, jsid id)
{
    JSBool ok;
    JSIdArray *ida;
    JSProperty *prop;
    JSObject *obj2, *target, *funobj, *closure;
    JSString *str;
    uintN attrs;
    jsint i;
    jsval value;

    if (JSVAL_IS_VOID(id)) {
        ida = JS_Enumerate(cx, obj);
        if (!ida)
            return JS_FALSE;
        ok = JS_TRUE;
        if (ida->length == 0)
            goto out;
    } else {
        ida = NULL;
        if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
            return JS_FALSE;
        if (!prop) {
            str = js_DecompileValueGenerator(cx, JS_FALSE, js_IdToValue(id),
                                             NULL);
            if (str)
                js_ReportIsNotDefined(cx, JS_GetStringBytes(str));
            return JS_FALSE;
        }
        ok = OBJ_GET_ATTRIBUTES(cx, obj, id, prop, &attrs);
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        if (!ok)
            return JS_FALSE;
        if (!(attrs & JSPROP_EXPORTED)) {
            str = js_DecompileValueGenerator(cx, JS_FALSE, js_IdToValue(id),
                                             NULL);
            if (str) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_NOT_EXPORTED,
                                     JS_GetStringBytes(str));
            }
            return JS_FALSE;
        }
    }

    target = cx->fp->varobj;
    i = 0;
    do {
        if (ida) {
            id = ida->vector[i];
            ok = OBJ_GET_ATTRIBUTES(cx, obj, id, NULL, &attrs);
            if (!ok)
                goto out;
            if (!(attrs & JSPROP_EXPORTED))
                continue;
        }
        ok = OBJ_CHECK_ACCESS(cx, obj, id, JSACC_IMPORT, &value, &attrs);
        if (!ok)
            goto out;
        if (JSVAL_IS_FUNCTION(cx, value)) {
            funobj = JSVAL_TO_OBJECT(value);
            closure = js_CloneFunctionObject(cx, funobj, obj);
            if (!closure) {
                ok = JS_FALSE;
                goto out;
            }
            value = OBJECT_TO_JSVAL(closure);
        }

        /*
         * Handle the case of importing a property that refers to a local
         * variable or formal parameter of a function activation.  Those
         * properties are accessed by opcodes using stack slot numbers
         * generated by the compiler rather than runtime name-lookup. These
         * local references, therefore, bypass the normal scope chain lookup.
         * So, instead of defining a new property in the activation object,
         * modify the existing value in the stack slot.
         */
        if (OBJ_GET_CLASS(cx, target) == &js_CallClass) {
            ok = OBJ_LOOKUP_PROPERTY(cx, target, id, &obj2, &prop);
            if (!ok)
                goto out;
        } else {
            prop = NULL;
        }
        if (prop && target == obj2) {
            ok = OBJ_SET_PROPERTY(cx, target, id, &value);
        } else {
            ok = OBJ_DEFINE_PROPERTY(cx, target, id, value, NULL, NULL,
                                     attrs & ~JSPROP_EXPORTED,
                                     NULL);
        }
        if (prop)
            OBJ_DROP_PROPERTY(cx, obj2, prop);
        if (!ok)
            goto out;
    } while (ida && ++i < ida->length);

out:
    if (ida)
        JS_DestroyIdArray(cx, ida);
    return ok;
}
#endif /* JS_HAS_EXPORT_IMPORT */

JSBool
js_CheckRedeclaration(JSContext *cx, JSObject *obj, jsid id, uintN attrs,
                      JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;
    uintN oldAttrs, report;
    JSBool isFunction;
    jsval value;

    if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
        return JS_FALSE;
    *foundp = (prop != NULL);
    if (!prop)
        return JS_TRUE;
    ok = OBJ_GET_ATTRIBUTES(cx, obj2, id, prop, &oldAttrs);
    OBJ_DROP_PROPERTY(cx, obj2, prop);
    if (!ok)
        return JS_FALSE;

    /* If either property is readonly, we have an error. */
    report = ((oldAttrs | attrs) & JSPROP_READONLY)
             ? JSREPORT_ERROR
             : JSREPORT_WARNING | JSREPORT_STRICT;

    if (report != JSREPORT_ERROR) {
        /*
         * Allow redeclaration of variables and functions, but insist that the
         * new value is not a getter or setter -- or if it is, insist that the
         * property being replaced is not permanent.
         */
        if (!(attrs & (JSPROP_GETTER | JSPROP_SETTER)))
            return JS_TRUE;
        if (!(oldAttrs & JSPROP_PERMANENT))
            return JS_TRUE;
        report = JSREPORT_ERROR;
    }

    isFunction = (oldAttrs & (JSPROP_GETTER | JSPROP_SETTER)) != 0;
    if (!isFunction) {
        if (!OBJ_GET_PROPERTY(cx, obj, id, &value))
            return JS_FALSE;
        isFunction = JSVAL_IS_FUNCTION(cx, value);
    }
    return JS_ReportErrorFlagsAndNumber(cx, report,
                                        js_GetErrorMessage, NULL,
                                        JSMSG_REDECLARED_VAR,
                                        isFunction
                                        ? js_function_str
                                        : (oldAttrs & JSPROP_READONLY)
                                        ? js_const_str
                                        : js_var_str,
                                        ATOM_BYTES((JSAtom *)id));
}

#if !defined XP_PC || !defined _MSC_VER || _MSC_VER > 800
#define MAX_INTERP_LEVEL 1000
#else
#define MAX_INTERP_LEVEL 30
#endif

#define MAX_INLINE_CALL_COUNT 1000

JSBool
js_Interpret(JSContext *cx, jsval *result)
{
    JSRuntime *rt;
    JSStackFrame *fp;
    JSScript *script;
    uintN inlineCallCount;
    JSObject *obj, *obj2, *proto, *parent;
    JSVersion currentVersion, originalVersion;
    JSBranchCallback onbranch;
    JSBool ok, cond;
    jsint depth, len;
    jsval *sp, *newsp;
    void *mark;
    jsbytecode *pc, *pc2, *endpc;
    JSOp op, op2;
    JSCodeSpec *cs;
    JSAtom *atom;
    uintN argc, slot, attrs;
    jsval *vp, lval, rval, ltmp, rtmp;
    jsid id;
    JSObject *withobj, *origobj, *propobj;
    JSIdArray *ida;
    jsval iter_state;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSString *str, *str2, *str3;
    size_t length, length2, length3;
    jschar *chars;
    jsint i, j;
    jsdouble d, d2;
    JSClass *clasp, *funclasp;
    JSFunction *fun;
    JSType type;
#ifdef DEBUG
    FILE *tracefp;
#endif
#if JS_HAS_SWITCH_STATEMENT
    jsint low, high, off, npairs;
    JSBool match;
#endif
#if JS_HAS_GETTER_SETTER
    JSPropertyOp getter, setter;
#endif
#if JS_HAS_EXCEPTIONS
    JSTryNote *tn;
    ptrdiff_t offset;
#endif

    if (cx->interpLevel == MAX_INTERP_LEVEL) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_OVER_RECURSED);
        return JS_FALSE;
    }
    cx->interpLevel++;
    *result = JSVAL_VOID;
    rt = cx->runtime;

    /* Set registerized frame pointer and derived script pointer. */
    fp = cx->fp;
    script = fp->script;

    /* Count of JS function calls that nest in this C js_Interpret frame. */
    inlineCallCount = 0;

    /* Optimized Get and SetVersion for proper script language versioning. */
    currentVersion = script->version;
    originalVersion = cx->version;
    if (currentVersion != originalVersion)
        JS_SetVersion(cx, currentVersion);

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
    onbranch = cx->branchCallback;
    ok = JS_TRUE;
#define CHECK_BRANCH(len) {                                                   \
    if (len <= 0 && onbranch) {                                               \
        SAVE_SP(fp);                                                          \
        if (!(ok = (*onbranch)(cx, script)))                                  \
            goto out;                                                         \
    }                                                                         \
}

    pc = script->code;
    endpc = pc + script->length;
    len = -1;

    /*
     * Allocate operand and pc stack slots for the script's worst-case depth.
     */
    depth = (jsint) script->depth;
    newsp = js_AllocStack(cx, (uintN)(2 * depth), &mark);
    if (!newsp) {
        ok = JS_FALSE;
        goto out;
    }
    newsp += depth;
    sp = newsp;
    SAVE_SP(fp);

    while (pc < endpc) {
        fp->pc = pc;
        op = (JSOp) *pc;
      do_op:
        cs = &js_CodeSpec[op];
        len = cs->length;

#ifdef DEBUG
        tracefp = (FILE *) cx->tracefp;
        if (tracefp) {
            intN nuses, n;

            fprintf(tracefp, "%4u: ", js_PCToLineNumber(script, pc));
            js_Disassemble1(cx, script, pc,
                            PTRDIFF(pc, script->code, jsbytecode), JS_FALSE,
                            tracefp);
            nuses = cs->nuses;
            if (nuses) {
                fp->sp = sp - nuses;
                for (n = 0; n < nuses; n++) {
                    str = js_DecompileValueGenerator(cx, JS_TRUE, *fp->sp,
                                                     NULL);
                    if (str != NULL) {
                        fprintf(tracefp, "%s %s",
                                (n == 0) ? "  inputs:" : ",",
                                JS_GetStringBytes(str));
                    }
                    fp->sp++;
                }
                putc('\n', tracefp);
            }
        }
#endif

        {
            JSTrapHandler handler = rt->interruptHandler;
            if (handler) {
                switch (handler(cx, script, pc, &rval,
                                rt->interruptHandlerData)) {
                  case JSTRAP_ERROR:
                    ok = JS_FALSE;
                    goto out;
                  case JSTRAP_CONTINUE:
                    break;
                  case JSTRAP_RETURN:
                    fp->rval = rval;
                    goto out;
#if JS_HAS_EXCEPTIONS
                  case JSTRAP_THROW:
                    cx->throwing = JS_TRUE;
                    cx->exception = rval;
                    ok = JS_FALSE;
                    goto out;
#endif /* JS_HAS_EXCEPTIONS */
                  default:;
                }
            }
        }

        switch (op) {
          case JSOP_NOP:
            break;

          case JSOP_GROUP:
            obj = NULL;
            break;

          case JSOP_PUSH:
            PUSH_OPND(JSVAL_VOID);
            break;

          case JSOP_POP:
            sp--;
            break;

          case JSOP_POP2:
            sp -= 2;
            break;

          case JSOP_POPV:
            *result = POP();
            break;

          case JSOP_ENTERWITH:
            rval = POP();
            VALUE_TO_OBJECT(cx, rval, obj);
            withobj = js_NewObject(cx, &js_WithClass, obj, fp->scopeChain);
            if (!withobj)
                goto out;
            fp->scopeChain = withobj;
            PUSH_OPND(OBJECT_TO_JSVAL(withobj));
            break;

          case JSOP_LEAVEWITH:
            rval = POP();
            JS_ASSERT(JSVAL_IS_OBJECT(rval));
            withobj = JSVAL_TO_OBJECT(rval);
            JS_ASSERT(OBJ_GET_CLASS(cx, withobj) == &js_WithClass);

            rval = OBJ_GET_SLOT(cx, withobj, JSSLOT_PARENT);
            JS_ASSERT(JSVAL_IS_OBJECT(rval));
            fp->scopeChain = JSVAL_TO_OBJECT(rval);
            break;

          case JSOP_RETURN:
            CHECK_BRANCH(-1);
            fp->rval = POP();
            if (inlineCallCount)
          inline_return:
            {
                JSInlineFrame *ifp = (JSInlineFrame *) fp;
                void *hookData = ifp->hookData;

                if (hookData)
                    cx->runtime->callHook(cx, fp, JS_FALSE, &ok, hookData);
#if JS_HAS_ARGS_OBJECT
                if (fp->argsobj)
                    ok &= js_PutArgsObject(cx, fp);
#endif

                /* Store the return value in the caller's operand frame. */
                vp = fp->argv - 2;
                *vp = fp->rval;

                /* Restore newsp for sanity-checking assertions about sp. */
                newsp = ifp->oldsp;

                /* Restore cx->fp and release the inline frame's space. */
                cx->fp = fp = fp->down;
                JS_ARENA_RELEASE(&cx->stackPool, ifp->mark);

                /* Restore sp to point just above the return value. */
                fp->sp = vp + 1;
                RESTORE_SP(fp);

                /* Restore the calling script's interpreter registers. */
                script = fp->script;
                depth = (jsint) script->depth;
                pc = fp->pc;
                endpc = script->code + script->length;

                /* Store the generating pc for the return value. */
                vp[-depth] = (jsval)pc;

                /* Restore version and version control variable. */
                if (script->version != currentVersion) {
                    currentVersion = script->version;
                    JS_SetVersion(cx, currentVersion);
                }

                /* Set remaining variables for 'goto advance_pc'. */
                op = (JSOp) *pc;
                cs = &js_CodeSpec[op];
                len = cs->length;

                /* Resume execution in the calling frame. */
                inlineCallCount--;
                if (ok)
                    goto advance_pc;
            }
            goto out;

#if JS_HAS_SWITCH_STATEMENT
          case JSOP_DEFAULT:
            (void) POP();
            /* FALL THROUGH */
#endif
          case JSOP_GOTO:
            len = GET_JUMP_OFFSET(pc);
            CHECK_BRANCH(len);
            break;

          case JSOP_IFEQ:
            POP_BOOLEAN(cx, rval, cond);
            if (cond == JS_FALSE) {
                len = GET_JUMP_OFFSET(pc);
                CHECK_BRANCH(len);
            }
            break;

          case JSOP_IFNE:
            POP_BOOLEAN(cx, rval, cond);
            if (cond != JS_FALSE) {
                len = GET_JUMP_OFFSET(pc);
                CHECK_BRANCH(len);
            }
            break;

#if !JS_BUG_SHORT_CIRCUIT
          case JSOP_OR:
            POP_BOOLEAN(cx, rval, cond);
            if (cond == JS_TRUE) {
                len = GET_JUMP_OFFSET(pc);
                PUSH_OPND(rval);
            }
            break;

          case JSOP_AND:
            POP_BOOLEAN(cx, rval, cond);
            if (cond == JS_FALSE) {
                len = GET_JUMP_OFFSET(pc);
                PUSH_OPND(rval);
            }
            break;
#endif

          case JSOP_TOOBJECT:
            SAVE_SP(fp);
            ok = js_ValueToObject(cx, sp[-1], &obj);
            if (!ok)
                goto out;
            sp[-1] = OBJECT_TO_JSVAL(obj);
            break;

#define POP_ELEMENT_ID(id) {                                                  \
    /* If the index is not a jsint, atomize it. */                            \
    id = (jsid) POP();                                                        \
    if (JSVAL_IS_INT(id)) {                                                   \
        atom = NULL;                                                          \
    } else {                                                                  \
        atom = js_ValueToStringAtom(cx, (jsval)id);                           \
        if (!atom) {                                                          \
            ok = JS_FALSE;                                                    \
            goto out;                                                         \
        }                                                                     \
        id = (jsid)atom;                                                      \
    }                                                                         \
}

#if JS_HAS_IN_OPERATOR
          case JSOP_IN:
            rval = POP();
            if (JSVAL_IS_PRIMITIVE(rval)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_IN_NOT_OBJECT);
                ok = JS_FALSE;
                goto out;
            }
            obj = JSVAL_TO_OBJECT(rval);
            POP_ELEMENT_ID(id);
            ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
            if (!ok)
                goto out;
            PUSH_OPND(BOOLEAN_TO_JSVAL(prop != NULL));
            if (prop)
                OBJ_DROP_PROPERTY(cx, obj2, prop);
            break;
#endif /* JS_HAS_IN_OPERATOR */

          case JSOP_FORPROP:
            /*
             * Handle JSOP_FORPROP first, so the cost of the goto do_forinloop
             * is not paid for the more common cases.
             */
            lval = POP();
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            goto do_forinloop;

          case JSOP_FORNAME:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;

            /*
             * ECMA 12.6.3 says to eval the LHS after looking for properties
             * to enumerate, and bail without LHS eval if there are no props.
             * We do Find here to share the most code at label do_forinloop.
             * If looking for enumerable properties could have side effects,
             * then we'd have to move this into the common code and condition
             * it on op == JSOP_FORNAME.
             */
            SAVE_SP(fp);
            ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
            if (!ok)
                goto out;
            if (prop)
                OBJ_DROP_PROPERTY(cx, obj2, prop);
            lval = OBJECT_TO_JSVAL(obj);
            /* FALL THROUGH */

          case JSOP_FORARG:
          case JSOP_FORVAR:
            /*
             * JSOP_FORARG and JSOP_FORVAR don't require any lval computation
             * here, because they address slots on the stack (in fp->args and
             * fp->vars, respectively).
             */
            /* FALL THROUGH */

          case JSOP_FORELEM:
            /*
             * JSOP_FORELEM simply initializes or updates the iteration state
             * and leaves the index expression evaluation and assignment to the
             * enumerator until after the next property has been acquired, via
             * a JSOP_ENUMELEM bytecode.
             */
            /* FALL THROUGH */

          do_forinloop:
            /*
             * ECMA-compatible for/in evals the object just once, before loop.
             * Bad old bytecodes (since removed) did it on every iteration.
             */
            obj = JSVAL_TO_OBJECT(sp[-1]);

            /* If the thing to the right of 'in' has no properties, break. */
            if (!obj) {
                rval = JSVAL_FALSE;
                goto end_forinloop;
            }

            /*
             * Save the thing to the right of 'in' as origobj.  Later on, we
             * use this variable to suppress enumeration of shadowed prototype
             * properties.
             */
            origobj = obj;

            /*
             * Reach under the top of stack to find our property iterator, a
             * JSObject that contains the iteration state.  (An object is used
             * rather than a native struct so that the iteration state is
             * cleaned up via GC if the for-in loop terminates abruptly.)
             */
            vp = sp - 2;
            rval = *vp;

            /* Is this the first iteration ? */
            if (JSVAL_IS_VOID(rval)) {
                /* Yes, create a new JSObject to hold the iterator state */
                propobj = js_NewObject(cx, &prop_iterator_class, NULL, NULL);
                if (!propobj) {
                    ok = JS_FALSE;
                    goto out;
                }

                /*
                 * Root the parent slot so we can get it even in our finalizer
                 * (otherwise, it would live as long as we do, but it might be
                 * finalized first).
                 */
                ok = js_AddRoot(cx, &propobj->slots[JSSLOT_PARENT], NULL);
                if (!ok)
                    goto out;

                /*
                 * Rewrite the iterator so we know to do the next case.
                 * Do this before calling the enumerator, which could
                 * displace cx->newborn and cause GC.
                 */
                *vp = OBJECT_TO_JSVAL(propobj);

                ok = OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, 0);
                propobj->slots[JSSLOT_ITER_STATE] = iter_state;
                if (!ok)
                    goto out;

                /*
                 * Stash private iteration state into property iterator object.
                 * NB: This code knows that the first slots are pre-allocated.
                 */
#if JS_INITIAL_NSLOTS < 5
#error JS_INITIAL_NSLOTS must be greater than or equal to 5.
#endif
                propobj->slots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(obj);
            } else {
                /* This is not the first iteration. Recover iterator state. */
                propobj = JSVAL_TO_OBJECT(rval);
                obj = JSVAL_TO_OBJECT(propobj->slots[JSSLOT_PARENT]);
                iter_state = propobj->slots[JSSLOT_ITER_STATE];
            }

          enum_next_property:
            /* Get the next jsid to be enumerated and store it in rval. */
            OBJ_ENUMERATE(cx, obj, JSENUMERATE_NEXT, &iter_state, &rval);
            propobj->slots[JSSLOT_ITER_STATE] = iter_state;

            /* No more jsids to iterate in obj? */
            if (iter_state == JSVAL_NULL) {
                /* Enumerate the properties on obj's prototype chain. */
                obj = OBJ_GET_PROTO(cx, obj);
                if (!obj) {
                    /* End of property list -- terminate loop. */
                    rval = JSVAL_FALSE;
                    goto end_forinloop;
                }

                ok = OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, 0);
                propobj->slots[JSSLOT_ITER_STATE] = iter_state;
                if (!ok)
                    goto out;

                /* Stash private iteration state into iterator JSObject. */
                propobj->slots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(obj);
                goto enum_next_property;
            }

            /* Skip properties not owned by obj, and leave next id in rval. */
            ok = OBJ_LOOKUP_PROPERTY(cx, origobj, rval, &obj2, &prop);
            if (!ok)
                goto out;
            if (prop) {
                OBJ_DROP_PROPERTY(cx, obj2, prop);

                /* Yes, don't enumerate again.  Go to the next property. */
                if (obj2 != obj)
                    goto enum_next_property;
            }

            /* Make sure rval is a string for uniformity and compatibility. */
            if (!JSVAL_IS_INT(rval)) {
                rval = ATOM_KEY((JSAtom *)rval);
            } else if (cx->version != JSVERSION_1_2) {
                str = js_NumberToString(cx, (jsdouble) JSVAL_TO_INT(rval));
                if (!str) {
                    ok = JS_FALSE;
                    goto out;
                }

                /* Hold via sp[0] in case the GC runs under OBJ_SET_PROPERTY. */
                rval = sp[0] = STRING_TO_JSVAL(str);
            }

            switch (op) {
              case JSOP_FORARG:
                slot = (uintN)GET_ARGNO(pc);
                JS_ASSERT(slot < fp->fun->nargs);
                fp->argv[slot] = rval;
                break;

              case JSOP_FORVAR:
                slot = (uintN)GET_VARNO(pc);
                JS_ASSERT(slot < fp->fun->nvars);
                fp->vars[slot] = rval;
                break;

              case JSOP_FORELEM:
                /* FORELEM is not a SET operation, it's more like BINDNAME. */
                PUSH_OPND(rval);
                break;

              default:
                /* Convert lval to a non-null object containing id. */
                VALUE_TO_OBJECT(cx, lval, obj);

                /* Set the variable obj[id] to refer to rval. */
                ok = OBJ_SET_PROPERTY(cx, obj, id, &rval);
                if (!ok)
                    goto out;
                break;
            }

            /* Push true to keep looping through properties. */
            rval = JSVAL_TRUE;

          end_forinloop:
            PUSH_OPND(rval);
            break;

          case JSOP_DUP:
            JS_ASSERT(sp > newsp);
            rval = sp[-1];
            PUSH_OPND(rval);
            break;

          case JSOP_DUP2:
            JS_ASSERT(sp - 1 > newsp);
            lval = sp[-2];
            rval = sp[-1];
            PUSH_OPND(lval);
            PUSH_OPND(rval);
            break;

#define PROPERTY_OP(call) {                                                   \
    /* Pop the left part and resolve it to a non-null object. */              \
    lval = POP();                                                             \
    VALUE_TO_OBJECT(cx, lval, obj);                                           \
                                                                              \
    /* Get or set the property, set ok false if error, true if success. */    \
    call;                                                                     \
    if (!ok)                                                                  \
        goto out;                                                             \
}

#define ELEMENT_OP(call) {                                                    \
    POP_ELEMENT_ID(id);                                                       \
    PROPERTY_OP(call);                                                        \
}

#define CACHED_GET(call) {                                                    \
    JS_LOCK_OBJ(cx, obj);                                                     \
    PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);                   \
    if (PROP_FOUND(prop)) {                                                   \
        JSScope *_scope = OBJ_SCOPE(obj);                                     \
        sprop = (JSScopeProperty *)prop;                                      \
        JS_ATOMIC_ADDREF(&sprop->nrefs, 1);                                   \
        slot = (uintN)sprop->slot;                                            \
        rval = LOCKED_OBJ_GET_SLOT(obj, slot);                                \
        JS_UNLOCK_SCOPE(cx, _scope);                                          \
        ok = SPROP_GET(cx, sprop, obj, obj, &rval);                           \
        if (ok) {                                                             \
            JS_LOCK_SCOPE(cx, _scope);                                        \
            sprop = js_DropScopeProperty(cx, _scope, sprop);                  \
            if (sprop)                                                        \
                LOCKED_OBJ_SET_SLOT(obj, slot, rval);                         \
            JS_UNLOCK_SCOPE(cx, _scope);                                      \
        }                                                                     \
    } else {                                                                  \
        JS_UNLOCK_OBJ(cx, obj);                                               \
        SAVE_SP(fp);                                                          \
        ok = call;                                                            \
        /* No fill here: js_GetProperty fills the cache. */                   \
    }                                                                         \
}

#if JS_BUG_SET_ENUMERATE
#define SET_ENUMERATE_ATTR(sprop) ((sprop)->attrs |= JSPROP_ENUMERATE)
#else
#define SET_ENUMERATE_ATTR(sprop) ((void)0)
#endif

#define CACHED_SET(call) {                                                    \
    JS_LOCK_OBJ(cx, obj);                                                     \
    PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);                   \
    if (PROP_FOUND(prop) &&                                                   \
        !(sprop = (JSScopeProperty *)prop,                                    \
          sprop->attrs & JSPROP_READONLY)) {                                  \
        JSScope *_scope = OBJ_SCOPE(obj);                                     \
        JS_ATOMIC_ADDREF(&sprop->nrefs, 1);                                   \
        JS_UNLOCK_SCOPE(cx, _scope);                                          \
        ok = SPROP_SET(cx, sprop, obj, obj, &rval);                           \
        if (ok) {                                                             \
            JS_LOCK_SCOPE(cx, _scope);                                        \
            sprop = js_DropScopeProperty(cx, _scope, sprop);                  \
            if (sprop) {                                                      \
                LOCKED_OBJ_SET_SLOT(obj, sprop->slot, rval);                  \
                SET_ENUMERATE_ATTR(sprop);                                    \
                GC_POKE(cx, JSVAL_NULL);  /* second arg ignored! */           \
            }                                                                 \
            JS_UNLOCK_SCOPE(cx, _scope);                                      \
        }                                                                     \
    } else {                                                                  \
        JS_UNLOCK_OBJ(cx, obj);                                               \
        SAVE_SP(fp);                                                          \
        ok = call;                                                            \
        /* No fill here: js_SetProperty writes through the cache. */          \
    }                                                                         \
}

          case JSOP_SETCONST:
            obj = fp->varobj;
            atom = GET_ATOM(cx, script, pc);
            rval = POP();
            ok = OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, rval, NULL, NULL,
                                     JSPROP_ENUMERATE | JSPROP_PERMANENT |
                                     JSPROP_READONLY,
                                     NULL);
            if (!ok)
                goto out;
            PUSH_OPND(rval);
            break;

          case JSOP_BINDNAME:
            atom = GET_ATOM(cx, script, pc);
            SAVE_SP(fp);
            ok = js_FindVariable(cx, (jsid)atom, &obj, &obj2, &prop);
            if (!ok)
                goto out;
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            PUSH_OPND(OBJECT_TO_JSVAL(obj));
            break;

          case JSOP_SETNAME:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            rval = POP();
            lval = POP();
            JS_ASSERT(!JSVAL_IS_PRIMITIVE(lval));
            obj  = JSVAL_TO_OBJECT(lval);
            CACHED_SET(OBJ_SET_PROPERTY(cx, obj, id, &rval));
            if (!ok)
                goto out;
            PUSH_OPND(rval);
            break;

#define INTEGER_OP(OP, EXTRA_CODE) {                                          \
    SAVE_SP(fp);                                                              \
    ok = PopInt(cx, &j) && PopInt(cx, &i);                                    \
    RESTORE_SP(fp);                                                           \
    if (!ok)                                                                  \
        goto out;                                                             \
    EXTRA_CODE                                                                \
    i = i OP j;                                                               \
    PUSH_NUMBER(cx, i);                                                       \
}

#define BITWISE_OP(OP)          INTEGER_OP(OP, (void) 0;)
#define SIGNED_SHIFT_OP(OP)     INTEGER_OP(OP, j &= 31;)

          case JSOP_BITOR:
            BITWISE_OP(|);
            break;

          case JSOP_BITXOR:
            BITWISE_OP(^);
            break;

          case JSOP_BITAND:
            BITWISE_OP(&);
            break;

#ifdef XP_PC
#define COMPARE_DOUBLES(LVAL, OP, RVAL, IFNAN)                                \
    ((JSDOUBLE_IS_NaN(LVAL) || JSDOUBLE_IS_NaN(RVAL))                         \
     ? (IFNAN)                                                                \
     : (LVAL) OP (RVAL))
#else
#define COMPARE_DOUBLES(LVAL, OP, RVAL, IFNAN) ((LVAL) OP (RVAL))
#endif

#define RELATIONAL_OP(OP) {                                                   \
    rval = POP();                                                             \
    lval = POP();                                                             \
    /* Optimize for two int-tagged operands (typical loop control). */        \
    if ((lval & rval) & JSVAL_INT) {                                          \
        ltmp = lval ^ JSVAL_VOID;                                             \
        rtmp = rval ^ JSVAL_VOID;                                             \
        if (ltmp && rtmp) {                                                   \
            cond = JSVAL_TO_INT(lval) OP JSVAL_TO_INT(rval);                  \
        } else {                                                              \
            d  = ltmp ? JSVAL_TO_INT(lval) : *rt->jsNaN;                      \
            d2 = rtmp ? JSVAL_TO_INT(rval) : *rt->jsNaN;                      \
            cond = COMPARE_DOUBLES(d, OP, d2, JS_FALSE);                      \
        }                                                                     \
    } else {                                                                  \
        VALUE_TO_PRIMITIVE(cx, lval, JSTYPE_NUMBER, &lval);                   \
        /* Need lval for strcmp or ToNumber later, so root it via sp[0]. */   \
        sp[0] = lval;                                                         \
        VALUE_TO_PRIMITIVE(cx, rval, JSTYPE_NUMBER, &rval);                   \
        if (JSVAL_IS_STRING(lval) && JSVAL_IS_STRING(rval)) {                 \
            str  = JSVAL_TO_STRING(lval);                                     \
            str2 = JSVAL_TO_STRING(rval);                                     \
            cond = js_CompareStrings(str, str2) OP 0;                         \
        } else {                                                              \
            /* Need rval after ToNumber(lval), so root it via sp[1]. */       \
            sp[1] = rval;                                                     \
            VALUE_TO_NUMBER(cx, lval, d);                                     \
            VALUE_TO_NUMBER(cx, rval, d2);                                    \
            cond = COMPARE_DOUBLES(d, OP, d2, JS_FALSE);                      \
        }                                                                     \
    }                                                                         \
    PUSH_OPND(BOOLEAN_TO_JSVAL(cond));                                        \
}

#define EQUALITY_OP(OP, IFNAN) {                                              \
    rval = POP();                                                             \
    lval = POP();                                                             \
    ltmp = JSVAL_TAG(lval);                                                   \
    rtmp = JSVAL_TAG(rval);                                                   \
    if (ltmp == rtmp) {                                                       \
        if (ltmp == JSVAL_STRING) {                                           \
            str  = JSVAL_TO_STRING(lval);                                     \
            str2 = JSVAL_TO_STRING(rval);                                     \
            cond = js_CompareStrings(str, str2) OP 0;                         \
        } else if (ltmp == JSVAL_DOUBLE) {                                    \
            d  = *JSVAL_TO_DOUBLE(lval);                                      \
            d2 = *JSVAL_TO_DOUBLE(rval);                                      \
            cond = COMPARE_DOUBLES(d, OP, d2, IFNAN);                         \
        } else {                                                              \
            /* Handle all undefined (=>NaN) and int combinations. */          \
            cond = lval OP rval;                                              \
        }                                                                     \
    } else {                                                                  \
        if (JSVAL_IS_NULL(lval) || JSVAL_IS_VOID(lval)) {                     \
            cond = (JSVAL_IS_NULL(rval) || JSVAL_IS_VOID(rval)) OP 1;         \
        } else if (JSVAL_IS_NULL(rval) || JSVAL_IS_VOID(rval)) {              \
            cond = 1 OP 0;                                                    \
        } else {                                                              \
            if (ltmp == JSVAL_OBJECT) {                                       \
                VALUE_TO_PRIMITIVE(cx, lval, JSTYPE_VOID, &lval);             \
                ltmp = JSVAL_TAG(lval);                                       \
            } else if (rtmp == JSVAL_OBJECT) {                                \
                VALUE_TO_PRIMITIVE(cx, rval, JSTYPE_VOID, &rval);             \
                rtmp = JSVAL_TAG(rval);                                       \
            }                                                                 \
            if (ltmp == JSVAL_STRING && rtmp == JSVAL_STRING) {               \
                str  = JSVAL_TO_STRING(lval);                                 \
                str2 = JSVAL_TO_STRING(rval);                                 \
                cond = js_CompareStrings(str, str2) OP 0;                     \
            } else {                                                          \
                /* Need rval after ToNumber(lval), so root it via sp[1]. */   \
                sp[1] = rval;                                                 \
                VALUE_TO_NUMBER(cx, lval, d);                                 \
                VALUE_TO_NUMBER(cx, rval, d2);                                \
                cond = COMPARE_DOUBLES(d, OP, d2, IFNAN);                     \
            }                                                                 \
        }                                                                     \
    }                                                                         \
    PUSH_OPND(BOOLEAN_TO_JSVAL(cond));                                        \
}

          case JSOP_EQ:
            EQUALITY_OP(==, JS_FALSE);
            break;

          case JSOP_NE:
            EQUALITY_OP(!=, JS_TRUE);
            break;

#if !JS_BUG_FALLIBLE_EQOPS
#define NEW_EQUALITY_OP(OP, IFNAN) {                                          \
    rval = POP();                                                             \
    lval = POP();                                                             \
    ltmp = JSVAL_TAG(lval);                                                   \
    rtmp = JSVAL_TAG(rval);                                                   \
    if (ltmp == rtmp) {                                                       \
        if (ltmp == JSVAL_STRING) {                                           \
            str  = JSVAL_TO_STRING(lval);                                     \
            str2 = JSVAL_TO_STRING(rval);                                     \
            cond = js_CompareStrings(str, str2) OP 0;                         \
        } else if (ltmp == JSVAL_DOUBLE) {                                    \
            d  = *JSVAL_TO_DOUBLE(lval);                                      \
            d2 = *JSVAL_TO_DOUBLE(rval);                                      \
            cond = COMPARE_DOUBLES(d, OP, d2, IFNAN);                         \
        } else {                                                              \
            cond = lval OP rval;                                              \
        }                                                                     \
    } else {                                                                  \
        if (ltmp == JSVAL_DOUBLE && JSVAL_IS_INT(rval)) {                     \
            d  = *JSVAL_TO_DOUBLE(lval);                                      \
            d2 = JSVAL_TO_INT(rval);                                          \
            cond = COMPARE_DOUBLES(d, OP, d2, IFNAN);                         \
        } else if (JSVAL_IS_INT(lval) && rtmp == JSVAL_DOUBLE) {              \
            d  = JSVAL_TO_INT(lval);                                          \
            d2 = *JSVAL_TO_DOUBLE(rval);                                      \
            cond = COMPARE_DOUBLES(d, OP, d2, IFNAN);                         \
        } else {                                                              \
            cond = lval OP rval;                                              \
        }                                                                     \
    }                                                                         \
    PUSH_OPND(BOOLEAN_TO_JSVAL(cond));                                        \
}

          case JSOP_NEW_EQ:
            NEW_EQUALITY_OP(==, JS_FALSE);
            break;

          case JSOP_NEW_NE:
            NEW_EQUALITY_OP(!=, JS_TRUE);
            break;

#if JS_HAS_SWITCH_STATEMENT
          case JSOP_CASE:
            NEW_EQUALITY_OP(==, JS_FALSE);
            (void) POP();
            if (cond) {
                len = GET_JUMP_OFFSET(pc);
                CHECK_BRANCH(len);
            } else {
                PUSH(lval);
            }
            break;
#endif

#endif /* !JS_BUG_FALLIBLE_EQOPS */

          case JSOP_LT:
            RELATIONAL_OP(<);
            break;

          case JSOP_LE:
            RELATIONAL_OP(<=);
            break;

          case JSOP_GT:
            RELATIONAL_OP(>);
            break;

          case JSOP_GE:
            RELATIONAL_OP(>=);
            break;

#undef EQUALITY_OP
#undef RELATIONAL_OP

          case JSOP_LSH:
            SIGNED_SHIFT_OP(<<);
            break;

          case JSOP_RSH:
            SIGNED_SHIFT_OP(>>);
            break;

          case JSOP_URSH:
          {
            uint32 u;

            SAVE_SP(fp);
            ok = PopInt(cx, &j) && PopUint(cx, &u);
            RESTORE_SP(fp);
            if (!ok)
                goto out;
            j &= 31;
            d = u >> j;
            PUSH_NUMBER(cx, d);
            break;
          }

#undef INTEGER_OP
#undef BITWISE_OP
#undef SIGNED_SHIFT_OP

          case JSOP_ADD:
            rval = rtmp = POP();
            lval = ltmp = POP();
            VALUE_TO_PRIMITIVE(cx, lval, JSTYPE_VOID, &lval);
            if ((cond = JSVAL_IS_STRING(lval)) != 0) {
                /*
                 * Keep lval on the stack so it isn't GC'd during either the
                 * next VALUE_TO_PRIMITIVE or the js_ValueToString(cx, rval).
                 */
                sp[0] = lval;
            }
            VALUE_TO_PRIMITIVE(cx, rval, JSTYPE_VOID, &rval);
            if (cond || JSVAL_IS_STRING(rval)) {
                if (cond) {
                    str = JSVAL_TO_STRING(lval);
                    SAVE_SP(fp);
                    ok = (str2 = js_ValueToString(cx, rval)) != NULL;
                } else {
                    /*
                     * Keep rval on the stack so it isn't GC'd during the next
                     * js_ValueToString.
                     */
                    sp[1] = rval;
                    str2 = JSVAL_TO_STRING(rval);
                    SAVE_SP(fp);
                    ok = (str = js_ValueToString(cx, lval)) != NULL;
                }
                if (!ok)
                    goto out;
                if ((length = str->length) == 0) {
                    str3 = str2;
                } else if ((length2 = str2->length) == 0) {
                    str3 = str;
                } else {
                    length3 = length + length2;
                    chars = JS_malloc(cx, (length3 + 1) * sizeof(jschar));
                    if (!chars) {
                        ok = JS_FALSE;
                        goto out;
                    }
                    js_strncpy(chars, str->chars, length);
                    js_strncpy(chars + length, str2->chars, length2);
                    chars[length3] = 0;
                    str3 = js_NewString(cx, chars, length3, 0);
                    if (!str3) {
                        JS_free(cx, chars);
                        ok = JS_FALSE;
                        goto out;
                    }
                }
                PUSH_OPND(STRING_TO_JSVAL(str3));
            } else {
                VALUE_TO_NUMBER(cx, ltmp, d);
                VALUE_TO_NUMBER(cx, rtmp, d2);
                d += d2;
                PUSH_NUMBER(cx, d);
            }
            break;

#define BINARY_OP(OP) {                                                       \
    POP_NUMBER(cx, d2);                                                       \
    POP_NUMBER(cx, d);                                                        \
    d = d OP d2;                                                              \
    PUSH_NUMBER(cx, d);                                                       \
}

          case JSOP_SUB:
            BINARY_OP(-);
            break;

          case JSOP_MUL:
            BINARY_OP(*);
            break;

          case JSOP_DIV:
            POP_NUMBER(cx, d2);
            POP_NUMBER(cx, d);
            if (d2 == 0) {
#ifdef XP_PC
                /* XXX MSVC miscompiles such that (NaN == 0) */
                if (JSDOUBLE_IS_NaN(d2))
                    rval = DOUBLE_TO_JSVAL(rt->jsNaN);
                else
#endif
                if (d == 0 || JSDOUBLE_IS_NaN(d))
                    rval = DOUBLE_TO_JSVAL(rt->jsNaN);
                else if ((JSDOUBLE_HI32(d) ^ JSDOUBLE_HI32(d2)) >> 31)
                    rval = DOUBLE_TO_JSVAL(rt->jsNegativeInfinity);
                else
                    rval = DOUBLE_TO_JSVAL(rt->jsPositiveInfinity);
                PUSH_OPND(rval);
            } else {
                d /= d2;
                PUSH_NUMBER(cx, d);
            }
            break;

          case JSOP_MOD:
            POP_NUMBER(cx, d2);
            POP_NUMBER(cx, d);
            if (d2 == 0) {
                PUSH_OPND(DOUBLE_TO_JSVAL(rt->jsNaN));
            } else {
#ifdef XP_PC
              /* Workaround MS fmod bug where 42 % (1/0) => NaN, not 42. */
              if (!(JSDOUBLE_IS_FINITE(d) && JSDOUBLE_IS_INFINITE(d2)))
#endif
                d = fmod(d, d2);
                PUSH_NUMBER(cx, d);
            }
            break;

          case JSOP_NOT:
            POP_BOOLEAN(cx, rval, cond);
            PUSH_OPND(BOOLEAN_TO_JSVAL(!cond));
            break;

          case JSOP_BITNOT:
            SAVE_SP(fp);
            ok = PopInt(cx, &i);
            RESTORE_SP(fp);
            if (!ok)
                goto out;
            d = (jsdouble) ~i;
            PUSH_NUMBER(cx, d);
            break;

          case JSOP_NEG:
            POP_NUMBER(cx, d);
#ifdef HPUX
            /*
             * Negation of a zero doesn't produce a negative
             * zero on HPUX. Perform the operation by bit
             * twiddling.
             */
            JSDOUBLE_HI32(d) ^= JSDOUBLE_HI32_SIGNBIT;
#else
            d = -d;
#endif
            PUSH_NUMBER(cx, d);
            break;

          case JSOP_POS:
            POP_NUMBER(cx, d);
            PUSH_NUMBER(cx, d);
            break;

          case JSOP_NEW:
            /* Get immediate argc and find the constructor function. */
            argc = GET_ARGC(pc);

#if JS_HAS_INITIALIZERS
          do_new:
#endif
            vp = sp - (2 + argc);
            JS_ASSERT(vp >= newsp);

            fun = NULL;
            obj2 = NULL;
            lval = *vp;
            if (!JSVAL_IS_OBJECT(lval) ||
                (obj2 = JSVAL_TO_OBJECT(lval)) == NULL ||
                /* XXX clean up to avoid special cases above ObjectOps layer */
                OBJ_GET_CLASS(cx, obj2) == &js_FunctionClass ||
                !obj2->map->ops->construct)
            {
                fun = js_ValueToFunction(cx, vp, JS_TRUE);
                if (!fun) {
                    ok = JS_FALSE;
                    goto out;
                }
            }

            clasp = &js_ObjectClass;
            if (!obj2) {
                proto = parent = NULL;
                fun = NULL;
            } else {
                /* Get the constructor prototype object for this function. */
                ok = OBJ_GET_PROPERTY(cx, obj2,
                                      (jsid)rt->atomState.classPrototypeAtom,
                                      &rval);
                if (!ok)
                    goto out;
                proto = JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : NULL;
                parent = OBJ_GET_PARENT(cx, obj2);

                if (OBJ_GET_CLASS(cx, obj2) == &js_FunctionClass) {
                    funclasp = ((JSFunction *)JS_GetPrivate(cx, obj2))->clasp;
                    if (funclasp)
                        clasp = funclasp;
                }
            }
            obj = js_NewObject(cx, clasp, proto, parent);
            if (!obj) {
                ok = JS_FALSE;
                goto out;
            }

            /* Now we have an object with a constructor method; call it. */
            vp[1] = OBJECT_TO_JSVAL(obj);
            SAVE_SP(fp);
            ok = js_Invoke(cx, argc, JSINVOKE_CONSTRUCT);
            RESTORE_SP(fp);
            if (!ok) {
                cx->newborn[GCX_OBJECT] = NULL;
                goto out;
            }

            /* Check the return value and update obj from it. */
            rval = *vp;
            if (JSVAL_IS_PRIMITIVE(rval)) {
                if (fun || !JSVERSION_IS_ECMA(cx->version)) {
                    *vp = OBJECT_TO_JSVAL(obj);
                    break;
                }
                /* native [[Construct]] returning primitive is error */
                str = js_ValueToString(cx, rval);
                if (str) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_BAD_NEW_RESULT,
                                         JS_GetStringBytes(str));
                }
                ok = JS_FALSE;
                goto out;
            }
            obj = JSVAL_TO_OBJECT(rval);
            break;

          case JSOP_DELNAME:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;

            SAVE_SP(fp);
            ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
            if (!ok)
                goto out;

            /* ECMA says to return true if name is undefined or inherited. */
            rval = JSVAL_TRUE;
            if (prop) {
                OBJ_DROP_PROPERTY(cx, obj2, prop);
                ok = OBJ_DELETE_PROPERTY(cx, obj, id, &rval);
                if (!ok)
                    goto out;
            }
            PUSH_OPND(rval);
            break;

          case JSOP_DELPROP:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            SAVE_SP(fp);
            PROPERTY_OP(ok = OBJ_DELETE_PROPERTY(cx, obj, id, &rval));
            PUSH_OPND(rval);
            break;

          case JSOP_DELELEM:
            SAVE_SP(fp);
            ELEMENT_OP(ok = OBJ_DELETE_PROPERTY(cx, obj, id, &rval));
            PUSH_OPND(rval);
            break;

          case JSOP_TYPEOF:
            rval = POP();
            type = JS_TypeOfValue(cx, rval);
            atom = rt->atomState.typeAtoms[type];
            str  = ATOM_TO_STRING(atom);
            PUSH_OPND(STRING_TO_JSVAL(str));
            break;

          case JSOP_VOID:
            (void) POP();
            PUSH_OPND(JSVAL_VOID);
            break;

          case JSOP_INCNAME:
          case JSOP_DECNAME:
          case JSOP_NAMEINC:
          case JSOP_NAMEDEC:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;

            SAVE_SP(fp);
            ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
            if (!ok)
                goto out;
            if (!prop) {
                js_ReportIsNotDefined(cx, ATOM_BYTES(atom));
                ok = JS_FALSE;
                goto out;
            }

            OBJ_DROP_PROPERTY(cx, obj2, prop);
            lval = OBJECT_TO_JSVAL(obj);
            goto do_incop;

          case JSOP_INCPROP:
          case JSOP_DECPROP:
          case JSOP_PROPINC:
          case JSOP_PROPDEC:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            lval = POP();
            goto do_incop;

          case JSOP_INCELEM:
          case JSOP_DECELEM:
          case JSOP_ELEMINC:
          case JSOP_ELEMDEC:
            POP_ELEMENT_ID(id);
            lval = POP();

          do_incop:
            VALUE_TO_OBJECT(cx, lval, obj);

            /* The operand must contain a number. */
            CACHED_GET(OBJ_GET_PROPERTY(cx, obj, id, &rval));
            if (!ok)
                goto out;
            VALUE_TO_NUMBER(cx, rval, d);

            /* Compute the post- or pre-incremented value. */
            if (cs->format & JOF_INC)
                d2 = (cs->format & JOF_POST) ? d++ : ++d;
            else
                d2 = (cs->format & JOF_POST) ? d-- : --d;

            /* Set obj[id] to the resulting number. */
            ok = js_NewNumberValue(cx, d, &rval);
            if (!ok)
                goto out;
            CACHED_SET(OBJ_SET_PROPERTY(cx, obj, id, &rval));
            if (!ok)
                goto out;
            PUSH_NUMBER(cx, d2);
            break;

          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC:
            slot = (uintN)GET_ARGNO(pc);
            JS_ASSERT(slot < fp->fun->nargs);
            rval = fp->argv[slot];
            VALUE_TO_NUMBER(cx, rval, d);

            /* Compute the post- or pre-incremented value. */
            if (cs->format & JOF_INC)
                d2 = (cs->format & JOF_POST) ? d++ : ++d;
            else
                d2 = (cs->format & JOF_POST) ? d-- : --d;

            ok = js_NewNumberValue(cx, d, &rval);
            if (!ok)
                goto out;
            fp->argv[slot] = rval;
            PUSH_NUMBER(cx, d2);
            break;

          case JSOP_INCVAR:
          case JSOP_DECVAR:
          case JSOP_VARINC:
          case JSOP_VARDEC:
            slot = (uintN)GET_VARNO(pc);
            JS_ASSERT(slot < fp->fun->nvars);
            rval = fp->vars[slot];
            VALUE_TO_NUMBER(cx, rval, d);

            /* Compute the post- or pre-incremented value. */
            if (cs->format & JOF_INC)
                d2 = (cs->format & JOF_POST) ? d++ : ++d;
            else
                d2 = (cs->format & JOF_POST) ? d-- : --d;

            ok = js_NewNumberValue(cx, d, &rval);
            if (!ok)
                goto out;
            fp->vars[slot] = rval;
            PUSH_NUMBER(cx, d2);
            break;

          case JSOP_GETPROP:
            /* Get an immediate atom naming the property. */
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            PROPERTY_OP(CACHED_GET(OBJ_GET_PROPERTY(cx, obj, id, &rval)));
            PUSH_OPND(rval);
            break;

          case JSOP_SETPROP:
            /* Pop the right-hand side into rval for OBJ_SET_PROPERTY. */
            rval = POP();

            /* Get an immediate atom naming the property. */
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            PROPERTY_OP(CACHED_SET(OBJ_SET_PROPERTY(cx, obj, id, &rval)));
            PUSH_OPND(rval);
            break;

          case JSOP_GETELEM:
            ELEMENT_OP(CACHED_GET(OBJ_GET_PROPERTY(cx, obj, id, &rval)));
            PUSH_OPND(rval);
            break;

          case JSOP_SETELEM:
            rval = POP();
            ELEMENT_OP(CACHED_SET(OBJ_SET_PROPERTY(cx, obj, id, &rval)));
            PUSH_OPND(rval);
            break;

          case JSOP_ENUMELEM:
            POP_ELEMENT_ID(id);
            lval = POP();
            VALUE_TO_OBJECT(cx, lval, obj);
            rval = POP();
            CACHED_SET(OBJ_SET_PROPERTY(cx, obj, id, &rval));
            if (!ok)
                goto out;
            break;

          case JSOP_PUSHOBJ:
            PUSH_OPND(OBJECT_TO_JSVAL(obj));
            break;

          case JSOP_CALL:
          case JSOP_EVAL:
            argc = GET_ARGC(pc);
            vp = sp - (argc + 2);
            lval = *vp;
            SAVE_SP(fp);

            if (JSVAL_IS_FUNCTION(cx, lval) &&
                (obj = JSVAL_TO_OBJECT(lval),
                 fun = (JSFunction *) JS_GetPrivate(cx, obj),
                 !fun->native &&
                 fun->flags == 0 &&
                 argc >= (uintN)(fun->nargs + fun->extra)))
          /* inline_call: */
            {
                uintN nframeslots, nvars;
                jsval *oldsp;
                void *newmark;
                JSInlineFrame *newifp;
                JSInterpreterHook hook;

                /* Restrict recursion of lightweight functions. */
                if (inlineCallCount == MAX_INLINE_CALL_COUNT) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_OVER_RECURSED);
                    return JS_FALSE;
                }

                /* Compute the number of stack slots needed for fun. */
                nframeslots = (sizeof(JSInlineFrame) + sizeof(jsval) - 1)
                              / sizeof(jsval);
                nvars = fun->nvars;
                script = fun->script;
                depth = (jsint) script->depth;

                /* Allocate the frame and space for vars and operands. */
                oldsp = newsp;
                newsp = js_AllocStack(cx, nframeslots + nvars + 2 * depth,
                                      &newmark);
                if (!newsp) {
                    ok = JS_FALSE;
                    goto bad_inline_call;
                }
                newifp = (JSInlineFrame *) newsp;
                newsp += nframeslots;

                /* Initialize the stack frame. */
                memset(newifp, 0, sizeof(JSInlineFrame));
                newifp->frame.script = script;
                newifp->frame.fun = fun;
                newifp->frame.argc = argc;
                newifp->frame.argv = vp + 2;
                newifp->frame.rval = JSVAL_VOID;
                newifp->frame.nvars = nvars;
                newifp->frame.vars = newsp;
                newifp->frame.down = fp;
                newifp->frame.scopeChain = OBJ_GET_PARENT(cx, obj);
                newifp->oldsp = oldsp;
                newifp->mark = newmark;

                /* Compute the 'this' parameter now that argv is set. */
                ok = ComputeThis(cx, JSVAL_TO_OBJECT(vp[1]), &newifp->frame);
                if (!ok) {
                    js_FreeStack(cx, newmark);
                    goto bad_inline_call;
                }

                /* Push void to initialize local variables. */
                sp = newsp;
                while (nvars--)
                    PUSH(JSVAL_VOID);
                sp += depth;
                newsp = sp;
                SAVE_SP(&newifp->frame);

                /* Call the debugger hook if present. */
                hook = cx->runtime->callHook;
                if (hook) {
                    newifp->hookData = hook(cx, &newifp->frame, JS_TRUE, 0,
                                            cx->runtime->callHookData);
                }

                /* Switch to new version if necessary. */
                if (script->version != currentVersion) {
                    currentVersion = script->version;
                    JS_SetVersion(cx, currentVersion);
                }

                /* Push the frame and set interpreter registers. */
                cx->fp = fp = &newifp->frame;
                pc = script->code;
                endpc = pc + script->length;
                inlineCallCount++;
                continue;

              bad_inline_call:
                script = fp->script;
                depth = (jsint) script->depth;
                newsp = oldsp;
                goto out;
            }

            ok = js_Invoke(cx, argc, 0);
            RESTORE_SP(fp);
            if (!ok)
                goto out;
            if (cx->rval2set) {
                /*
                 * Sneaky: use the stack depth we didn't claim in our budget,
                 * but that we know is there on account of [fun, this] already
                 * having been pushed, at a minimum (if no args).  Those two
                 * slots have been popped and [rval] has been pushed, which
                 * leaves one more slot for rval2 before we might overflow.
                 *
                 * NB: rval2 must be the property identifier, and rval the
                 * object from which to get the property.  The pair form an
                 * ECMA "reference type", which can be used on the right- or
                 * left-hand side of assignment op.  Only native methods can
                 * return reference types.  See JSOP_SETCALL just below for
                 * the left-hand-side case.
                 */
                PUSH_OPND(cx->rval2);
                cx->rval2set = JS_FALSE;
                ELEMENT_OP(CACHED_GET(OBJ_GET_PROPERTY(cx, obj, id, &rval)));
                PUSH_OPND(rval);
            }
            obj = NULL;
            break;

          case JSOP_SETCALL:
            argc = GET_ARGC(pc);
            SAVE_SP(fp);
            ok = js_Invoke(cx, argc, 0);
            RESTORE_SP(fp);
            if (!ok)
                goto out;
            if (!cx->rval2set) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_LEFTSIDE_OF_ASS);
                ok = JS_FALSE;
                goto out;
            }
            PUSH_OPND(cx->rval2);
            cx->rval2set = JS_FALSE;
            obj = NULL;
            break;

          case JSOP_NAME:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;

            SAVE_SP(fp);
            ok = js_FindProperty(cx, id, &obj, &obj2, &prop);
            if (!ok)
                goto out;
            if (!prop) {
                /* Kludge to allow (typeof foo == "undefined") tests. */
                for (pc2 = pc + len; pc2 < endpc; pc2++) {
                    op2 = (JSOp)*pc2;
                    if (op2 == JSOP_TYPEOF) {
                        PUSH_OPND(JSVAL_VOID);
                        goto advance_pc;
                    }
                    if (op2 != JSOP_GROUP)
                        break;
                }
                js_ReportIsNotDefined(cx, ATOM_BYTES(atom));
                ok = JS_FALSE;
                goto out;
            }

            /* Take the slow path if prop was not found in a native object. */
            if (!OBJ_IS_NATIVE(obj) || !OBJ_IS_NATIVE(obj2)) {
                OBJ_DROP_PROPERTY(cx, obj2, prop);
                ok = OBJ_GET_PROPERTY(cx, obj, id, &rval);
                if (!ok)
                    goto out;
                PUSH_OPND(rval);
                break;
            }

            /* Get and push the obj[id] property's value. */
            sprop = (JSScopeProperty *)prop;
            slot = (uintN)sprop->slot;
            rval = LOCKED_OBJ_GET_SLOT(obj2, slot);
            JS_UNLOCK_OBJ(cx, obj2);
            ok = SPROP_GET(cx, sprop, obj, obj2, &rval);
            JS_LOCK_OBJ(cx, obj2);
            if (!ok) {
                OBJ_DROP_PROPERTY(cx, obj2, prop);
                goto out;
            }
            LOCKED_OBJ_SET_SLOT(obj2, slot, rval);
            OBJ_DROP_PROPERTY(cx, obj2, prop);
            PUSH_OPND(rval);
            break;

          case JSOP_UINT16:
            i = (jsint) GET_ATOM_INDEX(pc);
            rval = INT_TO_JSVAL(i);
            PUSH_OPND(rval);
            break;

          case JSOP_NUMBER:
          case JSOP_STRING:
            atom = GET_ATOM(cx, script, pc);
            PUSH_OPND(ATOM_KEY(atom));
            break;

          case JSOP_OBJECT:
            atom = GET_ATOM(cx, script, pc);
            rval = ATOM_KEY(atom);
            JS_ASSERT(JSVAL_IS_OBJECT(rval));
            PUSH_OPND(rval);
            break;

          case JSOP_ZERO:
            PUSH_OPND(JSVAL_ZERO);
            break;

          case JSOP_ONE:
            PUSH_OPND(JSVAL_ONE);
            break;

          case JSOP_NULL:
            PUSH_OPND(JSVAL_NULL);
            break;

          case JSOP_THIS:
            PUSH_OPND(OBJECT_TO_JSVAL(fp->thisp));
            break;

          case JSOP_FALSE:
            PUSH_OPND(JSVAL_FALSE);
            break;

          case JSOP_TRUE:
            PUSH_OPND(JSVAL_TRUE);
            break;

#if JS_HAS_SWITCH_STATEMENT
          case JSOP_TABLESWITCH:
            pc2 = pc;
            len = GET_JUMP_OFFSET(pc2);

            /*
             * ECMAv2 forbids conversion of discriminant, so we will skip to
             * the default case if the discriminant isn't an int jsval.
             * (This opcode is emitted only for dense jsint-domain switches.)
             */
            if (cx->version == JSVERSION_DEFAULT ||
                cx->version >= JSVERSION_1_4) {
                rval = POP();
                if (!JSVAL_IS_INT(rval))
                    break;
                i = JSVAL_TO_INT(rval);
            } else {
                SAVE_SP(fp);
                ok = PopInt(cx, &i);
                RESTORE_SP(fp);
                if (!ok)
                    goto out;
            }

            pc2 += JUMP_OFFSET_LEN;
            low = GET_JUMP_OFFSET(pc2);
            pc2 += JUMP_OFFSET_LEN;
            high = GET_JUMP_OFFSET(pc2);

            i -= low;
            if ((jsuint)i <= (jsuint)(high - low)) {
                pc2 += JUMP_OFFSET_LEN + JUMP_OFFSET_LEN * i;
                off = (jsint) GET_JUMP_OFFSET(pc2);
                if (off)
                    len = off;
            }
            break;

          case JSOP_LOOKUPSWITCH:
            lval = POP();
            pc2 = pc;
            len = GET_JUMP_OFFSET(pc2);

            if (!JSVAL_IS_NUMBER(lval) &&
                !JSVAL_IS_STRING(lval) &&
                !JSVAL_IS_BOOLEAN(lval)) {
                goto advance_pc;
            }

            pc2 += JUMP_OFFSET_LEN;
            npairs = (jsint) GET_ATOM_INDEX(pc2);
            pc2 += ATOM_INDEX_LEN;

#define SEARCH_PAIRS(MATCH_CODE)                                              \
    while (npairs) {                                                          \
        atom = GET_ATOM(cx, script, pc2);                                     \
        rval = ATOM_KEY(atom);                                                \
        MATCH_CODE                                                            \
        if (match) {                                                          \
            pc2 += ATOM_INDEX_LEN;                                            \
            len = GET_JUMP_OFFSET(pc2);                                       \
            goto advance_pc;                                                  \
        }                                                                     \
        pc2 += ATOM_INDEX_LEN + JUMP_OFFSET_LEN;                              \
        npairs--;                                                             \
    }
            if (JSVAL_IS_STRING(lval)) {
                str  = JSVAL_TO_STRING(lval);
                SEARCH_PAIRS(
                    match = (JSVAL_IS_STRING(rval) &&
                             ((str2 = JSVAL_TO_STRING(rval)) == str ||
                              !js_CompareStrings(str2, str)));
                )
            } else if (JSVAL_IS_DOUBLE(lval)) {
                d = *JSVAL_TO_DOUBLE(lval);
                SEARCH_PAIRS(
                    match = (JSVAL_IS_DOUBLE(rval) &&
                             *JSVAL_TO_DOUBLE(rval) == d);
                )
            } else {
                SEARCH_PAIRS(
                    match = (lval == rval);
                )
            }
#undef SEARCH_PAIRS
            break;

          case JSOP_CONDSWITCH:
            break;

#endif /* JS_HAS_SWITCH_STATEMENT */

#if JS_HAS_EXPORT_IMPORT
          case JSOP_EXPORTALL:
            obj = fp->varobj;
            ida = JS_Enumerate(cx, obj);
            if (!ida) {
                ok = JS_FALSE;
            } else {
                for (i = 0, j = ida->length; i < j; i++) {
                    id = ida->vector[i];
                    ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
                    if (!ok)
                        break;
                    if (!prop)
                        continue;
                    ok = OBJ_GET_ATTRIBUTES(cx, obj, id, prop, &attrs);
                    if (ok) {
                        attrs |= JSPROP_EXPORTED;
                        ok = OBJ_SET_ATTRIBUTES(cx, obj, id, prop, &attrs);
                    }
                    OBJ_DROP_PROPERTY(cx, obj2, prop);
                    if (!ok)
                        break;
                }
                JS_DestroyIdArray(cx, ida);
            }
            break;

          case JSOP_EXPORTNAME:
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            obj  = fp->varobj;
            ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
            if (!ok)
                goto out;
            if (!prop) {
                ok = OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, NULL, NULL,
                                         JSPROP_EXPORTED, NULL);
            } else {
                ok = OBJ_GET_ATTRIBUTES(cx, obj, id, prop, &attrs);
                if (ok) {
                    attrs |= JSPROP_EXPORTED;
                    ok = OBJ_SET_ATTRIBUTES(cx, obj, id, prop, &attrs);
                }
                OBJ_DROP_PROPERTY(cx, obj2, prop);
            }
            if (!ok)
                goto out;
            break;

          case JSOP_IMPORTALL:
            id = (jsid)JSVAL_VOID;
            PROPERTY_OP(ok = ImportProperty(cx, obj, id));
            break;

          case JSOP_IMPORTPROP:
            /* Get an immediate atom naming the property. */
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            PROPERTY_OP(ok = ImportProperty(cx, obj, id));
            break;

          case JSOP_IMPORTELEM:
            ELEMENT_OP(ok = ImportProperty(cx, obj, id));
            break;
#endif /* JS_HAS_EXPORT_IMPORT */

          case JSOP_TRAP:
            switch (JS_HandleTrap(cx, script, pc, &rval)) {
              case JSTRAP_ERROR:
                ok = JS_FALSE;
                goto out;
              case JSTRAP_CONTINUE:
                JS_ASSERT(JSVAL_IS_INT(rval));
                op = (JSOp) JSVAL_TO_INT(rval);
                JS_ASSERT((uintN)op < (uintN)JSOP_LIMIT);
                goto do_op;
              case JSTRAP_RETURN:
                fp->rval = rval;
                goto out;
#if JS_HAS_EXCEPTIONS
              case JSTRAP_THROW:
                cx->throwing = JS_TRUE;
                cx->exception = rval;
                ok = JS_FALSE;
                goto out;
#endif /* JS_HAS_EXCEPTIONS */
              default:;
            }
            break;

          case JSOP_ARGUMENTS:
            /* XXXbe if (!TEST_BIT(slot, fp->overrides))... */
            obj = js_GetArgsObject(cx, fp);
            if (!obj) {
                ok = JS_FALSE;
                goto out;
            }
            PUSH_OPND(OBJECT_TO_JSVAL(obj));
            break;

          case JSOP_GETARG:
            obj = NULL;
            slot = (uintN)GET_ARGNO(pc);
            JS_ASSERT(slot < fp->fun->nargs);
            PUSH_OPND(fp->argv[slot]);
            break;

          case JSOP_SETARG:
            obj = NULL;
            slot = (uintN)GET_ARGNO(pc);
            JS_ASSERT(slot < fp->fun->nargs);
            vp = &fp->argv[slot];
            GC_POKE(cx, *vp);
            *vp = sp[-1];
            break;

          case JSOP_GETVAR:
            obj = NULL;
            slot = (uintN)GET_VARNO(pc);
            JS_ASSERT(slot < fp->fun->nvars);
            PUSH_OPND(fp->vars[slot]);
            break;

          case JSOP_SETVAR:
            obj = NULL;
            slot = (uintN)GET_VARNO(pc);
            JS_ASSERT(slot < fp->fun->nvars);
            vp = &fp->vars[slot];
            GC_POKE(cx, *vp);
            *vp = sp[-1];
            break;

          case JSOP_DEFCONST:
          case JSOP_DEFVAR:
          {
            JSBool defined;

            atom = GET_ATOM(cx, script, pc);
            obj = fp->varobj;
            attrs = JSPROP_ENUMERATE;
            if (!(fp->special & JSFRAME_EVAL))
                attrs |= JSPROP_PERMANENT;
            if (op == JSOP_DEFCONST)
                attrs |= JSPROP_READONLY;

            /* Lookup id in order to check for redeclaration problems. */
            id = (jsid)atom;
            ok = js_CheckRedeclaration(cx, obj, id, attrs, &defined);
            if (!ok)
                goto out;

            /* Bind a variable only if it's not yet defined. */
            if (!defined) {
                ok = OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, NULL, NULL,
                                         attrs, NULL);
                if (!ok)
                    goto out;
            }
            break;
          }

          case JSOP_DEFFUN:
          {
            uintN flags;

            atom = GET_ATOM(cx, script, pc);
            obj = ATOM_TO_OBJECT(atom);
            fun = (JSFunction *) JS_GetPrivate(cx, obj);
            id = (jsid) fun->atom;

            /*
             * We must be at top-level (default "box", either function body or
             * global) scope, not inside a with or other compound statement.
             *
             * If static link is not current scope, clone fun's object to link
             * to the current scope via parent.  This clause exists to enable
             * sharing of compiled functions among multiple equivalent scopes,
             * splitting the cost of compilation evenly among the scopes and
             * amortizing it over a number of executions.  Examples include XUL
             * scripts and event handlers shared among Mozilla chrome windows,
             * and server-side JS user-defined functions shared among requests.
             *
             * NB: The Script object exposes compile and exec in the language,
             * such that this clause introduces an incompatible change from old
             * JS versions that supported Script.  Such a JS version supported
             * executing a script that defined and called functions scoped by
             * the compile-time static link, not by the exec-time scope chain.
             *
             * We sacrifice compatibility, breaking such scripts, in order to
             * promote compile-cost sharing and amortizing, and because Script
             * is not and will not be standardized.
             */
            parent = fp->varobj;
            if (OBJ_GET_PARENT(cx, obj) != parent) {
                obj = js_CloneFunctionObject(cx, obj, parent);
                if (!obj) {
                    ok = JS_FALSE;
                    goto out;
                }
            }

            /* Load function flags that are also property attributes. */
            flags = fun->flags & (JSFUN_GETTER | JSFUN_SETTER);
            attrs = flags | JSPROP_ENUMERATE;

            /*
             * Check for a const property of the same name -- or any kind
             * of property if executing with the strict option.  We check
             * here at runtime as well as at compile-time, to handle eval
             * as well as multiple HTML script tags.
             */
            ok = js_CheckRedeclaration(cx, parent, id, attrs, &cond);
            if (!ok)
                goto out;

            ok = OBJ_DEFINE_PROPERTY(cx, parent, id,
                                     flags ? JSVAL_VOID : OBJECT_TO_JSVAL(obj),
                                     (flags & JSFUN_GETTER)
                                     ? (JSPropertyOp) obj
                                     : NULL,
                                     (flags & JSFUN_SETTER)
                                     ? (JSPropertyOp) obj
                                     : NULL,
                                     attrs,
                                     NULL);
            if (!ok)
                goto out;
            break;
          }

#if JS_HAS_LEXICAL_CLOSURE
          case JSOP_ANONFUNOBJ:
            /* Push the specified function object literal. */
            atom = GET_ATOM(cx, script, pc);
            obj = ATOM_TO_OBJECT(atom);

            /* If re-parenting, push a clone of the function object. */
            parent = fp->varobj;
            if (OBJ_GET_PARENT(cx, obj) != parent) {
                obj = js_CloneFunctionObject(cx, obj, parent);
                if (!obj) {
                    ok = JS_FALSE;
                    goto out;
                }
            }
            PUSH_OPND(OBJECT_TO_JSVAL(obj));
            break;

          case JSOP_NAMEDFUNOBJ:
            /* ECMA ed. 3 FunctionExpression: function Identifier [etc.]. */
            atom = GET_ATOM(cx, script, pc);
            rval = ATOM_KEY(atom);
            JS_ASSERT(JSVAL_IS_FUNCTION(cx, rval));

            /*
             * 1. Create a new object as if by the expression new Object().
             * 2. Add Result(1) to the front of the scope chain.
             *
             * Step 2 is achieved by making the new object's parent be the
             * current scope chain, and then making the new object the parent
             * of the Function object clone.
             */
            parent = js_ConstructObject(cx, &js_ObjectClass, NULL,
                                        fp->scopeChain);
            if (!parent) {
                ok = JS_FALSE;
                goto out;
            }

            /*
             * 3. Create a new Function object as specified in section 13.2
             * with [parameters and body specified by the function expression
             * that was parsed by the compiler into a Function object, and
             * saved in the script's atom map].
             */
            obj = js_CloneFunctionObject(cx, JSVAL_TO_OBJECT(rval), parent);
            if (!obj) {
                ok = JS_FALSE;
                goto out;
            }

            /*
             * 4. Create a property in the object Result(1).  The property's
             * name is [fun->atom, the identifier parsed by the compiler],
             * value is Result(3), and attributes are { DontDelete, ReadOnly }.
             */
            fun = (JSFunction *) JS_GetPrivate(cx, obj);
            attrs = fun->flags & (JSFUN_GETTER | JSFUN_SETTER);
            ok = OBJ_DEFINE_PROPERTY(cx, parent, (jsid)fun->atom,
                                     attrs ? JSVAL_VOID : OBJECT_TO_JSVAL(obj),
                                     (attrs & JSFUN_GETTER)
                                     ? (JSPropertyOp) obj
                                     : NULL,
                                     (attrs & JSFUN_SETTER)
                                     ? (JSPropertyOp) obj
                                     : NULL,
                                     attrs |
                                     JSPROP_ENUMERATE | JSPROP_PERMANENT |
                                     JSPROP_READONLY,
                                     NULL);
            if (!ok) {
                cx->newborn[GCX_OBJECT] = NULL;
                goto out;
            }

            /*
             * 5. Remove Result(1) from the front of the scope chain [no-op].
             * 6. Return Result(3).
             */
            PUSH_OPND(OBJECT_TO_JSVAL(obj));
            break;

          case JSOP_CLOSURE:
            /*
             * ECMA ed. 3 extension: a named function expression in a compound
             * statement (not at top-level or "box" scope, either global code
             * or in a function body).
             *
             * Name the closure in the object at the head of the scope chain,
             * referenced by parent.  Even if it's a With object?  Not for the
             * current version (1.5), which uses the object named by the with
             * statement's head.
             */
            parent = fp->varobj;
            if (fp->scopeChain != parent) {
                parent = fp->scopeChain;
                JS_ASSERT(OBJ_GET_CLASS(cx, parent) == &js_WithClass);
#if JS_BUG_WITH_CLOSURE
                /*
                 * If in a with statement, set parent to the With object's
                 * prototype, i.e., the object specified in the head of
                 * the with statement.
                 */
                while (OBJ_GET_CLASS(cx, parent) == &js_WithClass) {
                    proto = OBJ_GET_PROTO(cx, parent);
                    if (!proto)
                        break;
                    parent = proto;
                }
#endif
            }

            /*
             * Get immediate operand atom, which is a function object literal.
             * From it, get the function to close.
             */
            atom = GET_ATOM(cx, script, pc);
            JS_ASSERT(JSVAL_IS_FUNCTION(cx, ATOM_KEY(atom)));
            obj = ATOM_TO_OBJECT(atom);
            fun = (JSFunction *) JS_GetPrivate(cx, obj);

            /*
             * Clone the function object with the current scope chain as the
             * clone's parent.  The original function object is the prototype
             * of the clone.  Do this only if re-parenting; the compiler may
             * have seen the right parent already and created a sufficiently
             * well-scoped function object.
             */
            if (OBJ_GET_PARENT(cx, obj) != parent) {
                obj = js_CloneFunctionObject(cx, obj, parent);
                if (!obj) {
                    ok = JS_FALSE;
                    goto out;
                }
            }

            /*
             * Define a property in parent with id fun->atom and value obj,
             * unless fun is a getter or setter (in which case, obj is cast
             * to a JSPropertyOp and passed accordingly).
             */
            attrs = fun->flags & (JSFUN_GETTER | JSFUN_SETTER);
            ok = OBJ_DEFINE_PROPERTY(cx, parent, (jsid)fun->atom,
                                     attrs ? JSVAL_VOID : OBJECT_TO_JSVAL(obj),
                                     (attrs & JSFUN_GETTER)
                                     ? (JSPropertyOp) obj
                                     : NULL,
                                     (attrs & JSFUN_SETTER)
                                     ? (JSPropertyOp) obj
                                     : NULL,
                                     attrs | JSPROP_ENUMERATE,
                                     NULL);
            if (!ok) {
                cx->newborn[GCX_OBJECT] = NULL;
                goto out;
            }
            break;
#endif /* JS_HAS_LEXICAL_CLOSURE */

#if JS_HAS_GETTER_SETTER
          case JSOP_GETTER:
          case JSOP_SETTER:
            JS_ASSERT(len == 1);
            op2 = (JSOp) *++pc;
            cs = &js_CodeSpec[op2];
            len = cs->length;
            switch (op2) {
              case JSOP_SETNAME:
              case JSOP_SETPROP:
                atom = GET_ATOM(cx, script, pc);
                id   = (jsid)atom;
                rval = POP();
                goto gs_pop_lval;

              case JSOP_SETELEM:
                rval = POP();
                POP_ELEMENT_ID(id);
              gs_pop_lval:
                lval = POP();
                VALUE_TO_OBJECT(cx, lval, obj);
                break;

#if JS_HAS_INITIALIZERS
              case JSOP_INITPROP:
                JS_ASSERT(sp - newsp >= 2);
                rval = POP();
                atom = GET_ATOM(cx, script, pc);
                id   = (jsid)atom;
                goto gs_get_lval;

              case JSOP_INITELEM:
                JS_ASSERT(sp - newsp >= 3);
                rval = POP();
                POP_ELEMENT_ID(id);
              gs_get_lval:
                lval = sp[-1];
                JS_ASSERT(JSVAL_IS_OBJECT(lval));
                obj = JSVAL_TO_OBJECT(lval);
                break;
#endif /* JS_HAS_INITIALIZERS */

              default:
                JS_ASSERT(0);
            }

            if (JS_TypeOfValue(cx, rval) != JSTYPE_FUNCTION) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_GETTER_OR_SETTER,
                                     (op == JSOP_GETTER)
                                     ? js_getter_str
                                     : js_setter_str);
                ok = JS_FALSE;
                goto out;
            }

            /*
             * Getters and setters are just like watchpoints from an access
             * control point of view.
             */
            if (!OBJ_CHECK_ACCESS(cx, obj, id, JSACC_WATCH, &rtmp, &attrs))
                return JS_FALSE;

            if (op == JSOP_GETTER) {
                getter = (JSPropertyOp) JSVAL_TO_OBJECT(rval);
                setter = NULL;
                attrs = JSPROP_GETTER;
            } else {
                getter = NULL;
                setter = (JSPropertyOp) JSVAL_TO_OBJECT(rval);
                attrs = JSPROP_SETTER;
            }
            attrs |= JSPROP_ENUMERATE;

            /* Check for a readonly or permanent property of the same name. */
            ok = js_CheckRedeclaration(cx, obj, id, attrs, &cond);
            if (!ok)
                goto out;

            ok = OBJ_DEFINE_PROPERTY(cx, obj, id, JSVAL_VOID, getter, setter,
                                     attrs, NULL);
            if (!ok)
                goto out;
            if (cs->ndefs)
                PUSH_OPND(rval);
            break;
#endif /* JS_HAS_GETTER_SETTER */

#if JS_HAS_INITIALIZERS
          case JSOP_NEWINIT:
            argc = 0;
            fp->sharpDepth++;
            goto do_new;

          case JSOP_ENDINIT:
            if (--fp->sharpDepth == 0)
                fp->sharpArray = NULL;

            /* Re-set the newborn root to the top of this object tree. */
            JS_ASSERT(sp - newsp >= 1);
            lval = sp[-1];
            JS_ASSERT(JSVAL_IS_OBJECT(lval));
            cx->newborn[GCX_OBJECT] = JSVAL_TO_GCTHING(lval);
            break;

          case JSOP_INITPROP:
            /* Pop the property's value into rval. */
            JS_ASSERT(sp - newsp >= 2);
            rval = POP();

            /* Get the immediate property name into id. */
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;
            goto do_init;

          case JSOP_INITELEM:
            /* Pop the element's value into rval. */
            JS_ASSERT(sp - newsp >= 3);
            rval = POP();

            /* Pop and conditionally atomize the element id. */
            POP_ELEMENT_ID(id);

          do_init:
            /* Find the object being initialized at top of stack. */
            lval = sp[-1];
            JS_ASSERT(JSVAL_IS_OBJECT(lval));
            obj = JSVAL_TO_OBJECT(lval);

            /* Set the property named by obj[id] to rval. */
            ok = OBJ_SET_PROPERTY(cx, obj, id, &rval);
            if (!ok)
                goto out;
            break;

#if JS_HAS_SHARP_VARS
          case JSOP_DEFSHARP:
            obj = fp->sharpArray;
            if (!obj) {
                obj = js_NewArrayObject(cx, 0, NULL);
                if (!obj) {
                    ok = JS_FALSE;
                    goto out;
                }
                fp->sharpArray = obj;
            }
            i = (jsint) GET_ATOM_INDEX(pc);
            id = (jsid) INT_TO_JSVAL(i);
            rval = sp[-1];
            if (JSVAL_IS_PRIMITIVE(rval)) {
                char numBuf[12];
                JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_SHARP_DEF, numBuf);
                ok = JS_FALSE;
                goto out;
            }
            ok = OBJ_SET_PROPERTY(cx, obj, id, &rval);
            if (!ok)
                goto out;
            break;

          case JSOP_USESHARP:
            i = (jsint) GET_ATOM_INDEX(pc);
            id = (jsid) INT_TO_JSVAL(i);
            obj = fp->sharpArray;
            if (!obj) {
                rval = JSVAL_VOID;
            } else {
                ok = OBJ_GET_PROPERTY(cx, obj, id, &rval);
                if (!ok)
                    goto out;
            }
            if (!JSVAL_IS_OBJECT(rval)) {
                char numBuf[12];
                JS_snprintf(numBuf, sizeof numBuf, "%u", (unsigned) i);
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_SHARP_USE, numBuf);
                ok = JS_FALSE;
                goto out;
            }
            PUSH_OPND(rval);
            break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

#if JS_HAS_EXCEPTIONS
          /* Reset the stack to the given depth. */
          case JSOP_SETSP:
            i = (jsint) GET_ATOM_INDEX(pc);
            JS_ASSERT(i >= 0);
            sp = newsp + i;
            break;

          case JSOP_GOSUB:
            i = PTRDIFF(pc, script->main, jsbytecode) + len;
            len = GET_JUMP_OFFSET(pc);
            PUSH(INT_TO_JSVAL(i));
            break;

          case JSOP_RETSUB:
            rval = POP();
            JS_ASSERT(JSVAL_IS_INT(rval));
            i = JSVAL_TO_INT(rval);
            pc = script->main + i;
            len = 0;
            break;

          case JSOP_EXCEPTION:
            PUSH(cx->exception);
            break;

          case JSOP_THROW:
            cx->throwing = JS_TRUE;
            cx->exception = POP();
            ok = JS_FALSE;
            /* let the code at out try to catch the exception. */
            goto out;

          case JSOP_INITCATCHVAR:
            /* Pop the property's value into rval. */
            JS_ASSERT(sp - newsp >= 2);
            rval = POP();

            /* Get the immediate catch variable name into id. */
            atom = GET_ATOM(cx, script, pc);
            id   = (jsid)atom;

            /* Find the object being initialized at top of stack. */
            lval = sp[-1];
            JS_ASSERT(JSVAL_IS_OBJECT(lval));
            obj = JSVAL_TO_OBJECT(lval);

            /* Define obj[id] to contain rval and to be permanent. */
            ok = OBJ_DEFINE_PROPERTY(cx, obj, id, rval, NULL, NULL,
                                     JSPROP_PERMANENT, NULL);
            if (!ok)
                goto out;
            break;
#endif /* JS_HAS_EXCEPTIONS */

#if JS_HAS_INSTANCEOF
          case JSOP_INSTANCEOF:
            rval = POP();
            if (JSVAL_IS_PRIMITIVE(rval)) {
                str = js_DecompileValueGenerator(cx, JS_TRUE, rval, NULL);
                if (str) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_BAD_INSTANCEOF_RHS,
                                         JS_GetStringBytes(str));
                }
                ok = JS_FALSE;
                goto out;
            }
            obj = JSVAL_TO_OBJECT(rval);
            lval = POP();
            cond = JS_FALSE;
            if (obj->map->ops->hasInstance) {
                SAVE_SP(fp);
                ok = obj->map->ops->hasInstance(cx, obj, lval, &cond);
                if (!ok)
                    goto out;
            }
            PUSH_OPND(BOOLEAN_TO_JSVAL(cond));
            break;
#endif /* JS_HAS_INSTANCEOF */

#if JS_HAS_DEBUGGER_KEYWORD
          case JSOP_DEBUGGER:
          {
            JSTrapHandler handler = rt->debuggerHandler;
            if (handler) {
                switch (handler(cx, script, pc, &rval,
                                rt->debuggerHandlerData)) {
                  case JSTRAP_ERROR:
                    ok = JS_FALSE;
                    goto out;
                  case JSTRAP_CONTINUE:
                    break;
                  case JSTRAP_RETURN:
                    fp->rval = rval;
                    goto out;
#if JS_HAS_EXCEPTIONS
                  case JSTRAP_THROW:
                    cx->throwing = JS_TRUE;
                    cx->exception = rval;
                    ok = JS_FALSE;
                    goto out;
#endif /* JS_HAS_EXCEPTIONS */
                  default:;
                }
            }
            break;
          }
#endif /* JS_HAS_DEBUGGER_KEYWORD */

          default: {
            char numBuf[12];
            JS_snprintf(numBuf, sizeof numBuf, "%d", op);
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_BYTECODE, numBuf);
            ok = JS_FALSE;
            goto out;
          }
        }

    advance_pc:
        pc += len;

#ifdef DEBUG
        if (tracefp) {
            intN ndefs, n;

            ndefs = cs->ndefs;
            if (ndefs) {
                fp->sp = sp - ndefs;
                for (n = 0; n < ndefs; n++) {
                    str = js_DecompileValueGenerator(cx, JS_TRUE, *fp->sp,
                                                     NULL);
                    if (str) {
                        fprintf(tracefp, "%s %s",
                                (n == 0) ? "  output:" : ",",
                                JS_GetStringBytes(str));
                    }
                    fp->sp++;
                }
                putc('\n', tracefp);
            }
        }
#endif
    }
out:

#if JS_HAS_EXCEPTIONS
    /*
     * Has an exception been raised?
     */
    if (!ok && cx->throwing) {
        /*
         * Call debugger throw hook if set (XXX thread safety?).
         */
        JSTrapHandler handler = rt->throwHook;
        if (handler) {
            switch (handler(cx, script, pc, &rval, rt->throwHookData)) {
              case JSTRAP_ERROR:
                cx->throwing = JS_FALSE;
                goto no_catch;
              case JSTRAP_CONTINUE:
                cx->throwing = JS_FALSE;
                ok = JS_TRUE;
                goto advance_pc;
              case JSTRAP_RETURN:
                ok = JS_TRUE;
                cx->throwing = JS_FALSE;
                fp->rval = rval;
                goto no_catch;
              case JSTRAP_THROW:
                cx->exception = rval;
              default:;
            }
        }

        /*
         * Look for a try block within this frame that can catch the exception.
         */
        tn = script->trynotes;
        if (tn) {
            offset = PTRDIFF(pc, script->main, jsbytecode);
            while (JS_UPTRDIFF(offset, tn->start) >= (jsuword)tn->length)
                tn++;
            if (tn->catchStart) {
                pc = script->main + tn->catchStart;
                len = 0;
                cx->throwing = JS_FALSE; /* caught */
                ok = JS_TRUE;
                goto advance_pc;
            }
        }
    }
no_catch:
#endif

    /*
     * Check whether control fell off the end of a lightweight function, or an
     * exception thrown under such a function was not caught by it.  If so, go
     * to the inline code under JSOP_RETURN.
     */
    if (inlineCallCount)
        goto inline_return;

    /*
     * Restore the previous frame's execution state.
     */
    js_FreeStack(cx, mark);
    if (currentVersion != originalVersion)
        JS_SetVersion(cx, originalVersion);
    cx->interpLevel--;
    return ok;
}
