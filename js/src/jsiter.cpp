/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
 * JavaScript iterators.
 */
#include <string.h>     /* for memcpy */
#include "jstypes.h"
#include "jsstdint.h"
#include "jsutil.h"
#include "jsarena.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jstracer.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

JS_STATIC_ASSERT(JSSLOT_ITER_FLAGS < JS_INITIAL_NSLOTS);

#if JS_HAS_GENERATORS

static JS_REQUIRES_STACK JSBool
CloseGenerator(JSContext *cx, JSObject *genobj);

#endif

/*
 * Shared code to close iterator's state either through an explicit call or
 * when GC detects that the iterator is no longer reachable.
 */
void
js_CloseNativeIterator(JSContext *cx, JSObject *iterobj)
{
    jsval state;
    JSObject *iterable;

    JS_ASSERT(STOBJ_GET_CLASS(iterobj) == &js_IteratorClass);

    /* Avoid double work if js_CloseNativeIterator was called on obj. */
    state = STOBJ_GET_SLOT(iterobj, JSSLOT_ITER_STATE);
    if (JSVAL_IS_NULL(state))
        return;

    /* Protect against failure to fully initialize obj. */
    iterable = iterobj->getParent();
    if (iterable) {
#if JS_HAS_XML_SUPPORT
        uintN flags = JSVAL_TO_INT(STOBJ_GET_SLOT(iterobj, JSSLOT_ITER_FLAGS));
        if ((flags & JSITER_FOREACH) && OBJECT_IS_XML(cx, iterable)) {
            js_EnumerateXMLValues(cx, iterable, JSENUMERATE_DESTROY, &state,
                                  NULL, NULL);
        } else
#endif
            iterable->enumerate(cx, JSENUMERATE_DESTROY, &state, NULL);
    }
    STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_STATE, JSVAL_NULL);
}

static void
iterator_trace(JSTracer *trc, JSObject *obj)
{
    /*
     * The GC marks iter_state during the normal slot scanning if
     * JSVAL_IS_TRACEABLE(iter_state) is true duplicating the efforts of
     * js_MarkEnumeratorState. But this is rare so we optimize for code
     * simplicity.
     */
    JSObject *iterable = obj->getParent();
    if (!iterable) {
        /* for (x in null) creates an iterator object with a null parent. */
        return;
    }
    jsval iter_state = obj->fslots[JSSLOT_ITER_STATE];
    js_MarkEnumeratorState(trc, iterable, iter_state);
}

JSClass js_IteratorClass = {
    "Iterator",
    JSCLASS_HAS_RESERVED_SLOTS(2) | /* slots for state and flags */
    JSCLASS_HAS_CACHED_PROTO(JSProto_Iterator) |
    JSCLASS_MARK_IS_TRACE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   NULL,
    NULL,             NULL,            NULL,            NULL,
    NULL,             NULL,            JS_CLASS_TRACE(iterator_trace), NULL
};

static JSBool
InitNativeIterator(JSContext *cx, JSObject *iterobj, JSObject *obj, uintN flags)
{
    jsval state;
    JSBool ok;

    JS_ASSERT(STOBJ_GET_CLASS(iterobj) == &js_IteratorClass);

    /* Initialize iterobj in case of enumerate hook failure. */
    STOBJ_SET_PARENT(iterobj, obj);
    STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_STATE, JSVAL_NULL);
    STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_FLAGS, INT_TO_JSVAL(flags));
    if (!js_RegisterCloseableIterator(cx, iterobj))
        return JS_FALSE;
    if (!obj)
        return JS_TRUE;

    ok =
#if JS_HAS_XML_SUPPORT
         ((flags & JSITER_FOREACH) && OBJECT_IS_XML(cx, obj))
         ? js_EnumerateXMLValues(cx, obj, JSENUMERATE_INIT, &state, NULL, NULL)
         :
#endif
           obj->enumerate(cx, JSENUMERATE_INIT, &state, NULL);
    if (!ok)
        return JS_FALSE;

    STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_STATE, state);
    if (flags & JSITER_ENUMERATE) {
        /*
         * The enumerating iterator needs the original object to suppress
         * enumeration of deleted or shadowed prototype properties. Since the
         * enumerator never escapes to scripts, we use the prototype slot to
         * store the original object.
         */
        JS_ASSERT(obj != iterobj);
        STOBJ_SET_PROTO(iterobj, obj);
    }
    return JS_TRUE;
}

static JSBool
Iterator(JSContext *cx, JSObject *iterobj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool keyonly;
    uintN flags;
    JSObject *obj;

    keyonly = js_ValueToBoolean(argv[1]);
    flags = keyonly ? 0 : JSITER_FOREACH;

    if (JS_IsConstructing(cx)) {
        /* XXX work around old valueOf call hidden beneath js_ValueToObject */
        if (!JSVAL_IS_PRIMITIVE(argv[0])) {
            obj = JSVAL_TO_OBJECT(argv[0]);
        } else {
            obj = js_ValueToNonNullObject(cx, argv[0]);
            if (!obj)
                return JS_FALSE;
            argv[0] = OBJECT_TO_JSVAL(obj);
        }
        return InitNativeIterator(cx, iterobj, obj, flags);
    }

    *rval = argv[0];
    return js_ValueToIterator(cx, flags, rval);
}

static JSBool
NewKeyValuePair(JSContext *cx, jsid key, jsval val, jsval *rval)
{
    jsval vec[2];
    JSTempValueRooter tvr;
    JSObject *aobj;

    vec[0] = ID_TO_VALUE(key);
    vec[1] = val;

    JS_PUSH_TEMP_ROOT(cx, 2, vec, &tvr);
    aobj = js_NewArrayObject(cx, 2, vec);
    *rval = OBJECT_TO_JSVAL(aobj);
    JS_POP_TEMP_ROOT(cx, &tvr);

    return aobj != NULL;
}

static JSBool
IteratorNextImpl(JSContext *cx, JSObject *obj, jsval *rval)
{
    JSObject *iterable;
    jsval state;
    uintN flags;
    JSBool foreach, ok;
    jsid id;

    JS_ASSERT(OBJ_GET_CLASS(cx, obj) == &js_IteratorClass);

    iterable = OBJ_GET_PARENT(cx, obj);
    JS_ASSERT(iterable);
    state = STOBJ_GET_SLOT(obj, JSSLOT_ITER_STATE);
    if (JSVAL_IS_NULL(state))
        goto stop;

    flags = JSVAL_TO_INT(STOBJ_GET_SLOT(obj, JSSLOT_ITER_FLAGS));
    JS_ASSERT(!(flags & JSITER_ENUMERATE));
    foreach = (flags & JSITER_FOREACH) != 0;
    ok =
#if JS_HAS_XML_SUPPORT
         (foreach && OBJECT_IS_XML(cx, iterable))
         ? js_EnumerateXMLValues(cx, iterable, JSENUMERATE_NEXT, &state,
                                 &id, rval)
         :
#endif
           iterable->enumerate(cx, JSENUMERATE_NEXT, &state, &id);
    if (!ok)
        return JS_FALSE;

    STOBJ_SET_SLOT(obj, JSSLOT_ITER_STATE, state);
    if (JSVAL_IS_NULL(state))
        goto stop;

    if (foreach) {
#if JS_HAS_XML_SUPPORT
        if (!OBJECT_IS_XML(cx, iterable) &&
            !iterable->getProperty(cx, id, rval)) {
            return JS_FALSE;
        }
#endif
        if (!NewKeyValuePair(cx, id, *rval, rval))
            return JS_FALSE;
    } else {
        *rval = ID_TO_VALUE(id);
    }
    return JS_TRUE;

  stop:
    JS_ASSERT(STOBJ_GET_SLOT(obj, JSSLOT_ITER_STATE) == JSVAL_NULL);
    *rval = JSVAL_HOLE;
    return JS_TRUE;
}

JSBool
js_ThrowStopIteration(JSContext *cx)
{
    jsval v;

    JS_ASSERT(!JS_IsExceptionPending(cx));
    if (js_FindClassObject(cx, NULL, INT_TO_JSID(JSProto_StopIteration), &v))
        JS_SetPendingException(cx, v);
    return JS_FALSE;
}

static JSBool
iterator_next(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!JS_InstanceOf(cx, obj, &js_IteratorClass, vp + 2))
        return JS_FALSE;

    if (!IteratorNextImpl(cx, obj, vp))
        return JS_FALSE;

    if (*vp == JSVAL_HOLE) {
        *vp = JSVAL_NULL;
        js_ThrowStopIteration(cx);
        return JS_FALSE;
    }
    return JS_TRUE;
}

static JSBool
iterator_self(JSContext *cx, uintN argc, jsval *vp)
{
    *vp = JS_THIS(cx, vp);
    return !JSVAL_IS_NULL(*vp);
}

#define JSPROP_ROPERM   (JSPROP_READONLY | JSPROP_PERMANENT)

static JSFunctionSpec iterator_methods[] = {
    JS_FN(js_iterator_str,  iterator_self,  0,JSPROP_ROPERM),
    JS_FN(js_next_str,      iterator_next,  0,JSPROP_ROPERM),
    JS_FS_END
};

uintN
js_GetNativeIteratorFlags(JSContext *cx, JSObject *iterobj)
{
    if (OBJ_GET_CLASS(cx, iterobj) != &js_IteratorClass)
        return 0;
    return JSVAL_TO_INT(STOBJ_GET_SLOT(iterobj, JSSLOT_ITER_FLAGS));
}

/*
 * Call ToObject(v).__iterator__(keyonly) if ToObject(v).__iterator__ exists.
 * Otherwise construct the default iterator.
 */
JS_FRIEND_API(JSBool)
js_ValueToIterator(JSContext *cx, uintN flags, jsval *vp)
{
    JSObject *obj;
    JSTempValueRooter tvr;
    JSAtom *atom;
    JSClass *clasp;
    JSExtendedClass *xclasp;
    JSBool ok;
    JSObject *iterobj;
    jsval arg;

    JS_ASSERT(!(flags & ~(JSITER_ENUMERATE |
                          JSITER_FOREACH |
                          JSITER_KEYVALUE)));

    /* JSITER_KEYVALUE must always come with JSITER_FOREACH */
    JS_ASSERT(!(flags & JSITER_KEYVALUE) || (flags & JSITER_FOREACH));

    /* XXX work around old valueOf call hidden beneath js_ValueToObject */
    if (!JSVAL_IS_PRIMITIVE(*vp)) {
        obj = JSVAL_TO_OBJECT(*vp);
    } else {
        /*
         * Enumerating over null and undefined gives an empty enumerator.
         * This is contrary to ECMA-262 9.9 ToObject, invoked from step 3 of
         * the first production in 12.6.4 and step 4 of the second production,
         * but it's "web JS" compatible.
         */
        if ((flags & JSITER_ENUMERATE)) {
            if (!js_ValueToObject(cx, *vp, &obj))
                return JS_FALSE;
            if (!obj)
                goto default_iter;
        } else {
            obj = js_ValueToNonNullObject(cx, *vp);
            if (!obj)
                return JS_FALSE;
        }
    }

    JS_ASSERT(obj);
    JS_PUSH_TEMP_ROOT_OBJECT(cx, obj, &tvr);

    clasp = OBJ_GET_CLASS(cx, obj);
    if ((clasp->flags & JSCLASS_IS_EXTENDED) &&
        (xclasp = (JSExtendedClass *) clasp)->iteratorObject) {
        iterobj = xclasp->iteratorObject(cx, obj, !(flags & JSITER_FOREACH));
        if (!iterobj)
            goto bad;
        *vp = OBJECT_TO_JSVAL(iterobj);
    } else {
        atom = cx->runtime->atomState.iteratorAtom;
        if (!js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, vp))
            goto bad;
        if (JSVAL_IS_VOID(*vp)) {
          default_iter:
            /*
             * Fail over to the default enumerating native iterator.
             *
             * Create iterobj with a NULL parent to ensure that we use the
             * correct scope chain to lookup the iterator's constructor. Since
             * we use the parent slot to keep track of the iterable, we must
             * fix it up after.
             */
            iterobj = js_NewObject(cx, &js_IteratorClass, NULL, NULL);
            if (!iterobj)
                goto bad;

            /* Store in *vp to protect it from GC (callers must root vp). */
            *vp = OBJECT_TO_JSVAL(iterobj);

            if (!InitNativeIterator(cx, iterobj, obj, flags))
                goto bad;
        } else {
            js_LeaveTrace(cx);
            arg = BOOLEAN_TO_JSVAL((flags & JSITER_FOREACH) == 0);
            if (!js_InternalInvoke(cx, obj, *vp, JSINVOKE_ITERATOR, 1, &arg,
                                   vp)) {
                goto bad;
            }
            if (JSVAL_IS_PRIMITIVE(*vp)) {
                js_ReportValueError(cx, JSMSG_BAD_ITERATOR_RETURN,
                                    JSDVG_SEARCH_STACK, *vp, NULL);
                goto bad;
            }
        }
    }

    ok = JS_TRUE;
  out:
    if (obj)
        JS_POP_TEMP_ROOT(cx, &tvr);
    return ok;
  bad:
    ok = JS_FALSE;
    goto out;
}

JS_FRIEND_API(JSBool) JS_FASTCALL
js_CloseIterator(JSContext *cx, jsval v)
{
    JSObject *obj;
    JSClass *clasp;

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
    obj = JSVAL_TO_OBJECT(v);
    clasp = OBJ_GET_CLASS(cx, obj);

    if (clasp == &js_IteratorClass) {
        js_CloseNativeIterator(cx, obj);
    }
#if JS_HAS_GENERATORS
    else if (clasp == &js_GeneratorClass) {
        JS_ASSERT_NOT_ON_TRACE(cx);
        if (!CloseGenerator(cx, obj))
            return JS_FALSE;
    }
#endif
    return JS_TRUE;
}
JS_DEFINE_CALLINFO_2(FRIEND, BOOL, js_CloseIterator, CONTEXT, JSVAL, 0, 0)

static JSBool
CallEnumeratorNext(JSContext *cx, JSObject *iterobj, uintN flags, jsval *rval)
{
    JSObject *obj, *origobj;
    jsval state;
    JSBool foreach;
    jsid id;
    JSObject *obj2;
    JSBool cond;
    JSClass *clasp;
    JSExtendedClass *xclasp;
    JSProperty *prop;
    JSString *str;

    JS_ASSERT(flags & JSITER_ENUMERATE);
    JS_ASSERT(STOBJ_GET_CLASS(iterobj) == &js_IteratorClass);

    obj = STOBJ_GET_PARENT(iterobj);
    origobj = STOBJ_GET_PROTO(iterobj);
    state = STOBJ_GET_SLOT(iterobj, JSSLOT_ITER_STATE);
    if (JSVAL_IS_NULL(state))
        goto stop;

    foreach = (flags & JSITER_FOREACH) != 0;
#if JS_HAS_XML_SUPPORT
    /*
     * Treat an XML object specially only when it starts the prototype chain.
     * Otherwise we need to do the usual deleted and shadowed property checks.
     */
    if (obj == origobj && OBJECT_IS_XML(cx, obj)) {
        if (foreach) {
            if (!js_EnumerateXMLValues(cx, obj, JSENUMERATE_NEXT, &state,
                                       &id, rval)) {
                return JS_FALSE;
            }
        } else {
            if (!obj->enumerate(cx, JSENUMERATE_NEXT, &state, &id))
                return JS_FALSE;
        }
        STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_STATE, state);
        if (JSVAL_IS_NULL(state))
            goto stop;
    } else
#endif
    {
      restart:
        if (!obj->enumerate(cx, JSENUMERATE_NEXT, &state, &id))
            return JS_FALSE;

        STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_STATE, state);
        if (JSVAL_IS_NULL(state)) {
#if JS_HAS_XML_SUPPORT
            if (OBJECT_IS_XML(cx, obj)) {
                /*
                 * We just finished enumerating an XML obj that is present on
                 * the prototype chain of a non-XML origobj. Stop further
                 * prototype chain searches because XML objects don't
                 * enumerate prototypes.
                 */
                JS_ASSERT(origobj != obj);
                JS_ASSERT(!OBJECT_IS_XML(cx, origobj));
            } else
#endif
            {
                obj = OBJ_GET_PROTO(cx, obj);
                if (obj) {
                    STOBJ_SET_PARENT(iterobj, obj);
                    if (!obj->enumerate(cx, JSENUMERATE_INIT, &state, NULL))
                        return JS_FALSE;
                    STOBJ_SET_SLOT(iterobj, JSSLOT_ITER_STATE, state);
                    if (!JSVAL_IS_NULL(state))
                        goto restart;
                }
            }
            goto stop;
        }

        /* Skip properties not in obj when looking from origobj. */
        if (!origobj->lookupProperty(cx, id, &obj2, &prop))
            return JS_FALSE;
        if (!prop)
            goto restart;
        obj2->dropProperty(cx, prop);

        /*
         * If the id was found in a prototype object or an unrelated object
         * (specifically, not in an inner object for obj), skip it. This step
         * means that all lookupProperty implementations must return an
         * object further along on the prototype chain, or else possibly an
         * object returned by the JSExtendedClass.outerObject optional hook.
         */
        if (obj != obj2) {
            cond = JS_FALSE;
            clasp = OBJ_GET_CLASS(cx, obj2);
            if (clasp->flags & JSCLASS_IS_EXTENDED) {
                xclasp = (JSExtendedClass *) clasp;
                cond = xclasp->outerObject &&
                    xclasp->outerObject(cx, obj2) == obj;
            }
            if (!cond)
                goto restart;
        }

        if (foreach) {
            /* Get property querying the original object. */
            if (!origobj->getProperty(cx, id, rval))
                return JS_FALSE;
        }
    }

    if (foreach) {
        if (flags & JSITER_KEYVALUE) {
            if (!NewKeyValuePair(cx, id, *rval, rval))
                return JS_FALSE;
        }
    } else {
        /* Make rval a string for uniformity and compatibility. */
        str = js_ValueToString(cx, ID_TO_VALUE(id));
        if (!str)
            return JS_FALSE;
        *rval = STRING_TO_JSVAL(str);
    }
    return JS_TRUE;

  stop:
    JS_ASSERT(STOBJ_GET_SLOT(iterobj, JSSLOT_ITER_STATE) == JSVAL_NULL);
    *rval = JSVAL_HOLE;
    return JS_TRUE;
}

JS_FRIEND_API(JSBool)
js_CallIteratorNext(JSContext *cx, JSObject *iterobj, jsval *rval)
{
    uintN flags;

    /* Fast path for native iterators */
    if (OBJ_GET_CLASS(cx, iterobj) == &js_IteratorClass) {
        flags = JSVAL_TO_INT(STOBJ_GET_SLOT(iterobj, JSSLOT_ITER_FLAGS));
        if (flags & JSITER_ENUMERATE)
            return CallEnumeratorNext(cx, iterobj, flags, rval);

        /*
         * Call next directly as all the methods of the native iterator are
         * read-only and permanent.
         */
        if (!IteratorNextImpl(cx, iterobj, rval))
            return JS_FALSE;
    } else {
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.nextAtom);

        if (!JS_GetMethodById(cx, iterobj, id, &iterobj, rval))
            return JS_FALSE;
        if (!js_InternalCall(cx, iterobj, *rval, 0, NULL, rval)) {
            /* Check for StopIteration. */
            if (!cx->throwing || !js_ValueIsStopIteration(cx->exception))
                return JS_FALSE;

            /* Inline JS_ClearPendingException(cx). */
            cx->throwing = JS_FALSE;
            cx->exception = JSVAL_VOID;
            *rval = JSVAL_HOLE;
            return JS_TRUE;
        }
    }

    return JS_TRUE;
}

static JSBool
stopiter_hasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    *bp = js_ValueIsStopIteration(v);
    return JS_TRUE;
}

JSClass js_StopIterationClass = {
    js_StopIteration_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_StopIteration),
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   NULL,
    NULL,             NULL,
    NULL,             NULL,
    NULL,             stopiter_hasInstance,
    NULL,             NULL
};

#if JS_HAS_GENERATORS

static void
generator_finalize(JSContext *cx, JSObject *obj)
{
    JSGenerator *gen = (JSGenerator *) obj->getPrivate();
    if (!gen)
        return;

    /*
     * gen is open when a script has not called its close method while
     * explicitly manipulating it.
     */
    JS_ASSERT(gen->state == JSGEN_NEWBORN ||
              gen->state == JSGEN_CLOSED ||
              gen->state == JSGEN_OPEN);
    cx->free(gen);
}

static void
generator_trace(JSTracer *trc, JSObject *obj)
{
    JSGenerator *gen = (JSGenerator *) obj->getPrivate();
    if (!gen)
        return;

    /*
     * js_TraceStackFrame does not recursively trace the down-linked frame
     * chain, so we insist that gen->frame has no parent to trace when the
     * generator is not running.
     */
    JS_ASSERT_IF(gen->state != JSGEN_RUNNING && gen->state != JSGEN_CLOSING,
                 !gen->frame.down);

    /*
     * FIXME be 390950. Generator's frame is a part of the JS stack when the
     * generator is running or closing. Thus tracing the frame in this case
     * here duplicates the work done in js_TraceContext.
     */
    js_TraceStackFrame(trc, &gen->frame);
}

JS_FRIEND_DATA(JSClass) js_GeneratorClass = {
    js_Generator_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_IS_ANONYMOUS |
    JSCLASS_MARK_IS_TRACE | JSCLASS_HAS_CACHED_PROTO(JSProto_Generator),
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  generator_finalize,
    NULL,             NULL,            NULL,            NULL,
    NULL,             NULL,            JS_CLASS_TRACE(generator_trace), NULL
};

/*
 * Called from the JSOP_GENERATOR case in the interpreter, with fp referring
 * to the frame by which the generator function was activated.  Create a new
 * JSGenerator object, which contains its own JSStackFrame that we populate
 * from *fp.  We know that upon return, the JSOP_GENERATOR opcode will return
 * from the activation in fp, so we can steal away fp->callobj and fp->argsobj
 * if they are non-null.
 */
JSObject *
js_NewGenerator(JSContext *cx, JSStackFrame *fp)
{
    JSObject *obj;
    uintN argc, nargs, nslots;
    JSGenerator *gen;
    jsval *slots;

    obj = js_NewObject(cx, &js_GeneratorClass, NULL, NULL);
    if (!obj)
        return NULL;

    /* Load and compute stack slot counts. */
    argc = fp->argc;
    nargs = JS_MAX(argc, fp->fun->nargs);
    nslots = 2 + nargs + fp->script->nslots;

    /* Allocate obj's private data struct. */
    gen = (JSGenerator *)
        cx->malloc(sizeof(JSGenerator) + (nslots - 1) * sizeof(jsval));
    if (!gen)
        return NULL;

    gen->obj = obj;

    /* Steal away objects reflecting fp and point them at gen->frame. */
    gen->frame.callobj = fp->callobj;
    if (fp->callobj) {
        fp->callobj->setPrivate(&gen->frame);
        fp->callobj = NULL;
    }
    gen->frame.argsobj = fp->argsobj;
    if (fp->argsobj) {
        JSVAL_TO_OBJECT(fp->argsobj)->setPrivate(&gen->frame);
        fp->argsobj = NULL;
    }

    /* These two references can be shared with fp until it goes away. */
    gen->frame.varobj = fp->varobj;
    gen->frame.thisv = fp->thisv;

    /* Copy call-invariant script and function references. */
    gen->frame.script = fp->script;
    gen->frame.fun = fp->fun;

    /* Use slots to carve space out of gen->slots. */
    slots = gen->slots;
    gen->arena.next = NULL;
    gen->arena.base = (jsuword) slots;
    gen->arena.limit = gen->arena.avail = (jsuword) (slots + nslots);

    /* Copy rval, argv and vars. */
    gen->frame.rval = fp->rval;
    memcpy(slots, fp->argv - 2, (2 + nargs) * sizeof(jsval));
    gen->frame.argc = nargs;
    gen->frame.argv = slots + 2;
    slots += 2 + nargs;
    memcpy(slots, fp->slots, fp->script->nfixed * sizeof(jsval));

    /* Initialize or copy virtual machine state. */
    gen->frame.down = NULL;
    gen->frame.annotation = NULL;
    gen->frame.scopeChain = fp->scopeChain;

    gen->frame.imacpc = NULL;
    gen->frame.slots = slots;
    JS_ASSERT(StackBase(fp) == fp->regs->sp);
    gen->savedRegs.sp = slots + fp->script->nfixed;
    gen->savedRegs.pc = fp->regs->pc;
    gen->frame.regs = &gen->savedRegs;

    gen->frame.flags = (fp->flags & ~JSFRAME_ROOTED_ARGV) | JSFRAME_GENERATOR;
    gen->frame.dormantNext = NULL;

    /* JSOP_GENERATOR appears in the prologue, outside all blocks.  */
    JS_ASSERT(!fp->blockChain);
    gen->frame.blockChain = NULL;

    /* Note that gen is newborn. */
    gen->state = JSGEN_NEWBORN;

    obj->setPrivate(gen);
    return obj;
}

typedef enum JSGeneratorOp {
    JSGENOP_NEXT,
    JSGENOP_SEND,
    JSGENOP_THROW,
    JSGENOP_CLOSE
} JSGeneratorOp;

/*
 * Start newborn or restart yielding generator and perform the requested
 * operation inside its frame.
 */
static JS_REQUIRES_STACK JSBool
SendToGenerator(JSContext *cx, JSGeneratorOp op, JSObject *obj,
                JSGenerator *gen, jsval arg)
{
    JSStackFrame *fp;
    JSArena *arena;
    JSBool ok;

    if (gen->state == JSGEN_RUNNING || gen->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_NESTING_GENERATOR,
                            JSDVG_SEARCH_STACK, OBJECT_TO_JSVAL(obj),
                            JS_GetFunctionId(gen->frame.fun));
        return JS_FALSE;
    }

    JS_ASSERT(gen->state ==  JSGEN_NEWBORN || gen->state == JSGEN_OPEN);
    switch (op) {
      case JSGENOP_NEXT:
      case JSGENOP_SEND:
        if (gen->state == JSGEN_OPEN) {
            /*
             * Store the argument to send as the result of the yield
             * expression.
             */
            gen->savedRegs.sp[-1] = arg;
        }
        gen->state = JSGEN_RUNNING;
        break;

      case JSGENOP_THROW:
        JS_SetPendingException(cx, arg);
        gen->state = JSGEN_RUNNING;
        break;

      default:
        JS_ASSERT(op == JSGENOP_CLOSE);
        JS_SetPendingException(cx, JSVAL_ARETURN);
        gen->state = JSGEN_CLOSING;
        break;
    }

    /* Extend the current stack pool with gen->arena. */
    arena = cx->stackPool.current;
    JS_ASSERT(!arena->next);
    JS_ASSERT(!gen->arena.next);
    JS_ASSERT(cx->stackPool.current != &gen->arena);
    cx->stackPool.current = arena->next = &gen->arena;

    /* Push gen->frame around the interpreter activation. */
    fp = js_GetTopStackFrame(cx);
    cx->fp = &gen->frame;
    gen->frame.down = fp;
    ok = js_Interpret(cx);
    cx->fp = fp;
    gen->frame.down = NULL;

    /* Retract the stack pool and sanitize gen->arena. */
    JS_ASSERT(!gen->arena.next);
    JS_ASSERT(arena->next == &gen->arena);
    JS_ASSERT(cx->stackPool.current == &gen->arena);
    cx->stackPool.current = arena;
    arena->next = NULL;

    if (gen->frame.flags & JSFRAME_YIELDING) {
        /* Yield cannot fail, throw or be called on closing. */
        JS_ASSERT(ok);
        JS_ASSERT(!cx->throwing);
        JS_ASSERT(gen->state == JSGEN_RUNNING);
        JS_ASSERT(op != JSGENOP_CLOSE);
        gen->frame.flags &= ~JSFRAME_YIELDING;
        gen->state = JSGEN_OPEN;
        return JS_TRUE;
    }

    gen->frame.rval = JSVAL_VOID;
    gen->state = JSGEN_CLOSED;
    if (ok) {
        /* Returned, explicitly or by falling off the end. */
        if (op == JSGENOP_CLOSE)
            return JS_TRUE;
        return js_ThrowStopIteration(cx);
    }

    /*
     * An error, silent termination by operation callback or an exception.
     * Propagate the condition to the caller.
     */
    return JS_FALSE;
}

static JS_REQUIRES_STACK JSBool
CloseGenerator(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(STOBJ_GET_CLASS(obj) == &js_GeneratorClass);

    JSGenerator *gen = (JSGenerator *) obj->getPrivate();
    if (!gen) {
        /* Generator prototype object. */
        return JS_TRUE;
    }

    if (gen->state == JSGEN_CLOSED)
        return JS_TRUE;

    return SendToGenerator(cx, JSGENOP_CLOSE, obj, gen, JSVAL_VOID);
}

/*
 * Common subroutine of generator_(next|send|throw|close) methods.
 */
static JSBool
generator_op(JSContext *cx, JSGeneratorOp op, jsval *vp, uintN argc)
{
    JSObject *obj;
    jsval arg;

    js_LeaveTrace(cx);

    obj = JS_THIS_OBJECT(cx, vp);
    if (!JS_InstanceOf(cx, obj, &js_GeneratorClass, vp + 2))
        return JS_FALSE;

    JSGenerator *gen = (JSGenerator *) obj->getPrivate();
    if (!gen) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        goto closed_generator;
    }

    if (gen->state == JSGEN_NEWBORN) {
        switch (op) {
          case JSGENOP_NEXT:
          case JSGENOP_THROW:
            break;

          case JSGENOP_SEND:
            if (argc >= 1 && !JSVAL_IS_VOID(vp[2])) {
                js_ReportValueError(cx, JSMSG_BAD_GENERATOR_SEND,
                                    JSDVG_SEARCH_STACK, vp[2], NULL);
                return JS_FALSE;
            }
            break;

          default:
            JS_ASSERT(op == JSGENOP_CLOSE);
            gen->state = JSGEN_CLOSED;
            return JS_TRUE;
        }
    } else if (gen->state == JSGEN_CLOSED) {
      closed_generator:
        switch (op) {
          case JSGENOP_NEXT:
          case JSGENOP_SEND:
            return js_ThrowStopIteration(cx);
          case JSGENOP_THROW:
            JS_SetPendingException(cx, argc >= 1 ? vp[2] : JSVAL_VOID);
            return JS_FALSE;
          default:
            JS_ASSERT(op == JSGENOP_CLOSE);
            return JS_TRUE;
        }
    }

    arg = ((op == JSGENOP_SEND || op == JSGENOP_THROW) && argc != 0)
          ? vp[2]
          : JSVAL_VOID;
    if (!SendToGenerator(cx, op, obj, gen, arg))
        return JS_FALSE;
    *vp = gen->frame.rval;
    return JS_TRUE;
}

static JSBool
generator_send(JSContext *cx, uintN argc, jsval *vp)
{
    return generator_op(cx, JSGENOP_SEND, vp, argc);
}

static JSBool
generator_next(JSContext *cx, uintN argc, jsval *vp)
{
    return generator_op(cx, JSGENOP_NEXT, vp, argc);
}

static JSBool
generator_throw(JSContext *cx, uintN argc, jsval *vp)
{
    return generator_op(cx, JSGENOP_THROW, vp, argc);
}

static JSBool
generator_close(JSContext *cx, uintN argc, jsval *vp)
{
    return generator_op(cx, JSGENOP_CLOSE, vp, argc);
}

static JSFunctionSpec generator_methods[] = {
    JS_FN(js_iterator_str,  iterator_self,      0,JSPROP_ROPERM),
    JS_FN(js_next_str,      generator_next,     0,JSPROP_ROPERM),
    JS_FN(js_send_str,      generator_send,     1,JSPROP_ROPERM),
    JS_FN(js_throw_str,     generator_throw,    1,JSPROP_ROPERM),
    JS_FN(js_close_str,     generator_close,    0,JSPROP_ROPERM),
    JS_FS_END
};

#endif /* JS_HAS_GENERATORS */

JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj)
{
    JSObject *proto, *stop;

    /* Idempotency required: we initialize several things, possibly lazily. */
    if (!js_GetClassObject(cx, obj, JSProto_StopIteration, &stop))
        return NULL;
    if (stop)
        return stop;

    proto = JS_InitClass(cx, obj, NULL, &js_IteratorClass, Iterator, 2,
                         NULL, iterator_methods, NULL, NULL);
    if (!proto)
        return NULL;
    STOBJ_SET_SLOT(proto, JSSLOT_ITER_STATE, JSVAL_NULL);
    STOBJ_SET_SLOT(proto, JSSLOT_ITER_FLAGS, JSVAL_ZERO);

#if JS_HAS_GENERATORS
    /* Initialize the generator internals if configured. */
    if (!JS_InitClass(cx, obj, NULL, &js_GeneratorClass, NULL, 0,
                      NULL, generator_methods, NULL, NULL)) {
        return NULL;
    }
#endif

    return JS_InitClass(cx, obj, NULL, &js_StopIterationClass, NULL, 0,
                        NULL, NULL, NULL, NULL);
}
