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

static char js_arguments_str[] = "arguments";
static char js_arity_str[]     = "arity";
static char js_callee_str[]    = "callee";
static char js_caller_str[]    = "caller";

#if JS_HAS_CALL_OBJECT

JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp, JSObject *parent)
{
    JSObject *callobj, *funobj;

    PR_ASSERT(fp->fun);
    callobj = fp->object;
    if (callobj)
	return callobj;
    funobj = fp->fun->object;
    if (!parent)
	parent = OBJ_GET_PARENT(funobj);
    callobj = js_NewObject(cx, &js_CallClass, NULL, parent);
    if (!callobj || !JS_SetPrivate(cx, callobj, fp)) {
	cx->newborn[GCX_OBJECT] = NULL;
	return NULL;
    }
    fp->object = callobj;
    if (fp->scopeChain == funobj)
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
    jsval junk;

    /*
     * Reuse call_enumerate here to reflect all actual args and vars into the
     * call object from fp.
     */
    callobj = fp->object;
    ok = call_enumerate(cx, callobj);

    /*
     * Now get the prototype properties so we snapshot fp->fun, fp->down, and
     * fp->argc before fp goes away.  But the predefined call.length will not
     * override a formal arg or var named length -- the compiler redefines the
     * name, getter, and setter.
     */
    ok &= JS_GetProperty(cx, callobj, js_callee_str, &junk);
    ok &= JS_GetProperty(cx, callobj, js_caller_str, &junk);
    ok &= JS_GetProperty(cx, callobj, js_length_str, &junk);

    /*
     * Clear the private pointer to fp, which is about to go away (in Call).
     * Do this last because the js_GetProperty calls above need to follow the
     * private slot to find fp, in order to copy back args and locals.
     */
    ok &= JS_SetPrivate(cx, callobj, NULL);
    return ok;
}

JSBool
js_GetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBool ok;
    JSStackFrame *fp;
    JSSymbol *sym;

    ok = JS_TRUE;
    fp = JS_GetPrivate(cx, obj);
    if (fp && fp->script) {
	JS_LOCK(cx);
	for (sym = fp->script->vars; sym; sym = sym->next) {
	    if (sym_property(sym)->id == id) {
		if (!js_GetProperty(cx, obj, sym_id(sym), vp))
		    ok = JS_FALSE;
		break;
	    }
	}
	JS_UNLOCK(cx);
    }
    return ok;
}

JSBool
js_SetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSBool ok;
    JSStackFrame *fp;
    JSSymbol *sym;

    ok = JS_TRUE;
    fp = JS_GetPrivate(cx, obj);
    if (fp && fp->script) {
	JS_LOCK(cx);
	for (sym = fp->script->vars; sym; sym = sym->next) {
	    if (sym_property(sym)->id == id) {
		if (!js_SetProperty(cx, obj, sym_id(sym), vp))
		    ok = JS_FALSE;
		break;
	    }
	}
	JS_UNLOCK(cx);
    }
    return ok;
}

static JSBool
Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return JS_TRUE;
}

enum call_tinyid {
    CALL_ARGUMENTS = -1,
    CALL_CALLEE    = -2,
    CALL_CALLER    = -3,
    CALL_LENGTH    = -4
};

static JSPropertySpec call_props[] = {
    {js_arguments_str,  CALL_ARGUMENTS, JSPROP_READONLY},
    {js_callee_str,     CALL_CALLEE,    JSPROP_READONLY},
    {js_caller_str,     CALL_CALLER,    JSPROP_READONLY},
    {js_length_str,     CALL_LENGTH,    JSPROP_READONLY},
    {0}
};

#define IS_CALL_TINYID(slot)    (CALL_LENGTH <= slot && slot <= CALL_ARGUMENTS)
#define CALL_TINYID_NAME(slot)  call_props[-(slot)-1].name

static JSBool
call_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;
    JSObject *callobj;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    fp = JS_GetPrivate(cx, obj);
    slot = JSVAL_TO_INT(id);

    /* XXX don't pollute call scope with unqualified call property names */
    if (fp &&
	fp->pc &&
	(js_CodeSpec[*fp->pc].format & JOF_MODEMASK) == JOF_NAME &&
	IS_CALL_TINYID(slot)) {
	return JS_GetProperty(cx, fp->fun->object, CALL_TINYID_NAME(slot), vp);
    }

    switch (slot) {
      case CALL_ARGUMENTS:
	*vp = OBJECT_TO_JSVAL(obj);
	break;

      case CALL_CALLEE:
	if (fp)
	    *vp = OBJECT_TO_JSVAL(fp->fun->object);
	break;

      case CALL_CALLER:
	if (fp && fp->down && fp->down->fun) {
	    callobj = js_GetCallObject(cx, fp->down, NULL);
	    if (!callobj)
		return JS_FALSE;
	    *vp = OBJECT_TO_JSVAL(callobj);
	}
	break;

      case CALL_LENGTH:
	if (fp)
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
call_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;
    jsint slot;

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    fp = JS_GetPrivate(cx, obj);
    if (fp) {
	slot = JSVAL_TO_INT(id);
	if ((uintN)slot < fp->argc)
	    fp->argv[slot] = *vp;
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
call_reflectArgument(JSContext *cx, JSObject *obj, JSStackFrame *fp,
		     JSSymbol *sym)
{
    uintN slot;
    JSProperty *prop;
    JSScope *scope;
    JSBool ok;

    JS_LOCK(cx);
    slot = (uintN)JSVAL_TO_INT(sym_property(sym)->id);
    prop = js_DefineProperty(cx, obj, (jsval)sym_atom(sym),
			     (slot < fp->argc) ? fp->argv[slot] : JSVAL_VOID,
			     NULL, NULL, JSPROP_ENUMERATE | JSPROP_PERMANENT);
    if (!prop) {
	ok = JS_FALSE;
    } else {
	prop->id = INT_TO_JSVAL(slot);
	scope = (JSScope *)obj->map;
	ok = (scope->ops->add(cx, scope, INT_TO_JSVAL(slot), prop) != NULL);
    }
    JS_UNLOCK(cx);
    return ok;
}

static JSBool
call_reflectVariable(JSContext *cx, JSObject *obj, JSStackFrame *fp,
		     JSSymbol *sym)
{
    uintN slot;
    JSProperty *prop;
    JSBool ok;

    slot = (uintN)JSVAL_TO_INT(sym_property(sym)->id);
    JS_LOCK(cx);
    prop = js_DefineProperty(cx, obj, (jsval)sym_atom(sym),
			     (slot < fp->nvars) ? fp->vars[slot] : JSVAL_VOID,
			     call_getVariable, call_setVariable,
			     JSPROP_PERMANENT);
    ok = (prop != NULL);
    if (ok)
	prop->id = INT_TO_JSVAL(slot);
    JS_UNLOCK(cx);
    return ok;
}

static JSBool
call_enumerate(JSContext *cx, JSObject *obj)
{
    JSStackFrame *fp;
    JSFunction *fun;
    JSSymbol *sym;
    JSBool ok;
    JSProperty *prop;
    uintN slot;

    fp = JS_GetPrivate(cx, obj);
    if (!fp)
	return JS_TRUE;
    fun = fp->fun;
    if (!fun->script)
	return JS_TRUE;

    ok = JS_TRUE;
    JS_LOCK(cx);

    /* First, reflect actuals that match formal arguments. */
    if (fun->script) {
	for (sym = fun->script->args; sym; sym = sym->next) {
	    ok = js_LookupProperty(cx, obj, sym_id(sym), &obj, &prop);
	    if (!ok)
		goto out;
	}
    }

    /* Next, copy any actual arguments that lacked formals. */
    for (slot = fun->nargs; slot < fp->argc; slot++) {
	if (!js_SetProperty(cx, obj, INT_TO_JSVAL((jsint)slot),
			    &fp->argv[slot])) {
	    ok = JS_FALSE;
	    goto out;
	}
    }

    /* Finally, reflect variables. */
    if (fun->script) {
	for (sym = fun->script->vars; sym; sym = sym->next) {
	    ok = js_LookupProperty(cx, obj, sym_id(sym), &obj, &prop);
	    if (!ok)
		goto out;
	}
    }

out:
    JS_UNLOCK(cx);
    return ok;
}

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsval id, JSObject **objp)
{
    JSStackFrame *fp;
    JSString *str;
    JSAtom *atom;
    JSBool ok;
    JSProperty *prop;
    JSSymbol *sym;

    fp = JS_GetPrivate(cx, obj);
    if (!fp)
	return JS_TRUE;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    str = JSVAL_TO_STRING(id);
    JS_LOCK(cx);
    atom = js_AtomizeString(cx, str, 0);
    if (!atom) {
	ok = JS_FALSE;
    } else {
	ok = js_LookupProperty(cx, fp->fun->object, (jsval)atom, &obj, &prop);
	js_DropAtom(cx, atom);
    }
    JS_UNLOCK(cx);
    if (!ok)
	return JS_FALSE;

    if (prop) {
	sym = prop->symbols;
	if (prop->getter == js_GetArgument) {
	    if (!call_reflectArgument(cx, obj, fp, sym))
		return JS_FALSE;
	    *objp = obj;
	} else if (prop->getter == js_GetLocalVariable) {
	    if (!call_reflectVariable(cx, obj, fp, sym))
		return JS_FALSE;
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

/* Forward declaration. */
static JSBool
fun_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	     jsval *rval);

static JSBool
call_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	      jsval *rval)
{
    JSStackFrame *fp;

    if (!JS_InstanceOf(cx, obj, &js_CallClass, argv))
	return JS_FALSE;
    fp = JS_GetPrivate(cx, obj);
    if (fp)
	return fun_toString(cx, fp->fun->object, argc, argv, rval);
    return js_obj_toString(cx, obj, argc, argv, rval);
}

static JSBool
call_valueOf(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
	     jsval *rval)
{
    if (!JS_InstanceOf(cx, obj, &js_CallClass, argv))
	return JS_FALSE;
    return JS_GetProperty(cx, obj, js_callee_str, rval);
}

static JSFunctionSpec call_methods[] = {
    {js_toString_str,	call_toString,	1},
    {js_valueOf_str,	call_valueOf,	0},
    {0}
};

#endif /* !JS_HAS_CALL_OBJECT */

#if JS_HAS_LEXICAL_CLOSURE
static JSBool
Closure(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *caller;
    JSObject *varParent, *closureParent;
    JSFunction *fun;

    if (!JS_InstanceOf(cx, obj, &js_ClosureClass, argv))
	return JS_FALSE;
    if (!(caller = cx->fp->down) || !caller->scopeChain)
	return JS_TRUE;

    JS_LOCK_VOID(cx, varParent = js_FindVariableScope(cx, &fun));
    if (!varParent)
	return JS_FALSE;

    closureParent = caller->scopeChain;
    if (argc != 0) {
	fun = JS_ValueToFunction(cx, argv[0]);
	if (!fun)
	    return JS_FALSE;
	OBJ_SET_PROTO(obj, fun->object);
	if (argc > 1) {
	    if (!JS_ValueToObject(cx, argv[1], &closureParent))
		return JS_FALSE;
	}
    }
    OBJ_SET_PARENT(obj, closureParent);

    /* Make sure constructor is not inherited from fun->object. */
    if (!js_DefineProperty(cx, obj,
			   (jsval)cx->runtime->atomState.constructorAtom,
			   argv[-2], NULL, NULL,
			   JSPROP_READONLY | JSPROP_PERMANENT)) {
	return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
closure_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JSObject *proto;

    if (type == JSTYPE_FUNCTION) {
	proto = JS_GetPrototype(cx, obj);
	if (proto) {
	    PR_ASSERT(proto->map->clasp == &js_FunctionClass);
	    *vp = OBJECT_TO_JSVAL(proto);
	}
    }
    return JS_TRUE;
}

JSClass js_ClosureClass = {
    "Closure",
    0,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   closure_convert,  JS_FinalizeStub
};
#endif /* JS_HAS_LEXICAL_CLOSURE */

enum fun_tinyid {
    FUN_ARGUMENTS = -1,         /* actual arguments for this call to fun */
    FUN_CALLER    = -2,         /* bogus, the function that called fun */
    FUN_ARITY     = -3          /* number of formals if inactive, else argc */
};

static JSPropertySpec function_props[] = {
    {js_arguments_str,  FUN_ARGUMENTS,  JSPROP_READONLY | JSPROP_PERMANENT},
    {js_caller_str,     FUN_CALLER,     JSPROP_READONLY | JSPROP_PERMANENT},
    {js_arity_str,      FUN_ARITY,      JSPROP_READONLY | JSPROP_PERMANENT},
    {0}
};

static JSBool
fun_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSFunction *fun;
    JSStackFrame *fp, *bestfp;
    jsint slot;
#if defined XP_PC && defined _MSC_VER &&_MSC_VER <= 800
    /* MSVC1.5 coredumps */
    jsval bogus = *vp;
#endif

    if (!JSVAL_IS_INT(id))
	return JS_TRUE;

    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return JS_TRUE;

    /* Find fun's top-most frame, or the top-most non-native frame. */
    bestfp = NULL;
    for (fp = cx->fp; fp && fp->fun && fp->fun != fun; fp = fp->down) {
	if (fp->fun->call)
	    continue;
	if (!bestfp)
	    bestfp = fp;
    }

    /* Use bestfp only if this property reference is unqualified. */
    if ((!fp || !fp->fun) &&
	bestfp &&
	bestfp->pc &&
	(js_CodeSpec[*bestfp->pc].format & JOF_MODEMASK) == JOF_NAME) {
	fp = bestfp;
    }

    slot = (jsint)JSVAL_TO_INT(id);
    switch (slot) {
#if JS_HAS_CALL_OBJECT
      case FUN_ARGUMENTS:
	if (fp && fp->fun) {
	    JSObject *callobj = js_GetCallObject(cx, fp, NULL);
	    if (!callobj)
		return JS_FALSE;
	    *vp = OBJECT_TO_JSVAL(callobj);
	} else {
	    *vp = JSVAL_NULL;
	}
	break;
#else  /* !JS_HAS_CALL_OBJECT */
      case FUN_ARGUMENTS:
	*vp = OBJECT_TO_JSVAL(fp ? obj : NULL);
	break;
#endif /* !JS_HAS_CALL_OBJECT */

      case FUN_CALLER:
	if (fp && fp->down && fp->down->fun)
	    *vp = OBJECT_TO_JSVAL(fp->down->fun->object);
	else
	    *vp = JSVAL_NULL;
	break;

      case FUN_ARITY:
	if (cx->version < JSVERSION_1_2)
	    *vp = INT_TO_JSVAL((jsint)(fp && fp->fun ? fp->argc : fun->nargs));
	else
	    *vp = INT_TO_JSVAL((jsint)fun->nargs);
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
fun_resolve(JSContext *cx, JSObject *obj, jsval id, JSObject **objp)
{
    JSFunction *fun;
    JSString *str;

    if (!JSVAL_IS_STRING(id))
	return JS_TRUE;

    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return JS_TRUE;

    str = JSVAL_TO_STRING(id);
    if (str == ATOM_TO_STRING(cx->runtime->atomState.classPrototypeAtom)) {
	JSObject *proto;

	proto = js_NewObject(cx, &js_ObjectClass, NULL, NULL);
	if (!proto)
	    return JS_FALSE;
	if (!js_SetClassPrototype(cx, fun, proto)) {
	    cx->newborn[GCX_OBJECT] = NULL;
	    return JS_FALSE;
	}
	*objp = obj;
	return JS_TRUE;
    }

    return JS_TRUE;
}

static JSBool
fun_convert(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    switch (type) {
      case JSTYPE_OBJECT:
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

    fun = JS_GetPrivate(cx, obj);
    if (!fun)
	return;
    if (fun->atom)
	JS_LOCK_VOID(cx, js_DropAtom(cx, fun->atom));
    if (fun->script)
	js_DestroyScript(cx, fun->script);
    JS_free(cx, fun);
}

JSClass js_FunctionClass = {
    "Function",
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    fun_getProperty,  JS_PropertyStub,
    JS_EnumerateStub, (JSResolveOp)fun_resolve,
    fun_convert,      fun_finalize
};

static JSBool
fun_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSFunction *fun;
    jsdouble d;
    JSString *str;

    fun = JS_GetInstancePrivate(cx, obj, &js_FunctionClass, argv);
    if (!fun)
	return JS_FALSE;
    d = 0;
    if (argc && !JS_ValueToNumber(cx, argv[0], &d))
	return JS_FALSE;
    str = JS_DecompileFunction(cx, fun, (uintN)d);
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

#if JS_HAS_APPLY_FUNCTION
static JSBool
fun_apply(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval fval, *nargv;
    uintN i, nargc, nargvlen;
    JSObject *aobj;
    jsint index, length;
    JSBool ok;

    if (!obj->map->clasp->convert(cx, obj, JSTYPE_FUNCTION, &fval))
	return JS_FALSE;

    if (argc == 0) {
	/* Call fun with its parent as the 'this' parameter if no args. */
	obj = OBJ_GET_PARENT(obj);
    } else {
	/* Otherwise convert the first arg to 'this' and skip over it. */
	if (!JS_ValueToObject(cx, argv[0], &obj))
	    return JS_FALSE;
	argc--;
	argv++;
    }

    nargv = NULL;
    nargc = nargvlen = 0;
    for (i = 0; i < argc; i++) {
	if (JSVAL_IS_OBJECT(argv[i])) {
	    aobj = JSVAL_TO_OBJECT(argv[i]);
	    if (aobj && JS_HasLengthProperty(cx, aobj)) {
		ok = JS_GetArrayLength(cx, aobj, &length);
		if (!ok)
		    goto out;
		if (length == 0)
		    continue;
		if (!nargv)
		    nargvlen = argc;
		nargvlen += (uintN)(length - 1);
		nargv = nargv
			? JS_realloc(cx, nargv, nargvlen * sizeof(jsval))
			: JS_malloc(cx, nargvlen * sizeof(jsval));
		if (!nargv)
		    return JS_FALSE;
		for (index = 0; index < length; index++) {
		    ok = JS_GetElement(cx, aobj, index, &nargv[nargc]);
		    if (!ok)
			goto out;
		    nargc++;
		}
		continue;
	    }
	}

	if (!nargv) {
	    nargvlen = argc;
	    nargv = JS_malloc(cx, nargvlen * sizeof(jsval));
	    if (!nargv)
		return JS_FALSE;
	}
	nargv[nargc] = argv[i];
	nargc++;
    }

    ok = js_Call(cx, obj, fval, nargc, nargv, rval);
out:
    if (nargv)
	JS_free(cx, nargv);
    return ok;
}
#endif /* JS_HAS_APPLY_FUNCTION */

static JSFunctionSpec function_methods[] = {
    {js_toString_str,	fun_toString,	1},
#if JS_HAS_APPLY_FUNCTION
    {"apply",		fun_apply,	1},
#endif
    {0}
};

static JSFunction *
NewFunction(JSContext *cx, JSObject *obj, JSNative call, uintN nargs,
	    uintN flags, JSObject *parent, JSAtom *atom)
{
    JSFunction *fun;

    /* Allocate a function struct. */
    fun = JS_malloc(cx, sizeof *fun);
    if (!fun)
	return NULL;

    /* If obj is null, allocate an object for it. */
    if (!obj) {
	obj = js_NewObject(cx, &js_FunctionClass, NULL, parent);
	if (!obj) {
	    JS_free(cx, fun);
	    return NULL;
	}
    } else {
	OBJ_SET_PARENT(obj, parent);
    }

    /* Link fun to obj and vice versa. */
    fun->object = obj;
    if (!JS_SetPrivate(cx, obj, fun)) {
	cx->newborn[GCX_OBJECT] = NULL;
	JS_free(cx, fun);
	return NULL;
    }

    /* Initialize remaining function members. */
    fun->call = call;
    fun->nargs = nargs;
    fun->flags = flags;
    fun->nvars = 0;
    if (atom)
	JS_LOCK_VOID(cx, fun->atom = js_HoldAtom(cx, atom));
    else
	fun->atom = NULL;
    fun->script = NULL;
    return fun;
}

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
    JSProperty *prop;
    JSSymbol *arg, *args, **argp;
    JSString *str;
    JSStackFrame *fp;
    JSTokenStream *ts;
    JSBool ok;
    JSPrincipals *principals;

    if (obj->map->clasp != &js_FunctionClass) {
	obj = js_NewObject(cx, &js_FunctionClass, NULL, NULL);
	if (!obj)
	    return JS_FALSE;
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
    parent = argv ? OBJ_GET_PARENT(JSVAL_TO_OBJECT(argv[-2])) : NULL;
#else
    /* Set up for dynamic parenting (see Call in jsinterp.c). */
    parent = NULL;
#endif

    fun = NewFunction(cx, obj, NULL, 0, 0, parent,
		      (cx->version >= JSVERSION_1_2)
		      ? NULL
		      : cx->runtime->atomState.anonymousAtom);
    if (!fun)
	return JS_FALSE;

    args = NULL;
    argp = &args;
    n = argc ? argc - 1 : 0;

    JS_LOCK(cx);
    for (i = 0; i < n; i++) {
	atom = js_ValueToStringAtom(cx, argv[i]);
	if (!atom) {
	    ok = JS_FALSE;
	    goto out;
	}
	str = ATOM_TO_STRING(atom);
	if (!js_IsIdentifier(str)) {
	    JS_ReportError(cx, "illegal formal argument name %s",
			   JS_GetStringBytes(str));
	    js_DropAtom(cx, atom);
	    ok = JS_FALSE;
	    goto out;
	}

	if (!js_LookupProperty(cx, obj, (jsval)atom, &obj, &prop)) {
	    js_DropAtom(cx, atom);
	    ok = JS_FALSE;
	    goto out;
	}
	if (prop && prop->object == obj) {
	    PR_ASSERT(prop->getter == js_GetArgument);
	    JS_ReportError(cx, "duplicate formal argument %s",
			   ATOM_BYTES(atom));
	    js_DropAtom(cx, atom);
	    ok = JS_FALSE;
	    goto out;
	}
	prop = js_DefineProperty(cx, obj, (jsval)atom, JSVAL_VOID,
				 js_GetArgument, js_SetArgument,
				 JSPROP_ENUMERATE | JSPROP_PERMANENT);
	js_DropAtom(cx, atom);
	if (!prop) {
	    ok = JS_FALSE;
	    goto out;
	}
	prop->id = INT_TO_JSVAL(fun->nargs++);
	arg = prop->symbols;
	*argp = arg;
	argp = &arg->next;
    }

    if (argc) {
	str = js_ValueToString(cx, argv[argc-1]);
	if (!str) {
	    ok = JS_FALSE;
	    goto out;
	}
	js_LockGCThing(cx, str);
    } else {
	/* Can't use cx->runtime->emptyString because we're called too early. */
	str = js_NewStringCopyZ(cx, js_empty_ucstr, GCF_LOCK);
    }
    if ((fp = cx->fp) && (fp = fp->down) && fp->script) {
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
    if (ts) {
	ok = js_ParseFunctionBody(cx, ts, fun, args) &&
	     js_CloseTokenStream(cx, ts);
    } else {
	ok = JS_FALSE;
    }
    js_UnlockGCThing(cx, str);
out:
    JS_UNLOCK(cx);
    if (!ok)
	return JS_FALSE;
    *rval = OBJECT_TO_JSVAL(obj);
    return JS_TRUE;
}

JSObject *
js_InitFunctionClass(JSContext *cx, JSObject *obj)
{
    JSObject *proto;
    jsval rval;

    proto = JS_InitClass(cx, obj, NULL, &js_FunctionClass, Function, 1,
			 function_props, function_methods, NULL, NULL);
    if (!proto || !Function(cx, proto, 0, NULL, &rval))
	return NULL;

    /* Define fun.length as an alias of fun.arity iff < JS1.2. */
    if (cx->version < JSVERSION_1_2 &&
	!JS_AliasProperty(cx, proto, js_arity_str, js_length_str)) {
	return NULL;
    }
    return proto;
}

JSBool
js_InitCallAndClosureClasses(JSContext *cx, JSObject *obj,
			     JSObject *arrayProto)
{
#if JS_HAS_CALL_OBJECT
    if (!JS_InitClass(cx, obj, arrayProto, &js_CallClass, Call, 0,
		      call_props, call_methods, NULL, NULL)) {
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
js_NewFunction(JSContext *cx, JSNative call, uintN nargs, uintN flags,
	       JSObject *parent, JSAtom *atom)
{
    return NewFunction(cx, NULL, call, nargs, flags, parent, atom);
}

JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, JSAtom *atom, JSNative call,
		  uintN nargs, uintN flags)
{
    JSFunction *fun;

    fun = js_NewFunction(cx, call, nargs, flags, obj, atom);
    if (!fun)
	return NULL;
    if (!js_DefineProperty(cx, obj, (jsval)atom, OBJECT_TO_JSVAL(fun->object),
			   NULL, NULL, flags)) {
	return NULL;
    }
    return fun;
}

JSFunction *
js_ValueToFunction(JSContext *cx, jsval v)
{
    JSObject *obj;
    jsval rval;
    JSString *str;

    obj = NULL;
    if (JSVAL_IS_OBJECT(v)) {
	obj = JSVAL_TO_OBJECT(v);
	if (obj && obj->map->clasp != &js_FunctionClass) {
	    rval = JSVAL_VOID;
	    if (!obj->map->clasp->convert(cx, obj, JSTYPE_FUNCTION, &rval))
		return NULL;
	    if (JSVAL_IS_FUNCTION(rval))
		obj = JSVAL_TO_OBJECT(rval);
	    else
		obj = NULL;
	}
    }
    if (!obj) {
	str = js_ValueToSource(cx, v);
	if (str) {
	    JS_ReportError(cx, "%s is not a function",
			   JS_GetStringBytes(str));
	}
	return NULL;
    }
    return JS_GetPrivate(cx, obj);
}
