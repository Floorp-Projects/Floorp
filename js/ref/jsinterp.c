/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/*
 * JavaScript bytecode interpreter.
 */
#include "jsstddef.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "prtypes.h"
#include "prassert.h"
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

/*
 * Class for for/in loop property iterator objects.
 */
#define JSSLOT_ITR_STATE    (JSSLOT_START)

static void
prop_iterator_finalize(JSContext *cx, JSObject *obj)
{
    jsval iter_state;

    /* Protect against stillborn iterators. */
    iter_state = obj->slots[JSSLOT_ITR_STATE];
    if (iter_state != JSVAL_NULL)
        OBJ_ENUMERATE(cx, obj, JSENUMERATE_DESTROY, &iter_state, NULL);
}

static JSClass prop_iterator_class = {
    "PropertyIterator",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   prop_iterator_finalize
};

/*
 * Stack macros and functions.  These all use a local variable, jsval *sp, to
 * point to the next free stack slot.  SAVE_SP must be called before any call
 * to a function that may invoke the interpreter.  RESTORE_SP must be called
 * only after return from Call, because only Call changes fp->sp.
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
    PR_BEGIN_MACRO                                                            \
	jsint _i;                                                             \
	jsval _v;                                                             \
									      \
	_i = (jsint)d;                                                        \
	if (JSDOUBLE_IS_INT(d, _i) && INT_FITS_IN_JSVAL(_i)) {                \
	    _v = INT_TO_JSVAL(_i);                                            \
	} else {                                                              \
	    ok = js_NewDoubleValue(cx, d, &_v);                               \
	    if (!ok)                                                          \
		goto out;                                                     \
	}                                                                     \
	PUSH_OPND(_v);                                                        \
    PR_END_MACRO

#define POP_NUMBER(cx, d)                                                     \
    PR_BEGIN_MACRO                                                            \
	jsval _v;                                                             \
									      \
	_v = POP();                                                           \
	VALUE_TO_NUMBER(cx, _v, d);                                           \
    PR_END_MACRO

/*
 * This POP variant is called only for bitwise operators and for tableswitch,
 * so we don't bother to inline it.  The calls in Interpret must therefore
 * SAVE_SP first!
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
    PR_BEGIN_MACRO                                                            \
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
    PR_END_MACRO

#define POP_BOOLEAN(cx, v, b)                                                 \
    PR_BEGIN_MACRO                                                            \
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
    PR_END_MACRO

#define VALUE_TO_OBJECT(cx, v, obj)                                           \
    PR_BEGIN_MACRO                                                            \
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
    PR_END_MACRO

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
    PR_BEGIN_MACRO                                                            \
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
    PR_END_MACRO

jsval *
js_AllocStack(JSContext *cx, uintN nslots, void **markp)
{
    jsval *sp;

    if (markp)
    	*markp = PR_ARENA_MARK(&cx->stackPool);
    PR_ARENA_ALLOCATE(sp, &cx->stackPool, nslots * sizeof(jsval));
    if (!sp) {
	JS_ReportError(cx, "stack overflow in %s",
		       (cx->fp && cx->fp->fun)
		       ? JS_GetFunctionName(cx->fp->fun)
		       : "script");
    }
    return sp;
}

void
js_FreeStack(JSContext *cx, void *mark)
{
    PR_ARENA_RELEASE(&cx->stackPool, mark);
}

JSBool
js_GetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;

    PR_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun == fun) {
		PR_ASSERT((uintN)JSVAL_TO_INT(id) < fp->fun->nargs);
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

    PR_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun == fun) {
		PR_ASSERT((uintN)JSVAL_TO_INT(id) < fp->fun->nargs);
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

    PR_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun == fun) {
		slot = JSVAL_TO_INT(id);
		PR_ASSERT((uintN)slot < fp->fun->nvars);
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

    PR_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_FunctionClass);
    fun = JS_GetPrivate(cx, obj);
    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun == fun) {
		slot = JSVAL_TO_INT(id);
		PR_ASSERT((uintN)slot < fp->fun->nvars);
		if ((uintN)slot < fp->nvars)
		    fp->vars[slot] = *vp;
	    }
	    return JS_TRUE;
	}
    }
    return JS_TRUE;

}

/*
 * Find a function reference and its 'this' object implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
JSBool
js_Invoke(JSContext *cx, uintN argc, JSBool constructing)
{
    JSStackFrame *fp, frame;
    jsval *sp, *newsp;
    jsval *vp, v;
    JSObject *funobj, *parent, *thisp;
    JSClass *clasp;
    JSObjectOps *ops;
    JSNative native;
    JSFunction *fun;
    JSScript *script;
    uintN minargs, nvars;
    void *mark;
    intN nslots, nalloc, surplus;
    JSBool ok;

    /* Reach under args and this to find the callable on the stack. */
    fp = cx->fp;
    sp = fp->sp;

    /* Once vp is set, control must flow through label out2: to return. */
    vp = sp - (2 + argc);
    v = *vp;

    /* A callable must be an object reference. */
    if (JSVAL_IS_PRIMITIVE(v))
    	goto bad;
    funobj = JSVAL_TO_OBJECT(v);

    /* Load callable parent and this parameter for later. */
    parent = OBJ_GET_PARENT(cx, funobj);
    thisp = JSVAL_TO_OBJECT(vp[1]);

    clasp = OBJ_GET_CLASS(cx, funobj);
    if (clasp != &js_FunctionClass) {
	/* Function is inlined, all other classes use object ops. */
	ops = funobj->map->ops;

	/* Try converting to function, for closure and API compatibility. */
	ok = clasp->convert(cx, funobj, JSTYPE_FUNCTION, &v);
	if (!ok)
	    goto out2;
	if (!JSVAL_IS_FUNCTION(cx, v)) {
	    fun = NULL;      
	    script = NULL;
	    minargs = nvars = 0;
	} else {
	    funobj = JSVAL_TO_OBJECT(v);
            parent = OBJ_GET_PARENT(cx, funobj);

	    fun = JS_GetPrivate(cx, funobj);
            if (clasp != &js_ClosureClass) {
                /* Make vp refer to funobj to keep it available as argv[-2]. */
                *vp = v;
                goto have_fun;
            }

            /* Closure invocation may need extra arg and local var slots. */
            script = fun->script;
            minargs = fun->nargs + fun->extra;
            nvars = fun->nvars;
	}

	/* Try a call or construct native object op, using fun as fallback. */
	native = constructing ? ops->construct : ops->call;
	if (!native)
	    goto bad;
    } else {
	/* Get private data and set derived locals from it. */
	fun = JS_GetPrivate(cx, funobj);
have_fun:
	native = fun->call;
	script = fun->script;
	minargs = fun->nargs + fun->extra;
	nvars = fun->nvars;

	/* Handle bound method and dynamic scope special cases. */
	if (fun->flags) {
	    if (fun->flags & JSFUN_BOUND_METHOD)
		thisp = parent;
	    else
		parent = NULL;
        }
    }

    /* Initialize a stack frame, except for thisp and scopeChain. */
    frame.callobj = frame.argsobj = NULL;
    frame.script = script;
    frame.fun = fun;
    frame.argc = argc;
    frame.argv = sp - argc;
    frame.rval = constructing ? OBJECT_TO_JSVAL(thisp) : JSVAL_VOID;
    frame.nvars = nvars;
    frame.vars = sp;
    frame.down = fp;
    frame.annotation = NULL;
    frame.pc = NULL;
    frame.sp = sp;
    frame.sharpDepth = 0;
    frame.sharpArray = NULL;
    frame.constructing = constructing;
    frame.overrides = 0;
    frame.debugging = JS_FALSE;
    frame.exceptPending = JS_FALSE;

    /* Compute the 'this' parameter and store it in frame. */
    if (thisp && OBJ_GET_CLASS(cx, thisp) != &js_CallClass) {
	/* Some objects (e.g., With) delegate 'this' to another object. */
	thisp = OBJ_THIS_OBJECT(cx, thisp);
	if (!thisp) {
	    ok = JS_FALSE;
	    goto out2;
	}
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
        if (parent == NULL) {
            parent = cx->globalObject;
        } else {
            /* walk up to find the top-level object */
            JSObject *p;
            thisp = parent;
            while (p = OBJ_GET_PARENT(cx, thisp))
                thisp = p;
        }
    }
    frame.thisp = thisp;

    /* From here on, control must flow through label out: to return. */
    cx->fp = &frame;
    mark = PR_ARENA_MARK(&cx->stackPool);

    /* Check for missing arguments expected by the function. */
    nslots = (intN)((argc < minargs) ? minargs - argc : 0);
    if (nslots) {
	/* All arguments must be contiguous, so we may have to copy actuals. */
	nalloc = nslots;
	if ((pruword)(sp + nslots) > cx->stackPool.current->limit)
	    nalloc += argc;

	/* Take advantage of the surplus slots in the caller's frame depth. */
	surplus = (jsval *)mark - sp;
	PR_ASSERT(surplus >= 0);
	nalloc -= surplus;

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

    /* Call the function, either a native method or an interpreted script. */
    if (native) {
	frame.scopeChain = fp->scopeChain;
	ok = native(cx, thisp, argc, frame.argv, &frame.rval);
    } else if (script) {
	frame.scopeChain = funobj;
	if (!parent) {
#if JS_HAS_CALL_OBJECT
	    /* Scope with a call object parented by cx's global object. */
	    funobj = js_GetCallObject(cx, &frame, cx->globalObject, NULL);
	    if (!funobj) {
		ok = JS_FALSE;
		goto out;
	    }
#else
	    /* Bad old code slams globalObject directly into funobj. */
	    OBJ_SET_PARENT(funobj, cx->globalObject);
#endif
	}
	ok = js_Interpret(cx, &v);
#if !JS_HAS_CALL_OBJECT
	if (!parent)
	    OBJ_SET_PARENT(funobj, NULL);
#endif
    } else {
	/* fun might be onerror trying to report a syntax error in itself. */
	frame.scopeChain = NULL;
	ok = JS_TRUE;
    }

#if JS_HAS_EXCEPTIONS
    /* Native or script returns JS_FALSE on error or uncaught exception */
    if (!ok && frame.exceptPending) {
	fp->exceptPending = JS_TRUE;
	fp->exception = frame.exception;
    }
#endif

out:
    /*
     * Checking frame.annotation limits the use of this hook for uses other
     * than releasing annotations, but avoids one C function call for every
     * JS function call.
     */
    if (frame.annotation &&
	js_InterpreterHooks &&
	js_InterpreterHooks->destroyFrame) {
        js_InterpreterHooks->destroyFrame(cx, &frame);
    }

#if JS_HAS_CALL_OBJECT
    /* If frame has a call object, sync values and clear back-pointer. */
    if (frame.callobj)
	ok &= js_PutCallObject(cx, &frame);
#endif
#if JS_HAS_ARGS_OBJECT
    /* If frame has an arguments object, sync values clear back-pointer. */
    if (frame.argsobj)
	ok &= js_PutArgsObject(cx, &frame);
#endif

    /* Pop everything off the stack and store the return value. */
    PR_ARENA_RELEASE(&cx->stackPool, mark);
    cx->fp = fp;
    *vp = frame.rval;

out2:
    /* Must save sp so throw code can store an exception at sp[-1]. */
    fp->sp = vp + 1;
    return ok;

bad:
    js_ReportIsNotFunction(cx, vp, constructing);
    ok = JS_FALSE;
    goto out2;
}

JSBool
js_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval,
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
    ok = js_Invoke(cx, argc, JS_FALSE);
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
	   JSStackFrame *down, JSBool debugging, jsval *result)
{
    JSStackFrame *oldfp, frame;
    JSBool ok;

    oldfp = cx->fp;
    frame.callobj = frame.argsobj = NULL;
    frame.script = script;
    frame.fun = fun;
    if (down) {
	/* Propagate arg/var state for eval and the debugger API. */
	frame.thisp = down->thisp;
	frame.argc = down->argc;
	frame.argv = down->argv;
	frame.nvars = down->nvars;
	frame.vars = down->vars;
	frame.annotation = down->annotation;
	frame.sharpArray = down->sharpArray;
    } else {
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
    frame.debugging = debugging;
    frame.exceptPending = JS_FALSE;
    cx->fp = &frame;
    ok = js_Interpret(cx, result);
    cx->fp = oldfp;
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
    JSFunction *fun;
    jsval value;

    if (JSVAL_IS_VOID(id)) {
	ida = JS_Enumerate(cx, obj);
	if (!ida)
	    return JS_FALSE;
	if (ida->length == 0)
	    goto out;
	ok = JS_TRUE;
    } else {
	ida = NULL;
	if (!OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop))
	    return JS_FALSE;
	if (!prop) {
	    str = js_DecompileValueGenerator(cx, js_IdToValue(id), NULL);
	    if (str)
		js_ReportIsNotDefined(cx, JS_GetStringBytes(str));
	    return JS_FALSE;
	}
	ok = OBJ_GET_ATTRIBUTES(cx, obj, id, prop, &attrs);
	OBJ_DROP_PROPERTY(cx, obj2, prop);
	if (!ok)
	    return JS_FALSE;
	if (!(attrs & JSPROP_EXPORTED)) {
	    str = js_DecompileValueGenerator(cx, js_IdToValue(id), NULL);
	    if (str) {
		JS_ReportError(cx, "%s is not exported",
			       JS_GetStringBytes(str));
	    }
	    return JS_FALSE;
	}
    }

    target = js_FindVariableScope(cx, &fun);
    if (!target) {
	ok = JS_FALSE;
	goto out;
    }

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
	    closure = js_ConstructObject(cx, &js_ClosureClass, funobj, obj);
	    if (!closure) {
		ok = JS_FALSE;
		goto out;
	    }
            /*
             * The Closure() constructor resets the closure object's parent to be
             * the current scope chain.  Set it to the object that the imported
             * function is being defined in.
             */
            OBJ_SET_PARENT(cx, closure, obj);
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
        if (prop && (target == obj2)) {
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

#if !defined XP_PC || !defined _MSC_VER || _MSC_VER > 800
#define MAX_INTERP_LEVEL 1000
#else
#define MAX_INTERP_LEVEL 30
#endif

JSBool
js_Interpret(JSContext *cx, jsval *result)
{
    JSRuntime *rt;
    JSStackFrame *fp;
    JSScript *script;
    JSObject *obj, *obj2, *proto, *parent;
    JSBranchCallback onbranch;
    JSBool ok, cond;
    ptrdiff_t depth, len;
    jsval *sp, *newsp;
    void *mark;
    jsbytecode *pc, *pc2, *endpc;
    JSOp op, op2;
    JSCodeSpec *cs;
    JSAtom *atom;
    uintN argc, slot;
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
    JSFunction *fun;
    JSType type;
#ifdef DEBUG
    FILE *tracefp;
#endif
#if JS_HAS_SWITCH_STATEMENT
    jsint low, high;
    uintN off, npairs;
    JSBool match;
#endif
#if JS_HAS_LEXICAL_CLOSURE
    JSFunction *fun2;
    JSObject *closure;
#endif
#if JS_HAS_EXPORT_IMPORT
    uintN attrs;
#endif
#if JS_HAS_EXCEPTIONS
    JSTryNote *tn;
    ptrdiff_t offset;
#endif

    if (cx->interpLevel == MAX_INTERP_LEVEL) {
	JS_ReportError(cx, "too much recursion");
	return JS_FALSE;
    }
    cx->interpLevel++;
    *result = JSVAL_VOID;
    rt = cx->runtime;

    /*
     * Set registerized frame pointer and derived pointers.
     */
    fp = cx->fp;
    script = fp->script;
    obj = fp->scopeChain;

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
    onbranch = cx->branchCallback;
    ok = JS_TRUE;
#define CHECK_BRANCH(len) {                                                   \
    if (len < 0 && onbranch) {                                                \
	SAVE_SP(fp);                                                          \
	if (!(ok = (*onbranch)(cx, script)))                                  \
	    goto out;                                                         \
    }                                                                         \
}

    /*
     * Allocate operand and pc stack slots for the script's worst-case depth.
     */
    depth = (ptrdiff_t)script->depth;
    newsp = js_AllocStack(cx, (uintN)(2 * depth), &mark);
    if (!newsp) {
	ok = JS_FALSE;
	goto out;
    }
    newsp += depth;
    fp->sp = sp = newsp;

    pc = script->code;
    endpc = pc + script->length;

    while (pc < endpc) {
	fp->pc = pc;
	op = (JSOp)*pc;
      do_op:
	cs = &js_CodeSpec[op];
	len = cs->length;

#ifdef DEBUG
	tracefp = cx->tracefp;
	if (tracefp) {
	    intN nuses, n;

	    fprintf(tracefp, "%4u: ", js_PCToLineNumber(script, pc));
	    js_Disassemble1(cx, script, pc, pc - script->code, JS_FALSE,
			    tracefp);
	    nuses = cs->nuses;
	    if (nuses) {
		fp->sp = sp - nuses;
		for (n = 0; n < nuses; n++) {
		    str = js_DecompileValueGenerator(cx, *fp->sp, NULL);
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

	if (rt->interruptHandler) {
	    JSTrapHandler handler = (JSTrapHandler) rt->interruptHandler;

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
	      default:;
	    }
	}

	switch (op) {
	  case JSOP_NOP:
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
	    PR_ASSERT(JSVAL_IS_OBJECT(rval));
	    withobj = JSVAL_TO_OBJECT(rval);
	    PR_ASSERT(OBJ_GET_CLASS(cx, withobj) == &js_WithClass);

	    rval = OBJ_GET_SLOT(cx, withobj, JSSLOT_PARENT);
	    PR_ASSERT(JSVAL_IS_OBJECT(rval));
	    fp->scopeChain = JSVAL_TO_OBJECT(rval);
	    break;

	  case JSOP_RETURN:
	    CHECK_BRANCH(-1);
	    fp->rval = POP();
	    goto out;

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
	    rval = POP();
	    SAVE_SP(fp);
	    ok = js_ValueToObject(cx, rval, &obj);
	    if (!ok)
		goto out;
	    PUSH(OBJECT_TO_JSVAL(obj));
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

	  case JSOP_IN:
	    rval = POP();
	    VALUE_TO_OBJECT(cx, rval, obj);
	    POP_ELEMENT_ID(id);
	    ok = OBJ_LOOKUP_PROPERTY(cx, obj, id, &obj2, &prop);
	    if (!ok)
		goto out;
	    PUSH_OPND(BOOLEAN_TO_JSVAL(prop != NULL));
	    break;

	  case JSOP_FORNAME:
	    rval = POP();
	    /* FALL THROUGH */
	  case JSOP_FORNAME2:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;

	    SAVE_SP(fp);
	    ok = js_FindVariable(cx, id, &obj, &obj2, &prop);
	    if (!ok)
		goto out;
	    PR_ASSERT(prop);
	    OBJ_DROP_PROPERTY(cx, obj2, prop);
	    lval = OBJECT_TO_JSVAL(obj);
	    goto do_forinloop;

	  case JSOP_FORPROP:
	    rval = POP();
	    /* FALL THROUGH */
	  case JSOP_FORPROP2:
	    lval = POP();
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;
	    goto do_forinloop;

	  case JSOP_FORELEM:
	    rval = POP();
	    /* FALL THROUGH */
	  case JSOP_FORELEM2:
	    POP_ELEMENT_ID(id);
	    lval = POP();

	  do_forinloop:
	    /*
             * ECMA-compatible for/in bytecodes eval object just once, before loop.
             */
	    if (cs->format & JOF_FOR2) {
		obj = JSVAL_TO_OBJECT(sp[-1]);
		vp = sp - 2;
	    } else {
                /* Old-style (pre-ECMA) JSOPs */
		SAVE_SP(fp);
		ok = js_ValueToObject(cx, rval, &obj);
		if (!ok)
		    goto out;
		vp = sp - 1;
	    }

	    /* If the thing to the right of 'in' has no properties, break. */
	    if (!obj) {
		rval = JSVAL_FALSE;
		goto end_forinloop;
	    }

	    /*
             * Save the thing to the right of 'in' as origobj.  Later on we use
             * this variable to suppress enumeration of shadowed prototype
             * properties.
             */
	    origobj = obj;

	    /*
             * Reach under the top of stack to find our property iterator, a
             * JSObject that contains the iteration state.  (An object is used
             * rather than a native struct so that the iteration state is
             * cleaned up via GC if the for-in loop terminates abruptly.)
             */
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
		 * Rewrite the iterator so we know to do the next case.
		 * Do this before calling the enumerator, which could
		 * displace cx->newborn and cause GC.
		 */
		*vp = OBJECT_TO_JSVAL(propobj);

                ok = OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, 0);
		propobj->slots[JSSLOT_ITR_STATE] = iter_state;
		if (!ok)
                    goto out;

		/* 
                 * Stash private iteration state into property iterator object.
                 * NB: This code knows that the first slots are pre-allocated.
                 */
		PR_ASSERT(JS_INITIAL_NSLOTS >= 5);
		propobj->slots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(obj);

	    } else {
		/* This is not the first iteration. Recover iterator state. */
		propobj = JSVAL_TO_OBJECT(rval);
                obj = JSVAL_TO_OBJECT(propobj->slots[JSSLOT_PARENT]);
                iter_state = propobj->slots[JSSLOT_ITR_STATE];
	    }

          enum_next_property:

            /* Get the next jsid to be enumerated and store it in rval */
            OBJ_ENUMERATE(cx, obj, JSENUMERATE_NEXT, &iter_state, &rval);
            propobj->slots[JSSLOT_ITR_STATE] = iter_state;

            /* No more jsids to iterate in obj ? */
            if (iter_state == JSVAL_NULL) {

                /* Enumerate the properties on obj's prototype chain */
		obj = OBJ_GET_PROTO(cx, obj);
		if (!obj) {
		    /* End of property list -- terminate loop. */
		    rval = JSVAL_FALSE;
		    goto end_forinloop;
		}

                ok = OBJ_ENUMERATE(cx, obj, JSENUMERATE_INIT, &iter_state, 0);
		propobj->slots[JSSLOT_ITR_STATE] = iter_state;
		if (!ok)
                    goto out;

		/* Stash private iteration state into iterator JSObject. */
		propobj->slots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(obj);
                goto enum_next_property;
            }

	    /* Skip deleted and shadowed properties, leave next id in rval. */
            if (obj != origobj) {

                /* Have we already enumerated a clone of this property? */
		ok = OBJ_LOOKUP_PROPERTY(cx, origobj, rval, &obj2, &prop);
		if (!ok)
		    goto out;
		if (prop) {
		    OBJ_DROP_PROPERTY(cx, obj2, prop);

                    /* Yes, don't enumerate again.  Go to the next property */
		    if (obj2 != obj)
                        goto enum_next_property;
                }
	    }

	    /* Convert lval to a non-null object containing id. */
	    VALUE_TO_OBJECT(cx, lval, obj);

	    /* Make sure rval is a string for uniformity and compatibility. */
	    if (!JSVAL_IS_INT(rval)) {
		rval = ATOM_KEY((JSAtom *)rval);
	    } else if (cx->version < JSVERSION_1_2) {
		str = js_NumberToString(cx, (jsdouble) JSVAL_TO_INT(rval));
		if (!str) {
		    ok = JS_FALSE;
		    goto out;
		}

		/* Hold via sp[0] in case the GC runs under OBJ_SET_PROPERTY. */
		rval = sp[0] = STRING_TO_JSVAL(str);
	    }

	    /* Set the variable obj[id] to refer to rval. */
	    ok = OBJ_SET_PROPERTY(cx, obj, id, &rval);
	    if (!ok)
		goto out;

	    /* Push true to keep looping through properties. */
	    rval = JSVAL_TRUE;

	  end_forinloop:
	    PUSH_OPND(rval);
	    break;

	  case JSOP_DUP:
	    PR_ASSERT(sp > newsp);
	    rval = sp[-1];
	    PUSH_OPND(rval);
	    break;

	  case JSOP_DUP2:
	    PR_ASSERT(sp - 1 > newsp);
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
    PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);                   \
    if (PROP_FOUND(prop)) {                                                   \
	sprop = (JSScopeProperty *)prop;                                      \
	slot = (uintN)sprop->slot;                                            \
	rval = OBJ_GET_SLOT(cx, obj, slot);                                   \
	ok = SPROP_GET(cx, sprop, obj, obj, &rval);                           \
	if (ok)                                                               \
	    OBJ_SET_SLOT(cx, obj, slot, rval);                                \
    } else {                                                                  \
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
    PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);                   \
    if (PROP_FOUND(prop) &&                                                   \
	!(sprop = (JSScopeProperty *)prop,                                    \
	  sprop->attrs & (JSPROP_READONLY | JSPROP_ASSIGNHACK))) {            \
	ok = SPROP_SET(cx, sprop, obj, obj, &rval);                           \
	if (ok) {                                                             \
	    SET_ENUMERATE_ATTR(sprop);                                        \
	    OBJ_SET_SLOT(cx, obj, sprop->slot, rval);                         \
	}                                                                     \
    } else {                                                                  \
	SAVE_SP(fp);                                                          \
	ok = call;                                                            \
	/* No fill here: js_SetProperty writes through the cache. */          \
    }                                                                         \
}

	  case JSOP_SETNAME:
	    /* Get an immediate atom naming the variable to set. */
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;

	    SAVE_SP(fp);
	    ok = js_FindVariable(cx, id, &obj, &obj2, &prop);
	    if (!ok)
		goto out;
	    PR_ASSERT(prop);

	    OBJ_DROP_PROPERTY(cx, obj2, prop);

	    /* Pop the right-hand side and set obj[id] to it. */
	    rval = POP();

	    /* Try to hit the property cache, FindVariable primes it. */
	    CACHED_SET(OBJ_SET_PROPERTY(cx, obj, id, &rval));
	    if (!ok)
		goto out;

	    /* Push the right-hand side as our result. */
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

	  case JSOP_SETNAME2:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;
	    rval = POP();
	    lval = POP();
	    PR_ASSERT(!JSVAL_IS_PRIMITIVE(lval));
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

#define BITWISE_OP(OP)		INTEGER_OP(OP, (void) 0;)
#define SIGNED_SHIFT_OP(OP)	INTEGER_OP(OP, j &= 31;)

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
            i = ~i;
            PUSH_NUMBER(cx, i);
	    break;

	  case JSOP_NEG:
	    POP_NUMBER(cx, d);
	    d = -d;
	    PUSH_NUMBER(cx, d);
	    break;

	  case JSOP_POS:
	    POP_NUMBER(cx, d);
	    PUSH_NUMBER(cx, d);
	    break;

	  case JSOP_NEW:
	    /* Get immediate argc and find the constructor function. */
	    argc = GET_ARGC(pc);

	  do_new:
	    vp = sp - (2 + argc);
	    PR_ASSERT(vp >= newsp);

	    lval = *vp;
	    if (!JSVAL_IS_OBJECT(lval) ||
		(obj2 = JSVAL_TO_OBJECT(lval)) == NULL ||
		!obj2->map->ops->construct) {
		fun = js_ValueToFunction(cx, vp, JS_TRUE);
		if (!fun) {
		    ok = JS_FALSE;
		    goto out;
		}
		obj2 = fun->object;
	    }

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
	    }

	    /* If there is no class prototype, use js_ObjectClass. */
	    if (!proto)
		obj = js_NewObject(cx, &js_ObjectClass, NULL, parent);
	    else
		obj = js_NewObject(cx, OBJ_GET_CLASS(cx, proto), proto, parent);
	    if (!obj) {
		ok = JS_FALSE;
		goto out;
	    }

	    /* Now we have an object with a constructor method; call it. */
	    vp[1] = OBJECT_TO_JSVAL(obj);
	    SAVE_SP(fp);
	    ok = js_Invoke(cx, argc, JS_TRUE);
	    RESTORE_SP(fp);
	    if (!ok) {
		cx->newborn[GCX_OBJECT] = NULL;
#if JS_HAS_EXCEPTIONS
		if (fp->exceptPending) {
		    /* throw expects to have the exception on the stack */
		    sp[-1] = fp->exception;
		    goto throw;
		}
#endif
		goto out;
	    }

	    /* Check the return value and update obj from it. */
	    rval = *vp;
	    if (JSVAL_IS_PRIMITIVE(rval)) {
		if (fun || !JSVERSION_IS_ECMA(cx->version)) {
		    /* Pre-ECMA code would coerce result to obj. */
		    *vp = OBJECT_TO_JSVAL(obj);
		    break;
		}
		str = js_ValueToString(cx, rval);
		if (str) {
		    JS_ReportError(cx, "invalid new expression result %s",
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
	    PR_ASSERT(slot < fp->fun->nargs);
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
	    PR_ASSERT(slot < fp->fun->nvars);
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

	  case JSOP_PUSHOBJ:
	    PUSH_OPND(OBJECT_TO_JSVAL(obj));
	    break;

	  case JSOP_CALL:
	    argc = GET_ARGC(pc);
	    SAVE_SP(fp);
	    ok = js_Invoke(cx, argc, JS_FALSE);
	    RESTORE_SP(fp);
	    if (!ok) {
#if JS_HAS_EXCEPTIONS
		if (fp->exceptPending) {
		    /* throw expects to have the exception on the stack */
		    sp[-1] = fp->exception;
		    goto throw;
		}
#endif
		goto out;
	    }
	    obj = NULL;
	    break;

	  case JSOP_NAME:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;

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
		    if (op2 != JSOP_NOP)
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
	    PR_ASSERT(JSVAL_IS_OBJECT(rval));
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
	    SAVE_SP(fp);
	    ok = PopInt(cx, &i);
	    RESTORE_SP(fp);
	    if (!ok)
		goto out;

	    pc2 = pc;
	    len = GET_JUMP_OFFSET(pc2);

	    pc2 += 2;
	    low = GET_JUMP_OFFSET(pc2);
	    pc2 += 2;
	    high = GET_JUMP_OFFSET(pc2);

	    i -= low;
	    if ((jsuint)i <= (jsuint)(high - low)) {
		pc2 += 2 + 2 * i;
		off = GET_JUMP_OFFSET(pc2);
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

	    pc2 += 2;
	    npairs = GET_ATOM_INDEX(pc2);
	    pc2 += 2;

#define SEARCH_PAIRS(MATCH_CODE)                                              \
    while (npairs) {                                                          \
	atom = GET_ATOM(cx, script, pc2);                                     \
	rval = ATOM_KEY(atom);                                                \
	MATCH_CODE                                                            \
	if (match) {                                                          \
	    pc2 += 2;                                                         \
	    len = GET_JUMP_OFFSET(pc2);                                       \
	    goto advance_pc;                                                  \
	}                                                                     \
	pc2 += 4;                                                             \
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
#endif /* JS_HAS_SWITCH_STATEMENT */

#if JS_HAS_LEXICAL_CLOSURE
	  case JSOP_CLOSURE:
	    /*
	     * If the nearest variable scope is a function, not a call object,
	     * replace it in the scope chain with its call object.
	     */
	    obj = js_FindVariableScope(cx, &fun);
	    if (!obj) {
		ok = JS_FALSE;
		goto out;
	    }

	    /*
	     * If in a with statement, set obj to the With object's prototype,
	     * i.e., the object specified in the with statement head.
	     */
	    if (fp->scopeChain != obj) {
		obj = fp->scopeChain;
		PR_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_WithClass);
#if JS_BUG_WITH_CLOSURE
		while (OBJ_GET_CLASS(cx, obj) == &js_WithClass) {
		    proto = OBJ_GET_PROTO(cx, obj);
		    if (!proto)
			break;
		    obj = proto;
		}
#endif
	    }

	    /*
	     * Get immediate operand atom, which is a function object literal.
	     * From it, get the function to close.
	     */
	    atom = GET_ATOM(cx, script, pc);
	    PR_ASSERT(ATOM_IS_OBJECT(atom));
	    obj2 = ATOM_TO_OBJECT(atom);
	    fun2 = JS_GetPrivate(cx, obj2);

	    /*
	     * Let closure = new Closure(obj2).
	     * NB: js_ConstructObject does not use the "constructor" property
	     * of the new object it creates, because in this case and others
	     * such as js_WithClass, that property refers to the prototype's
	     * constructor function.
	     */
	    closure = js_ConstructObject(cx, &js_ClosureClass, obj2,
					 fp->scopeChain);
	    if (!closure) {
		ok = JS_FALSE;
		goto out;
	    }

	    /*
	     * Define a property in obj with id fun2->atom and value closure,
	     * but only if fun2 is not anonymous.
	     */
	    if (fun2->atom) {
		ok = OBJ_DEFINE_PROPERTY(cx, obj, (jsid)fun2->atom,
					 OBJECT_TO_JSVAL(closure),
					 NULL, NULL, JSPROP_ENUMERATE,
					 NULL);
		if (!ok) {
		    cx->newborn[GCX_OBJECT] = NULL;
		    goto out;
		}
	    }
	    PUSH_OPND(OBJECT_TO_JSVAL(closure));
	    break;
#endif /* JS_HAS_LEXICAL_CLOSURE */

#if JS_HAS_EXPORT_IMPORT
	  case JSOP_EXPORTALL:
	    obj = js_FindVariableScope(cx, &fun);
	    if (!obj) {
		ok = JS_FALSE;
	    } else {
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
	    }
	    break;

	  case JSOP_EXPORTNAME:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;
	    obj  = js_FindVariableScope(cx, &fun);
	    if (!obj) {
		ok = JS_FALSE;
		goto out;
	    }
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
		PR_ASSERT(JSVAL_IS_INT(rval));
		op = (JSOp) JSVAL_TO_INT(rval);
		PR_ASSERT((uintN)op < (uintN)JSOP_LIMIT);
		goto do_op;
	      case JSTRAP_RETURN:
		fp->rval = rval;
		goto out;
	      default:;
	    }
	    break;

	  case JSOP_GETARG:
	    obj = NULL;
	    slot = (uintN)GET_ARGNO(pc);
	    PR_ASSERT(slot < fp->fun->nargs);
	    PUSH_OPND(fp->argv[slot]);
	    break;

	  case JSOP_SETARG:
	    obj = NULL;
	    slot = (uintN)GET_ARGNO(pc);
	    PR_ASSERT(slot < fp->fun->nargs);
	    vp = &fp->argv[slot];
	    GC_POKE(cx, *vp);
	    *vp = sp[-1];
	    break;

	  case JSOP_GETVAR:
	    obj = NULL;
	    slot = (uintN)GET_VARNO(pc);
	    PR_ASSERT(slot < fp->fun->nvars);
	    PUSH_OPND(fp->vars[slot]);
	    break;

	  case JSOP_SETVAR:
	    obj = NULL;
	    slot = (uintN)GET_VARNO(pc);
	    PR_ASSERT(slot < fp->fun->nvars);
	    vp = &fp->vars[slot];
	    GC_POKE(cx, *vp);
	    *vp = sp[-1];
	    break;

#ifdef JS_HAS_INITIALIZERS
	  case JSOP_NEWINIT:
	    argc = 0;
	    fp->sharpDepth++;
	    goto do_new;

	  case JSOP_ENDINIT:
	    if (--fp->sharpDepth == 0)
		fp->sharpArray = NULL;
	    break;

	  case JSOP_INITPROP:
	    /* Pop the property's value into rval. */
	    PR_ASSERT(sp - newsp >= 2);
	    rval = POP();

	    /* Get the immediate property name into id. */
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsid)atom;
	    goto do_init;

	  case JSOP_INITELEM:
	    /* Pop the element's value into rval. */
	    PR_ASSERT(sp - newsp >= 3);
	    rval = POP();

	    /* Pop and conditionally atomize the element id. */
	    POP_ELEMENT_ID(id);

	  do_init:
	    /* Find the object being initialized at top of stack. */
	    lval = sp[-1];
	    PR_ASSERT(JSVAL_IS_OBJECT(lval));
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
		JS_ReportError(cx, "invalid sharp variable definition #%u=",
			       (unsigned) i);
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
		JS_ReportError(cx, "invalid sharp variable use #%u#",
			       (unsigned) i);
		ok = JS_FALSE;
		goto out;
	    }
	    PUSH_OPND(rval);
	    break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_INITIALIZERS */

#if JS_HAS_EXCEPTIONS
	  case JSOP_THROW:
	    /*
	     * We enter here with an exception on the stack already, so we
	     * need to pop it off if we're leaving this frame, but we leave
	     * it on if we're jumping to a try section.
	     */
	    PR_ASSERT(!fp->exceptPending);
	  throw:
	    tn = script->trynotes;
	    offset = PTRDIFF(pc, script->code, jsbytecode);
	    if (tn) {
		for (;
		     tn->start && tn->end && (tn->start > offset || tn->end < offset);
		     tn++)
		    /* nop */
		    ;
		if (tn->catch) {
		    pc = script->code + tn->catch;
		    len = 0;
		    goto advance_pc;
		}
	    }

	    /* Not in a catch block, so propagate the exception. */
	    fp->exceptPending = JS_TRUE;
	    fp->exception = POP();
	    ok = JS_FALSE;
	    goto out;
#endif /* JS_HAS_EXCEPTIONS */

#if JS_HAS_INSTANCEOF
	  case JSOP_INSTANCEOF:
	    rval = POP();
	    if (JSVAL_IS_PRIMITIVE(rval)) {
		str = js_DecompileValueGenerator(cx, rval, NULL);
		if (str) {
		    JS_ReportError(cx, "invalid %s operand %s",
				   js_instanceof_str, JS_GetStringBytes(str));
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

	  default:
	    JS_ReportError(cx, "unimplemented JavaScript bytecode %d", op);
	    ok = JS_FALSE;
	    goto out;
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
		    str = js_DecompileValueGenerator(cx, *fp->sp, NULL);
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
    /*
     * Restore the previous frame's execution state.
     */
    js_FreeStack(cx, mark);
    cx->interpLevel--;
    return ok;
}
