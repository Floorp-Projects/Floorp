/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "jsstddef.h"
#include <string.h>
#include "jstypes.h"
#include "jsbit.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
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
#include "jsparse.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsexn.h"

/* Generic function/call/arguments tinyids -- also reflected bit numbers. */
enum {
    CALL_ARGUMENTS  = -1,       /* predefined arguments local variable */
    CALL_CALLEE     = -2,       /* reference to active function's object */
    ARGS_LENGTH     = -3,       /* number of actual args, arity if inactive */
    ARGS_CALLEE     = -4,       /* reference from arguments to active funobj */
    FUN_ARITY       = -5,       /* number of formal parameters; desired argc */
    FUN_NAME        = -6,       /* function name, "" if anonymous */
    FUN_CALLER      = -7        /* Function.prototype.caller, backward compat */
};

#if JSFRAME_OVERRIDE_BITS < 8
# error "not enough override bits in JSStackFrame.flags!"
#endif

#define TEST_OVERRIDE_BIT(fp, tinyid) \
    ((fp)->flags & JS_BIT(JSFRAME_OVERRIDE_SHIFT - ((tinyid) + 1)))

#define SET_OVERRIDE_BIT(fp, tinyid) \
    ((fp)->flags |= JS_BIT(JSFRAME_OVERRIDE_SHIFT - ((tinyid) + 1)))

#if JS_HAS_ARGS_OBJECT

JSBool
js_GetArgsValue(JSContext *cx, JSStackFrame *fp, jsval *vp)
{
    JSObject *argsobj;

    if (TEST_OVERRIDE_BIT(fp, CALL_ARGUMENTS)) {
        JS_ASSERT(fp->callobj);
        return OBJ_GET_PROPERTY(cx, fp->callobj,
                                (jsid) cx->runtime->atomState.argumentsAtom,
                                vp);
    }
    argsobj = js_GetArgsObject(cx, fp);
    if (!argsobj)
        return JS_FALSE;
    *vp = OBJECT_TO_JSVAL(argsobj);
    return JS_TRUE;
}

#define MAXARGS(fp)     ((fp)->fun ? JS_MAX((fp)->argc, (fp)->fun->nargs)     \
                                   : (fp)->argc)

static JSBool
MarkArgDeleted(JSContext *cx, JSStackFrame *fp, uintN slot)
{
    JSObject *argsobj;
    jsval bmapval, bmapint;
    size_t nbits, nbytes;
    jsbitmap *bitmap;

    argsobj = fp->argsobj;
    (void) JS_GetReservedSlot(cx, argsobj, 0, &bmapval);
    nbits = MAXARGS(fp);
    JS_ASSERT(slot < nbits);
    if (JSVAL_IS_VOID(bmapval)) {
        if (nbits <= JSVAL_INT_BITS) {
            bmapint = 0;
            bitmap = (jsbitmap *) &bmapint;
        } else {
            nbytes = JS_HOWMANY(nbits, JS_BITS_PER_WORD) * sizeof(jsbitmap);
            bitmap = (jsbitmap *) JS_malloc(cx, nbytes);
            if (!bitmap)
                return JS_FALSE;
            memset(bitmap, 0, nbytes);
            bmapval = PRIVATE_TO_JSVAL(bitmap);
            JS_SetReservedSlot(cx, argsobj, 0, bmapval);
        }
    } else {
        if (nbits <= JSVAL_INT_BITS) {
            bmapint = JSVAL_TO_INT(bmapval);
            bitmap = (jsbitmap *) &bmapint;
        } else {
            bitmap = (jsbitmap *) JSVAL_TO_PRIVATE(bmapval);
        }
    }
    JS_SET_BIT(bitmap, slot);
    if (bitmap == (jsbitmap *) &bmapint) {
        bmapval = INT_TO_JSVAL(bmapint);
        JS_SetReservedSlot(cx, argsobj, 0, bmapval);
    }
    return JS_TRUE;
}

/* NB: Infallible predicate, false does not mean error/exception. */
static JSBool
ArgWasDeleted(JSContext *cx, JSStackFrame *fp, uintN slot)
{
    JSObject *argsobj;
    jsval bmapval, bmapint;
    jsbitmap *bitmap;

    argsobj = fp->argsobj;
    (void) JS_GetReservedSlot(cx, argsobj, 0, &bmapval);
    if (JSVAL_IS_VOID(bmapval))
        return JS_FALSE;
    if (MAXARGS(fp) <= JSVAL_INT_BITS) {
        bmapint = JSVAL_TO_INT(bmapval);
        bitmap = (jsbitmap *) &bmapint;
    } else {
        bitmap = (jsbitmap *) JSVAL_TO_PRIVATE(bmapval);
    }
    return JS_TEST_BIT(bitmap, slot) != 0;
}

JSBool
js_GetArgsProperty(JSContext *cx, JSStackFrame *fp, jsid id,
                   JSObject **objp, jsval *vp)
{
    jsval val;
    JSObject *obj;
    uintN slot;

    if (TEST_OVERRIDE_BIT(fp, CALL_ARGUMENTS)) {
        JS_ASSERT(fp->callobj);
        if (!OBJ_GET_PROPERTY(cx, fp->callobj,
                              (jsid) cx->runtime->atomState.argumentsAtom,
                              &val)) {
            return JS_FALSE;
        }
        if (JSVAL_IS_PRIMITIVE(val)) {
            obj = js_ValueToNonNullObject(cx, val);
            if (!obj)
                return JS_FALSE;
        } else {
            obj = JSVAL_TO_OBJECT(val);
        }
        *objp = obj;
        return OBJ_GET_PROPERTY(cx, obj, id, vp);
    }

    *objp = NULL;
    *vp = JSVAL_VOID;
    if (JSVAL_IS_INT(id)) {
        slot = (uintN) JSVAL_TO_INT(id);
        if (slot < MAXARGS(fp)) {
            if (fp->argsobj && ArgWasDeleted(cx, fp, slot))
                return OBJ_GET_PROPERTY(cx, fp->argsobj, id, vp);
            *vp = fp->argv[slot];
        }
    } else {
        if (id == (jsid) cx->runtime->atomState.lengthAtom) {
            if (fp->argsobj && TEST_OVERRIDE_BIT(fp, ARGS_LENGTH))
                return OBJ_GET_PROPERTY(cx, fp->argsobj, id, vp);
            *vp = INT_TO_JSVAL((jsint) fp->argc);
        }
    }
    return JS_TRUE;
}

JSObject *
js_GetArgsObject(JSContext *cx, JSStackFrame *fp)
{
    JSObject *argsobj;

    /* Create an arguments object for fp only if it lacks one. */
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
    jsval bmapval, rval;
    JSBool ok;
    JSRuntime *rt;

    /*
     * Reuse args_enumerate here to reflect fp's actual arguments as indexed
     * elements of argsobj.  Do this first, before clearing and freeing the
     * deleted argument slot bitmap, because args_enumerate depends on that.
     */
    argsobj = fp->argsobj;
    ok = args_enumerate(cx, argsobj);

    /*
     * Now clear the deleted argument number bitmap slot and free the bitmap,
     * if one was actually created due to 'delete arguments[0]' or similar.
     */
    (void) JS_GetReservedSlot(cx, argsobj, 0, &bmapval);
    if (!JSVAL_IS_VOID(bmapval)) {
        JS_SetReservedSlot(cx, argsobj, 0, JSVAL_VOID);
        if (MAXARGS(fp) > JSVAL_INT_BITS)
            JS_free(cx, JSVAL_TO_PRIVATE(bmapval));
    }

    /*
     * Now get the prototype properties so we snapshot fp->fun and fp->argc
     * before fp goes away.
     */
    rt = cx->runtime;
    ok &= js_GetProperty(cx, argsobj, (jsid)rt->atomState.calleeAtom, &rval);
    ok &= js_SetProperty(cx, argsobj, (jsid)rt->atomState.calleeAtom, &rval);
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
args_delProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSStackFrame *fp;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    fp = (JSStackFrame *)
         JS_GetInstancePrivate(cx, obj, &js_ArgumentsClass, NULL);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->argsobj);

    slot = JSVAL_TO_INT(id);
    switch (slot) {
      case ARGS_CALLEE:
      case ARGS_LENGTH:
        SET_OVERRIDE_BIT(fp, slot);
        break;

      default:
        if ((uintN)slot < MAXARGS(fp) && !MarkArgDeleted(cx, fp, slot))
            return JS_FALSE;
        break;
    }
    return JS_TRUE;
}

static JSBool
args_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSStackFrame *fp;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    fp = (JSStackFrame *)
         JS_GetInstancePrivate(cx, obj, &js_ArgumentsClass, NULL);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->argsobj);

    slot = JSVAL_TO_INT(id);
    switch (slot) {
      case ARGS_CALLEE:
        if (!TEST_OVERRIDE_BIT(fp, slot))
            *vp = fp->argv ? fp->argv[-2] : OBJECT_TO_JSVAL(fp->fun->object);
        break;

      case ARGS_LENGTH:
        if (!TEST_OVERRIDE_BIT(fp, slot))
            *vp = INT_TO_JSVAL((jsint)fp->argc);
        break;

      default:
        if ((uintN)slot < MAXARGS(fp) && !ArgWasDeleted(cx, fp, slot))
            *vp = fp->argv[slot];
        break;
    }
    return JS_TRUE;
}

static JSBool
args_setProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;
    jsint slot;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    fp = (JSStackFrame *)
         JS_GetInstancePrivate(cx, obj, &js_ArgumentsClass, NULL);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->argsobj);

    slot = JSVAL_TO_INT(id);
    switch (slot) {
      case ARGS_CALLEE:
      case ARGS_LENGTH:
        SET_OVERRIDE_BIT(fp, slot);
        break;

      default:
        if ((uintN)slot < MAXARGS(fp) && !ArgWasDeleted(cx, fp, slot))
            fp->argv[slot] = *vp;
        break;
    }
    return JS_TRUE;
}

static JSBool
args_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
             JSObject **objp)
{
    JSStackFrame *fp;
    uintN slot;
    JSString *str;
    JSAtom *atom;
    intN tinyid;
    jsval value;

    *objp = NULL;
    fp = (JSStackFrame *)
         JS_GetInstancePrivate(cx, obj, &js_ArgumentsClass, NULL);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->argsobj);

    if (JSVAL_IS_INT(id)) {
        slot = JSVAL_TO_INT(id);
        if (slot < MAXARGS(fp) && !ArgWasDeleted(cx, fp, slot)) {
            /* XXX ECMA specs DontEnum, contrary to other array-like objects */
            if (!js_DefineProperty(cx, obj, (jsid) id, fp->argv[slot],
                                   args_getProperty, args_setProperty,
                                   JSVERSION_IS_ECMA(cx->version)
                                   ? 0
                                   : JSPROP_ENUMERATE,
                                   NULL)) {
                return JS_FALSE;
            }
            *objp = obj;
        }
    } else {
        str = JSVAL_TO_STRING(id);
        atom = cx->runtime->atomState.lengthAtom;
        if (str == ATOM_TO_STRING(atom)) {
            tinyid = ARGS_LENGTH;
            value = INT_TO_JSVAL(fp->argc);
        } else {
            atom = cx->runtime->atomState.calleeAtom;
            if (str == ATOM_TO_STRING(atom)) {
                tinyid = ARGS_CALLEE;
                value = fp->argv ? fp->argv[-2]
                                 : OBJECT_TO_JSVAL(fp->fun->object);
            } else {
                atom = NULL;

                /* Quell GCC overwarnings. */
                tinyid = 0;
                value = JSVAL_NULL;
            }
        }

        if (atom && !TEST_OVERRIDE_BIT(fp, tinyid)) {
            if (!js_DefineNativeProperty(cx, obj, (jsid) atom, value,
                                         args_getProperty, args_setProperty, 0,
                                         SPROP_HAS_SHORTID, tinyid, NULL)) {
                return JS_FALSE;
            }
            *objp = obj;
        }
    }

    return JS_TRUE;
}

static JSBool
args_enumerate(JSContext *cx, JSObject *obj)
{
    JSStackFrame *fp;
    JSObject *pobj;
    JSProperty *prop;
    uintN slot, nargs;

    fp = (JSStackFrame *)
         JS_GetInstancePrivate(cx, obj, &js_ArgumentsClass, NULL);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->argsobj);

    /*
     * Trigger reflection with value snapshot in args_resolve using a series
     * of js_LookupProperty calls.  We handle length, callee, and the indexed
     * argument properties.  We know that args_resolve covers all these cases
     * and creates direct properties of obj, but that it may fail to resolve
     * length or callee if overridden.
     */
    if (!js_LookupProperty(cx, obj, (jsid) cx->runtime->atomState.lengthAtom,
                           &pobj, &prop)) {
        return JS_FALSE;
    }
    if (prop)
        OBJ_DROP_PROPERTY(cx, pobj, prop);

    if (!js_LookupProperty(cx, obj, (jsid) cx->runtime->atomState.calleeAtom,
                           &pobj, &prop)) {
        return JS_FALSE;
    }
    if (prop)
        OBJ_DROP_PROPERTY(cx, pobj, prop);

    nargs = MAXARGS(fp);
    for (slot = 0; slot < nargs; slot++) {
        if (!js_LookupProperty(cx, obj, (jsid) INT_TO_JSVAL((jsint)slot),
                               &pobj, &prop)) {
            return JS_FALSE;
        }
        if (prop)
            OBJ_DROP_PROPERTY(cx, pobj, prop);
    }
    return JS_TRUE;
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
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(1),
    JS_PropertyStub,  args_delProperty,
    args_getProperty, args_setProperty,
    args_enumerate,   (JSResolveOp) args_resolve,
    JS_ConvertStub,   JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#endif /* JS_HAS_ARGS_OBJECT */

#if JS_HAS_CALL_OBJECT

JSObject *
js_GetCallObject(JSContext *cx, JSStackFrame *fp, JSObject *parent)
{
    JSObject *callobj, *funobj;

    /* Create a call object for fp only if it lacks one. */
    JS_ASSERT(fp->fun);
    callobj = fp->callobj;
    if (callobj)
        return callobj;
    JS_ASSERT(fp->fun);

    /* The default call parent is its function's parent (static link). */
    if (!parent) {
        funobj = fp->argv ? JSVAL_TO_OBJECT(fp->argv[-2]) : fp->fun->object;
        if (funobj)
            parent = OBJ_GET_PARENT(cx, funobj);
    }

    /* Create the call object and link it to its stack frame. */
    callobj = js_NewObject(cx, &js_CallClass, NULL, parent);
    if (!callobj || !JS_SetPrivate(cx, callobj, fp)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    fp->callobj = callobj;

    /* Make callobj be the scope chain and the variables object. */
    fp->scopeChain = callobj;
    fp->varobj = callobj;
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
    if (fp->argsobj) {
        argsid = (jsid) cx->runtime->atomState.argumentsAtom;
        ok &= js_GetProperty(cx, callobj, argsid, &aval);
        ok &= js_SetProperty(cx, callobj, argsid, &aval);
        ok &= js_PutArgsObject(cx, fp);
    }

    /*
     * Clear the private pointer to fp, which is about to go away (js_Invoke).
     * Do this last because the call_enumerate and js_GetProperty calls above
     * need to follow the private slot to find fp.
     */
    ok &= JS_SetPrivate(cx, callobj, NULL);
    fp->callobj = NULL;
    return ok;
}

static JSPropertySpec call_props[] = {
    {js_arguments_str,  CALL_ARGUMENTS, JSPROP_PERMANENT,0,0},
    {"__callee__",      CALL_CALLEE,    0,0,0},
    {0,0,0,0,0}
};

static JSBool
call_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;
    jsint slot;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->fun);

    slot = JSVAL_TO_INT(id);
    switch (slot) {
      case CALL_ARGUMENTS:
        if (!TEST_OVERRIDE_BIT(fp, slot)) {
            JSObject *argsobj = js_GetArgsObject(cx, fp);
            if (!argsobj)
                return JS_FALSE;
            *vp = OBJECT_TO_JSVAL(argsobj);
        }
        break;

      case CALL_CALLEE:
        if (!TEST_OVERRIDE_BIT(fp, slot))
            *vp = fp->argv ? fp->argv[-2] : OBJECT_TO_JSVAL(fp->fun->object);
        break;

      default:
        if ((uintN)slot < JS_MAX(fp->argc, fp->fun->nargs))
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
    fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->fun);

    slot = JSVAL_TO_INT(id);
    switch (slot) {
      case CALL_ARGUMENTS:
      case CALL_CALLEE:
        SET_OVERRIDE_BIT(fp, slot);
        break;

      default:
        if ((uintN)slot < JS_MAX(fp->argc, fp->fun->nargs))
            fp->argv[slot] = *vp;
        break;
    }
    return JS_TRUE;
}

JSBool
js_GetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;

    JS_ASSERT(JSVAL_IS_INT(id));
    fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
    if (fp) {
        /* XXX no jsint slot commoning here to avoid MSVC1.52 crashes */
        if ((uintN)JSVAL_TO_INT(id) < fp->nvars)
            *vp = fp->vars[JSVAL_TO_INT(id)];
    }
    return JS_TRUE;
}

JSBool
js_SetCallVariable(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    JSStackFrame *fp;

    JS_ASSERT(JSVAL_IS_INT(id));
    fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
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
    JSObject *funobj;
    JSScope *scope;
    JSScopeProperty *sprop, *cprop;
    JSPropertyOp getter;
    jsval *vec;
    JSProperty *prop;

    fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
    if (!fp)
        return JS_TRUE;

    /*
     * Do not enumerate a cloned function object at fp->argv[-2], it may have
     * gained its own (mutable) scope (e.g., a brutally-shared XUL script sets
     * the clone's prototype property).  We must enumerate the function object
     * that was decorated with parameter and local variable properties by the
     * compiler when the compiler created fp->fun, namely fp->fun->object.
     *
     * Contrast with call_resolve, where we prefer fp->argv[-2], because we'll
     * use js_LookupProperty to find any overridden properties in that object,
     * if it was a mutated clone; and if not, we will search its prototype,
     * fp->fun->object, to find compiler-created params and locals.
     */
    funobj = fp->fun->object;
    if (!funobj)
        return JS_TRUE;

    /*
     * Reflect actual args from fp->argv for formal parameters, and local vars
     * and functions in fp->vars for declared variables and nested-at-top-level
     * local functions.
     */
    scope = OBJ_SCOPE(funobj);
    for (sprop = SCOPE_LAST_PROP(scope); sprop; sprop = sprop->parent) {
        getter = sprop->getter;
        if (getter == js_GetArgument)
            vec = fp->argv;
        else if (getter == js_GetLocalVariable)
            vec = fp->vars;
        else
            continue;

        /* Trigger reflection in call_resolve by doing a lookup. */
        if (!js_LookupProperty(cx, obj, sprop->id, &obj, &prop))
            return JS_FALSE;
        JS_ASSERT(obj && prop);
        cprop = (JSScopeProperty *)prop;
        LOCKED_OBJ_SET_SLOT(obj, cprop->slot, vec[sprop->shortid]);
        OBJ_DROP_PROPERTY(cx, obj, prop);
    }

    return JS_TRUE;
}

static JSBool
call_resolve(JSContext *cx, JSObject *obj, jsval id, uintN flags,
             JSObject **objp)
{
    JSStackFrame *fp;
    JSObject *funobj;
    JSString *str;
    JSAtom *atom;
    JSObject *obj2;
    JSProperty *prop;
    JSScopeProperty *sprop;
    jsid propid;
    JSPropertyOp getter, setter;
    uintN attrs, slot, nslots, spflags;
    jsval *vp, value;
    intN shortid;

    fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
    if (!fp)
        return JS_TRUE;
    JS_ASSERT(fp->fun);

    if (!JSVAL_IS_STRING(id))
        return JS_TRUE;

    funobj = fp->argv ? JSVAL_TO_OBJECT(fp->argv[-2]) : fp->fun->object;
    if (!funobj)
        return JS_TRUE;

    str = JSVAL_TO_STRING(id);
    atom = js_AtomizeString(cx, str, 0);
    if (!atom)
        return JS_FALSE;
    if (!js_LookupProperty(cx, funobj, (jsid)atom, &obj2, &prop))
        return JS_FALSE;

    sprop = (JSScopeProperty *) prop;
    if (sprop && OBJ_IS_NATIVE(obj2)) {
        propid = sprop->id;
        getter = sprop->getter;
        attrs = sprop->attrs & ~JSPROP_SHARED;
        slot = (uintN) sprop->shortid;
        OBJ_DROP_PROPERTY(cx, obj2, prop);
        if (getter == js_GetArgument || getter == js_GetLocalVariable) {
            if (getter == js_GetArgument) {
                vp = fp->argv;
                nslots = JS_MAX(fp->argc, fp->fun->nargs);
                getter = setter = NULL;
            } else {
                vp = fp->vars;
                nslots = fp->nvars;
                getter = js_GetCallVariable;
                setter = js_SetCallVariable;
            }
            if (slot < nslots) {
                value = vp[slot];
                spflags = SPROP_HAS_SHORTID;
                shortid = (intN) slot;
            } else {
                value = JSVAL_VOID;
                spflags = 0;
                shortid = 0;
            }
            if (!js_DefineNativeProperty(cx, obj, propid, value,
                                         getter, setter, attrs,
                                         spflags, shortid, NULL)) {
                return JS_FALSE;
            }
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
        fp = (JSStackFrame *) JS_GetPrivate(cx, obj);
        if (fp) {
            JS_ASSERT(fp->fun);
            *vp = fp->argv ? fp->argv[-2] : OBJECT_TO_JSVAL(fp->fun->object);
        }
    }
    return JS_TRUE;
}

JSClass js_CallClass = {
    js_Call_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE,
    JS_PropertyStub,  JS_PropertyStub,
    call_getProperty, call_setProperty,
    call_enumerate,   (JSResolveOp)call_resolve,
    call_convert,     JS_FinalizeStub,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

#endif /* JS_HAS_CALL_OBJECT */

/* SHARED because fun_getProperty always computes a new value. */
#define FUNCTION_PROP_ATTRS (JSPROP_READONLY|JSPROP_PERMANENT|JSPROP_SHARED)

static JSPropertySpec function_props[] = {
    {js_arguments_str, CALL_ARGUMENTS, FUNCTION_PROP_ATTRS,0,0},
    {js_arity_str,     FUN_ARITY,      FUNCTION_PROP_ATTRS,0,0},
    {js_length_str,    ARGS_LENGTH,    FUNCTION_PROP_ATTRS,0,0},
    {js_name_str,      FUN_NAME,       FUNCTION_PROP_ATTRS,0,0},
    {js_caller_str,    FUN_CALLER,     FUNCTION_PROP_ATTRS,0,0},
    {0,0,0,0,0}
};

static JSBool
fun_getProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    jsint slot;
    JSFunction *fun;
    JSStackFrame *fp;

    if (!JSVAL_IS_INT(id))
        return JS_TRUE;
    slot = JSVAL_TO_INT(id);

    /* No valid function object should lack private data, but check anyway. */
    fun = (JSFunction *)JS_GetInstancePrivate(cx, obj, &js_FunctionClass, NULL);
    if (!fun)
        return JS_TRUE;

    /* Find fun's top-most activation record. */
    for (fp = cx->fp; fp && (fp->fun != fun || (fp->flags & JSFRAME_SPECIAL));
         fp = fp->down) {
        continue;
    }

    switch (slot) {
      case CALL_ARGUMENTS:
#if JS_HAS_ARGS_OBJECT
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
#else  /* !JS_HAS_ARGS_OBJECT */
        *vp = OBJECT_TO_JSVAL(fp ? obj : NULL);
        break;
#endif /* !JS_HAS_ARGS_OBJECT */

      case ARGS_LENGTH:
        if (!JSVERSION_IS_ECMA(cx->version))
            *vp = INT_TO_JSVAL((jsint)(fp && fp->fun ? fp->argc : fun->nargs));
        else
      case FUN_ARITY:
            *vp = INT_TO_JSVAL((jsint)fun->nargs);
        break;

      case FUN_NAME:
        *vp = fun->atom
              ? ATOM_KEY(fun->atom)
              : STRING_TO_JSVAL(cx->runtime->emptyString);
        break;

      case FUN_CALLER:
        while (fp && (fp->flags & JSFRAME_SKIP_CALLER) && fp->down)
            fp = fp->down;
        if (fp && fp->down && fp->down->fun && fp->down->argv)
            *vp = fp->down->argv[-2];
        else
            *vp = JSVAL_NULL;
        if (!JSVAL_IS_PRIMITIVE(*vp) && cx->runtime->checkObjectAccess) {
            id = ATOM_KEY(cx->runtime->atomState.callerAtom);
            if (!cx->runtime->checkObjectAccess(cx, obj, id, JSACC_READ, vp))
                return JS_FALSE;
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
    fun = (JSFunction *)JS_GetInstancePrivate(cx, obj, &js_FunctionClass, NULL);
    if (!fun || !fun->object)
        return JS_TRUE;

    /* No need to reflect fun.prototype in 'fun.prototype = ...'. */
    if (flags & JSRESOLVE_ASSIGNING)
        return JS_TRUE;

    /*
     * Ok, check whether id is 'prototype' and bootstrap the function object's
     * prototype property.
     */
    str = JSVAL_TO_STRING(id);
    prototypeAtom = cx->runtime->atomState.classPrototypeAtom;
    if (str == ATOM_TO_STRING(prototypeAtom)) {
        JSObject *proto, *parentProto;
        jsval pval;

        proto = parentProto = NULL;
        if (fun->object != obj && fun->object) {
            /*
             * Clone of a function: make its prototype property value have the
             * same class as the clone-parent's prototype.
             */
            if (!OBJ_GET_PROPERTY(cx, fun->object, (jsid)prototypeAtom, &pval))
                return JS_FALSE;
            if (JSVAL_IS_OBJECT(pval))
                parentProto = JSVAL_TO_OBJECT(pval);
        }

        /*
         * Beware of the wacky case of a user function named Object -- trying
         * to find a prototype for that will recur back here ad perniciem.
         */
        if (!parentProto && fun->atom == cx->runtime->atomState.ObjectAtom)
            return JS_TRUE;

        /*
         * If resolving "prototype" in a clone, clone the parent's prototype.
         * Pass the constructor's (obj's) parent as the prototype parent, to
         * avoid defaulting to parentProto.constructor.__parent__.
         */
        proto = js_NewObject(cx, &js_ObjectClass, parentProto,
                             OBJ_GET_PARENT(cx, obj));
        if (!proto)
            return JS_FALSE;

        /*
         * ECMA (15.3.5.2) says that constructor.prototype is DontDelete for
         * user-defined functions, but DontEnum | ReadOnly | DontDelete for
         * native "system" constructors such as Object or Function.  So lazily
         * set the former here in fun_resolve, but eagerly define the latter
         * in JS_InitClass, with the right attributes.
         */
        if (!js_SetClassPrototype(cx, obj, proto,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT)) {
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
        return JS_TRUE;
      default:
        return js_TryValueOf(cx, obj, type, vp);
    }
}

static void
fun_finalize(JSContext *cx, JSObject *obj)
{
    JSFunction *fun;

    /* No valid function object should lack private data, but check anyway. */
    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    if (!fun)
        return;
    if (fun->object == obj)
        fun->object = NULL;
    JS_ATOMIC_DECREMENT(&fun->nrefs);
    if (fun->nrefs)
        return;
    if (fun->interpreted)
        js_DestroyScript(cx, fun->u.script);
    JS_free(cx, fun);
}

#if JS_HAS_XDR

#include "jsxdrapi.h"

enum {
    JSXDR_FUNARG = 1,
    JSXDR_FUNVAR = 2,
    JSXDR_FUNCONST = 3
};

/* XXX store parent and proto, if defined */
static JSBool
fun_xdrObject(JSXDRState *xdr, JSObject **objp)
{
    JSContext *cx;
    JSFunction *fun;
    JSString *atomstr;
    uint32 flagsword;           /* originally only flags was JS_XDRUint8'd */
    char *propname;
    JSScopeProperty *sprop;
    uint32 userid;              /* NB: holds a signed int-tagged jsval */
    JSAtom *atom;
    uintN i, n, dupflag;
    uint32 type;
#ifdef DEBUG
    uintN nvars = 0, nargs = 0;
#endif

    cx = xdr->cx;
    if (xdr->mode == JSXDR_ENCODE) {
        /*
         * No valid function object should lack private data, but fail soft
         * (return true, no error report) in case one does due to API pilot
         * or internal error.
         */
        fun = (JSFunction *) JS_GetPrivate(cx, *objp);
        if (!fun)
            return JS_TRUE;
        if (!fun->interpreted) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_NOT_SCRIPTED_FUNCTION,
                                 JS_GetFunctionName(fun));
            return JS_FALSE;
        }
        atomstr = fun->atom ? ATOM_TO_STRING(fun->atom) : NULL;
        flagsword = ((uint32)fun->nregexps << 16) | fun->flags;
    } else {
        fun = js_NewFunction(cx, NULL, NULL, 0, 0, NULL, NULL);
        if (!fun)
            return JS_FALSE;
        atomstr = NULL;
    }

    if (!JS_XDRStringOrNull(xdr, &atomstr) ||
        !JS_XDRUint16(xdr, &fun->nargs) ||
        !JS_XDRUint16(xdr, &fun->extra) ||
        !JS_XDRUint16(xdr, &fun->nvars) ||
        !JS_XDRUint32(xdr, &flagsword)) {
        return JS_FALSE;
    }

    /* do arguments and local vars */
    if (fun->object) {
        n = fun->nargs + fun->nvars;
        if (xdr->mode == JSXDR_ENCODE) {
            JSScope *scope;
            JSScopeProperty **spvec, *auto_spvec[8];
            void *mark;

            if (n <= sizeof auto_spvec / sizeof auto_spvec[0]) {
                spvec = auto_spvec;
                mark = NULL;
            } else {
                mark = JS_ARENA_MARK(&cx->tempPool);
                JS_ARENA_ALLOCATE_CAST(spvec, JSScopeProperty **, &cx->tempPool,
                                       n * sizeof(JSScopeProperty *));
                if (!spvec) {
                    JS_ReportOutOfMemory(cx);
                    return JS_FALSE;
                }
            }
            scope = OBJ_SCOPE(fun->object);
            for (sprop = SCOPE_LAST_PROP(scope); sprop;
                 sprop = sprop->parent) {
                if (sprop->getter == js_GetArgument) {
                    JS_ASSERT(nargs++ <= fun->nargs);
                    spvec[sprop->shortid] = sprop;
                } else if (sprop->getter == js_GetLocalVariable) {
                    JS_ASSERT(nvars++ <= fun->nvars);
                    spvec[fun->nargs + sprop->shortid] = sprop;
                }
            }
            for (i = 0; i < n; i++) {
                sprop = spvec[i];
                JS_ASSERT(sprop->flags & SPROP_HAS_SHORTID);
                type = (i < fun->nargs)
                       ? JSXDR_FUNARG
                       : (sprop->attrs & JSPROP_READONLY)
                       ? JSXDR_FUNCONST
                       : JSXDR_FUNVAR;
                userid = INT_TO_JSVAL(sprop->shortid);
                /* XXX lossy conversion, need new XDR version for ECMAv3 */
                propname = JS_GetStringBytes(ATOM_TO_STRING((JSAtom *)sprop->id));
                if (!propname ||
                    !JS_XDRUint32(xdr, &type) ||
                    !JS_XDRUint32(xdr, &userid) ||
                    !JS_XDRCString(xdr, &propname)) {
                    if (mark)
                        JS_ARENA_RELEASE(&cx->tempPool, mark);
                    return JS_FALSE;
                }
            }
            if (mark)
                JS_ARENA_RELEASE(&cx->tempPool, mark);
        } else {
            JSPropertyOp getter, setter;

            for (i = n; i != 0; i--) {
                uintN attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT;

                if (!JS_XDRUint32(xdr, &type) ||
                    !JS_XDRUint32(xdr, &userid) ||
                    !JS_XDRCString(xdr, &propname)) {
                    return JS_FALSE;
                }
                JS_ASSERT(type == JSXDR_FUNARG || type == JSXDR_FUNVAR ||
                          type == JSXDR_FUNCONST);
                if (type == JSXDR_FUNARG) {
                    getter = js_GetArgument;
                    setter = js_SetArgument;
                    JS_ASSERT(nargs++ <= fun->nargs);
                } else if (type == JSXDR_FUNVAR || type == JSXDR_FUNCONST) {
                    getter = js_GetLocalVariable;
                    setter = js_SetLocalVariable;
                    if (type == JSXDR_FUNCONST)
                        attrs |= JSPROP_READONLY;
                    JS_ASSERT(nvars++ <= fun->nvars);
                } else {
                    getter = NULL;
                    setter = NULL;
                }
                atom = js_Atomize(cx, propname, strlen(propname), 0);
                JS_free(cx, propname);
                if (!atom)
                    return JS_FALSE;

                /* Flag duplicate argument if atom is bound in fun->object. */
                dupflag = SCOPE_GET_PROPERTY(OBJ_SCOPE(fun->object), (jsid)atom)
                          ? SPROP_IS_DUPLICATE
                          : 0;

                if (!js_AddNativeProperty(cx, fun->object, (jsid)atom,
                                          getter, setter, SPROP_INVALID_SLOT,
                                          attrs | JSPROP_SHARED,
                                          SPROP_HAS_SHORTID | dupflag,
                                          JSVAL_TO_INT(userid))) {
                    return JS_FALSE;
                }
            }
        }
    }

    if (!js_XDRScript(xdr, &fun->u.script, NULL))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
        fun->interpreted = JS_TRUE;
        fun->flags = (uint8) flagsword;
        fun->nregexps = (uint16) (flagsword >> 16);

        *objp = fun->object;
        if (atomstr) {
            /* XXX only if this was a top-level function! */
            fun->atom = js_AtomizeString(cx, atomstr, 0);
            if (!fun->atom)
                return JS_FALSE;
        }

        js_CallNewScriptHook(cx, fun->u.script, fun);
    }

    return JS_TRUE;
}

#else  /* !JS_HAS_XDR */

#define fun_xdrObject NULL

#endif /* !JS_HAS_XDR */

#if JS_HAS_INSTANCEOF

/*
 * [[HasInstance]] internal method for Function objects: fetch the .prototype
 * property of its 'this' parameter, and walks the prototype chain of v (only
 * if v is an object) returning true if .prototype is found.
 */
static JSBool
fun_hasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    jsval pval, cval;
    JSString *str;
    JSObject *proto, *obj2;
    JSFunction *cfun, *ofun;

    if (!OBJ_GET_PROPERTY(cx, obj,
                          (jsid)cx->runtime->atomState.classPrototypeAtom,
                          &pval)) {
        return JS_FALSE;
    }

    if (JSVAL_IS_PRIMITIVE(pval)) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        str = js_DecompileValueGenerator(cx, -1, OBJECT_TO_JSVAL(obj), NULL);
        if (str) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_BAD_PROTOTYPE, JS_GetStringBytes(str));
        }
        return JS_FALSE;
    }

    proto = JSVAL_TO_OBJECT(pval);
    return js_IsDelegate(cx, proto, v, bp);
}

#else  /* !JS_HAS_INSTANCEOF */

#define fun_hasInstance NULL

#endif /* !JS_HAS_INSTANCEOF */

static uint32
fun_mark(JSContext *cx, JSObject *obj, void *arg)
{
    JSFunction *fun;

    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    if (fun) {
        if (fun->atom)
            GC_MARK_ATOM(cx, fun->atom, arg);
        if (fun->interpreted)
            js_MarkScript(cx, fun->u.script, arg);
    }
    return 0;
}

static uint32
fun_reserveSlots(JSContext *cx, JSObject *obj)
{
    JSFunction *fun;

    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    return fun ? fun->nregexps : 0;
}

/*
 * Reserve two slots in all function objects for XPConnect.  Note that this
 * does not bloat every instance, only those on which reserved slots are set,
 * and those on which ad-hoc properties are defined.
 */
JSClass js_FunctionClass = {
    js_Function_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE | JSCLASS_HAS_RESERVED_SLOTS(2),
    JS_PropertyStub,  JS_PropertyStub,
    fun_getProperty,  JS_PropertyStub,
    JS_EnumerateStub, (JSResolveOp)fun_resolve,
    fun_convert,      fun_finalize,
    NULL,             NULL,
    NULL,             NULL,
    fun_xdrObject,    fun_hasInstance,
    fun_mark,         fun_reserveSlots
};

JSBool
js_fun_toString(JSContext *cx, JSObject *obj, uint32 indent,
                uintN argc, jsval *argv, jsval *rval)
{
    jsval fval;
    JSFunction *fun;
    JSString *str;

    if (!argv) {
        JS_ASSERT(JS_ObjectIsFunction(cx, obj));
    } else {
        fval = argv[-1];
        if (!JSVAL_IS_FUNCTION(cx, fval)) {
            /*
             * If we don't have a function to start off with, try converting
             * the object to a function.  If that doesn't work, complain.
             */
            if (JSVAL_IS_OBJECT(fval)) {
                obj = JSVAL_TO_OBJECT(fval);
                if (!OBJ_GET_CLASS(cx, obj)->convert(cx, obj, JSTYPE_FUNCTION,
                                                     &fval)) {
                    return JS_FALSE;
                }
            }
            if (!JSVAL_IS_FUNCTION(cx, fval)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_INCOMPATIBLE_PROTO,
                                     js_Function_str, js_toString_str,
                                     JS_GetTypeName(cx,
                                                    JS_TypeOfValue(cx, fval)));
                return JS_FALSE;
            }
        }

        obj = JSVAL_TO_OBJECT(fval);
    }

    fun = (JSFunction *) JS_GetPrivate(cx, obj);
    if (!fun)
        return JS_TRUE;
    if (argc && !js_ValueToECMAUint32(cx, argv[0], &indent))
        return JS_FALSE;
    str = JS_DecompileFunction(cx, fun, (uintN)indent);
    if (!str)
        return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSBool
fun_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return js_fun_toString(cx, obj, 0, argc, argv, rval);
}

#if JS_HAS_TOSOURCE
static JSBool
fun_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    return js_fun_toString(cx, obj, JS_DONT_PRETTY_PRINT, argc, argv, rval);
}
#endif

static const char js_call_str[] = "call";

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

    if (!JSVAL_IS_FUNCTION(cx, fval)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_call_str,
                             JS_GetStringBytes(JS_ValueToString(cx, fval)));
        return JS_FALSE;
    }

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
    ok = js_Invoke(cx, argc, JSINVOKE_INTERNAL | JSINVOKE_SKIP_CALLER);

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

    if (argc == 0) {
        /* Will get globalObject as 'this' and no other arguments. */
        return fun_call(cx, obj, argc, argv, rval);
    }

    if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_FUNCTION, &argv[-1]))
        return JS_FALSE;
    fval = argv[-1];

    if (!JSVAL_IS_FUNCTION(cx, fval)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, "apply",
                             JS_GetStringBytes(JS_ValueToString(cx, fval)));
        return JS_FALSE;
    }

    /* Quell GCC overwarnings. */
    aobj = NULL;
    length = 0;

    if (argc >= 2) {
        /* If the 2nd arg is null or void, call the function with 0 args. */
        if (JSVAL_IS_NULL(argv[1]) || JSVAL_IS_VOID(argv[1])) {
            argc = 0;
        } else {
            /* The second arg must be an array (or arguments object). */
            if (JSVAL_IS_PRIMITIVE(argv[1]) ||
                (aobj = JSVAL_TO_OBJECT(argv[1]),
                OBJ_GET_CLASS(cx, aobj) != &js_ArgumentsClass &&
                OBJ_GET_CLASS(cx, aobj) != &js_ArrayClass))
            {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_APPLY_ARGS);
                return JS_FALSE;
            }
            if (!js_GetLengthProperty(cx, aobj, &length))
                return JS_FALSE;
        }
    }

    /* Convert the first arg to 'this' and skip over it. */
    if (!js_ValueToObject(cx, argv[0], &obj))
        return JS_FALSE;

    /* Allocate stack space for fval, obj, and the args. */
    argc = (uintN)JS_MIN(length, ARGC_LIMIT - 1);
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
    ok = js_Invoke(cx, argc, JSINVOKE_INTERNAL | JSINVOKE_SKIP_CALLER);

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
    {js_toSource_str,   fun_toSource,   0,0,0},
#endif
    {js_toString_str,   fun_toString,   1,0,0},
#if JS_HAS_APPLY_FUNCTION
    {"apply",           fun_apply,      2,0,0},
#endif
#if JS_HAS_CALL_FUNCTION
    {js_call_str,       fun_call,       1,0,0},
#endif
    {0,0,0,0,0}
};

JSBool
js_IsIdentifier(JSString *str)
{
    size_t n;
    jschar *s, c;

    n = JSSTRING_LENGTH(str);
    if (n == 0)
        return JS_FALSE;
    s = JSSTRING_CHARS(str);
    c = *s;
    if (!JS_ISIDENT_START(c))
        return JS_FALSE;
    for (n--; n != 0; n--) {
        c = *++s;
        if (!JS_ISIDENT(c))
            return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
Function(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSStackFrame *fp, *caller;
    JSFunction *fun;
    JSObject *parent;
    uintN i, n, lineno, dupflag;
    JSAtom *atom;
    const char *filename;
    JSObject *obj2;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSString *str, *arg;
    void *mark;
    JSTokenStream *ts;
    JSPrincipals *principals;
    jschar *collected_args, *cp;
    size_t arg_length, args_length;
    JSTokenType tt;
    JSBool ok;

    fp = cx->fp;
    if (fp && !(fp->flags & JSFRAME_CONSTRUCTING)) {
        obj = js_NewObject(cx, &js_FunctionClass, NULL, NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }
    fun = (JSFunction *) JS_GetPrivate(cx, obj);
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
    /* Set up for dynamic parenting (see js_Invoke in jsinterp.c). */
    parent = NULL;
#endif

    fun = js_NewFunction(cx, obj, NULL, 0, JSFUN_LAMBDA, parent,
                         JSVERSION_IS_ECMA(cx->version)
                         ? cx->runtime->atomState.anonymousAtom
                         : NULL);

    if (!fun)
        return JS_FALSE;

    /*
     * Function is static and not called directly by other functions in this
     * file, therefore it is callable only as a native function by js_Invoke.
     * Find the scripted caller, possibly skipping other native frames such as
     * are built for Function.prototype.call or .apply activations that invoke
     * Function indirectly from a script.
     */
    JS_ASSERT(!fp->script && fp->fun && fp->fun->u.native == Function);
    caller = JS_GetScriptedCaller(cx, fp);
    if (caller) {
        filename = caller->script->filename;
        lineno = js_PCToLineNumber(cx, caller->script, caller->pc);
        principals = JS_EvalFramePrincipals(cx, fp, caller);
    } else {
        filename = NULL;
        lineno = 0;
        principals = NULL;
    }

    n = argc ? argc - 1 : 0;
    if (n > 0) {
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
        args_length = 0;
        for (i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            arg = js_ValueToString(cx, argv[i]);
            if (!arg)
                return JS_FALSE;
            argv[i] = STRING_TO_JSVAL(arg);
            args_length += JSSTRING_LENGTH(arg);
        }
        /* Add 1 for each joining comma. */
        args_length += n - 1;

        /*
         * Allocate a string to hold the concatenated arguments, including room
         * for a terminating 0.  Mark cx->tempPool for later release, to free
         * collected_args and its tokenstream in one swoop.
         */
        mark = JS_ARENA_MARK(&cx->tempPool);
        JS_ARENA_ALLOCATE_CAST(cp, jschar *, &cx->tempPool,
                               (args_length+1) * sizeof(jschar));
        if (!cp)
            return JS_FALSE;
        collected_args = cp;

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (i = 0; i < n; i++) {
            arg = JSVAL_TO_STRING(argv[i]);
            arg_length = JSSTRING_LENGTH(arg);
            (void) js_strncpy(cp, JSSTRING_CHARS(arg), arg_length);
            cp += arg_length;

            /* Add separating comma or terminating 0. */
            *cp++ = (i + 1 < n) ? ',' : 0;
        }

        /*
         * Make a tokenstream (allocated from cx->tempPool) that reads from
         * the given string.
         */
        ts = js_NewTokenStream(cx, collected_args, args_length, filename,
                               lineno, principals);
        if (!ts) {
            JS_ARENA_RELEASE(&cx->tempPool, mark);
            return JS_FALSE;
        }

        /* The argument string may be empty or contain no tokens. */
        tt = js_GetToken(cx, ts);
        if (tt != TOK_EOF) {
            for (;;) {
                /*
                 * Check that it's a name.  This also implicitly guards against
                 * TOK_ERROR, which was already reported.
                 */
                if (tt != TOK_NAME)
                    goto bad_formal;

                /*
                 * Get the atom corresponding to the name from the tokenstream;
                 * we're assured at this point that it's a valid identifier.
                 */
                atom = CURRENT_TOKEN(ts).t_atom;
                if (!js_LookupProperty(cx, obj, (jsid)atom, &obj2, &prop))
                    goto bad_formal;
                sprop = (JSScopeProperty *) prop;
                dupflag = 0;
                if (sprop) {
                    ok = JS_TRUE;
                    if (obj2 == obj) {
                        const char *name = js_AtomToPrintableString(cx, atom);

                        /*
                         * A duplicate parameter name. We force a duplicate
                         * node on the SCOPE_LAST_PROP(scope) list with the
                         * same id, distinguished by the SPROP_IS_DUPLICATE
                         * flag, and not mapped by an entry in scope.
                         */
                        JS_ASSERT(sprop->getter == js_GetArgument);
                        ok = name &&
                             js_ReportCompileErrorNumber(cx, ts, NULL,
                                                         JSREPORT_WARNING |
                                                         JSREPORT_STRICT,
                                                         JSMSG_DUPLICATE_FORMAL,
                                                         name);

                        dupflag = SPROP_IS_DUPLICATE;
                    }
                    OBJ_DROP_PROPERTY(cx, obj2, prop);
                    if (!ok)
                        goto bad_formal;
                    sprop = NULL;
                }
                if (!js_AddNativeProperty(cx, fun->object, (jsid)atom,
                                          js_GetArgument, js_SetArgument,
                                          SPROP_INVALID_SLOT,
                                          JSPROP_ENUMERATE | JSPROP_PERMANENT |
                                          JSPROP_SHARED,
                                          SPROP_HAS_SHORTID | dupflag,
                                          fun->nargs)) {
                    goto bad_formal;
                }
                fun->nargs++;

                /*
                 * Get the next token.  Stop on end of stream.  Otherwise
                 * insist on a comma, get another name, and iterate.
                 */
                tt = js_GetToken(cx, ts);
                if (tt == TOK_EOF)
                    break;
                if (tt != TOK_COMMA)
                    goto bad_formal;
                tt = js_GetToken(cx, ts);
            }
        }

        /* Clean up. */
        ok = js_CloseTokenStream(cx, ts);
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (!ok)
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
        argv[(intN)(argc-1)] = STRING_TO_JSVAL(str);
    }

    mark = JS_ARENA_MARK(&cx->tempPool);
    ts = js_NewTokenStream(cx, JSSTRING_CHARS(str), JSSTRING_LENGTH(str),
                           filename, lineno, principals);
    if (!ts) {
        ok = JS_FALSE;
    } else {
        ok = js_CompileFunctionBody(cx, ts, fun) &&
             js_CloseTokenStream(cx, ts);
    }
    JS_ARENA_RELEASE(&cx->tempPool, mark);
    return ok;

bad_formal:
    /*
     * Report "malformed formal parameter" iff no illegal char or similar
     * scanner error was already reported.
     */
    if (!(ts->flags & TSF_ERROR))
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_FORMAL);

    /*
     * Clean up the arguments string and tokenstream if we failed to parse
     * the arguments.
     */
    (void)js_CloseTokenStream(cx, ts);
    JS_ARENA_RELEASE(&cx->tempPool, mark);
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
    fun = js_NewFunction(cx, proto, NULL, 0, 0, obj, NULL);
    if (!fun)
        goto bad;
    fun->u.script = js_NewScript(cx, 0, 0, 0);
    if (!fun->u.script)
        goto bad;
    fun->interpreted = JS_TRUE;
    return proto;

bad:
    cx->newborn[GCX_OBJECT] = NULL;
    return NULL;
}

#if JS_HAS_CALL_OBJECT
JSObject *
js_InitCallClass(JSContext *cx, JSObject *obj)
{
    return JS_InitClass(cx, obj, NULL, &js_CallClass, NULL, 0,
                        call_props, NULL, NULL, NULL);
}
#endif

JSFunction *
js_NewFunction(JSContext *cx, JSObject *funobj, JSNative native, uintN nargs,
               uintN flags, JSObject *parent, JSAtom *atom)
{
    JSFunction *fun;

    /* Allocate a function struct. */
    fun = (JSFunction *) JS_malloc(cx, sizeof *fun);
    if (!fun)
        return NULL;

    /* If funobj is null, allocate an object for it. */
    if (funobj) {
        OBJ_SET_PARENT(cx, funobj, parent);
    } else {
        funobj = js_NewObject(cx, &js_FunctionClass, NULL, parent);
        if (!funobj) {
            JS_free(cx, fun);
            return NULL;
        }
    }

    /* Initialize all function members. */
    fun->nrefs = 0;
    fun->object = NULL;
    fun->u.native = native;
    fun->nargs = nargs;
    fun->extra = 0;
    fun->nvars = 0;
    fun->flags = flags & JSFUN_FLAGS_MASK;
    fun->interpreted = JS_FALSE;
    fun->nregexps = 0;
    fun->spare = 0;
    fun->atom = atom;
    fun->clasp = NULL;

    /* Link fun to funobj and vice versa. */
    if (!js_LinkFunctionObject(cx, fun, funobj)) {
        cx->newborn[GCX_OBJECT] = NULL;
        JS_free(cx, fun);
        return NULL;
    }
    return fun;
}

JSObject *
js_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent)
{
    JSObject *newfunobj;
    JSFunction *fun;

    JS_ASSERT(OBJ_GET_CLASS(cx, funobj) == &js_FunctionClass);
    newfunobj = js_NewObject(cx, &js_FunctionClass, funobj, parent);
    if (!newfunobj)
        return NULL;
    fun = (JSFunction *) JS_GetPrivate(cx, funobj);
    if (!js_LinkFunctionObject(cx, fun, newfunobj)) {
        cx->newborn[GCX_OBJECT] = NULL;
        return NULL;
    }
    return newfunobj;
}

JSBool
js_LinkFunctionObject(JSContext *cx, JSFunction *fun, JSObject *funobj)
{
    if (!fun->object)
        fun->object = funobj;
    if (!JS_SetPrivate(cx, funobj, fun))
        return JS_FALSE;
    JS_ATOMIC_INCREMENT(&fun->nrefs);
    return JS_TRUE;
}

JSFunction *
js_DefineFunction(JSContext *cx, JSObject *obj, JSAtom *atom, JSNative native,
                  uintN nargs, uintN attrs)
{
    JSFunction *fun;

    fun = js_NewFunction(cx, NULL, native, nargs, attrs, obj, atom);
    if (!fun)
        return NULL;
    if (!OBJ_DEFINE_PROPERTY(cx, obj, (jsid)atom, OBJECT_TO_JSVAL(fun->object),
                             NULL, NULL, attrs & ~JSFUN_FLAGS_MASK, NULL)) {
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
            if (!OBJ_DEFAULT_VALUE(cx, obj, JSTYPE_FUNCTION, &v))
                return NULL;
            obj = JSVAL_IS_FUNCTION(cx, v) ? JSVAL_TO_OBJECT(v) : NULL;
        }
    }
    if (!obj) {
        js_ReportIsNotFunction(cx, vp, flags);
        return NULL;
    }
    return (JSFunction *) JS_GetPrivate(cx, obj);
}

void
js_ReportIsNotFunction(JSContext *cx, jsval *vp, uintN flags)
{
    JSType type;
    JSString *fallback;
    JSString *str;

    /*
     * We provide the typename as the fallback to handle the case when
     * valueOf is not a function, which prevents ValueToString from being
     * called as the default case inside js_DecompileValueGenerator (and
     * so recursing back to here).
     */
    type = JS_TypeOfValue(cx, *vp);
    fallback = ATOM_TO_STRING(cx->runtime->atomState.typeAtoms[type]);
    str = js_DecompileValueGenerator(cx,
                                     (flags & JSV2F_SEARCH_STACK)
                                     ? JSDVG_SEARCH_STACK
                                     : cx->fp
                                     ? vp - cx->fp->sp
                                     : JSDVG_IGNORE_STACK,
                                     *vp,
                                     fallback);
    if (str) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             (uintN)((flags & JSV2F_CONSTRUCT)
                                     ? JSMSG_NOT_CONSTRUCTOR
                                     : JSMSG_NOT_FUNCTION),
                             JS_GetStringBytes(str));
    }
}
