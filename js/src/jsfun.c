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
 * JS function support.
 */
#include "jsstddef.h"
#include <string.h>
#include "prtypes.h"
#include "prlog.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsconfig.h"
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

enum {
    CALL_ARGUMENTS  = -1,       /* predefined arguments local variable */
    ARGS_LENGTH     = -2,       /* number of actual args, arity if inactive */
    ARGS_CALLEE     = -3,       /* reference to active function's object */
    ARGS_CALLER     = -4,       /* arguments or call object that invoked us */
    FUN_ARITY       = -5,       /* number of formal parameters; desired argc */
    FUN_NAME        = -6,       /* function name, "" if anonymous */
    FUN_CALL        = -7        /* __call__ var, function's top Call object */
};

#undef TEST_BIT
#undef SET_BIT
#define TEST_BIT(tinyid, bitset)    ((bitset) & PR_BIT(-1 - (tinyid)))
#define SET_BIT(tinyid, bitset)     ((bitset) |= PR_BIT(-1 - (tinyid)))

#if JS_HAS_ARGS_OBJECT

JSObject *
js_GetArgsObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *argsobj;

    /* Create an arguments object for fp only if it lacks one. */
    PR_ASSERT(fp->fun);
    argsobj = fp->argsobj;
    if (argsobj)
    	return argsobj;

    /* Link the new object to fp so it can get actual argument values. */
    argsobj = js_NewObject(cx, &js_ArgumentsClass, NULL, NULL);
    if (!argsobj || !JS_SetPrivate(cx, argsobj, fp)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    fp->argsobj = argsobj;
    return argsobj;
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj);

JSBool
js_PutArgsObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *argsobj;
    JSBool ok;
    JSRuntime *rt;
    jsval rval;

    /*
     * Reuse args_enumerate here to reflect fp's actual arguments as indexed
     * elements of argsobj.
     */
    argsobj = fp->argsobj;
    if (!argsobj)
	return JS_TRUE;
    ok = args_enumerate(cx, argsobj);

    /*
     * Now get the prototype properties so we snapshot fp->fun, fp->down->fun,
     * and fp->argc before fp goes away.
     */
    rt = cx->runtime;
    ok &= js_GetProperty(cx, argsobj, (jsid)rt->atomState.calleeAtom, &rval);
    ok &= js_SetProperty(cx, argsobj, (jsid)rt->atomState.calleeAtom, &rval);
    ok &= js_GetProperty(cx, argsobj, (jsid)rt->atomState.callerAtom, &rval);
    ok &= js_SetProperty(cx, argsobj, (jsid)rt->atomState.callerAtom, &rval);
    ok &= js_GetProperty(cx, argsobj, (jsid)rt->atomState.lengthAtom, &rval);
    ok &= js_SetProperty(cx, argsobj, (jsid)rt->atomState.lengthAtom, &rval);

    /*
     * Clear the private pointer to fp, which is about to go away (js_Invoke).
     * Do this last because the args_enumerate and js_GetProperty calls above
     * need to follow the private slot to find fp.
     */
    ok &= JS_SetPrivate(cx, argsobj, NULL);
    fp->argsobj = NULL;
    return ok;
}

static JSBool
Arguments(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!cx->fp->constructing) {
	obj = js_NewObject(cx, &js_ArgumentsClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    return JS_TRUE;
}

static JSPropertySpec args_props[] = {
    {js_length_str,     ARGS_LENGTH,    0},
    {js_callee_str,     ARGS_CALLEE,    0},
    {js_caller_str,     ARGS_CALLER,    0},
    {0}
};

static JSBool
args_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSStackFrame *fp;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);
    fp = JS_GetPrivate(cx, obj);

    switch (slot) {
      case ARGS_CALLEE:
	if (fp && !TEST_BIT(slot, fp->overrides))
	    *vp = OBJECT_TO_JSVAL(fp->fun->object);
	break;

      case ARGS_CALLER:
	if (fp && !TEST_BIT(slot, fp->overrides)) {
	    if (fp->down && fp->down->fun) {
		JSObject *argsobj = js_GetArgsObject(cx, fp->down);
		if (!argsobj)
		    return JS_FALSE;
		*vp = OBJECT_TO_JSVAL(argsobj);
	    } else {
		*vp = JSVAL_NULL;
	    }
	}
	break;

      case ARGS_LENGTH:
	if (fp && !TEST_BIT(slot, fp->overrides))
	    *vp = INT_TO_JSVAL((jsint)fp->argc);
	break;

      default:
	if (fp && (uintN)slot < fp->argc)
	    *vp = fp->argv[slot];
	break;
    }
    return JS_TRUE;
}

static JSBool
args_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSStackFrame *fp;
    jsdouble argc;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);
    fp = JS_GetPrivate(cx, obj);

    switch (slot) {
      case ARGS_CALLEE:
      case ARGS_CALLER:
	if (fp)
	    SET_BIT(slot, fp->overrides);
	break;

      case ARGS_LENGTH:
	if (fp) {
	    if (!js_ValueToNumber(cx, *vp, &argc))
	    	return JS_FALSE;
	    argc = js_DoubleToInteger(argc);
	    if (0 <= argc && argc < fp->argc)
		fp->argc = (uintN)argc;
	    SET_BIT(slot, fp->overrides);
	}
	break;

      default:
	if (fp && (uintN)slot < fp->argc)
	    fp->argv[slot] = *vp;
	break;
    }
    return JS_TRUE;
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj)
{
    JSStackFrame *fp;
    uintN attrs, slot;

    fp = JS_GetPrivate(cx, obj);
    if (!fp)
	return JS_TRUE;

    /* XXX ECMA specs DontEnum, contrary to all other array-like objects */
    attrs = JSVERSION_IS_ECMA(cx->version) ? 0 : JSPROP_ENUMERATE;

    for (slot = 0; slot < fp->argc; slot++) {
	if (!js_DefineProperty(cx, obj, (jsid) INT_TO_JSVAL((jsint)slot),
			       fp->argv[slot], NULL, NULL, attrs,
			       NULL)) {
	    return JS_FALSE;
	}
    }
    return JS_TRUE;
}

JSClass js_ArgumentsClass = {
    "Arguments",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    args_getProperty, args_setProperty,
    args_enumerate,   JS_ResolveStub,
    JS_ConvertStub,   JS_FinalizeStub
};

#endif /* JS_HAS_ARGS_OBJECT */

#if JS_HAS_CALL_OBJECT

JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp, JSObject *parent,
		 JSObject *withobj)
{
    JSObject *callobj, *funobj, *obj;

    /* Create a call object for fp only if it lacks one. */
    PR_ASSERT(fp->fun);
    callobj = fp->callobj;
    if (callobj)
	return callobj;

    /* The default call parent is its function's parent (static link). */
    funobj = fp->fun->object;
    if (!parent && funobj)
	parent = OBJ_GET_PARENT(cx, funobj);
    callobj = js_NewObject(cx, &js_CallClass, NULL, parent);
    if (!callobj || !JS_SetPrivate(cx, callobj, fp)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    fp->callobj = callobj;

    /* Splice callobj into the scope chain. */
    if (!withobj) {
	for (obj = fp->scopeChain; obj; obj = OBJ_GET_PARENT(cx, obj)) {
	    if (OBJ_GET_CLASS(cx, obj) != &js_WithClass)
	    	break;
	    withobj = obj;
	}
    }
    if (withobj)
	OBJ_SET_PARENT(cx, withobj, callobj);
    else
	fp->scopeChain = callobj;
    return callobj;
}

static JSBool
call_enumerate(JSContext *cx, JSObject *obj);

JSBool
js_PutCallObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *callobj;
    JSBool ok;
    jsid argsid;
    jsval aval;

    /*
     * Reuse call_enumerate here to reflect all actual args and vars into the
     * call object from fp.
     */
    callobj = fp->callobj;
    if (!callobj)
	return JS_TRUE;
    ok = call_enumerate(cx, callobj);

    /*
     * Get the arguments object to snapshot fp's actual argument values.
     */
    argsid = (jsid) cx->runtime->atomState.argumentsAtom;
    ok &= js_GetProperty(cx, callobj, argsid, &aval);
    ok &= js_SetProperty(cx, callobj, argsid, &aval);
    ok &= js_PutArgsObject(cx, fp);

    /*
     * Clear the private pointer to fp, which is about to go away (js_Invoke).
     * Do this last because the call_enumerate and js_GetProperty calls above
     * need to follow the private slot to find fp.
     */
    ok &= JS_SetPrivate(cx, callobj, NULL);
    fp->callobj = NULL;
    return ok;
}

static JSBool
Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!cx->fp->constructing) {
	obj = js_NewObject(cx, &js_CallClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    return JS_TRUE;
}

static JSPropertySpec call_props[] = {
    {js_arguments_str,  CALL_ARGUMENTS, JSPROP_PERMANENT},
    {"__callee__",      ARGS_CALLEE,    0},
    {"__caller__",      ARGS_CALLER,    0},
    {"__call__",        FUN_CALL,       0},
    {0}
};

static JSBool
call_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;
    jsint slot;

    fp = JS_GetPrivate(cx, obj);
    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);

    switch (slot) {
      case CALL_ARGUMENTS:
	if (fp && !TEST_BIT(slot, fp->overrides)) {
	    JSObject *argsobj = js_GetArgsObject(cx, fp);
	    if (!argsobj)
	    	return JS_FALSE;
	    *vp = OBJECT_TO_JSVAL(argsobj);
	}
	break;

      case ARGS_CALLEE:
	if (fp && !TEST_BIT(slot, fp->overrides))
	    *vp = OBJECT_TO_JSVAL(fp->fun->object);
	break;

      case ARGS_CALLER:
	if (fp && !TEST_BIT(slot, fp->overrides)) {
	    if (fp->down && fp->down->fun) {
		JSObject *callobj = js_GetCallObject(cx, fp->down, NULL, NULL);
		if (!callobj)
		    return JS_FALSE;
		*vp = OBJECT_TO_JSVAL(callobj);
	    } else {
		*vp = JSVAL_NULL;
	    }
	}
	break;

      case FUN_CALL:
	*vp = OBJECT_TO_JSVAL(obj);
	break;

      default:
	if (fp && (uintN)slot < fp->argc)
	    *vp = fp->argv[slot];
	break;
    }
    return JS_TRUE;
}

static JSBool
call_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;
    jsint slot;

    fp = JS_GetPrivate(cx, obj);
    if (!JSVAL_IS_INT(id))
	return JS_TRUE;
    slot = JSVAL_TO_INT(id);

    switch (slot) {
      case CALL_ARGUMENTS:
      case ARGS_CALLEE:
      case ARGS_CALLER:
      case FUN_CALL:
	if (fp)
	    SET_BIT(slot, fp->overrides);
	break;

      default:
	if (fp && (uintN)slot < fp->argc)
	    fp->argv[slot] = *vp;
	break;
    }
    return JS_TRUE;
}

static JSBool
call_getVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;

    PR_ASSERT(JSVAL_IS_INT(id));
    fp = JS_GetPrivate(cx, obj);
    if (fp) {
	/* XXX no jsint slot commoning here to avoid MSVC1.52 crashes */
	if ((uintN)JSVAL_TO_INT(id) < fp->nvars)
	    *vp = fp->vars[JSVAL_TO_INT(id)];
    }
    return JS_TRUE;
}

static JSBool
call_setVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;

    PR_ASSERT(JSVAL_IS_INT(id));
    fp = JS_GetPrivate(cx, obj);
    if (fp) {
	/* XXX jsint slot is block-local here to avoid MSVC1.52 crashes */
	jsint slot = JSVAL_TO_INT(id);
	if ((uintN)slot < fp->nvars)
	    fp->vars[slot] = *vp;
    }
    return JS_TRUE;
}

static JSBool
call_enumerate(JSContext *cx, JSObject *obj)
{
    JSStackFrame *fp;
    JSFunction *fun;
    JSScope *scope;
    JSScopeProperty *sprop;
    JSPropertyOp getter;
    JSProperty *prop;

    fp = JS_GetPrivate(cx, obj);
    if (!fp)
	return JS_TRUE;
    fun = fp->fun;
    if (!fun->script || !fun->object)
	return JS_TRUE;

    /* Reflect actual args for formal parameters, and all local variables. */
    scope = (JSScope *)fun->object->map;
    for (sprop = scope->props; sprop; sprop = sprop->next) {
	getter = sprop->getter;
	if (getter != js_GetArgument && getter != js_GetLocalVariable)
	    continue;

	/* Trigger reflection in call_resolve by doing a lookup. */
	if (!js_LookupProperty(cx, obj, sym_id(sprop->symbols), &obj, &prop))
	    return JS_FALSE;
	OBJ_DROP_PROPERTY(cx, obj, prop);
    }

    return JS_TRUE;
}

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	     JSObject **objp)
{
    JSStackFrame *fp;
    JSString *str;
    JSAtom *atom;
    JSObject *obj2;
    JSScopeProperty *sprop;
    JSPropertyOp getter, setter;
    jsval propid, *vp;
    jsid symid;
    uintN slot, nslots;

    fp = JS_GetPrivate(cx, obj);
    if (!fp)
	return JS_TRUE;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    if (!fp->fun->object)
	return JS_TRUE;

    str = JSVAL_TO_STRING(id);
    atom = js_AtomizeString(cx, str, 0);
    if (!atom)
	return JS_FALSE;
    if (!OBJ_LOOKUP_PROPERTY(cx, fp->fun->object, (jsid)atom, &obj2,
			     (JSProperty **)&sprop)) {
	return JS_FALSE;
    }

    if (sprop) {
	getter = sprop->getter;
	propid = sprop->id;
	symid = (jsid) sym_atom(sprop->symbols);
	OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
	vp = NULL;
	if (getter == js_GetArgument) {
	    vp = fp->argv;
	    nslots = fp->argc;
	    getter = setter = NULL;
	} else if (getter == js_GetLocalVariable) {
	    vp = fp->vars;
	    nslots = fp->nvars;
	    getter = call_getVariable;
	    setter = call_setVariable;
	}
	if (vp) {
	    slot = (uintN)JSVAL_TO_INT(propid);
	    if (!js_DefineProperty(cx, obj, symid,
				   (slot < nslots) ? vp[slot] : JSVAL_VOID,
				   getter, setter,
				   JSPROP_ENUMERATE | JSPROP_PERMANENT,
				   (JSProperty **)&sprop)) {
	    	return JS_FALSE;
	    }
	    PR_ASSERT(sprop);
	    if (slot < nslots)
		sprop->id = INT_TO_JSVAL(slot);
	    OBJ_DROP_PROPERTY(cx, obj, (JSProperty *)sprop);
	    *objp = obj;
	}
    }
    return JS_TRUE;
}

static JSBool
call_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSStackFrame *fp;

    if (type == JSTYPE_FUNCTION) {
	fp = JS_GetPrivate(cx, obj);
	if (fp)
	    *vp = OBJECT_TO_JSVAL(fp->fun->object);
    }
    return JS_TRUE;
}

JSClass js_CallClass = {
    "Call",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    call_getProperty, call_setProperty,
    call_enumerate,   (JSResolveOp)call_resolve,
    call_convert,     JS_FinalizeStub
};

#endif /* JS_HAS_CALL_OBJECT */

#if JS_HAS_LEXICAL_CLOSURE
static JSBool
Closure(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *caller;
    JSObject *varParent, *closureParent;
    JSFunction *fun;

    if (!cx->fp->constructing) {
	obj = js_NewObject(cx, &js_ClosureClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    if (!(caller = cx->fp->down) || !caller->scopeChain)
	return JS_TRUE;

    varParent = js_FindVariableScope(cx, &fun);
    if (!varParent)
	return JS_FALSE;

    closureParent = caller->scopeChain;
    if (argc != 0) {
	fun = js_ValueToFunction(cx, &argv[0], JS_FALSE);
	if (!fun)
	    return JS_FALSE;
	OBJ_SET_PROTO(cx, obj, fun->object);
	if (argc > 1) {
	    if (!js_ValueToObject(cx, argv[1], &closureParent))
		return JS_FALSE;
	}
    }
    OBJ_SET_PARENT(cx, obj, closureParent);

    /* Make sure constructor is not inherited from fun->object. */
    return js_DefineProperty(cx, obj,
			     (jsid)cx->runtime->atomState.constructorAtom,
			     argv[-2], NULL, NULL,
			     JSPROP_READONLY | JSPROP_PERMANENT,
			     NULL);
}

static JSBool
closure_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSObject *proto;

    if (type == JSTYPE_FUNCTION) {
	proto = OBJ_GET_PROTO(cx, obj);
	if (proto)
	    *vp = OBJECT_TO_JSVAL(proto);
    }
    return JS_TRUE;
}

static JSBool
closure_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *fp;
    JSObject *closure, *callobj;
    JSFunction *fun;
    jsval junk;

    /* Get a call object to link the closure's parent into the scope chain. */
    fp = cx->fp;
    closure = JSVAL_TO_OBJECT(argv[-2]);
    PR_ASSERT(OBJ_GET_CLASS(cx, closure) == &js_ClosureClass);
    callobj = js_GetCallObject(cx, fp, OBJ_GET_PARENT(cx, closure), NULL);
    if (!callobj)
	return JS_FALSE;
    fp->scopeChain = callobj;

    /* Make the function object, not its closure, available as argv[-2]. */
    fun = fp->fun;
    argv[-2] = OBJECT_TO_JSVAL(fun->object);
    if (fun->call)
	return fun->call(cx, obj, argc, argv, rval);
    if (fun->script)
	return js_Interpret(cx, &junk);
    return JS_TRUE;
}

JSClass js_ClosureClass = {
    "Closure",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   closure_convert,  JS_FinalizeStub,
    NULL,             NULL,             closure_call,     closure_call
};
#endif /* JS_HAS_LEXICAL_CLOSURE */

static JSPropertySpec function_props[] = {
    /*
     * We make fun.arguments readonly in fun_setProperty, unless it is being
     * set by an unqualified assignment 'arguments = ...' within a call where
     * fun->object is proxying for a Call object.
     */
    {js_arguments_str,  CALL_ARGUMENTS, JSPROP_PERMANENT},
    {"__arity__",       FUN_ARITY,      JSPROP_READONLY | JSPROP_PERMANENT},
    {"__length__",      ARGS_LENGTH,    JSPROP_READONLY | JSPROP_PERMANENT},
    {"__caller__",      ARGS_CALLER,    JSPROP_READONLY | JSPROP_PERMANENT},
    {"__name__",        FUN_NAME,       JSPROP_READONLY | JSPROP_PERMANENT},
    {"__call__",        FUN_CALL,       JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

static JSBool
fun_delProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    /* Make delete f.length fail even though length is in f.__proto__. */
    if (!JSVAL_IS_INT(id)) {
	if (id == ATOM_KEY(cx->runtime->atomState.arityAtom) ||
	    id == ATOM_KEY(cx->runtime->atomState.lengthAtom) ||
	    id == ATOM_KEY(cx->runtime->atomState.callerAtom) ||
	    id == ATOM_KEY(cx->runtime->atomState.nameAtom)) {
	    *vp = JSVAL_FALSE;
	}
    }
    return JS_TRUE;
}

static JSBool
fun_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;
    jsint slot;
#if defined XP_PC && defined _MSC_VER &&_MSC_VER <= 800
    /* MSVC1.5 coredumps */
    jsval bogus = *vp;
#endif

    if (!JSVAL_IS_INT(id)) {
	/* Map qualified access of the form f.arity to f.__arity__. */
	if (id == ATOM_KEY(cx->runtime->atomState.arityAtom)) {
	    id = INT_TO_JSVAL(FUN_ARITY);
	} else if (id == ATOM_KEY(cx->runtime->atomState.lengthAtom)) {
	    id = INT_TO_JSVAL(ARGS_LENGTH);
	} else if (id == ATOM_KEY(cx->runtime->atomState.callerAtom)) {
	    id = INT_TO_JSVAL(ARGS_CALLER);
	} else if (id == ATOM_KEY(cx->runtime->atomState.nameAtom)) {
	    id = INT_TO_JSVAL(FUN_NAME);
	} else {
	    return JS_TRUE;
	}
    }

    /* No valid function object should lack private data, but check anyway. */
    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return JS_TRUE;

    /* Find fun's top-most activation record. */
    for (fp = cx->fp; fp && (fp->fun != fun || fp->debugging); fp = fp->down)
	continue;

    slot = (jsint)JSVAL_TO_INT(id);
    switch (slot) {
#if JS_HAS_ARGS_OBJECT
      case CALL_ARGUMENTS:
	if (fp && fp->fun) {
	    JSObject *argsobj = js_GetArgsObject(cx, fp);
	    if (!argsobj)
		return JS_FALSE;
	    *vp = OBJECT_TO_JSVAL(argsobj);
	} else {
	    *vp = JSVAL_NULL;
	}
	break;
#else  /* !JS_HAS_ARGS_OBJECT */
      case CALL_ARGUMENTS:
	*vp = OBJECT_TO_JSVAL(fp ? obj : NULL);
	break;
#endif /* !JS_HAS_ARGS_OBJECT */

      case ARGS_LENGTH:
	if (!JSVERSION_IS_ECMA(cx->version))
	    *vp = INT_TO_JSVAL((jsint)(fp->fun ? fp->argc : fun->nargs));
	else
      case FUN_ARITY:
	    *vp = INT_TO_JSVAL((jsint)fun->nargs);
	break;

      case ARGS_CALLER:
	if (fp && fp->down && fp->down->fun)
	    *vp = OBJECT_TO_JSVAL(fp->down->fun->object);
	else
	    *vp = JSVAL_NULL;
	break;

      case FUN_NAME:
	*vp = fun->atom
	      ? ATOM_KEY(fun->atom)
	      : STRING_TO_JSVAL(cx->runtime->emptyString);
	break;

      case FUN_CALL:
      	if (fp && fp->fun) {
	    JSObject *callobj = js_GetCallObject(cx, fp, NULL, NULL);
	    if (!callobj)
	    	return JS_FALSE;
	    *vp = OBJECT_TO_JSVAL(callobj);
      	} else {
	    *vp = JSVAL_NULL;
      	}
      	break;

      default:
	/* XXX fun[0] and fun.arguments[0] are equivalent. */
	if (fp && fp->fun && (uintN)slot < fp->argc)
#if defined XP_PC && defined _MSC_VER &&_MSC_VER <= 800
	  /* MSVC1.5 coredumps */
	  if (bogus == *vp)
#endif
	    *vp = fp->argv[slot];
	break;
    }

    return JS_TRUE;
}

static JSBool
fun_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp;
    JSObject *callobj;

    /* Handle only the setting of arguments or fun.arguments in active fun. */
    if (!JSVAL_IS_INT(id) || JSVAL_TO_INT(id) != CALL_ARGUMENTS)
	return JS_TRUE;

    /* No valid function object should lack private data, but check anyway. */
    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return JS_TRUE;

    /* Find the top-most non-native activation record, which must be fun's. */
    for (fp = cx->fp; ; fp = fp->down) {
	if (!fp)
	    goto _readonly;
	if (fp->fun == fun) {
	    if (!fp->debugging)
		break;
	} else {
	    if (fp->script)
		goto _readonly;
	}
    }

    /* Set only if unqualified: 'arguments = ...' not 'fun.arguments = ...'. */
    if (!fp->pc || (js_CodeSpec[*fp->pc].format & JOF_MODEMASK) != JOF_NAME)
    	goto _readonly;

    /* Get a Call object for fp and set its arguments property to vp. */
    callobj = js_GetCallObject(cx, fp, NULL, NULL);
    if (!callobj)
	return JS_FALSE;
    return js_SetProperty(cx, callobj,
			  (jsid)cx->runtime->atomState.argumentsAtom,
			  vp);

/* "readonly" can be a language extension on OSF */
_readonly:
    if (JSVERSION_IS_ECMA(cx->version))
	return fun_getProperty(cx, obj, id, vp);
    JS_ReportError(cx, "%s is read-only", js_arguments_str);
    return JS_FALSE;
}

static JSBool
fun_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
	    JSObject **objp)
{
    JSFunction *fun;
    JSString *str;
    JSAtom *prototypeAtom;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    /* No valid function object should lack private data, but check anyway. */
    fun = JS_GetPrivate(cx, obj);
    if (!fun || !fun->object)
	return JS_TRUE;

    /* No need to reflect fun.prototype in 'fun.prototype = ...'. */
    flags &= JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING;
    if (flags == (JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING))
	return JS_TRUE;

    /* Hide prototype if fun->object is proxying for a Call object. */
    if (!(flags & JSRESOLVE_QUALIFIED)) {
    	if (cx->fp && cx->fp->fun == fun && !cx->fp->callobj)
	    return JS_TRUE;
    }

    str = JSVAL_TO_STRING(id);
    prototypeAtom = cx->runtime->atomState.classPrototypeAtom;
    if (str == ATOM_TO_STRING(prototypeAtom)) {
	JSObject *proto;
	jsval pval;

	proto = NULL;
	if (fun->object != obj) {
	    /*
	     * Clone of a function: make its prototype property value have the
	     * same class as the clone-parent's prototype.
	     */
	    if (!OBJ_GET_PROPERTY(cx, fun->object, (jsid)prototypeAtom, &pval))
		return JS_FALSE;
	    if (JSVAL_IS_OBJECT(pval))
		proto = JSVAL_TO_OBJECT(pval);
	}

	/* If resolving "prototype" in a clone, clone the parent's prototype. */
	if (proto)
	    proto = js_NewObject(cx, OBJ_GET_CLASS(cx, proto), proto, NULL);
	else
	    proto = js_NewObject(cx, &js_ObjectClass, NULL, NULL);
	if (!proto)
	    return JS_FALSE;

	/*
	 * ECMA says that constructor.prototype is DontEnum for user-defined
	 * functions, but DontEnum | ReadOnly | DontDelete for native "system"
	 * constructors such as Object or Function.  So lazily set the former
	 * here in fun_resolve, but eagerly define the latter in JS_InitClass,
	 * with the right attributes.
	 */
	if (!js_SetClassPrototype(cx, fun->object, proto, 0)) {
	    cx->newborn[GCX_OBJECT] = NULL;
	    return JS_FALSE;
	}
	*objp = obj;
    }

    return JS_TRUE;
}

static JSBool
fun_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    switch (type) {
      case JSTYPE_FUNCTION:
	*vp = OBJECT_TO_JSVAL(obj);
	break;
      default:;
    }
    return JS_TRUE;
}

static void
fun_finalize(JSContext *cx, JSObject *obj)
{
    JSFunction *fun;

    /* No valid function object should lack private data, but check anyway. */
    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return;
    if (fun->object == obj)
	fun->object = NULL;
    JS_ATOMIC_ADDREF(&fun->nrefs, -1);
    if (fun->nrefs)
	return;
    if (fun->script)
	js_DestroyScript(cx, fun->script);
    JS_free(cx, fun);
}

#if JS_HAS_XDR

#include "jsxdrapi.h"

enum {
    JSXDR_FUNARG = 1,
    JSXDR_FUNVAR = 2
};

/* XXX store parent and proto, if defined */
static JSBool
fun_xdrObject(JSXDRState *xdr, JSObject **objp)
{
    JSFunction *fun;
    JSString *atomstr;
    char *propname;
    JSScopeProperty *sprop;
    JSBool magic;
    jsid propid;
    JSAtom *atom;
    uintN i;
    uint32 type;
#ifdef DEBUG
    uintN nvars = 0, nargs = 0;
#endif

    if (xdr->mode == JSXDR_ENCODE) {
	/*
	 * No valid function object should lack private data, but fail soft
	 * (return true, no error report) in case one does due to API pilot
	 * or internal error.
	 */
	fun = JS_GetPrivate(xdr->cx, *objp);
	if (!fun)
	    return JS_TRUE;
	atomstr = fun->atom ? ATOM_TO_STRING(fun->atom) : NULL;
    } else {
	fun = js_NewFunction(xdr->cx, NULL, NULL, 0, 0, NULL, NULL);
	if (!fun)
	    return JS_FALSE;
    }

    if (!JS_XDRStringOrNull(xdr, &atomstr) ||
	!JS_XDRUint8(xdr, &fun->nargs) ||
	!JS_XDRUint8(xdr, &fun->flags) ||
	!JS_XDRUint16(xdr, &fun->extra) ||
	!JS_XDRUint16(xdr, &fun->nvars))
	return JS_FALSE;

    /* do arguments and local vars */
    if (fun->object) {
	if (xdr->mode == JSXDR_ENCODE) {
	    JSScope *scope = (JSScope *) fun->object->map;

	    for (sprop = scope->props; sprop; sprop = sprop->next) {
		if (sprop->getter == js_GetArgument) {
		    type = JSXDR_FUNARG;
		    PR_ASSERT(nargs++ <= fun->nargs);
		} else if (sprop->getter == js_GetLocalVariable) {
		    type = JSXDR_FUNVAR;
		    PR_ASSERT(nvars++ <= fun->nvars);
		} else {
		    continue;
		}
		propname = ATOM_BYTES(sym_atom(sprop->symbols));
                propid = sprop->id;
		if (!JS_XDRUint32(xdr, &type) ||
                    !JS_XDRUint32(xdr, (uint32 *)&propid) ||
		    !JS_XDRCString(xdr, &propname))
		    return JS_FALSE;
	    }
	} else {
	    JSPropertyOp getter, setter;

	    i = fun->nvars + fun->nargs;
	    while (i--) {
		if (!JS_XDRUint32(xdr, &type) ||
                    !JS_XDRUint32(xdr, (uint32 *)&propid) ||
		    !JS_XDRCString(xdr, &propname))
		    return JS_FALSE;
		PR_ASSERT(type == JSXDR_FUNARG || type == JSXDR_FUNVAR);
		if (type == JSXDR_FUNARG) {
		    getter = js_GetArgument;
		    setter = js_SetArgument;
		    PR_ASSERT(nargs++ <= fun->nargs);
		} else if (type == JSXDR_FUNVAR) {
		    getter = js_GetLocalVariable;
		    setter = js_SetLocalVariable;
		    PR_ASSERT(nvars++ <= fun->nvars);
		}
                atom = js_Atomize(xdr->cx, propname, strlen(propname), 0);
		if (!atom ||
                    !OBJ_DEFINE_PROPERTY(xdr->cx, fun->object, (jsid)atom,
                                         JSVAL_VOID, getter, setter,
                                         JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                         (JSProperty **)&sprop) ||
                    !sprop){
		    JS_free(xdr->cx, propname);
		    return JS_FALSE;
		}
                sprop->id = propid;
		JS_free(xdr->cx, propname);
	    }
	}
    }
    if (!js_XDRScript(xdr, &fun->script, &magic) ||
	!magic)
	return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
	if (atomstr) {
	    fun->atom = js_AtomizeString(xdr->cx, atomstr, 0);
	    if (!fun->atom)
		return JS_FALSE;
	}
	*objp = fun->object;
        if (!OBJ_DEFINE_PROPERTY(xdr->cx, xdr->cx->globalObject,
                                 (jsid)fun->atom, OBJECT_TO_JSVAL(*objp),
                                 NULL, NULL, JSPROP_ENUMERATE,
                                 (JSProperty **)&sprop))
            return JS_FALSE;
    }

    return JS_TRUE;
}

#else  /* !JS_HAS_XDR */

#define fun_xdrObject NULL

#endif /* !JS_HAS_XDR */

#if JS_HAS_INSTANCEOF

static JSBool
fun_hasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    jsval pval;

    if (!OBJ_GET_PROPERTY(cx, obj,
			  (jsid)cx->runtime->atomState.classPrototypeAtom,
			  &pval)) {
	return JS_FALSE;
    }
    if (JSVAL_IS_PRIMITIVE(pval)) {
	*bp = JS_FALSE;
	return JS_TRUE;
    }
    return js_IsDelegate(cx, JSVAL_TO_OBJECT(pval), v, bp);
}

#else  /* !JS_HAS_INSTANCEOF */

#define fun_hasInstance NULL

#endif /* !JS_HAS_INSTANCEOF */

JSClass js_FunctionClass = {
    "Function",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  fun_delProperty,
    fun_getProperty,  fun_setProperty,
    JS_EnumerateStub, (JSResolveOp)fun_resolve,
    fun_convert,      fun_finalize,
    NULL,             NULL,
    NULL,             NULL,
    fun_xdrObject,    fun_hasInstance
};

static JSBool
fun_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval;
    JSFunction *fun;
    uint32 indent;
    JSString *str;

    if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_FUNCTION, &argv[-1]))
	return JS_FALSE;
    fval = argv[-1];
    if (!JSVAL_IS_FUNCTION(cx, fval))
	return js_obj_toString(cx, obj, argc, argv, rval);
    obj = JSVAL_TO_OBJECT(fval);
    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return JS_TRUE;
    indent = 0;
    if (argc && !js_ValueToECMAUint32(cx, argv[0], &indent))
	return JS_FALSE;
    str = JS_DecompileFunction(cx, fun, (uintN)indent);
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#if JS_HAS_CALL_FUNCTION
static JSBool
fun_call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval, *sp, *oldsp;
    void *mark;
    uintN i;
    JSStackFrame *fp;
    JSBool ok;

    if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_FUNCTION, &argv[-1]))
	return JS_FALSE;
    fval = argv[-1];

    if (argc == 0) {
	/* Call fun with its parent as the 'this' parameter if no args. */
	obj = OBJ_GET_PARENT(cx, obj);
    } else {
	/* Otherwise convert the first arg to 'this' and skip over it. */
	if (!js_ValueToObject(cx, argv[0], &obj))
	    return JS_FALSE;
	argc--;
	argv++;
    }

    /* Allocate stack space for fval, obj, and the args. */
    sp = js_AllocStack(cx, 2 + argc, &mark);
    if (!sp)
	return JS_FALSE;

    /* Push fval, obj, and the args. */
    *sp++ = fval;
    *sp++ = OBJECT_TO_JSVAL(obj);
    for (i = 0; i < argc; i++)
	*sp++ = argv[i];

    /* Lift current frame to include the args and do the call. */
    fp = cx->fp;
    oldsp = fp->sp;
    fp->sp = sp;
    ok = js_Invoke(cx, argc, JS_FALSE);

    /* Store rval and pop stack back to our frame's sp. */
    *rval = fp->sp[-1];
    fp->sp = oldsp;
    js_FreeStack(cx, mark);
    return ok;
}
#endif /* JS_HAS_CALL_FUNCTION */

#if JS_HAS_APPLY_FUNCTION
static JSBool
fun_apply(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval, *sp, *oldsp;
    JSObject *aobj;
    jsuint length;
    void *mark;
    uintN i;
    JSBool ok;
    JSStackFrame *fp;

    if (argc != 2 ||
	!JSVAL_IS_OBJECT(argv[1]) ||
	!(aobj = JSVAL_TO_OBJECT(argv[1])) ||
	!js_HasLengthProperty(cx, aobj, &length)) {
    	return fun_call(cx, obj, argc, argv, rval);
    }

    if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_FUNCTION, &argv[-1]))
	return JS_FALSE;
    fval = argv[-1];

    /* Convert the first arg to 'this' and skip over it. */
    if (!js_ValueToObject(cx, argv[0], &obj))
	return JS_FALSE;

    /* Allocate stack space for fval, obj, and the args. */
    argc = (uintN)length;
    sp = js_AllocStack(cx, 2 + argc, &mark);
    if (!sp)
	return JS_FALSE;

    /* Push fval, obj, and aobj's elements as args. */
    *sp++ = fval;
    *sp++ = OBJECT_TO_JSVAL(obj);
    for (i = 0; i < argc; i++) {
	ok = JS_GetElement(cx, aobj, (jsint)i, sp);
	if (!ok)
	    goto out;
	sp++;
    }

    /* Lift current frame to include the args and do the call. */
    fp = cx->fp;
    oldsp = fp->sp;
    fp->sp = sp;
    ok = js_Invoke(cx, argc, JS_FALSE);

    /* Store rval and pop stack back to our frame's sp. */
    *rval = fp->sp[-1];
    fp->sp = oldsp;
out:
    js_FreeStack(cx, mark);
    return ok;
}
#endif /* JS_HAS_APPLY_FUNCTION */

static JSFunctionSpec function_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   fun_toString,   0},
#endif
    {js_toString_str,	fun_toString,	1},
#if JS_HAS_APPLY_FUNCTION
    {"apply",		fun_apply,	1},
#endif
#if JS_HAS_CALL_FUNCTION
    {"call",		fun_call,	1},
#endif
    {0}
};

JSBool
js_IsIdentifier(JSString *str)
{
    size_t n;
    jschar *s, c;

    n = str->length;
    s = str->chars;
    c = *s;
    if (n == 0 || !JS_ISIDENT(c))
	return JS_FALSE;
    for (n--; n != 0; n--) {
	c = *++s;
	if (!JS_ISIDENT2(c))
	    return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
Function(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    JSObject *parent;
    uintN i, n, lineno;
    JSAtom *atom;
    const char *filename;
    JSObject *obj2;
    JSScopeProperty *sprop;
    JSString *str, *arg;
    JSStackFrame *fp;
    JSTokenStream *ts;
    JSPrincipals *principals;
    jschar *collected_args, *cp;
    size_t args_length;
    JSTokenType tt;

    if (cx->fp && !cx->fp->constructing) {
	obj = js_NewObject(cx, &js_FunctionClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
	*rval = OBJECT_TO_JSVAL(obj);
    }
    fun = JS_GetPrivate(cx, obj);
    if (fun)
	return JS_TRUE;

#if JS_HAS_CALL_OBJECT
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
    parent = OBJ_GET_PARENT(cx, JSVAL_TO_OBJECT(argv[-2]));
#else
    /* Set up for dynamic parenting (see Call in jsinterp.c). */
    parent = NULL;
#endif

    fun = js_NewFunction(cx, obj, NULL, 0, 0, parent,
			 (JSVERSION_IS_ECMA(cx->version))
			 ? cx->runtime->atomState.anonymousAtom
                         : NULL);

    if (!fun)
	return JS_FALSE;

    if ((fp = cx->fp) != NULL && (fp = fp->down) != NULL && fp->script) {
	filename = fp->script->filename;
	lineno = js_PCToLineNumber(fp->script, fp->pc);
        principals = fp->script->principals;
    } else {
	filename = NULL;
	lineno = 0;
        principals = NULL;
    }

    n = argc ? argc - 1 : 0;
    if (n > 0) {
        /* Collect the function-argument arguments into one string, separated
         * by commas, then make a tokenstream from that string, and scan it to
         * get the arguments.  We need to throw the full scanner at the
         * problem, because the argument string can legitimately contain
         * comments and linefeeds.  XXX It might be better to concatenate
         * everything up into a function definition and pass it to the
         * compiler, but doing it this way is less of a delta from the old
         * code.  See ECMA 15.3.2.1.
         */
        args_length = 0;
        for (i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            arg = JSVAL_TO_STRING(argv[i]);
            args_length += arg->length;
        }
        /* Add 1 for each joining comma. */
        args_length += n - 1;

        /* Allocate a string to hold the concatenated arguments, including room
         * for a terminating 0.
         */
        cp = collected_args = (jschar *) JS_malloc(cx, (args_length + 1) *
                                                   sizeof(jschar));
        /* Concatenate the arguments into the new string, separated by commas. */
        for (i = 0; i < n; i++) {
            arg = JSVAL_TO_STRING(argv[i]);
            (void)js_strncpy(cp, arg->chars, arg->length);
            cp += arg->length;
            /* Add separating comma or terminating 0. */
            *(cp++) = (i + 1 < n) ? ',' : 0;
        }

        /* Make a tokenstream that reads from the given string. */
        ts = js_NewTokenStream(cx, collected_args, args_length, filename, lineno,
                               principals);
        if (!ts) {
            JS_free(cx, collected_args);
            return JS_FALSE;
        }
        
        tt = js_GetToken(cx, ts);
        /* The argument string may be empty or contain no tokens. */
        if (tt != TOK_EOF) {
            while (1) {
                /* Check that it's a name.  This also implicitly guards against
                 * TOK_ERROR.
                 */
                if (tt != TOK_NAME) {
                    JS_ReportError(cx, "missing formal parameter");
                    goto badargs;
                }
                /* Get the atom corresponding to the name from the tokenstream;
                 * we're assured at this point that it's a valid identifier.
                 */
                atom = ts->token.t_atom;
                if (!js_LookupProperty(cx, obj, (jsid)atom, &obj2,
                                       (JSProperty **)&sprop)) {
                    goto badargs;
                }
                if (sprop && obj2 == obj) {
#ifdef CHECK_ARGUMENT_HIDING
                    PR_ASSERT(sprop->getter == js_GetArgument);
                    OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
                    JS_ReportError(cx, "duplicate formal argument %s",
                                   ATOM_BYTES(atom));
                    goto badargs;
#else
                /* A duplicate parameter name. We create a dummy symbol
                 * entry with property id of the parameter number and set
                 * the id to the name of the parameter.
                 * The decompiler will know to treat this case specially.
                 */
                jsid oldArgId = (jsid) sprop->id;
                OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
                sprop = NULL;
                if (!js_DefineProperty(cx, obj, oldArgId, JSVAL_VOID,
                                       js_GetArgument, js_SetArgument,
                                       JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                       (JSProperty **)&sprop)) {
                    goto badargs;
                }
                sprop->id = (jsid) atom;
#endif
                }
                if (sprop)
                    OBJ_DROP_PROPERTY(cx, obj2, (JSProperty *)sprop);
                if (!js_DefineProperty(cx, obj, (jsid)atom, JSVAL_VOID,
                                       js_GetArgument, js_SetArgument,
                                       JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                       (JSProperty **)&sprop)) {
                    goto badargs;
                }
                PR_ASSERT(sprop);
                sprop->id = INT_TO_JSVAL(fun->nargs++);
                OBJ_DROP_PROPERTY(cx, obj, (JSProperty *)sprop);
        
                /* Done with the NAME; get the next token. */
                tt = js_GetToken(cx, ts);
                /* Stop if we've reached the end of the string. */
                if (tt == TOK_EOF)
                    break;

                /* If a comma is seen, get the next token.  Otherwise, let the
                 * loop catch the error.
                 */
                if (tt == TOK_COMMA)
                    tt = js_GetToken(cx, ts);
            }
        }        
        /* Clean up. */
        JS_free(cx, collected_args);
        if (!js_CloseTokenStream(cx, ts))
            return JS_FALSE;
    }

    if (argc) {
	str = js_ValueToString(cx, argv[argc-1]);
    } else {
	/* Can't use cx->runtime->emptyString because we're called too early. */
	str = js_NewStringCopyZ(cx, js_empty_ucstr, 0);
    }
    if (!str)
	return JS_FALSE;
    if (argv) {
	/* Use the last arg (or this if argc == 0) as a local GC root. */
	argv[argc-1] = STRING_TO_JSVAL(str);
    }

    if ((fp = cx->fp) != NULL && (fp = fp->down) != NULL && fp->script) {
	filename = fp->script->filename;
	lineno = js_PCToLineNumber(fp->script, fp->pc);
        principals = fp->script->principals;
    } else {
	filename = NULL;
	lineno = 0;
        principals = NULL;
    }
    ts = js_NewTokenStream(cx, str->chars, str->length, filename, lineno,
                           principals);
    if (!ts)
	return JS_FALSE;
    return js_ParseFunctionBody(cx, ts, fun) &&
	   js_CloseTokenStream(cx, ts);

badargs:
    /* Clean up the arguments string and tokenstream if we failed to parse
     * the arguments.
     */
    JS_free(cx, collected_args);
    (void)js_CloseTokenStream(cx, ts);
    return JS_FALSE;
}

JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;
    JSAtom *atom;
    JSFunction *fun;

    proto = JS_InitClass(cx, obj, NULL, &js_FunctionClass, Function, 1,
			 function_props, function_methods, NULL, NULL);
    if (!proto)
	return NULL;
    atom = js_Atomize(cx, js_FunctionClass.name, strlen(js_FunctionClass.name),
		      0);
    if (!atom)
	goto bad;
    fun = js_NewFunction(cx, proto, NULL, 0, 0, obj, atom);
    if (!fun)
    	goto bad;
    fun->script = js_NewScript(cx, 0);
    if (!fun->script)
    	goto bad;
    return proto;

bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

JSBool
js_InitArgsCallClosureClasses(JSContext *cx, JSObject *obj,
			      JSObject *arrayProto)
{
#if JS_HAS_ARGS_OBJECT
    if (!JS_InitClass(cx, obj, arrayProto, &js_ArgumentsClass, Arguments, 0,
		      args_props, NULL, NULL, NULL)) {
	return JS_FALSE;
    }
#endif

#if JS_HAS_CALL_OBJECT
    if (!JS_InitClass(cx, obj, NULL, &js_CallClass, Call, 0,
		      call_props, NULL, NULL, NULL)) {
	return JS_FALSE;
    }
#endif

#if JS_HAS_LEXICAL_CLOSURE
    if (!JS_InitClass(cx, obj, NULL, &js_ClosureClass, Closure, 0,
		      NULL, NULL, NULL, NULL)) {
	return JS_FALSE;
    }
#endif

    return JS_TRUE;
}

JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, JSNative call, uintN nargs,
	       uintN flags, JSObject *parent, JSAtom *atom)
{
    JSFunction *fun;

    /* Allocate a function struct. */
    fun = JS_malloc(cx, sizeof *fun);
    if (!fun)
	return NULL;
    fun->nrefs = 0;
    fun->object = NULL;

    /* If funobj is null, allocate an object for it. */
    if (!funobj) {
	funobj = js_NewObject(cx, &js_FunctionClass, NULL, parent);
	if (!funobj) {
	    JS_free(cx, fun);
	    return NULL;
	}
    } else {
	OBJ_SET_PARENT(cx, funobj, parent);
    }

    /* Link fun to funobj and vice versa. */
    if (!js_LinkFunctionObject(cx, fun, funobj)) {
	cx->newborn[GCX_OBJECT] = NULL;
	JS_free(cx, fun);
	return NULL;
    }

    /* Initialize remaining function members. */
    fun->call = call;
    fun->nargs = nargs;
    fun->flags = flags & JSFUN_FLAGS_MASK;
    fun->extra = 0;
    fun->nvars = 0;
    fun->spare = 0;
    fun->atom = atom;
    fun->script = NULL;
    return fun;
}

JSBool
js_LinkFunctionObject(JSContext *cx, JSFunction *fun, JSObject *object)
{
    if (!fun->object)
	fun->object = object;
    if (!JS_SetPrivate(cx, object, fun))
	return JS_FALSE;
    JS_ATOMIC_ADDREF(&fun->nrefs, 1);
    return JS_TRUE;
}

JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, JSAtom *atom, JSNative call,
		  uintN nargs, uintN attrs)
{
    JSFunction *fun;

    fun = js_NewFunction(cx, NULL, call, nargs, attrs, obj, atom);
    if (!fun)
	return NULL;
    if (!OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, OBJECT_TO_JSVAL(fun->object),
			     NULL, NULL, attrs, NULL)) {
	return NULL;
    }
    return fun;
}

JSFunction *
js_ValueToFunction(JSContext *cx, jsval *vp, JSBool constructing)
{
    jsval v;
    JSObject *obj;

    v = *vp;
    obj = NULL;
    if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (obj && OBJ_GET_CLASS(cx, obj) != &js_FunctionClass) {
	    if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_FUNCTION, &v))
		return NULL;
	    obj = JSVAL_IS_FUNCTION(cx, v) ? JSVAL_TO_OBJECT(v) : NULL;
	}
    }
    if (!obj) {
	js_ReportIsNotFunction(cx, vp, constructing);
	return NULL;
    }
    return JS_GetPrivate(cx, obj);
}

void
js_ReportIsNotFunction(JSContext *cx, jsval *vp, JSBool constructing)
{
    JSStackFrame *fp;
    jsval *sp;
    JSString *str;

    fp = cx->fp;
    if (fp)
	sp = fp->sp, fp->sp = vp;
    str = js_DecompileValueGenerator(cx, *vp, NULL);
    if (fp)
	fp->sp = sp;
    if (str) {
	JS_ReportError(cx, "%s is not a %s",
		       JS_GetStringBytes(str),
		       constructing ? "constructor" : "function");
    }
}
