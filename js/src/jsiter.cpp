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
#include "jshashtable.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsproxy.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jstracer.h"
#include "jsvector.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "jscntxtinlines.h"
#include "jsobjinlines.h"
#include "jsstrinlines.h"

using namespace js;

static void iterator_finalize(JSContext *cx, JSObject *obj);
static void iterator_trace(JSTracer *trc, JSObject *obj);
static JSObject *iterator_iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

JSExtendedClass js_IteratorClass = {
  { "Iterator",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Iterator) |
    JSCLASS_MARK_IS_TRACE |
    JSCLASS_IS_EXTENDED,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,   iterator_finalize,
    NULL,             NULL,            NULL,             NULL,
    NULL,             NULL,            JS_CLASS_TRACE(iterator_trace), NULL },
    NULL,             NULL,            NULL,             iterator_iterator,
    NULL,
    JSCLASS_NO_RESERVED_MEMBERS
};

void
NativeIterator::mark(JSTracer *trc)
{
    for (jsval *vp = props_array; vp < props_end; ++vp) {
        JS_SET_TRACING_INDEX(trc, "props", (vp - props_array));
        js_CallValueTracerIfGCThing(trc, *vp);
    }
}

/*
 * Shared code to close iterator's state either through an explicit call or
 * when GC detects that the iterator is no longer reachable.
 */
static void
iterator_finalize(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &js_IteratorClass.base);

    /* Avoid double work if the iterator was closed by JSOP_ENDITER. */
    NativeIterator *ni = obj->getNativeIterator();
    if (ni) {
        cx->free(ni);
        obj->setNativeIterator(NULL);
    }
}

static void
iterator_trace(JSTracer *trc, JSObject *obj)
{
    NativeIterator *ni = obj->getNativeIterator();

    if (ni)
        ni->mark(trc);
}

static bool
NewKeyValuePair(JSContext *cx, jsid key, jsval val, jsval *rval)
{
    jsval vec[2] = { ID_TO_VALUE(key), val };
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(vec), vec);

    JSObject *aobj = js_NewArrayObject(cx, 2, vec);
    if (!aobj)
        return false;
    *rval = OBJECT_TO_JSVAL(aobj);
    return true;
}

static inline bool
Enumerate(JSContext *cx, JSObject *obj, JSObject *pobj, jsid id,
          bool enumerable, uintN flags, HashSet<jsid>& ht,
          AutoValueVector& vec)
{
    JS_ASSERT(JSVAL_IS_INT(id) || JSVAL_IS_STRING(id));

    if (JS_LIKELY(!(flags & JSITER_OWNONLY))) {
        HashSet<jsid>::AddPtr p = ht.lookupForAdd(id);
        /* property already encountered, done. */
        if (JS_UNLIKELY(!!p))
            return true;
        /* no need to add properties to the hash table at the end of the prototype chain */
        if (pobj->getProto() && !ht.add(p, id))
            return false;
    }
    if (enumerable || (flags & JSITER_HIDDEN)) {
        if (!vec.append(ID_TO_VALUE(id))) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        if (flags & JSITER_FOREACH) {
            jsval *vp = vec.end() - 1;

            /* Do the lookup on the original object instead of the prototype. */
            if (!obj->getProperty(cx, id, vp))
                return false;
            if ((flags & JSITER_KEYVALUE) && !NewKeyValuePair(cx, id, *vp, vp))
                return false;
        }
    }
    return true;
}

static bool
EnumerateNativeProperties(JSContext *cx, JSObject *obj, JSObject *pobj, uintN flags,
                          HashSet<jsid> &ht, AutoValueVector& props)
{
    AutoValueVector sprops(cx);

    JS_LOCK_OBJ(cx, pobj);

    /* Collect all unique properties from this object's scope. */
    JSScope *scope = pobj->scope();
    for (JSScopeProperty *sprop = scope->lastProperty(); sprop; sprop = sprop->parent) {
        if (sprop->id != JSVAL_VOID &&
            !sprop->isAlias() &&
            !Enumerate(cx, obj, pobj, sprop->id, sprop->enumerable(), flags, ht, sprops)) {
            return false;
        }
    }

    while (sprops.length() > 0) {
        if (!props.append(sprops.back())) {
            JS_ReportOutOfMemory(cx);
            return false;
        }
        sprops.popBack();
    }

    JS_UNLOCK_SCOPE(cx, scope);

    return true;
}

static bool
EnumerateDenseArrayProperties(JSContext *cx, JSObject *obj, JSObject *pobj, uintN flags,
                              HashSet<jsid> &ht, AutoValueVector& props)
{
    size_t count = pobj->getDenseArrayCount();

    if (count) {
        size_t capacity = pobj->getDenseArrayCapacity();
        jsval *vp = pobj->dslots;
        for (size_t i = 0; i < capacity; ++i, ++vp) {
            if (*vp != JSVAL_HOLE) {
                /* Dense arrays never get so large that i would not fit into an integer id. */
                if (!Enumerate(cx, obj, pobj, INT_TO_JSVAL(i), true, flags, ht, props))
                    return false;
            }
        }
    }
    return true;
}

static bool
MakeNativeIterator(JSContext *cx, uintN flags, uint32 *sarray, uint32 slength, uint32 key,
                   jsval *parray, uint32 plength, NativeIterator **nip)
{
    NativeIterator *ni = (NativeIterator *)
        cx->malloc(sizeof(NativeIterator) + plength * sizeof(jsval) + slength * sizeof(uint32));
    if (!ni) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    ni->props_array = ni->props_cursor = (jsval *) (ni + 1);
    ni->props_end = ni->props_array + plength;
    if (plength)
        memcpy(ni->props_array, parray, plength * sizeof(jsval));
    ni->shapes_array = (uint32 *) ni->props_end;
    ni->shapes_length = slength;
    ni->shapes_key = key;
    ni->flags = flags;
    if (slength)
        memcpy(ni->shapes_array, sarray, slength * sizeof(uint32));

    *nip = ni;

    return true;
}

static bool
InitNativeIterator(JSContext *cx, JSObject *obj, uintN flags, uint32 *sarray, uint32 slength,
                   uint32 key, NativeIterator **nip)
{
    HashSet<jsid> ht(cx);
    if (!(flags & JSITER_OWNONLY) && !ht.init(32))
        return false;

    AutoValueVector props(cx);

    JSObject *pobj = obj;
    while (pobj) {
        JSClass *clasp = pobj->getClass();
        if (pobj->isNative() &&
            pobj->map->ops->enumerate == js_Enumerate &&
            !(clasp->flags & JSCLASS_NEW_ENUMERATE)) {
            if (!clasp->enumerate(cx, pobj))
                return false;
            if (!EnumerateNativeProperties(cx, obj, pobj, flags, ht, props))
                return false;
        } else if (pobj->isDenseArray()) {
            if (!EnumerateDenseArrayProperties(cx, obj, pobj, flags, ht, props))
                return false;
        } else {
            if (pobj->isProxy()) {
                JSIdArray *ida;
                if (flags & JSITER_OWNONLY) {
                    if (!JSProxy::enumerateOwn(cx, pobj, &ida))
                        return false;
                } else {
                    if (!JSProxy::enumerate(cx, pobj, &ida))
                        return false;
                }
                AutoIdArray idar(cx, ida);
                for (size_t n = 0; n < size_t(ida->length); ++n) {
                    if (!Enumerate(cx, obj, pobj, ida->vector[n], true, flags, ht, props))
                        return false;
                }
                /* Proxy objects enumerate the prototype on their own, so we are done here. */
                break;
            }
            jsval state;
            if (!pobj->enumerate(cx, JSENUMERATE_INIT, &state, NULL))
                return false;
            if (state == JSVAL_NATIVE_ENUMERATE_COOKIE) {
                if (!EnumerateNativeProperties(cx, obj, pobj, flags, ht, props))
                    return false;
            } else {
                while (true) {
                    jsid id;
                    if (!pobj->enumerate(cx, JSENUMERATE_NEXT, &state, &id))
                        return false;
                    if (state == JSVAL_NULL)
                        break;
                    if (!Enumerate(cx, obj, pobj, id, true, flags, ht, props))
                        return false;
                }
            }
        }

        if (JS_UNLIKELY(pobj->isXML() || (flags & JSITER_OWNONLY)))
            break;

        pobj = pobj->getProto();
    }

    return MakeNativeIterator(cx, flags, sarray, slength, key, props.begin(), props.length(), nip);
}

bool
NativeIteratorToJSIdArray(JSContext *cx, NativeIterator *ni, JSIdArray **idap)
{
    /* Morph the NativeIterator into a JSIdArray. The caller will deallocate it. */
    JS_ASSERT(sizeof(NativeIterator) > sizeof(JSIdArray));
    JS_ASSERT(ni->props_array == (jsid *) (ni + 1));
    size_t length = size_t(ni->props_end - ni->props_array);
    JSIdArray *ida = (JSIdArray *) (uintptr_t(ni->props_array) - (sizeof(JSIdArray) - sizeof(jsid)));
    ida->self = ni;
    ida->length = length;
    JS_ASSERT(&ida->vector[0] == &ni->props_array[0]);
    *idap = ida;
    return true;
}

bool
EnumerateOwnProperties(JSContext *cx, JSObject *obj, JSIdArray **idap)
{
    NativeIterator *ni;
    if (!InitNativeIterator(cx, obj, JSITER_OWNONLY, NULL, 0, true, &ni))
        return false;
    return NativeIteratorToJSIdArray(cx, ni, idap);
}

bool
EnumerateAllProperties(JSContext *cx, JSObject *obj, JSIdArray **idap)
{
    NativeIterator *ni;
    if (!InitNativeIterator(cx, obj, 0, NULL, 0, true, &ni))
        return false;
    return NativeIteratorToJSIdArray(cx, ni, idap);
}

bool
GetOwnProperties(JSContext *cx, JSObject *obj, JSIdArray **idap)
{
    NativeIterator *ni;
    if (!InitNativeIterator(cx, obj, JSITER_OWNONLY | JSITER_HIDDEN, NULL, 0, true, &ni))
        return false;
    return NativeIteratorToJSIdArray(cx, ni, idap);
}

static inline bool
GetCustomIterator(JSContext *cx, JSObject *obj, uintN flags, jsval *vp)
{
    /* Check whether we have a valid __iterator__ method. */
    JSAtom *atom = cx->runtime->atomState.iteratorAtom;
    if (!js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, vp))
        return false;

    /* If there is no custom __iterator__ method, we are done here. */
    if (*vp == JSVAL_VOID)
        return true;

    /* Otherwise call it and return that object. */
    LeaveTrace(cx);
    jsval arg = BOOLEAN_TO_JSVAL((flags & JSITER_FOREACH) == 0);
    if (!js_InternalInvoke(cx, obj, *vp, JSINVOKE_ITERATOR, 1, &arg, vp))
        return false;
    if (JSVAL_IS_PRIMITIVE(*vp)) {
        js_ReportValueError(cx, JSMSG_BAD_ITERATOR_RETURN, JSDVG_SEARCH_STACK, *vp, NULL);
        return false;
    }
    return true;
}

template <typename T>
static inline bool
Compare(T *a, T *b, size_t c)
{
    size_t n = (c + size_t(7)) / size_t(8);
    switch (c % 8) {
      case 0: do { if (*a++ != *b++) return false;
      case 7:      if (*a++ != *b++) return false;
      case 6:      if (*a++ != *b++) return false;
      case 5:      if (*a++ != *b++) return false;
      case 4:      if (*a++ != *b++) return false;
      case 3:      if (*a++ != *b++) return false;
      case 2:      if (*a++ != *b++) return false;
      case 1:      if (*a++ != *b++) return false;
              } while (--n > 0);
    }
    return true;
}

static inline bool
GetIterator(JSContext *cx, JSObject *obj, uintN flags, jsval *vp)
{
    uint32 hash;
    JSObject **hp;
    NativeIterator *ni;
    Vector<uint32, 8> shapes(cx);
    uint32 key = 0;

    bool escaping = !(flags & JSITER_ENUMERATE);
    bool keysOnly = (flags == JSITER_ENUMERATE);

    if (obj) {
        if (keysOnly) {
            /*
             * The iterator object for JSITER_ENUMERATE never escapes, so we
             * don't care for the proper parent/proto to be set. This also
             * allows us to re-use a previous iterator object that was freed
             * by JSOP_ENDITER.
             */
            JSObject *pobj = obj;
            do {
                if (!pobj->isNative() ||
                    obj->map->ops->enumerate != js_Enumerate ||
                    pobj->getClass()->enumerate != JS_EnumerateStub) {
                    shapes.clear();
                    goto miss;
                }
                uint32 shape = pobj->shape();
                key = (key + (key << 16)) ^ shape;
                if (!shapes.append(shape))
                    return false;
                pobj = pobj->getProto();
            } while (pobj);

            hash = key % JS_ARRAY_LENGTH(JS_THREAD_DATA(cx)->cachedNativeIterators);
            hp = &JS_THREAD_DATA(cx)->cachedNativeIterators[hash];
            JSObject *iterobj = *hp;
            if (iterobj) {
                ni = iterobj->getNativeIterator();
                if (ni->shapes_key == key &&
                    ni->shapes_length == shapes.length() &&
                    Compare(ni->shapes_array, shapes.begin(), ni->shapes_length)) {
                    *vp = OBJECT_TO_JSVAL(iterobj);
                    *hp = ni->next;
                    return true;
                }
            }
        }

      miss:
        if (!obj->isProxy()) {
            if (!GetCustomIterator(cx, obj, flags, vp))
                return false;
            if (*vp != JSVAL_VOID)
                return true;
        }
    }

    JSObject *iterobj = escaping
                      ? NewObject(cx, &js_IteratorClass.base, NULL, NULL)
                      : NewObjectWithGivenProto(cx, &js_IteratorClass.base, NULL, NULL);
    if (!iterobj)
        return false;

    /* Store in *vp to protect it from GC (callers must root vp). */
    *vp = OBJECT_TO_JSVAL(iterobj);

    if (!InitNativeIterator(cx, obj, flags, shapes.begin(), shapes.length(), key, &ni))
        return false;
    iterobj->setNativeIterator(ni);

    return true;
}

static JSObject *
iterator_iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
    return obj;
}

static JSBool
Iterator(JSContext *cx, JSObject *iterobj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool keyonly;
    uintN flags;

    keyonly = js_ValueToBoolean(argv[1]);
    flags = keyonly ? 0 : (JSITER_FOREACH | JSITER_KEYVALUE);
    *rval = argv[0];
    return js_ValueToIterator(cx, flags, rval);
}

JSBool
js_ThrowStopIteration(JSContext *cx)
{
    jsval v;

    JS_ASSERT(!JS_IsExceptionPending(cx));
    if (js_FindClassObject(cx, NULL, JSProto_StopIteration, &v))
        JS_SetPendingException(cx, v);
    return JS_FALSE;
}

static JSBool
iterator_next(JSContext *cx, uintN argc, jsval *vp)
{
    JSObject *obj;

    obj = JS_THIS_OBJECT(cx, vp);
    if (!JS_InstanceOf(cx, obj, &js_IteratorClass.base, vp + 2))
        return false;

    if (!js_IteratorMore(cx, obj, vp))
        return false;
    if (*vp == JSVAL_FALSE) {
        js_ThrowStopIteration(cx);
        return false;
    }
    JS_ASSERT(*vp == JSVAL_TRUE);
    return js_IteratorNext(cx, obj, vp);
}

#define JSPROP_ROPERM   (JSPROP_READONLY | JSPROP_PERMANENT)

static JSFunctionSpec iterator_methods[] = {
    JS_FN(js_next_str,      iterator_next,  0,JSPROP_ROPERM),
    JS_FS_END
};

/*
 * Call ToObject(v).__iterator__(keyonly) if ToObject(v).__iterator__ exists.
 * Otherwise construct the default iterator.
 */
JS_FRIEND_API(JSBool)
js_ValueToIterator(JSContext *cx, uintN flags, jsval *vp)
{
    JSObject *obj;
    JSClass *clasp;
    JSExtendedClass *xclasp;
    JSObject *iterobj;

    /* JSITER_KEYVALUE must always come with JSITER_FOREACH */
    JS_ASSERT_IF(flags & JSITER_KEYVALUE, flags & JSITER_FOREACH);

    /*
     * Make sure the more/next state machine doesn't get stuck. A value might be
     * left in iterValue when a trace is left due to an operation time-out after
     * JSOP_MOREITER but before the value is picked up by FOR*.
     */
    cx->iterValue = JSVAL_HOLE;

    AutoValueRooter tvr(cx);

    if (!JSVAL_IS_PRIMITIVE(*vp)) {
        /* Common case. */
        obj = JSVAL_TO_OBJECT(*vp);
    } else {
        /*
         * Enumerating over null and undefined gives an empty enumerator.
         * This is contrary to ECMA-262 9.9 ToObject, invoked from step 3 of
         * the first production in 12.6.4 and step 4 of the second production,
         * but it's "web JS" compatible. ES5 fixed for-in to match this de-facto
         * standard.
         */
        if ((flags & JSITER_ENUMERATE)) {
            if (!js_ValueToObject(cx, *vp, &obj))
                return false;
            if (!obj)
                return GetIterator(cx, obj, flags, vp);
        } else {
            obj = js_ValueToNonNullObject(cx, *vp);
            if (!obj)
                return false;
        }
    }

    tvr.setObject(obj);

    clasp = obj->getClass();
    if ((clasp->flags & JSCLASS_IS_EXTENDED) &&
        (xclasp = (JSExtendedClass *) clasp)->iteratorObject) {
        /* Enumerate Iterator.prototype directly. */
        if (clasp != &js_IteratorClass.base || obj->getNativeIterator()) {
            iterobj = xclasp->iteratorObject(cx, obj, !(flags & JSITER_FOREACH));
            if (!iterobj)
                return false;
            *vp = OBJECT_TO_JSVAL(iterobj);
            return true;
        }
    }

    return GetIterator(cx, obj, flags, vp);
}

#if JS_HAS_GENERATORS
static JS_REQUIRES_STACK JSBool
CloseGenerator(JSContext *cx, JSObject *genobj);
#endif

JS_FRIEND_API(JSBool)
js_CloseIterator(JSContext *cx, jsval v)
{
    JSObject *obj;
    JSClass *clasp;

    cx->iterValue = JSVAL_HOLE;

    JS_ASSERT(!JSVAL_IS_PRIMITIVE(v));
    obj = JSVAL_TO_OBJECT(v);
    clasp = obj->getClass();

    if (clasp == &js_IteratorClass.base) {
        /* Cache the iterator object if possible. */
        NativeIterator *ni = obj->getNativeIterator();
        if (ni->shapes_length) {
            uint32 hash = ni->shapes_key % JS_ARRAY_LENGTH(JS_THREAD_DATA(cx)->cachedNativeIterators);
            JSObject **hp = &JS_THREAD_DATA(cx)->cachedNativeIterators[hash];
            ni->props_cursor = ni->props_array;
            ni->next = *hp;
            *hp = obj;
        } else {
            iterator_finalize(cx, obj);
        }
    }
#if JS_HAS_GENERATORS
    else if (clasp == &js_GeneratorClass.base) {
        return CloseGenerator(cx, obj);
    }
#endif
    return JS_TRUE;
}

JSBool
js_IteratorMore(JSContext *cx, JSObject *iterobj, jsval *rval)
{
    /* Fast path for native iterators */
    if (iterobj->getClass() == &js_IteratorClass.base) {
        /*
         * Implement next directly as all the methods of native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->getNativeIterator();
        *rval = BOOLEAN_TO_JSVAL(ni->props_cursor < ni->props_end);
        return true;
    }

    /* We might still have a pending value. */
    if (cx->iterValue != JSVAL_HOLE) {
        *rval = JSVAL_TRUE;
        return true;
    }

    /* Fetch and cache the next value from the iterator. */
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.nextAtom);
    if (!JS_GetMethodById(cx, iterobj, id, &iterobj, rval))
        return false;
    if (!js_InternalCall(cx, iterobj, *rval, 0, NULL, rval)) {
        /* Check for StopIteration. */
        if (!cx->throwing || !js_ValueIsStopIteration(cx->exception))
            return false;

        /* Inline JS_ClearPendingException(cx). */
        cx->throwing = JS_FALSE;
        cx->exception = JSVAL_VOID;
        cx->iterValue = JSVAL_HOLE;
        *rval = JSVAL_FALSE;
        return true;
    }

    /* Cache the value returned by iterobj.next() so js_IteratorNext() can find it. */
    JS_ASSERT(*rval != JSVAL_HOLE);
    cx->iterValue = *rval;
    *rval = JSVAL_TRUE;
    return true;
}

JSBool
js_IteratorNext(JSContext *cx, JSObject *iterobj, jsval *rval)
{
    /* Fast path for native iterators */
    if (iterobj->getClass() == &js_IteratorClass.base) {
        /*
         * Implement next directly as all the methods of the native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->getNativeIterator();
        JS_ASSERT(ni->props_cursor < ni->props_end);
        *rval = *ni->props_cursor++;

        if (JSVAL_IS_STRING(*rval) || (ni->flags & JSITER_FOREACH))
            return true;

        JSString *str;
        jsint i;
        if (JSVAL_IS_INT(*rval) && (jsuint(i = JSVAL_TO_INT(*rval)) < INT_STRING_LIMIT)) {
            str = JSString::intString(i);
        } else {
            str = js_ValueToString(cx, *rval);
            if (!str)
                return false;
        }

        *rval = STRING_TO_JSVAL(str);
        return true;
    }

    JS_ASSERT(cx->iterValue != JSVAL_HOLE);
    *rval = cx->iterValue;
    cx->iterValue = JSVAL_HOLE;

    return true;
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
     * Do not mark if the generator is running; the contents may be trash and
     * will be replaced when the generator stops.
     */
    if (gen->state == JSGEN_RUNNING || gen->state == JSGEN_CLOSING)
        return;

    JSStackFrame *fp = gen->getFloatingFrame();
    JS_ASSERT(gen->getLiveFrame() == fp);
    TraceValues(trc, gen->floatingStack, fp->argEnd(), "generator slots");
    js_TraceStackFrame(trc, fp);
    TraceValues(trc, fp->slots(), gen->savedRegs.sp, "generator slots");
}

JSExtendedClass js_GeneratorClass = {
  { js_Generator_str,
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Generator) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_MARK_IS_TRACE |
    JSCLASS_IS_EXTENDED,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  generator_finalize,
    NULL,             NULL,            NULL,            NULL,
    NULL,             NULL,            JS_CLASS_TRACE(generator_trace), NULL },
    NULL,             NULL,            NULL,            iterator_iterator,
    NULL,
    JSCLASS_NO_RESERVED_MEMBERS
};

/*
 * Called from the JSOP_GENERATOR case in the interpreter, with fp referring
 * to the frame by which the generator function was activated.  Create a new
 * JSGenerator object, which contains its own JSStackFrame that we populate
 * from *fp.  We know that upon return, the JSOP_GENERATOR opcode will return
 * from the activation in fp, so we can steal away fp->callobj and fp->argsobj
 * if they are non-null.
 */
JS_REQUIRES_STACK JSObject *
js_NewGenerator(JSContext *cx)
{
    JSObject *obj = NewObject(cx, &js_GeneratorClass.base, NULL, NULL);
    if (!obj)
        return NULL;

    /* Load and compute stack slot counts. */
    JSStackFrame *fp = cx->fp;
    uintN argc = fp->argc;
    uintN nargs = JS_MAX(argc, fp->fun->nargs);
    uintN vplen = 2 + nargs;

    /* Compute JSGenerator size. */
    uintN nbytes = sizeof(JSGenerator) +
                   (-1 + /* one jsval included in JSGenerator */
                    vplen +
                    VALUES_PER_STACK_FRAME +
                    fp->script->nslots) * sizeof(jsval);

    JSGenerator *gen = (JSGenerator *) cx->malloc(nbytes);
    if (!gen)
        return NULL;

    /* Cut up floatingStack space. */
    jsval *vp = gen->floatingStack;
    JSStackFrame *newfp = reinterpret_cast<JSStackFrame *>(vp + vplen);
    jsval *slots = newfp->slots();

    /* Initialize JSGenerator. */
    gen->obj = obj;
    gen->state = JSGEN_NEWBORN;
    gen->savedRegs.pc = cx->regs->pc;
    JS_ASSERT(cx->regs->sp == fp->slots() + fp->script->nfixed);
    gen->savedRegs.sp = slots + fp->script->nfixed;
    gen->vplen = vplen;
    gen->liveFrame = newfp;

    /* Copy generator's stack frame copy in from |cx->fp|. */
    newfp->imacpc = NULL;
    newfp->callobj = fp->callobj;
    if (fp->callobj) {      /* Steal call object. */
        fp->callobj->setPrivate(newfp);
        fp->callobj = NULL;
    }
    newfp->argsobj = fp->argsobj;
    if (fp->argsobj) {      /* Steal args object. */
        JSVAL_TO_OBJECT(fp->argsobj)->setPrivate(newfp);
        fp->argsobj = NULL;
    }
    newfp->script = fp->script;
    newfp->fun = fp->fun;
    newfp->thisv = fp->thisv;
    newfp->argc = fp->argc;
    newfp->argv = vp + 2;
    newfp->rval = fp->rval;
    newfp->annotation = NULL;
    newfp->scopeChain = fp->scopeChain;
    JS_ASSERT(!fp->blockChain);
    newfp->blockChain = NULL;
    newfp->flags = fp->flags | JSFRAME_GENERATOR | JSFRAME_FLOATING_GENERATOR;

    /* Copy in arguments and slots. */
    memcpy(vp, fp->argv - 2, vplen * sizeof(jsval));
    memcpy(slots, fp->slots(), fp->script->nfixed * sizeof(jsval));

    obj->setPrivate(gen);
    return obj;
}

JSGenerator *
js_FloatingFrameToGenerator(JSStackFrame *fp)
{
    JS_ASSERT(fp->isGenerator() && fp->isFloatingGenerator());
    char *floatingStackp = (char *)(fp->argv - 2);
    char *p = floatingStackp - offsetof(JSGenerator, floatingStack);
    return reinterpret_cast<JSGenerator *>(p);
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
    if (gen->state == JSGEN_RUNNING || gen->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_NESTING_GENERATOR,
                            JSDVG_SEARCH_STACK, OBJECT_TO_JSVAL(obj),
                            JS_GetFunctionId(gen->getFloatingFrame()->fun));
        return JS_FALSE;
    }

    /* Check for OOM errors here, where we can fail easily. */
    if (!cx->ensureGeneratorStackSpace())
        return JS_FALSE;

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

    JSStackFrame *genfp = gen->getFloatingFrame();
    JSBool ok;
    {
        jsval *genVp = gen->floatingStack;
        uintN vplen = gen->vplen;
        uintN nfixed = genfp->script->nslots;

        /*
         * Get a pointer to new frame/slots. This memory is not "claimed", so
         * the code before pushExecuteFrame must not reenter the interpreter.
         */
        ExecuteFrameGuard frame;
        if (!cx->stack().getExecuteFrame(cx, cx->fp, vplen, nfixed, frame)) {
            gen->state = JSGEN_CLOSED;
            return JS_FALSE;
        }

        jsval *vp = frame.getvp();
        JSStackFrame *fp = frame.getFrame();

        /*
         * Copy and rebase stack frame/args/slots. The "floating" flag must
         * only be set on the generator's frame. See args_or_call_trace.
         */
        uintN usedBefore = gen->savedRegs.sp - genVp;
        memcpy(vp, genVp, usedBefore * sizeof(jsval));
        fp->flags &= ~JSFRAME_FLOATING_GENERATOR;
        fp->argv = vp + 2;
        gen->savedRegs.sp = fp->slots() + (gen->savedRegs.sp - genfp->slots());
        JS_ASSERT(uintN(gen->savedRegs.sp - fp->slots()) <= fp->script->nslots);

#ifdef DEBUG
        JSObject *callobjBefore = fp->callobj;
        jsval argsobjBefore = fp->argsobj;
#endif

        /*
         * Repoint Call, Arguments, Block and With objects to the new live
         * frame. Call and Arguments are done directly because we have
         * pointers to them. Block and With objects are done indirectly through
         * 'liveFrame'. See js_LiveFrameToFloating comment in jsiter.h.
         */
        if (genfp->callobj)
            fp->callobj->setPrivate(fp);
        if (genfp->argsobj)
            JSVAL_TO_OBJECT(fp->argsobj)->setPrivate(fp);
        gen->liveFrame = fp;
        (void)cx->enterGenerator(gen); /* OOM check above. */

        /* Officially push |fp|. |frame|'s destructor pops. */
        cx->stack().pushExecuteFrame(cx, frame, gen->savedRegs, NULL);

        ok = js_Interpret(cx);

        /* Restore call/args/block objects. */
        cx->leaveGenerator(gen);
        gen->liveFrame = genfp;
        if (fp->argsobj)
            JSVAL_TO_OBJECT(fp->argsobj)->setPrivate(genfp);
        if (fp->callobj)
            fp->callobj->setPrivate(genfp);

        JS_ASSERT_IF(argsobjBefore, argsobjBefore == fp->argsobj);
        JS_ASSERT_IF(callobjBefore, callobjBefore == fp->callobj);

        /* Copy and rebase stack frame/args/slots. Restore "floating" flag. */
        JS_ASSERT(uintN(gen->savedRegs.sp - fp->slots()) <= fp->script->nslots);
        uintN usedAfter = gen->savedRegs.sp - vp;
        memcpy(genVp, vp, usedAfter * sizeof(jsval));
        genfp->flags |= JSFRAME_FLOATING_GENERATOR;
        genfp->argv = genVp + 2;
        gen->savedRegs.sp = genfp->slots() + (gen->savedRegs.sp - fp->slots());
        JS_ASSERT(uintN(gen->savedRegs.sp - genfp->slots()) <= genfp->script->nslots);
    }

    if (gen->getFloatingFrame()->flags & JSFRAME_YIELDING) {
        /* Yield cannot fail, throw or be called on closing. */
        JS_ASSERT(ok);
        JS_ASSERT(!cx->throwing);
        JS_ASSERT(gen->state == JSGEN_RUNNING);
        JS_ASSERT(op != JSGENOP_CLOSE);
        genfp->flags &= ~JSFRAME_YIELDING;
        gen->state = JSGEN_OPEN;
        return JS_TRUE;
    }

    genfp->rval = JSVAL_VOID;
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
    JS_ASSERT(obj->getClass() == &js_GeneratorClass.base);

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

    LeaveTrace(cx);

    obj = JS_THIS_OBJECT(cx, vp);
    if (!JS_InstanceOf(cx, obj, &js_GeneratorClass.base, vp + 2))
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
    *vp = gen->getFloatingFrame()->rval;
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

    proto = JS_InitClass(cx, obj, NULL, &js_IteratorClass.base, Iterator, 2,
                         NULL, iterator_methods, NULL, NULL);
    if (!proto)
        return NULL;

#if JS_HAS_GENERATORS
    /* Initialize the generator internals if configured. */
    if (!JS_InitClass(cx, obj, NULL, &js_GeneratorClass.base, NULL, 0,
                      NULL, generator_methods, NULL, NULL)) {
        return NULL;
    }
#endif

    return JS_InitClass(cx, obj, NULL, &js_StopIterationClass, NULL, 0,
                        NULL, NULL, NULL, NULL);
}
