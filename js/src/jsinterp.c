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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "prtypes.h"
#include "prlog.h"
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
    JSPropertyCacheEntry *end, *pce;

    cache = &cx->runtime->propertyCache;
    if (cache->empty)
	return;

    end = &cache->table[PROPERTY_CACHE_SIZE];
    for (pce = &cache->table[0]; pce < end; pce++) {
	if (pce->property) {
	    pce->property = NULL;
	    if (!JSVAL_IS_INT(pce->symbolid))
		js_DropAtom(cx, (JSAtom *)pce->symbolid);
	    pce->symbolid = JSVAL_NULL;
	}
    }
    cache->empty = JS_TRUE;
    cache->flushes++;
}

void
js_FlushPropertyCacheByProp(JSContext *cx, JSProperty *prop)
{
    JSPropertyCache *cache;
    JSBool empty;
    JSPropertyCacheEntry *end, *pce;

    cache = &cx->runtime->propertyCache;
    if (cache->empty)
	return;

    empty = JS_TRUE;
    end = &cache->table[PROPERTY_CACHE_SIZE];
    for (pce = &cache->table[0]; pce < end; pce++) {
	if (pce->property) {
	    if (pce->property == prop) {
		pce->property = NULL;
		if (!JSVAL_IS_INT(pce->symbolid))
		    js_DropAtom(cx, (JSAtom *)pce->symbolid);
		pce->symbolid = JSVAL_NULL;
	    } else {
		empty = JS_FALSE;
	    }
	}
    }
    cache->empty = empty;
    cache->pflushes++;
}

/*
 * Class for for/in loop property iterator objects.
 */
#define JSSLOT_PROP_OBJECT  (JSSLOT_START)
#define JSSLOT_PROP_NEXT    (JSSLOT_START+1)

static void
prop_iterator_finalize(JSContext *cx, JSObject *obj)
{
    jsval pval;
    JSProperty *prop;

    pval = OBJ_GET_SLOT(obj, JSSLOT_PROP_NEXT);
    prop = JSVAL_TO_PRIVATE(pval);
    if (prop)
	js_DropProperty(cx, prop);
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
 * This POP variant is called only for bitwise operators, so we don't bother
 * to inline it.  The calls in Interpret must therefore SAVE_SP first!
 */
static JSBool
PopInt(JSContext *cx, jsint *ip, JSBool *validp)
{
    JSStackFrame *fp;
    jsval *sp, v;
    jsdouble d;
    jsint i;

    fp = cx->fp;
    RESTORE_SP(fp);
    v = POP();
    SAVE_SP(fp);
    if (JSVAL_IS_INT(v)) {
	*ip = JSVAL_TO_INT(v);
	return JS_TRUE;
    }
    if (!JS_ValueToNumber(cx, v, &d))
	return JS_FALSE;
    i = (jsint)d;
    *ip = i;
#ifdef XP_PC
    if (JSDOUBLE_IS_NaN(d)) {
	*validp = JS_FALSE;
	return JS_TRUE;
    }
#endif
    *validp &= ((jsdouble)i == d || (jsdouble)(jsuint)i == d);
    return JS_TRUE;
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
	    JS_LOCK_RUNTIME_VOID(rt, ok = js_ValueToNumber(cx, v, &d));       \
	    if (!ok)                                                          \
		goto out;                                                     \
	}                                                                     \
    PR_END_MACRO

#define VALUE_TO_BOOLEAN(cx, v, b)                                            \
    PR_BEGIN_MACRO                                                            \
	if (v == JSVAL_NULL) {                                                \
	    b = JS_FALSE;                                                     \
	} else if (JSVAL_IS_BOOLEAN(v)) {                                     \
	    b = JSVAL_TO_BOOLEAN(v);                                          \
	} else {                                                              \
	    SAVE_SP(fp);                                                      \
	    JS_LOCK_RUNTIME_VOID(rt, ok = js_ValueToBoolean(cx, v, &b));      \
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
	    JS_LOCK_RUNTIME_VOID(rt, obj = js_ValueToNonNullObject(cx, v));   \
	    if (!obj) {                                                       \
		ok = JS_FALSE;                                                \
		goto out;                                                     \
	    }                                                                 \
	}                                                                     \
    PR_END_MACRO

#if JS_BUG_VOID_TOSTRING
#define CHECK_VOID_TOSTRING(cx, v, vp)                                        \
    if (JSVAL_IS_VOID(v)) {                                                   \
	_str = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]); \
	*vp = STRING_TO_JSVAL(_str);                                          \
    }
#else
#define CHECK_VOID_TOSTRING(cx, v, vp)  /* nothing */
#endif

#if JS_BUG_EAGER_TOSTRING
#define TRY_VALUE_OF(cx, v, vp, TOSTRING_CODE) TOSTRING_CODE
#else
#define TRY_VALUE_OF(cx, v, vp, TOSTRING_CODE)                                \
    js_TryValueOf(cx, JSVAL_TO_OBJECT(v), JSTYPE_VOID, vp);                   \
    v = *vp;                                                                  \
    if (JSVAL_IS_OBJECT(v) && v != JSVAL_NULL) {                              \
        TOSTRING_CODE                                                         \
    }
#endif

#define VALUE_TO_PRIMITIVE(cx, v, vp)                                         \
    PR_BEGIN_MACRO                                                            \
	JSString *_str;                                                       \
                                                                              \
	if (!JSVAL_IS_OBJECT(v) || v == JSVAL_NULL) {                         \
	    CHECK_VOID_TOSTRING(cx, v, vp);                                   \
	} else {                                                              \
	    SAVE_SP(cx->fp);                                                  \
	    TRY_VALUE_OF(cx, v, vp,                                           \
		JS_LOCK_VOID(cx,                                              \
		    _str = js_ObjectToString(cx, JSVAL_TO_OBJECT(v)));        \
		if (!_str) {                                                  \
		    ok = JS_FALSE;                                            \
		    goto out;                                                 \
		}                                                             \
		*vp = STRING_TO_JSVAL(_str);                                  \
	    )                                                                 \
	}                                                                     \
    PR_END_MACRO

static jsval *
AllocStack(JSContext *cx, uintN nslots)
{
    jsval *sp;

    PR_ARENA_ALLOCATE(sp, &cx->stackPool, nslots * sizeof(jsval));
    if (!sp) {
	JS_ReportError(cx, "stack overflow in %s",
		       (cx->fp && cx->fp->fun)
		       ? JS_GetFunctionName(cx->fp->fun)
		       : "script");
    }
    return sp;
}

JSBool
js_GetArgument(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;

    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun->object == obj) {
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
    JSStackFrame *fp;

    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun->object == obj) {
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
    JSStackFrame *fp;
    jsint slot;

    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun->object == obj) {
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
    JSStackFrame *fp;
    jsint slot;

    for (fp = cx->fp; fp; fp = fp->down) {
	/* Find most recent non-native function frame. */
	if (fp->fun && !fp->fun->call) {
	    if (fp->fun->object == obj) {
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

static JSBool
Interpret(JSContext *cx, jsval *result);

/*
 * Find a function reference and its 'this' object implicit first parameter
 * under argc arguments on cx's stack, and call the function.  Push missing
 * required arguments, allocate declared local variables, and pop everything
 * when done.  Then push the return value.
 */
JSBool
js_DoCall(JSContext *cx, uintN argc)
{
    JSStackFrame *fp, frame;
    jsval *sp, *newsp;
    jsval *vp, aval;
    JSObject *closure, *funobj, *parent, *thisp;
    JSFunction *fun;
    void *mark;
    intN nslots, nalloc, surplus;
    JSBool ok;

    /* Reach under our args to find the function ref on the stack. */
    fp = cx->fp;
    sp = fp->sp;
    vp = sp - (2 + argc);

    /* Reset fp->sp so error reports decompile *vp's generator. */
    fp->sp = vp;
    closure = JSVAL_TO_OBJECT(*vp);
    fun = JS_ValueToFunction(cx, *vp);
    fp->sp = sp;
    if (!fun)
	return JS_FALSE;

    /* Get the function's parent object in case it's bound or orphaned. */
    funobj = fun->object;
    parent = OBJ_GET_PARENT(funobj);
    thisp = JSVAL_TO_OBJECT(vp[1]);
    if (fun->flags & (JSFUN_BOUND_METHOD | JSFUN_GLOBAL_PARENT)) {
	/* Combine the above to optimize the common case (neither bit set). */
	if (fun->flags & JSFUN_BOUND_METHOD)
	    thisp = parent;
	else
	    parent = 0;
    }

    /* Make vp refer to funobj to keep it available as argv[-2]. */
    *vp = OBJECT_TO_JSVAL(funobj);

    /* Initialize a stack frame, except for thisp and scopeChain. */
    frame.object = NULL;
    frame.script = fun->script;
    frame.fun = fun;
    frame.argc = argc;
    frame.argv = sp - argc;
    frame.rval = JSVAL_VOID;
    frame.nvars = fun->nvars;
    frame.vars = sp;
    frame.down = fp;
    frame.annotation = NULL;
    frame.pc = NULL;
    frame.sp = sp;
#if JS_HAS_SHARP_VARS
    frame.sharpDepth = 0;
    frame.sharpArray = NULL;
#endif

#if JS_HAS_LEXICAL_CLOSURE
    /* If calling a closure, set funobj to the call object eagerly. */
    if (closure->map->clasp == &js_ClosureClass) {
	frame.scopeChain = NULL;
	parent = OBJ_GET_PARENT(closure);
	funobj = js_GetCallObject(cx, &frame, parent);
	if (!funobj)
	    return JS_FALSE;
	thisp = parent;
#if JS_BUG_WITH_CLOSURE
	while (thisp->map->clasp == &js_WithClass) {
	    JSObject *proto = OBJ_GET_PROTO(thisp);
	    if (!proto)
		break;
	    thisp = proto;
	}
#endif
    }
#endif

    /* Now set frame.thisp, so that the closure special case can reset it. */
    frame.thisp = thisp;

    /* From here on, control must flow through label out: to return. */
    JS_LOCK_VOID(cx, cx->fp = &frame);
    mark = PR_ARENA_MARK(&cx->stackPool);

    /* Check for missing arguments expected by the function. */
    nslots = (intN)((argc < fun->nargs) ? fun->nargs - argc : 0);
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
	    newsp = AllocStack(cx, (uintN)nalloc);
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
	    newsp = AllocStack(cx, (uintN)nslots);
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
    if (fun->call) {
	frame.scopeChain = fp->scopeChain;
	ok = fun->call(cx, thisp, argc, frame.argv, &frame.rval);
    } else if (fun->script) {
	frame.scopeChain = funobj;
	if (!parent) {
#if JS_HAS_CALL_OBJECT
	    /* Scope with a call object parented by cx's global object. */
	    funobj = js_GetCallObject(cx, &frame, cx->globalObject);
	    if (!funobj) {
		ok = JS_FALSE;
		goto out;
	    }
#else
	    /* Bad old code slams globalObject directly into funobj. */
	    OBJ_SET_PARENT(funobj, cx->globalObject);
#endif
	}
	ok = Interpret(cx, &aval);
#if !JS_HAS_CALL_OBJECT
	if (!parent)
	    OBJ_SET_PARENT(funobj, NULL);
#endif
    } else {
	/* fun might be onerror trying to report a syntax error in itself. */
	frame.scopeChain = NULL;
	ok = JS_TRUE;
    }

out:
    /*
     * XXX - Checking frame.annotation limits the use of the hook for
     * uses other than releasing annotations, but avoids one C function
     * call for every JS function call.
     */
    if (frame.annotation &&
	js_InterpreterHooks &&
	js_InterpreterHooks->destroyFrame) {
        js_InterpreterHooks->destroyFrame(cx, &frame);
    }

#if JS_HAS_CALL_OBJECT
    /* If frame has a call object, clear its back-pointer. */
    if (frame.object)
	ok &= js_PutCallObject(cx, &frame);
#endif

    /* Pop everything off the stack and store the return value. */
    PR_ARENA_RELEASE(&cx->stackPool, mark);
    JS_LOCK_VOID(cx, cx->fp = fp);
    fp->sp = vp + 1;
    *vp = frame.rval;
    return ok;
}

JSBool
js_Call(JSContext *cx, JSObject *obj, jsval fval, uintN argc, jsval *argv,
	jsval *rval)
{
    void *mark;
    JSStackFrame *fp, *oldfp, frame;
    jsval *oldsp, *sp;
    uintN i;
    JSBool ok;

    mark = PR_ARENA_MARK(&cx->stackPool);
    fp = oldfp = cx->fp;
    if (!fp) {
	memset(&frame, 0, sizeof frame);
	JS_LOCK_VOID(cx, cx->fp = fp = &frame);
    }
    oldsp = fp->sp;
    sp = AllocStack(cx, 2 + argc);
    if (!sp)
	return JS_FALSE;
    fp->sp = sp;

    PUSH(fval);
    PUSH(OBJECT_TO_JSVAL(obj));
    for (i = 0; i < argc; i++)
	PUSH(argv[i]);
    SAVE_SP(fp);
    ok = js_DoCall(cx, argc);
    if (ok) {
	RESTORE_SP(fp);
	*rval = POP();
    }

    PR_ARENA_RELEASE(&cx->stackPool, mark);
    fp->sp = oldsp;
    if (oldfp != fp)
	JS_LOCK_VOID(cx, cx->fp = oldfp);
    return ok;
}

JSBool
js_Execute(JSContext *cx, JSObject *chain, JSScript *script, JSStackFrame *down,
	   jsval *result)
{
    JSStackFrame *oldfp, frame;
    JSBool ok;

    oldfp = cx->fp;
    frame.object = NULL;
    frame.script = script;
    frame.fun = NULL;
    frame.thisp = down ? down->thisp : chain;
    frame.argc = frame.nvars = 0;
    frame.argv = frame.vars = NULL;
    frame.rval = JSVAL_VOID;
    frame.down = down;
    frame.annotation = NULL;
    frame.scopeChain = chain;
    frame.pc = NULL;
    frame.sp = oldfp ? oldfp->sp : NULL;
#if JS_HAS_SHARP_VARS
    frame.sharpDepth = 0;
    frame.sharpArray = down ? down->sharpArray : NULL;
#endif
    JS_LOCK_VOID(cx, cx->fp = &frame);
    ok = Interpret(cx, result);
    JS_LOCK_VOID(cx, cx->fp = oldfp);
    return ok;
}

#if JS_HAS_EXPORT_IMPORT
/*
 * If id is JSVAL_VOID, import all exported properties from obj.
 */
static JSBool
ImportProperty(JSContext *cx, JSObject *obj, jsval id)
{
    JSBool all;
    JSProperty *prop, *prop2;
    JSString *str;
    JSObject *source, *target, *funobj, *closure;
    JSFunction *fun;
    jsval value;

    PR_ASSERT(JS_IS_LOCKED(cx));

    all = JSVAL_IS_VOID(id);
    if (all) {
	if (!obj->map->clasp->enumerate(cx, obj))
	    return JS_FALSE;
	prop = obj->map->props;
	if (!prop)
	    return JS_TRUE;
    } else {
	if (!js_LookupProperty(cx, obj, id, &obj, &prop))
	    return JS_FALSE;
	if (!prop) {
	    str = js_ValueToSource(cx, js_IdToValue(id));
	    if (str)
		js_ReportIsNotDefined(cx, JS_GetStringBytes(str));
	    return JS_FALSE;
	}
	if (!(prop->flags & JSPROP_EXPORTED)) {
	    str = js_ValueToSource(cx, js_IdToValue(id));
	    if (str) {
		JS_ReportError(cx, "%s is not exported",
			       JS_GetStringBytes(str));
	    }
	    return JS_FALSE;
	}
    }

    source = prop->object;
    target = js_FindVariableScope(cx, &fun);
    if (!target)
	return JS_FALSE;
    do {
	if (!(prop->flags & JSPROP_EXPORTED))
	    continue;
	value = source->slots[prop->slot];
	if (JSVAL_IS_FUNCTION(value)) {
	    funobj = JSVAL_TO_OBJECT(value);
	    closure = js_ConstructObject(cx, &js_ClosureClass, funobj, obj);
	    if (!closure)
		return JS_FALSE;
	    value = OBJECT_TO_JSVAL(closure);
	}
	prop2 = js_DefineProperty(cx, target,
				  all ? sym_id(prop->symbols) : id,
				  value, NULL, NULL,
				  prop->flags & ~JSPROP_EXPORTED);
	if (!prop2)
	    return JS_FALSE;
    } while (all && (prop = prop->next) != NULL);
    return JS_TRUE;
}
#endif /* JS_HAS_EXPORT_IMPORT */

#if !defined XP_PC || !defined _MSC_VER || _MSC_VER > 800
#define MAX_INTERP_LEVEL 1000
#else
#define MAX_INTERP_LEVEL 30
#endif

static JSBool
Interpret(JSContext *cx, jsval *result)
{
    JSRuntime *rt;
    JSStackFrame *fp;
    JSScript *script;
    jsbytecode *pc, *pc2, *endpc;
    JSBranchCallback onbranch;
    JSBool ok, dropAtom, cond, valid;
    void *mark;
    jsval *sp, *newsp;
    ptrdiff_t depth, len;
    uintN argc, slot;
    JSOp op, op2;
    JSCodeSpec *cs;
    JSAtom *atom;
    jsval *vp, lval, rval, ltmp, rtmp, id;
    JSObject *obj, *obj2, *proto, *parent;
    JSObject *withobj;
    JSObject *origobj, *propobj, *iterobj;
    JSProperty *prop, *prop2;
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

    if (cx->interpLevel == MAX_INTERP_LEVEL) {
	JS_ReportError(cx, "too much recursion");
	return JS_FALSE;
    }
    cx->interpLevel++;
    *result = JSVAL_VOID;
    rt = cx->runtime;

    /*
     * Set registerized frame pointer and derived script pointer.
     */
    fp = cx->fp;
    script = fp->script;

    /*
     * Prepare to call a user-supplied branch handler, and abort the script
     * if it returns false.
     */
    onbranch = cx->branchCallback;
    ok = JS_TRUE;
    dropAtom = JS_FALSE;
#define CHECK_BRANCH(len) {                                                   \
    if (len < 0 && onbranch && !(ok = (*onbranch)(cx, script)))               \
	goto out;                                                             \
}

    /*
     * Allocate operand and pc stack slots for the script's worst-case depth.
     */
    mark = PR_ARENA_MARK(&cx->stackPool);
    depth = (ptrdiff_t)script->depth;
    newsp = AllocStack(cx, (uintN)(2 * depth));
    if (!newsp) {
	ok = JS_FALSE;
	goto out;
    }
    newsp += depth;
    fp->sp = sp = newsp;

#ifdef JS_THREADSAFE
    obj = NULL;
    ok = JS_AddRoot(cx, &obj);
    if (!ok)
	goto out;
#endif

    pc = script->code;
    endpc = pc + script->length;

    while (pc < endpc) {
	fp->pc = pc;
	op = *pc;
      do_op:
	cs = &js_CodeSpec[op];
	len = cs->length;

#ifdef DEBUG
	tracefp = cx->tracefp;
	if (tracefp) {
	    intN nuses, n;
	    jsval v;

	    fprintf(tracefp, "%4u: ", js_PCToLineNumber(script, pc));
	    js_Disassemble1(cx, script, pc, pc - script->code, JS_FALSE,
			    tracefp);
	    nuses = cs->nuses;
	    if (nuses) {
		fp->sp = sp - nuses;
		for (n = nuses; n > 0; n--) {
		    v = sp[-n];
		    if ((str = js_ValueToSource(cx, v)) != NULL) {
			fprintf(tracefp, "%s %s",
				(n == nuses) ? "  inputs:" : ",",
				JS_GetStringBytes(str));
		    }
		}
		putc('\n', tracefp);
		SAVE_SP(fp);
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
	    PUSH(OBJECT_TO_JSVAL(withobj));
	    break;

	  case JSOP_LEAVEWITH:
	    rval = POP();
	    PR_ASSERT(JSVAL_IS_OBJECT(rval));
	    withobj = JSVAL_TO_OBJECT(rval);
	    PR_ASSERT(withobj->map->clasp == &js_WithClass);

	    rval = OBJ_GET_SLOT(withobj, JSSLOT_PARENT);
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
	    rval = POP();
	    VALUE_TO_BOOLEAN(cx, rval, cond);
	    if (cond == JS_FALSE) {
		len = GET_JUMP_OFFSET(pc);
		CHECK_BRANCH(len);
	    }
	    break;

	  case JSOP_IFNE:
	    rval = POP();
	    VALUE_TO_BOOLEAN(cx, rval, cond);
	    if (cond != JS_FALSE) {
		len = GET_JUMP_OFFSET(pc);
		CHECK_BRANCH(len);
	    }
	    break;

#if !JS_BUG_SHORT_CIRCUIT
	  case JSOP_OR:
	    rval = POP();
	    VALUE_TO_BOOLEAN(cx, rval, cond);
	    if (cond == JS_TRUE) {
		len = GET_JUMP_OFFSET(pc);
		PUSH_OPND(rval);
	    }
	    break;

	  case JSOP_AND:
	    rval = POP();
	    VALUE_TO_BOOLEAN(cx, rval, cond);
	    if (cond == JS_FALSE) {
		len = GET_JUMP_OFFSET(pc);
		PUSH_OPND(rval);
	    }
	    break;
#endif

	  case JSOP_FORNAME:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    rval = POP();

	    SAVE_SP(fp);
	    JS_LOCK_RUNTIME_VOID(rt, ok = js_FindVariable(cx, id, &obj, &prop));
	    if (!ok)
		goto out;
	    PR_ASSERT(prop);
	    lval = OBJECT_TO_JSVAL(obj);
	    goto do_forinloop;

	  case JSOP_FORPROP:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    rval = POP();
	    lval = POP();
	    goto do_forinloop;

#define FIX_ELEMENT_ID(id) {                                                  \
    /* If the index is other than a nonnegative int, atomize it. */           \
    i = JSVAL_IS_INT(id) ? JSVAL_TO_INT(id) : -1;                             \
    if (i < 0) {                                                              \
	JS_LOCK_RUNTIME_VOID(rt, atom = js_ValueToStringAtom(cx, id));        \
	if (!atom) {                                                          \
	    ok = JS_FALSE;                                                    \
	    goto out;                                                         \
	}                                                                     \
	id = (jsval)atom;                                                     \
	dropAtom = JS_TRUE;                                                   \
    }                                                                         \
}

	  case JSOP_FORELEM:
	    rval = POP();
	    id   = POP();
	    lval = POP();

	    /* If the index is other than a nonnegative int, atomize it. */
	    FIX_ELEMENT_ID(id);

	  do_forinloop:
	    /* Lock the entire bytecode's critical section if threadsafe. */
	    SAVE_SP(fp);
	    JS_LOCK_RUNTIME(rt);
	    ok = js_ValueToObject(cx, rval, &obj);
	    if (!ok) {
		JS_UNLOCK_RUNTIME(rt);
		goto out;
	    }

	    /* If the thing to the right of 'in' has no properties, break. */
	    if (!obj) {
		rval = JSVAL_FALSE;
		goto end_forinloop;
	    }

	    /*
	     * Start from the shared scope object that owns the properties.
	     * This means we won't visit properties added during the loop if
	     * the nominal object was unmutated before the loop.  But call
	     * enumerate first in case it mutates obj.
	     */
	    vp = sp - 1;
	    if (JSVAL_IS_VOID(*vp)) {
		/* Let lazy reflectors be eager so for/in finds everything. */
		ok = obj->map->clasp->enumerate(cx, obj);
		if (!ok) {
		    JS_UNLOCK_RUNTIME(rt);
		    goto out;
		}
	    }
	    obj = ((JSScope *)obj->map)->object;

	    /* Save obj as origobj to suppress prototype properties. */
	    origobj = obj;
	  do_forin_again:
	    proto = JS_GetPrototype(cx, obj);

	    /* Reach under top of stack to find our property iterator. */
	    if (JSVAL_IS_VOID(*vp)) {
		prop = obj->map->props;

		/* Set the iterator to point to the first property. */
		propobj = js_NewObject(cx, &prop_iterator_class, NULL, NULL);
		if (!propobj) {
		    JS_UNLOCK_RUNTIME(rt);
		    ok = JS_FALSE;
		    goto out;
		}
		OBJ_SET_SLOT(propobj, JSSLOT_PROP_OBJECT, OBJECT_TO_JSVAL(obj));
		OBJ_SET_SLOT(propobj, JSSLOT_PROP_NEXT, PRIVATE_TO_JSVAL(NULL));

		/* Rewrite the iterator so we know to do the next case. */
		*vp = OBJECT_TO_JSVAL(propobj);
	    } else {
		/* Use the iterator to find the next property. */
		propobj = JSVAL_TO_OBJECT(*vp);
		rval = OBJ_GET_SLOT(propobj, JSSLOT_PROP_NEXT);
		prop = JSVAL_TO_PRIVATE(rval);

		rval = OBJ_GET_SLOT(propobj, JSSLOT_PROP_OBJECT);
		iterobj = JSVAL_TO_OBJECT(rval);

		/* If we're enumerating a prototype, reset obj and prototype. */
		if (obj != iterobj) {
		    obj = iterobj;
		    proto = JS_GetPrototype(cx, obj);
		}
		PR_ASSERT(!prop || prop->object == obj);
	    }

	    /* Skip deleted and predefined/hidden properties. */
	    while (prop) {
		if (prop->symbols && (prop->flags & JSPROP_ENUMERATE)) {
		    /* Have we already enumerated a clone of this property? */
		    ok = js_LookupProperty(cx, origobj,
					   sym_id(prop->symbols), NULL,
					   &prop2);
		    if (!ok) {
			JS_UNLOCK_RUNTIME(rt);
			goto out;
		    }
		    if (prop2 == prop) {
			/* No, go ahead and enumerate it. */
			break;
		    }
		}
		prop = prop->next;
	    }

	    if (!prop) {
		/* Enumerate prototype properties, if there are any. */
		if (proto) {
		    obj = proto;
		    OBJ_SET_SLOT(propobj, JSSLOT_PROP_OBJECT,
				 OBJECT_TO_JSVAL(obj));
		    rval = OBJ_GET_SLOT(propobj, JSSLOT_PROP_NEXT);
		    prop = JSVAL_TO_PRIVATE(rval);
		    if (prop)
			js_DropProperty(cx, prop);
		    prop = obj->map->props;
		    OBJ_SET_SLOT(propobj, JSSLOT_PROP_NEXT,
				 PRIVATE_TO_JSVAL(prop));
		    js_HoldProperty(cx, prop);
		    goto do_forin_again;
		}

		/* End of property list -- terminate this loop. */
		rval = JSVAL_FALSE;
		goto end_forinloop;
	    }

	    /* Advance the iterator to the next property (or null). */
	    rval = OBJ_GET_SLOT(propobj, JSSLOT_PROP_NEXT);
	    prop2 = JSVAL_TO_PRIVATE(rval);
	    if (prop2)
		js_DropProperty(cx, prop2);
	    prop2 = prop->next;
	    OBJ_SET_SLOT(propobj, JSSLOT_PROP_NEXT, PRIVATE_TO_JSVAL(prop2));
	    if (!ok) {
		JS_UNLOCK_RUNTIME(rt);
		goto out;
	    }
	    js_HoldProperty(cx, prop2);

	    /* Get the int- or string-valued property name into rval. */
	    rval = js_IdToValue(sym_id(prop->symbols));

	    /* Convert lval to a non-null object containing id. */
	    VALUE_TO_OBJECT(cx, lval, obj);

	    /* Make sure rval is a string for uniformity and compatibility. */
	    if (cx->version < JSVERSION_1_2 && JSVAL_IS_INT(rval)) {
		str = js_NumberToString(cx, (jsdouble) JSVAL_TO_INT(rval));
		if (!str) {
		    JS_UNLOCK_RUNTIME(rt);
		    ok = JS_FALSE;
		    goto out;
		}

		/* Hold via sp[0] in case the GC runs under js_SetProperty. */
		rval = sp[0] = STRING_TO_JSVAL(str);
	    }

	    /* Set the variable obj[id] to refer to rval. */
	    prop = js_SetProperty(cx, obj, id, &rval);
	    if (!prop) {
		JS_UNLOCK_RUNTIME(rt);
		ok = JS_FALSE;
		goto out;
	    }

	    /* Push true to keep looping through properties. */
	    rval = JSVAL_TRUE;

	  end_forinloop:
	    if (dropAtom) {
		js_DropAtom(cx, atom);
		dropAtom = JS_FALSE;
	    }
	    JS_UNLOCK_RUNTIME(rt);
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

#define PROPERTY_OP(call, result) {                                           \
    /* Pop the left part and resolve it to a non-null object. */              \
    lval = POP();                                                             \
    VALUE_TO_OBJECT(cx, lval, obj);                                           \
									      \
    /* Get or set the property, set result zero if error, non-zero if ok. */  \
    JS_LOCK_RUNTIME(rt);                                                      \
    call;                                                                     \
    JS_UNLOCK_RUNTIME(rt);                                                    \
    if (!result) {                                                            \
	ok = JS_FALSE;                                                        \
	goto out;                                                             \
    }                                                                         \
}

#define ELEMENT_OP(call, result) {                                            \
    id = POP();                                                               \
    FIX_ELEMENT_ID(id);                                                       \
    PROPERTY_OP(call, result);                                                \
    if (dropAtom) {                                                           \
	JS_LOCK_RUNTIME_VOID(rt, js_DropAtom(cx, atom));                      \
	dropAtom = JS_FALSE;                                                  \
    }                                                                         \
}

#define CACHED_GET(call) {                                                    \
    PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);                   \
    if (PROP_FOUND(prop)) {                                                   \
	obj2 = prop->object;                                                  \
	slot = (uintN)prop->slot;                                             \
	rval = obj2->slots[slot];                                             \
	ok = prop->getter(cx, obj, prop->id, &rval);                          \
	if (ok)                                                               \
	    obj2->slots[slot] = rval;                                         \
	else                                                                  \
	    prop = NULL;                                                      \
    } else {                                                                  \
	SAVE_SP(fp);                                                          \
	prop = call;                                                          \
	if (prop)                                                             \
	    PROPERTY_CACHE_FILL(cx, &rt->propertyCache, obj, id, prop);       \
    }                                                                         \
}

#define CACHED_SET(call) {                                                    \
    PROPERTY_CACHE_TEST(&rt->propertyCache, obj, id, prop);                   \
    if (PROP_FOUND(prop) &&                                                   \
	!(prop->flags & (JSPROP_READONLY | JSPROP_ASSIGNHACK))) {             \
	ok = prop->setter(cx, obj, prop->id, &rval);                          \
	if (ok) {                                                             \
	    prop->flags |= JSPROP_ENUMERATE;                                  \
	    prop->object->slots[prop->slot] = rval;                           \
	} else {                                                              \
	    prop = NULL;                                                      \
	}                                                                     \
    } else {                                                                  \
	SAVE_SP(fp);                                                          \
	prop = call;                                                          \
	/* No fill here: js_SetProperty writes through the cache. */          \
    }                                                                         \
}

	  case JSOP_SETNAME:
	    /* Get an immediate atom naming the variable to set. */
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;

	    SAVE_SP(fp);
	    JS_LOCK_RUNTIME(rt);
	    ok = js_FindVariable(cx, id, &obj, &prop);
	    if (!ok) {
		JS_UNLOCK_RUNTIME(rt);
		goto out;
	    }

	    /* Pop the right-hand side and set obj[id] to it. */
	    rval = POP();

	    /* Try to hit the property cache, FindVariable primes it. */
	    CACHED_SET(js_SetProperty(cx, obj, id, &rval));
	    if (!prop) {
		JS_UNLOCK_RUNTIME(rt);
		ok = JS_FALSE;
		goto out;
	    }

	    /* Push the right-hand side as our result. */
	    PUSH_OPND(rval);

	    /* Quicken this bytecode if possible. */
	    if (obj == fp->scopeChain &&
		prop && (prop->flags & JSPROP_PERMANENT)) {
		if (prop->setter == js_SetArgument) {
		    js_PatchOpcode(cx, script, pc, JSOP_SETARG);
		    slot = (uintN)JSVAL_TO_INT(prop->id);
		    SET_ARGNO(pc, slot);
		} else if (prop->setter == js_SetLocalVariable) {
		    js_PatchOpcode(cx, script, pc, JSOP_SETVAR);
		    slot = (uintN)JSVAL_TO_INT(prop->id);
		    SET_VARNO(pc, slot);
		}
	    }

	    JS_UNLOCK_RUNTIME(rt);
	    break;

#define INTEGER_OP(OP, EXTRA_CODE, LEFT_CAST) {                               \
    valid = JS_TRUE;                                                          \
    SAVE_SP(fp);                                                              \
    ok = PopInt(cx, &j, &valid) && PopInt(cx, &i, &valid);                    \
    RESTORE_SP(fp);                                                           \
    if (!ok)                                                                  \
	goto out;                                                             \
    EXTRA_CODE                                                                \
    if (!valid) {                                                             \
	PUSH_OPND(DOUBLE_TO_JSVAL(rt->jsNaN));                                \
    } else {                                                                  \
	i = LEFT_CAST i OP j;                                                 \
	PUSH_NUMBER(cx, i);                                                   \
    }                                                                         \
}

#define BITWISE_OP(OP)		INTEGER_OP(OP, (void) 0;, (jsint))
#define SIGNED_SHIFT_OP(OP)	INTEGER_OP(OP, j &= 31;, (jsint))
#define UNSIGNED_SHIFT_OP(OP)	INTEGER_OP(OP, j &= 31;, (jsuint))

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
	    d  = ltmp ? *rt->jsNaN : JSVAL_TO_INT(lval);                      \
	    d2 = rtmp ? *rt->jsNaN : JSVAL_TO_INT(rval);                      \
	    cond = COMPARE_DOUBLES(d, OP, d2, JS_FALSE);                      \
	}                                                                     \
    } else {                                                                  \
	ltmp = lval, rtmp = rval;                                             \
	VALUE_TO_PRIMITIVE(cx, lval, &lval);                                  \
	VALUE_TO_PRIMITIVE(cx, rval, &rval);                                  \
	if (JSVAL_IS_STRING(lval) && JSVAL_IS_STRING(rval)) {                 \
	    str  = JSVAL_TO_STRING(lval);                                     \
	    str2 = JSVAL_TO_STRING(rval);                                     \
	    cond = js_CompareStrings(str, str2) OP 0;                         \
	} else {                                                              \
	    VALUE_TO_NUMBER(cx, ltmp, d);                                     \
	    VALUE_TO_NUMBER(cx, rtmp, d2);                                    \
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
	    ltmp = lval, rtmp = rval;                                         \
	    VALUE_TO_PRIMITIVE(cx, lval, &lval);                              \
	    VALUE_TO_PRIMITIVE(cx, rval, &rval);                              \
	    if (JSVAL_IS_STRING(lval) && JSVAL_IS_STRING(rval)) {             \
		str  = JSVAL_TO_STRING(lval);                                 \
		str2 = JSVAL_TO_STRING(rval);                                 \
		cond = js_CompareStrings(str, str2) OP 0;                     \
	    } else {                                                          \
		VALUE_TO_NUMBER(cx, ltmp, d);                                 \
		VALUE_TO_NUMBER(cx, rtmp, d2);                                \
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
	    UNSIGNED_SHIFT_OP(>>);
	    break;

#undef INTEGER_OP
#undef BITWISE_OP
#undef SIGNED_SHIFT_OP
#undef UNSIGNED_SHIFT_OP

	  case JSOP_ADD:
	    rval = rtmp = POP();
	    lval = ltmp = POP();
	    VALUE_TO_PRIMITIVE(cx, lval, &lval);
	    VALUE_TO_PRIMITIVE(cx, rval, &rval);
	    if ((cond = JSVAL_IS_STRING(lval)) || JSVAL_IS_STRING(rval)) {
		if (cond) {
		    /* Keep a ref to str on the stack so it isn't GC'd. */
		    str = JSVAL_TO_STRING(lval);
		    PUSH(STRING_TO_JSVAL(str));
		    SAVE_SP(fp);
		    ok = (str2 = JS_ValueToString(cx, rval)) != 0;
		    (void) POP();
		} else {
		    /* Keep a ref to str2 on the stack so it isn't GC'd. */
		    str2 = JSVAL_TO_STRING(rval);
		    PUSH(STRING_TO_JSVAL(str2));
		    SAVE_SP(fp);
		    ok = (str = JS_ValueToString(cx, lval)) != 0;
		    (void) POP();
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
	    rval = POP();
	    VALUE_TO_BOOLEAN(cx, rval, cond);
	    PUSH_OPND(BOOLEAN_TO_JSVAL(!cond));
	    break;

	  case JSOP_BITNOT:
	    valid = JS_TRUE;
	    SAVE_SP(fp);
	    ok = PopInt(cx, &i, &valid);
	    RESTORE_SP(fp);
	    if (!ok)
		goto out;
	    if (!valid) {
		PUSH_OPND(DOUBLE_TO_JSVAL(rt->jsNaN));
	    } else {
		i = ~i;
		PUSH_NUMBER(cx, i);
	    }
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

	    /* Reset fp->sp so error reports decompile *vp's generator. */
	    fp->sp = vp;
	    JS_LOCK_RUNTIME(rt);
	    fun = js_ValueToFunction(cx, *vp);
	    if (!fun) {
		JS_UNLOCK_RUNTIME(rt);
		ok = JS_FALSE;
		goto out;
	    }

	    /* Get the constructor prototype object for this function. */
	    prop = js_GetProperty(cx, fun->object,
				  (jsval)rt->atomState.classPrototypeAtom,
				  &rval);
	    if (!prop) {
		JS_UNLOCK_RUNTIME(rt);
		ok = JS_FALSE;
		goto out;
	    }
	    proto = JSVAL_IS_OBJECT(rval) ? JSVAL_TO_OBJECT(rval) : NULL;
	    parent = OBJ_GET_PARENT(fun->object);

	    /* If there is no class prototype, use js_ObjectClass. */
	    if (!proto)
		obj = js_NewObject(cx, &js_ObjectClass, NULL, parent);
	    else
		obj = js_NewObject(cx, proto->map->clasp, proto, parent);
	    JS_UNLOCK_RUNTIME(rt);
	    if (!obj) {
		ok = JS_FALSE;
		goto out;
	    }

	    /* Now we have an object with a constructor method; call it. */
	    vp[1] = OBJECT_TO_JSVAL(obj);
	    SAVE_SP(fp);
	    ok = js_DoCall(cx, argc);
	    RESTORE_SP(fp);
	    if (!ok) {
		cx->newborn[GCX_OBJECT] = NULL;
		goto out;
	    }

	    /* Check the return value and see if it does not refer to obj. */
	    rval = *vp;
	    if (!JSVAL_IS_OBJECT(rval)) {
		*vp = OBJECT_TO_JSVAL(obj);
	    } else {
		obj2 = JSVAL_TO_OBJECT(rval);
		if (obj2 != obj)
		    obj = obj2;
	    }
	    break;

	  case JSOP_DELNAME:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;

	    JS_LOCK_RUNTIME(rt);
	    ok = js_FindProperty(cx, id, &obj, &prop);
	    if (ok && prop) {
		/* Call js_DeleteProperty2 to avoid id re-lookup. */
		ok = js_DeleteProperty2(cx, obj, prop, id, &rval);
	    }
	    JS_UNLOCK_RUNTIME(rt);
	    if (!ok)
		goto out;
	    PUSH_OPND(rval);
	    break;

	  case JSOP_DELPROP:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    SAVE_SP(fp);
	    PROPERTY_OP(ok = js_DeleteProperty(cx, obj, id, &rval), ok);
	    PUSH_OPND(rval);
	    break;

	  case JSOP_DELELEM:
	    SAVE_SP(fp);
	    ELEMENT_OP(ok = js_DeleteProperty(cx, obj, id, &rval), ok);
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
	    id   = (jsval)atom;

	    SAVE_SP(fp);
	    JS_LOCK_RUNTIME_VOID(rt, ok = js_FindVariable(cx, id, &obj, &prop));
	    if (!ok)
		goto out;

	    /* Quicken this bytecode if possible. */
	    if (obj == fp->scopeChain && (prop->flags & JSPROP_PERMANENT)) {
		if (prop->setter == js_SetArgument) {
		    js_PatchOpcode(cx, script, pc,
				   (cs->format & JOF_POST)
				   ? (cs->format & JOF_INC)
				     ? JSOP_ARGINC : JSOP_ARGDEC
				   : (cs->format & JOF_INC)
				     ? JSOP_INCARG : JSOP_DECARG);
		    slot = (uintN)JSVAL_TO_INT(prop->id);
		    SET_ARGNO(pc, slot);
		    goto do_argincop;
		}
		if (prop->setter == js_SetLocalVariable) {
		    js_PatchOpcode(cx, script, pc,
				   (cs->format & JOF_POST)
				   ? (cs->format & JOF_INC)
				     ? JSOP_VARINC : JSOP_VARDEC
				   : (cs->format & JOF_INC)
				     ? JSOP_INCVAR : JSOP_DECVAR);
		    slot = (uintN)JSVAL_TO_INT(prop->id);
		    SET_VARNO(pc, slot);
		    goto do_varincop;
		}
	    }

	    lval = OBJECT_TO_JSVAL(obj);
	    goto do_incop;

	  case JSOP_INCPROP:
	  case JSOP_DECPROP:
	  case JSOP_PROPINC:
	  case JSOP_PROPDEC:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    lval = POP();
	    goto do_incop;

	  case JSOP_INCELEM:
	  case JSOP_DECELEM:
	  case JSOP_ELEMINC:
	  case JSOP_ELEMDEC:
	    id   = POP();
	    lval = POP();

	    /* If the index is other than a nonnegative int, atomize it. */
	    FIX_ELEMENT_ID(id);

	  do_incop:
	    VALUE_TO_OBJECT(cx, lval, obj);

	    /* The operand must contain a number. */
	    CACHED_GET(js_GetProperty(cx, obj, id, &rval));
	    if (!prop) {
		ok = JS_FALSE;
		goto out;
	    }
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
	    CACHED_SET(js_SetProperty(cx, obj, id, &rval));
	    if (!prop) {
		ok = JS_FALSE;
		goto out;
	    }
	    if (dropAtom) {
		JS_LOCK_RUNTIME_VOID(rt, js_DropAtom(cx, atom));
		dropAtom = JS_FALSE;
	    }
	    PUSH_NUMBER(cx, d2);
	    break;

	  case JSOP_INCARG:
	  case JSOP_DECARG:
	  case JSOP_ARGINC:
	  case JSOP_ARGDEC:
	  do_argincop:
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
	  do_varincop:
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
	    id   = (jsval)atom;
	    PROPERTY_OP(CACHED_GET(js_GetProperty(cx, obj, id, &rval)),
			prop);
	    PUSH_OPND(rval);
	    break;

	  case JSOP_SETPROP:
	    /* Pop the right-hand side into rval for js_SetProperty. */
	    rval = POP();

	    /* Get an immediate atom naming the property. */
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    PROPERTY_OP(CACHED_SET(js_SetProperty(cx, obj, id, &rval)),
			prop);
	    PUSH_OPND(rval);
	    break;

	  case JSOP_GETELEM:
	    ELEMENT_OP(CACHED_GET(js_GetProperty(cx, obj, id, &rval)),
		       prop);
	    PUSH_OPND(rval);
	    break;

	  case JSOP_SETELEM:
	    rval = POP();
	    ELEMENT_OP(CACHED_SET(js_SetProperty(cx, obj, id, &rval)),
		       prop);
	    PUSH_OPND(rval);
	    break;

	  case JSOP_PUSHOBJ:
	    PUSH_OPND(OBJECT_TO_JSVAL(obj));
	    break;

	  case JSOP_CALL:
	    argc = GET_ARGC(pc);
	    SAVE_SP(fp);
	    ok = js_DoCall(cx, argc);
	    RESTORE_SP(fp);
	    if (!ok)
		goto out;
	    break;

	  case JSOP_NAME:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;

	    JS_LOCK_RUNTIME(rt);
	    ok = js_FindProperty(cx, id, &obj, &prop);
	    if (!ok) {
		JS_UNLOCK_RUNTIME(rt);
		goto out;
	    }
	    if (!prop) {
		/* Kludge to allow (typeof foo == "undefined") tests. */
		for (pc2 = pc + len; pc2 < endpc; pc2++) {
		    op2 = (JSOp)*pc2;
		    if (op2 == JSOP_TYPEOF) {
			JS_UNLOCK_RUNTIME(rt);
			PUSH_OPND(JSVAL_VOID);
			goto advance_pc;
		    }
		    if (op2 != JSOP_NOP)
			break;
		}
		JS_UNLOCK_RUNTIME(rt);
		js_ReportIsNotDefined(cx, ATOM_BYTES(atom));
		ok = JS_FALSE;
		goto out;
	    }

	    /* Get and push the obj[id] property's value. */
	    obj2 = prop->object;
	    slot = (uintN)prop->slot;
	    rval = obj2->slots[slot];
	    ok = prop->getter(cx, obj, prop->id, &rval);
	    if (!ok) {
		JS_UNLOCK_RUNTIME(rt);
		goto out;
	    }
	    obj2->slots[slot] = rval;
	    PUSH_OPND(rval);

	    /* Quicken this bytecode if possible. */
	    if (obj == fp->scopeChain && (prop->flags & JSPROP_PERMANENT)) {
		if (prop->getter == js_GetArgument) {
		    js_PatchOpcode(cx, script, pc, JSOP_GETARG);
		    slot = (uintN)JSVAL_TO_INT(prop->id);
		    SET_ARGNO(pc, slot);
		} else if (prop->getter == js_GetLocalVariable) {
		    js_PatchOpcode(cx, script, pc, JSOP_GETVAR);
		    slot = (uintN)JSVAL_TO_INT(prop->id);
		    SET_VARNO(pc, slot);
		}
	    }
	    JS_UNLOCK_RUNTIME(rt);
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
	    obj = NULL;
	    PUSH_OPND(rval);
	    break;

	  case JSOP_ZERO:
	    PUSH_OPND(JSVAL_ZERO);
	    break;

	  case JSOP_ONE:
	    PUSH_OPND(JSVAL_ONE);
	    break;

	  case JSOP_NULL:
	    obj = NULL;
	    PUSH_OPND(JSVAL_NULL);
	    break;

	  case JSOP_THIS:
	    obj = fp->thisp;
	    PUSH_OPND(OBJECT_TO_JSVAL(obj));
	    break;

	  case JSOP_FALSE:
	    PUSH_OPND(JSVAL_FALSE);
	    break;

	  case JSOP_TRUE:
	    PUSH_OPND(JSVAL_TRUE);
	    break;

#if JS_HAS_SWITCH_STATEMENT
	  case JSOP_TABLESWITCH:
	    valid = JS_TRUE;
	    SAVE_SP(fp);
	    ok = PopInt(cx, &i, &valid);
	    RESTORE_SP(fp);
	    if (!ok)
		goto out;

	    pc2 = pc;
	    len = GET_JUMP_OFFSET(pc2);
	    if (!valid)
		goto advance_pc;

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
	    JS_LOCK_RUNTIME_VOID(rt, obj = js_FindVariableScope(cx, &fun));
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
		PR_ASSERT(obj->map->clasp == &js_WithClass);
#if JS_BUG_WITH_CLOSURE
		while (obj->map->clasp == &js_WithClass) {
		    proto = OBJ_GET_PROTO(obj);
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
		JS_LOCK_RUNTIME_VOID(rt,
		    prop = js_DefineProperty(cx, obj, (jsval)fun2->atom,
					     OBJECT_TO_JSVAL(closure),
					     NULL, NULL, JSPROP_ENUMERATE));
		if (!prop) {
		    cx->newborn[GCX_OBJECT] = NULL;
		    ok = JS_FALSE;
		    goto out;
		}
	    }
	    PUSH(OBJECT_TO_JSVAL(closure));
	    break;
#endif /* JS_HAS_LEXICAL_CLOSURE */

#if JS_HAS_EXPORT_IMPORT
	  case JSOP_EXPORTALL:
	    JS_LOCK_RUNTIME(rt);
	    obj = js_FindVariableScope(cx, &fun);
	    if (!obj) {
		ok = JS_FALSE;
	    } else {
		ok = obj->map->clasp->enumerate(cx, obj);
		if (ok) {
		    for (prop = obj->map->props; prop; prop = prop->next) {
			if (prop->flags & JSPROP_ENUMERATE) {
			    ok = prop->getter(cx, obj, id, &rval);
			    if (!ok)
				break;
			    prop->flags |= JSPROP_EXPORTED;
			}
		    }
		}
	    }
	    JS_UNLOCK_RUNTIME(rt);
	    break;

	  case JSOP_EXPORTNAME:
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    JS_LOCK_RUNTIME(rt);
	    obj  = js_FindVariableScope(cx, &fun);
	    prop = obj ? js_GetProperty(cx, obj, id, &rval) : NULL;
	    if (!prop) {
		JS_UNLOCK_RUNTIME(rt);
		ok = JS_FALSE;
		goto out;
	    }
	    prop->flags |= JSPROP_EXPORTED;
	    JS_UNLOCK_RUNTIME(rt);
	    break;

	  case JSOP_IMPORTALL:
	    id = JSVAL_VOID;
	    PROPERTY_OP(ok = ImportProperty(cx, obj, id),
			ok);
	    break;

	  case JSOP_IMPORTPROP:
	    /* Get an immediate atom naming the property. */
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    PROPERTY_OP(ok = ImportProperty(cx, obj, id),
			ok);
	    break;

	  case JSOP_IMPORTELEM:
	    ELEMENT_OP(ok = ImportProperty(cx, obj, id),
		       ok);
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
	    obj = fp->scopeChain;
	    slot = (uintN)GET_ARGNO(pc);
	    PR_ASSERT(slot < fp->fun->nargs);
	    PUSH_OPND(fp->argv[slot]);
	    break;

	  case JSOP_SETARG:
	    obj = fp->scopeChain;
	    slot = (uintN)GET_ARGNO(pc);
	    PR_ASSERT(slot < fp->fun->nargs);
	    vp = &fp->argv[slot];
	    GC_POKE(cx, *vp);
	    *vp = sp[-1];
	    break;

	  case JSOP_GETVAR:
	    obj = fp->scopeChain;
	    slot = (uintN)GET_VARNO(pc);
	    PR_ASSERT(slot < fp->fun->nvars);
	    PUSH_OPND(fp->vars[slot]);
	    break;

	  case JSOP_SETVAR:
	    obj = fp->scopeChain;
	    slot = (uintN)GET_VARNO(pc);
	    PR_ASSERT(slot < fp->fun->nvars);
	    vp = &fp->vars[slot];
	    GC_POKE(cx, *vp);
	    *vp = sp[-1];
	    break;

#ifdef JS_HAS_OBJECT_LITERAL
	  case JSOP_NEWINIT:
	    argc = 0;
#if JS_HAS_SHARP_VARS
	    fp->sharpDepth++;
#endif
	    goto do_new;

	  case JSOP_ENDINIT:
#if JS_HAS_SHARP_VARS
	    if (--fp->sharpDepth == 0)
		fp->sharpArray = NULL;
#endif
	    break;

	  case JSOP_INITPROP:
	    /* Pop the property's value into rval. */
	    PR_ASSERT(sp - newsp >= 2);
	    rval = POP();

	    /* Get the immediate property name into id. */
	    atom = GET_ATOM(cx, script, pc);
	    id   = (jsval)atom;
	    goto do_init;

	  case JSOP_INITELEM:
	    /* Pop the element's value into rval. */
	    PR_ASSERT(sp - newsp >= 3);
	    rval = POP();

	    /* Pop and conditionally atomize the element id. */
	    id = POP();
	    FIX_ELEMENT_ID(id);

	  do_init:
	    /* Find the object being initialized at top of stack. */
	    lval = sp[-1];
	    PR_ASSERT(JSVAL_IS_OBJECT(lval));
	    obj = JSVAL_TO_OBJECT(lval);

	    /* Set the property named by obj[id] to rval. */
	    JS_LOCK_RUNTIME_VOID(rt, prop = js_SetProperty(cx, obj, id, &rval));
	    if (!prop) {
		ok = JS_FALSE;
		goto out;
	    }
	    if (dropAtom) {
		JS_LOCK_RUNTIME_VOID(rt, js_DropAtom(cx, atom));
		dropAtom = JS_FALSE;
	    }
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
	    id = INT_TO_JSVAL(i);
	    rval = sp[-1];
	    PR_ASSERT(JSVAL_IS_OBJECT(rval));
	    JS_LOCK_RUNTIME_VOID(rt, prop = js_SetProperty(cx, obj, id, &rval));
	    if (!prop) {
		ok = JS_FALSE;
		goto out;
	    }
	    break;

	  case JSOP_USESHARP:
	    i = (jsint) GET_ATOM_INDEX(pc);
	    id = INT_TO_JSVAL(i);
	    obj = fp->sharpArray;
	    if (!obj) {
		rval = JSVAL_VOID;
	    } else {
		JS_LOCK_RUNTIME_VOID(rt,
				     prop = js_GetProperty(cx, obj, id, &rval));
		if (!prop) {
		    ok = JS_FALSE;
		    goto out;
		}
	    }
	    if (!JSVAL_IS_OBJECT(rval)) {
		JS_ReportError(cx, "invalid sharp variable #%u#", (unsigned) i);
		ok = JS_FALSE;
		goto out;
	    }
	    PUSH_OPND(rval);
	    break;
#endif /* JS_HAS_SHARP_VARS */
#endif /* JS_HAS_OBJECT_LITERAL */

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
	    jsval v;

	    ndefs = cs->ndefs;
	    if (ndefs) {
		fp->sp = sp - ndefs;
		for (n = ndefs; n > 0; n--) {
		    v = sp[-n];
		    str = js_ValueToSource(cx, v);
		    if (str) {
			fprintf(tracefp, "%s %s",
				(n == ndefs) ? "  output:" : ",",
				JS_GetStringBytes(str));
		    }
		}
		putc('\n', tracefp);
		SAVE_SP(fp);
	    }
	}
#endif
    }

    /*
     * Treat end of script as a backward branch to handle all kinds of return
     * from function.
     */
    CHECK_BRANCH(-1);

out:
    /*
     * Restore the previous frame's execution state.
     */
    PR_ARENA_RELEASE(&cx->stackPool, mark);

    if (dropAtom) {
	JS_LOCK_RUNTIME_VOID(rt, js_DropAtom(cx, atom));
	dropAtom = JS_FALSE;
    }

#ifdef JS_THREADSAFE
    ok &= JS_RemoveRoot(cx, &obj);
#endif

    cx->interpLevel--;
    return ok;
}
