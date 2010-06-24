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

#define __STDC_LIMIT_MACROS

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

ExtendedClass js_IteratorClass = {
  { "Iterator",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Iterator) |
    JSCLASS_MARK_IS_TRACE |
    JSCLASS_IS_EXTENDED,
    PropertyStub,     PropertyStub,    PropertyStub,     PropertyStub,
    EnumerateStub,    ResolveStub,     ConvertStub,      iterator_finalize,
    NULL,             NULL,            NULL,             NULL,
    NULL,             NULL,            JS_CLASS_TRACE(iterator_trace), NULL },
    NULL,             NULL,            NULL,             iterator_iterator,
    NULL,
    JSCLASS_NO_RESERVED_MEMBERS
};

void
NativeIterator::mark(JSTracer *trc)
{
    if ((flags & JSITER_FOREACH) == 0)
        MarkIdRange(trc, (jsid *)props_array, (jsid *)props_end, "props");
    else
        MarkValueRange(trc, (Value *)props_array, (Value *)props_end, "props");
    MarkObject(trc, obj, "obj");
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

static inline bool
NewKeyValuePair(JSContext *cx, jsid id, const Value &val, Value *rval)
{
    Value vec[2] = { IdToValue(id), val };
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(vec), vec);

    JSObject *aobj = js_NewArrayObject(cx, 2, vec);
    if (!aobj)
        return false;
    rval->setNonFunObj(*aobj);
    return true;
}

template <class T>
static inline void
Reverse(T *beg, T *end)
{
    while (beg != end) {
        if (--end == beg)
            return;
        T tmp = *beg;
        *beg = *end;
        *end = tmp;
        ++beg;
    }
}

struct IterateKeyResults
{
    AutoIdVector vec;
    IterateKeyResults(JSContext *cx) : vec(cx) {}

    const jsid *begin() const { return vec.begin(); }
    size_t length() const { return vec.length(); }

    void reverse(size_t startingAt) {
        Reverse(vec.begin() + startingAt, vec.end());
    }

    JS_ALWAYS_INLINE bool addProperty(JSContext *, JSObject *, jsid id, uintN flags)
    {
        JS_ASSERT((flags & JSITER_FOREACH) == 0);
        return vec.append(id);
    }
};

struct IterateValueResults
{
    AutoValueVector vec;
    IterateValueResults(JSContext *cx) : vec(cx) {}

    const Value *begin() const { return vec.begin(); }
    size_t length() const { return vec.length(); }

    void reverse(size_t startingAt) {
        Reverse(vec.begin() + startingAt, vec.end());
    }

    bool addProperty(JSContext *cx, JSObject *obj, jsid id, uintN flags)
    {
        JS_ASSERT(flags & JSITER_FOREACH);

        if (!vec.growBy(1))
            return false;

        /* Do the lookup on the original object instead of the prototype. */
        Value *vp = vec.end() - 1;
        if (!obj->getProperty(cx, id, vp))
            return false;
        if ((flags & JSITER_KEYVALUE) && !NewKeyValuePair(cx, id, *vp, vp))
            return false;

        return true;
    }
};

struct IdHashPolicy {
    typedef jsid Lookup;
    static HashNumber hash(jsid id) {
        return JSID_BITS(id);
    }
    static bool match(jsid id1, jsid id2) {
        return id1 == id2;
    }
};

typedef HashSet<jsid, IdHashPolicy, ContextAllocPolicy> IdSet;

template <class ResultSet>
static inline bool
Enumerate(JSContext *cx, JSObject *obj, JSObject *pobj, jsid id,
          bool enumerable, uintN flags, IdSet& seen, ResultSet &results)
{
    JS_ASSERT(JSID_IS_INT(id) || JSID_IS_ATOM(id));

    if (JS_LIKELY(!(flags & JSITER_OWNONLY))) {
        IdSet::AddPtr p = seen.lookupForAdd(id);
        /* property already encountered, done. */
        if (JS_UNLIKELY(!!p))
            return true;
        /* no need to add properties to the hash table at the end of the prototype chain */
        if (pobj->getProto() && !seen.add(p, id))
            return false;
    }

    if (enumerable || (flags & JSITER_HIDDEN))
        return results.addProperty(cx, obj, id, flags);

    return true;
}

template <class ResultSet>
static bool
EnumerateNativeProperties(JSContext *cx, JSObject *obj, JSObject *pobj, uintN flags,
                          IdSet &seen, ResultSet &results)
{
    JS_LOCK_OBJ(cx, pobj);

    size_t initialLength = results.length();

    /* Collect all unique properties from this object's scope. */
    JSScope *scope = pobj->scope();
    for (JSScopeProperty *sprop = scope->lastProperty(); sprop; sprop = sprop->parent) {
        if (!JSID_IS_DEFAULT_XML_NAMESPACE(sprop->id) &&
            !sprop->isAlias() &&
            !Enumerate(cx, obj, pobj, sprop->id, sprop->enumerable(), flags, seen, results)) {
            return false;
        }
    }

    results.reverse(initialLength);

    JS_UNLOCK_SCOPE(cx, scope);
    return true;
}

template <class ResultSet>
static bool
EnumerateDenseArrayProperties(JSContext *cx, JSObject *obj, JSObject *pobj, uintN flags,
                              IdSet &seen, ResultSet &results)
{
    size_t count = pobj->getDenseArrayCount();

    if (count) {
        size_t capacity = pobj->getDenseArrayCapacity();
        Value *vp = pobj->dslots;
        for (size_t i = 0; i < capacity; ++i, ++vp) {
            if (!vp->isMagic(JS_ARRAY_HOLE)) {
                /* Dense arrays never get so large that i would not fit into an integer id. */
                if (!Enumerate(cx, obj, pobj, INT_TO_JSID(i), true, flags, seen, results))
                    return false;
            }
        }
    }
    return true;
}

inline void
NativeIterator::init(JSObject *obj, uintN flags, uint32 *sarray, uint32 slength, uint32 key)
{
    this->obj = obj;
    this->flags = flags;
    this->shapes_array = (uint32 *) this->props_end;
    this->shapes_length = slength;
    this->shapes_key = key;
    if (slength)
        memcpy(this->shapes_array, sarray, slength * sizeof(uint32));
}

NativeIterator *
NativeIterator::allocateKeyIterator(JSContext *cx, uint32 slength,
                                    const jsid *parray, uint32 plength)
{
    NativeIterator *ni = (NativeIterator *)
        cx->malloc(sizeof(NativeIterator) + plength * sizeof(jsid) + slength * sizeof(uint32));
    if (!ni)
        return NULL;
    ni->props_array = ni->props_cursor = (jsid *) (ni + 1);
    ni->props_end = (jsid *)ni->props_array + plength;
    if (plength)
        memcpy(ni->props_array, parray, plength * sizeof(jsid));
    return ni;
}

NativeIterator *
NativeIterator::allocateValueIterator(JSContext *cx, uint32 slength,
                                      const Value *parray, uint32 plength)
{
    NativeIterator *ni = (NativeIterator *)
        cx->malloc(sizeof(NativeIterator) + plength * sizeof(Value) + slength * sizeof(uint32));
    if (!ni)
        return NULL;
    ni->props_array = ni->props_cursor = (Value *) (ni + 1);
    ni->props_end = (Value *)ni->props_array + plength;
    if (plength)
        memcpy(ni->props_array, parray, plength * sizeof(Value));
    return ni;
}

template <class ResultSet>
static bool
Snapshot(JSContext *cx, JSObject *obj, uintN flags, ResultSet &results)
{
    IdSet seen(cx);
    if (!(flags & JSITER_OWNONLY) && !seen.init(32))
        return NULL;

    JSObject *pobj = obj;
    while (pobj) {
        Class *clasp = pobj->getClass();
        if (pobj->isNative() &&
            pobj->map->ops->enumerate == js_Enumerate &&
            !(clasp->flags & JSCLASS_NEW_ENUMERATE)) {
            if (!clasp->enumerate(cx, pobj))
                return false;
            if (!EnumerateNativeProperties(cx, obj, pobj, flags, seen, results))
                return false;
        } else if (pobj->isDenseArray()) {
            if (!EnumerateDenseArrayProperties(cx, obj, pobj, flags, seen, results))
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
                    if (!Enumerate(cx, obj, pobj, ida->vector[n], true, flags, seen, results))
                        return false;
                }
                /* Proxy objects enumerate the prototype on their own, so we are done here. */
                break;
            }
            Value state;
            if (!pobj->enumerate(cx, JSENUMERATE_INIT, &state, NULL))
                return false;
            if (state.isMagic(JS_NATIVE_ENUMERATE)) {
                if (!EnumerateNativeProperties(cx, obj, pobj, flags, seen, results))
                    return false;
            } else {
                while (true) {
                    jsid id;
                    if (!pobj->enumerate(cx, JSENUMERATE_NEXT, &state, &id))
                        return false;
                    if (state.isNull())
                        break;
                    if (!Enumerate(cx, obj, pobj, id, true, flags, seen, results))
                        return false;
                }
            }
        }

        if (JS_UNLIKELY(pobj->isXML() || (flags & JSITER_OWNONLY)))
            break;

        pobj = pobj->getProto();
    }

    return true;
}

static NativeIterator *
SnapshotValues(JSContext *cx, JSObject *obj, uintN flags, uint32 *sarray, uint32 slength, uint32 key)
{
    JS_ASSERT(flags & JSITER_FOREACH);

    IterateValueResults results(cx);
    if (!Snapshot(cx, obj, flags, results))
        return NULL;

    NativeIterator *ni = NativeIterator::allocateValueIterator(cx, slength,
                                                               results.begin(), results.length());
    if (!ni)
        return NULL;
    ni->init(obj, flags, sarray, slength, key);
    return ni;
}

static NativeIterator *
SnapshotKeys(JSContext *cx, JSObject *obj, uintN flags, uint32 *sarray, uint32 slength, uint32 key)
{
    JS_ASSERT(!(flags & JSITER_FOREACH));

    IterateKeyResults results(cx);
    if (!Snapshot(cx, obj, flags, results))
        return NULL;

    NativeIterator *ni = NativeIterator::allocateKeyIterator(cx, slength,
                                                             results.begin(), results.length());
    if (!ni)
        return NULL;
    ni->init(obj, flags, sarray, slength, key);
    return ni;
}

static inline bool
NativeIteratorToJSIdArray(JSContext *cx, NativeIterator *ni, JSIdArray **idap)
{
    /* Morph the NativeIterator into a JSIdArray. The caller will deallocate it. */
    JS_ASSERT(sizeof(NativeIterator) > sizeof(JSIdArray));
    JS_ASSERT(!(ni->flags & JSITER_FOREACH));
    JS_ASSERT(ni->props_array == (jsid *) (ni + 1));
    size_t length = size_t((jsid *)ni->props_end - (jsid *)ni->props_array);
    JSIdArray *ida = (JSIdArray *) (uintptr_t(ni->props_array) - (sizeof(JSIdArray) - sizeof(jsid)));
    ida->self = ni;
    ida->length = length;
    JS_ASSERT(&ida->vector[0] == (jsid *)ni->props_array);
    *idap = ida;
    return true;
}

bool
GetPropertyNames(JSContext *cx, JSObject *obj, uintN flags, JSIdArray **idap)
{
    NativeIterator *ni = SnapshotKeys(cx, obj, flags & (JSITER_OWNONLY | JSITER_HIDDEN), NULL, 0, true);
    if (!ni)
        return false;
    return NativeIteratorToJSIdArray(cx, ni, idap);
}

static inline bool
GetCustomIterator(JSContext *cx, JSObject *obj, uintN flags, Value *vp)
{
    /* Check whether we have a valid __iterator__ method. */
    JSAtom *atom = cx->runtime->atomState.iteratorAtom;
    if (!js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, vp))
        return false;

    /* If there is no custom __iterator__ method, we are done here. */
    if (vp->isUndefined())
        return true;

    /* Otherwise call it and return that object. */
    LeaveTrace(cx);
    Value arg = BooleanTag((flags & JSITER_FOREACH) == 0);
    if (!InternalCall(cx, obj, *vp, 1, &arg, vp))
        return false;
    if (vp->isPrimitive()) {
        /*
         * We are always coming from js_ValueToIterator, and we are no longer on
         * trace, so the object we are iterating over is on top of the stack (-1).
         */
        js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                             -1, ObjectTag(*obj), NULL,
                             js_AtomToPrintableString(cx, atom));
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

static JSObject *
NewIteratorObject(JSContext *cx, uintN flags)
{
    return !(flags & JSITER_ENUMERATE)
           ? NewObject(cx, &js_IteratorClass.base, NULL, NULL)
           : NewObjectWithGivenProto(cx, &js_IteratorClass.base, NULL, NULL);
}

static inline void
RegisterEnumerator(JSContext *cx, JSObject *iterobj, NativeIterator *ni)
{
    /* Register non-escaping native enumerators (for-in) with the current context. */
    if (ni->flags & JSITER_ENUMERATE) {
        ni->next = cx->enumerators;
        cx->enumerators = iterobj;
    }
}

bool
JSIdArrayToIterator(JSContext *cx, JSObject *obj, uintN flags, JSIdArray *ida, Value *vp)
{
    JS_ASSERT((flags & JSITER_FOREACH) == 0);

    JSObject *iterobj = NewIteratorObject(cx, flags);
    if (!iterobj)
        return false;

    vp->setNonFunObj(*iterobj);

    NativeIterator *ni = NativeIterator::allocateKeyIterator(cx, 0, ida->vector, ida->length);
    if (!ni)
        return false;
    ni->init(obj, flags, NULL, 0, 0);

    iterobj->setNativeIterator(ni);

    RegisterEnumerator(cx, iterobj, ni);
    return true;
}

bool
GetIterator(JSContext *cx, JSObject *obj, uintN flags, Value *vp)
{
    uint32 hash;
    JSObject **hp;
    Vector<uint32, 8> shapes(cx);
    uint32 key = 0;

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
                NativeIterator *ni = iterobj->getNativeIterator();
                if (ni->shapes_key == key &&
                    ni->shapes_length == shapes.length() &&
                    Compare(ni->shapes_array, shapes.begin(), ni->shapes_length)) {
                    vp->setNonFunObj(*iterobj);
                    *hp = ni->next;

                    RegisterEnumerator(cx, iterobj, ni);
                    return true;
                }
            }
        }

      miss:
        if (obj->isProxy())
            return JSProxy::iterate(cx, obj, flags, vp);
        if (!GetCustomIterator(cx, obj, flags, vp))
            return false;
        if (!vp->isUndefined())
            return true;
    }

    JSObject *iterobj = NewIteratorObject(cx, flags);
    if (!iterobj)
        return false;

    /* Store in *vp to protect it from GC (callers must root vp). */
    vp->setNonFunObj(*iterobj);

    NativeIterator *ni = (flags & JSITER_FOREACH)
                         ? SnapshotValues(cx, obj, flags, shapes.begin(), shapes.length(), key)
                         : SnapshotKeys(cx, obj, flags, shapes.begin(), shapes.length(), key);
    if (!ni)
        return false;

    iterobj->setNativeIterator(ni);

    RegisterEnumerator(cx, iterobj, ni);
    return true;
}

static JSObject *
iterator_iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
    return obj;
}

static JSBool
Iterator(JSContext *cx, JSObject *iterobj, uintN argc, Value *argv, Value *rval)
{
    JSBool keyonly;
    uintN flags;

    keyonly = js_ValueToBoolean(argv[1]);
    flags = JSITER_OWNONLY | (keyonly ? 0 : (JSITER_FOREACH | JSITER_KEYVALUE));
    *rval = argv[0];
    return js_ValueToIterator(cx, flags, rval);
}

JSBool
js_ThrowStopIteration(JSContext *cx)
{
    Value v;

    JS_ASSERT(!JS_IsExceptionPending(cx));
    if (js_FindClassObject(cx, NULL, JSProto_StopIteration, &v))
        SetPendingException(cx, v);
    return JS_FALSE;
}

static JSBool
iterator_next(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj;

    obj = ComputeThisObjectFromVp(cx, vp);
    if (!InstanceOf(cx, obj, &js_IteratorClass.base, vp + 2))
        return false;

    if (!js_IteratorMore(cx, obj, vp))
        return false;
    if (!vp->asBoolean()) {
        js_ThrowStopIteration(cx);
        return false;
    }
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
js_ValueToIterator(JSContext *cx, uintN flags, Value *vp)
{
    /* JSITER_KEYVALUE must always come with JSITER_FOREACH */
    JS_ASSERT_IF(flags & JSITER_KEYVALUE, flags & JSITER_FOREACH);

    /*
     * Make sure the more/next state machine doesn't get stuck. A value might be
     * left in iterValue when a trace is left due to an operation time-out after
     * JSOP_MOREITER but before the value is picked up by FOR*.
     */
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    AutoValueRooter tvr(cx);

    if (vp->isObject()) {
        /* Common case. */
        *tvr.addr() = *vp;
    } else {
        /*
         * Enumerating over null and undefined gives an empty enumerator.
         * This is contrary to ECMA-262 9.9 ToObject, invoked from step 3 of
         * the first production in 12.6.4 and step 4 of the second production,
         * but it's "web JS" compatible. ES5 fixed for-in to match this de-facto
         * standard.
         */
        if ((flags & JSITER_ENUMERATE)) {
            if (!js_ValueToObjectOrNull(cx, *vp, tvr.addr()))
                return false;
            if (tvr.value().isNull())
                return GetIterator(cx, NULL, flags, vp);
        } else {
            if (!js_ValueToNonNullObject(cx, *vp, tvr.addr()))
                return false;
        }
    }
    JSObject *obj = &tvr.value().asObject();

    Class *clasp = obj->getClass();
    ExtendedClass *xclasp;
    if ((clasp->flags & JSCLASS_IS_EXTENDED) &&
        (xclasp = (ExtendedClass *) clasp)->iteratorObject) {
        /* Enumerate Iterator.prototype directly. */
        if (clasp != &js_IteratorClass.base || obj->getNativeIterator()) {
            JSObject *iterobj = xclasp->iteratorObject(cx, obj, !(flags & JSITER_FOREACH));
            if (!iterobj)
                return false;
            vp->setNonFunObj(*iterobj);
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
js_CloseIterator(JSContext *cx, const Value &v)
{
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    JS_ASSERT(v.isObject());
    JSObject *obj = &v.asObject();
    Class *clasp = obj->getClass();

    if (clasp == &js_IteratorClass.base) {
        /* Remove enumerators from the active list, which is a stack. */
        NativeIterator *ni = obj->getNativeIterator();
        if (ni->flags & JSITER_ENUMERATE) {
            JS_ASSERT(cx->enumerators == obj);
            cx->enumerators = ni->next;
        }

        /* Cache the iterator object if possible. */
        if (ni->shapes_length) {
            uint32 hash = ni->shapes_key % NATIVE_ITER_CACHE_SIZE;
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

/*
 * Suppress enumeration of deleted properties. We maintain a list of all active
 * non-escaping for-in enumerators. Whenever a property is deleted, we check
 * whether any active enumerator contains the (obj, id) pair and has not
 * enumerated id yet. If so, we delete the id from the list (or advance the
 * cursor if it is the next id to be enumerated).
 *
 * We do not suppress enumeration of a property deleted along an object's
 * prototype chain. Only direct deletions on the object are handled.
 */
bool
js_SuppressDeletedProperty(JSContext *cx, JSObject *obj, jsid id)
{
    JSObject *iterobj = cx->enumerators;
    while (iterobj) {
      again:
        NativeIterator *ni = iterobj->getNativeIterator();
        /* This only works for identified surpressed keys, not values. */
        if ((ni->flags & JSITER_FOREACH) && ni->obj == obj && ni->props_cursor < ni->props_end) {
            /* Check whether id is still to come. */
            jsid *props_cursor = (jsid *)ni->props_cursor;
            jsid *props_end = (jsid *)ni->props_end;
            for (jsid *idp = props_cursor; idp < props_end; ++idp) {
                if (*idp == id) {
                    /*
                     * Check whether another property along the prototype chain
                     * became visible as a result of this deletion.
                     */
                    if (obj->getProto()) {
                        AutoObjectRooter proto(cx, obj->getProto());
                        AutoObjectRooter obj2(cx);
                        JSProperty *prop;
                        if (!proto.object()->lookupProperty(cx, id, obj2.addr(), &prop))
                            return false;
                        if (prop) {
                            uintN attrs;
                            if (obj2.object()->isNative()) {
                                attrs = ((JSScopeProperty *) prop)->attributes();
                                JS_UNLOCK_OBJ(cx, obj2.object());
                            } else if (!obj2.object()->getAttributes(cx, id, &attrs)) {
                                return false;
                            }
                            if (attrs & JSPROP_ENUMERATE)
                                continue;
                        }
                    }

                    /*
                     * If lookupProperty or getAttributes above removed a property from
                     * ni, start over.
                     */
                    if (props_end != ni->props_end || props_cursor != ni->props_cursor)
                        goto again;

                    /*
                     * No property along the prototype chain steppeded in to take the
                     * property's place, so go ahead and delete id from the list.
                     * If it is the next property to be enumerated, just skip it.
                     */
                    if (idp == props_cursor) {
                        ni->props_cursor = (jsid *)ni->props_cursor + 1;
                    } else {
                        memmove(idp, idp + 1, (props_end - (idp + 1)) * sizeof(jsid));
                        ni->props_end = (jsid *)ni->props_end - 1;
                    }
                    break;
                }
            }
        }
        iterobj = ni->next;
    }
    return true;
}

JSBool
js_IteratorMore(JSContext *cx, JSObject *iterobj, Value *rval)
{
    /* Fast path for native iterators */
    if (iterobj->getClass() == &js_IteratorClass.base) {
        /*
         * Implement next directly as all the methods of native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->getNativeIterator();
        rval->setBoolean(ni->props_cursor < ni->props_end);
        return true;
    }

    /* We might still have a pending value. */
    if (!cx->iterValue.isMagic(JS_NO_ITER_VALUE)) {
        rval->setBoolean(true);
        return true;
    }

    /* Fetch and cache the next value from the iterator. */
    jsid id = ATOM_TO_JSID(cx->runtime->atomState.nextAtom);
    if (!js_GetMethod(cx, iterobj, id, JSGET_METHOD_BARRIER, rval))
        return false;
    if (!InternalCall(cx, iterobj, *rval, 0, NULL, rval)) {
        /* Check for StopIteration. */
        if (!cx->throwing || !js_ValueIsStopIteration(cx->exception))
            return false;

        /* Inline JS_ClearPendingException(cx). */
        cx->throwing = JS_FALSE;
        cx->exception.setUndefined();
        cx->iterValue.setMagic(JS_NO_ITER_VALUE);
        rval->setBoolean(false);
        return true;
    }

    /* Cache the value returned by iterobj.next() so js_IteratorNext() can find it. */
    JS_ASSERT(!rval->isMagic(JS_NO_ITER_VALUE));
    cx->iterValue = *rval;
    rval->setBoolean(true);
    return true;
}

JSBool
js_IteratorNext(JSContext *cx, JSObject *iterobj, Value *rval)
{
    /* Fast path for native iterators */
    if (iterobj->getClass() == &js_IteratorClass.base) {
        /*
         * Implement next directly as all the methods of the native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->getNativeIterator();
        JS_ASSERT(ni->props_cursor < ni->props_end);
        if ((ni->flags & JSITER_FOREACH) == 0) {
            *rval = IdToValue(ni->currentId());
            ni->incIdCursor();
        } else {
            *rval = ni->currentValue();
            ni->incValueCursor();
        }

        if (rval->isString() || (ni->flags & JSITER_FOREACH))
            return true;

        JSString *str;
        jsint i;
        if (rval->isInt32() && (jsuint(i = rval->asInt32()) < INT_STRING_LIMIT)) {
            str = JSString::intString(i);
        } else {
            str = js_ValueToString(cx, *rval);
            if (!str)
                return false;
        }

        rval->setString(str);
        return true;
    }

    JS_ASSERT(!cx->iterValue.isMagic(JS_NO_ITER_VALUE));
    *rval = cx->iterValue;
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    return true;
}

static JSBool
stopiter_hasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    *bp = js_ValueIsStopIteration(*v);
    return JS_TRUE;
}

Class js_StopIterationClass = {
    js_StopIteration_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_StopIteration),
    PropertyStub,     PropertyStub,
    PropertyStub,     PropertyStub,
    EnumerateStub,    ResolveStub,
    ConvertStub,      NULL,
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
    MarkValueRange(trc, gen->floatingStack, fp->argEnd(), "generator slots");
    js_TraceStackFrame(trc, fp);
    MarkValueRange(trc, fp->slots(), gen->savedRegs.sp, "generator slots");
}

ExtendedClass js_GeneratorClass = {
  { js_Generator_str,
    JSCLASS_HAS_PRIVATE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Generator) |
    JSCLASS_IS_ANONYMOUS |
    JSCLASS_MARK_IS_TRACE |
    JSCLASS_IS_EXTENDED,
    PropertyStub,     PropertyStub,    PropertyStub,    PropertyStub,
    EnumerateStub,    ResolveStub,     ConvertStub,     generator_finalize,
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
                   (-1 + /* one Value included in JSGenerator */
                    vplen +
                    VALUES_PER_STACK_FRAME +
                    fp->script->nslots) * sizeof(Value);

    JSGenerator *gen = (JSGenerator *) cx->malloc(nbytes);
    if (!gen)
        return NULL;

    /* Cut up floatingStack space. */
    Value *vp = gen->floatingStack;
    JSStackFrame *newfp = reinterpret_cast<JSStackFrame *>(vp + vplen);
    Value *slots = newfp->slots();

    /* Initialize JSGenerator. */
    gen->obj = obj;
    gen->state = JSGEN_NEWBORN;
    gen->savedRegs.pc = cx->regs->pc;
    JS_ASSERT(cx->regs->sp == fp->slots() + fp->script->nfixed);
    gen->savedRegs.sp = slots + fp->script->nfixed;
    gen->vplen = vplen;
    gen->enumerators = NULL;
    gen->liveFrame = newfp;

    /* Copy generator's stack frame copy in from |cx->fp|. */
    newfp->imacpc = NULL;
    newfp->callobj = fp->callobj;
    if (fp->callobj) {      /* Steal call object. */
        fp->callobj->setPrivate(newfp);
        fp->callobj = NULL;
    }
    JSObject *argsobj = fp->argsObj();
    newfp->setArgsObj(argsobj);
    if (argsobj) {      /* Steal args object. */
        argsobj->setPrivate(newfp);
        fp->setArgsObj(NULL);
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
    memcpy(vp, fp->argv - 2, vplen * sizeof(Value));
    memcpy(slots, fp->slots(), fp->script->nfixed * sizeof(Value));

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
                JSGenerator *gen, const Value &arg)
{
    if (gen->state == JSGEN_RUNNING || gen->state == JSGEN_CLOSING) {
        js_ReportValueError(cx, JSMSG_NESTING_GENERATOR,
                            JSDVG_SEARCH_STACK, ObjectOrNullTag(obj),
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
        SetPendingException(cx, arg);
        gen->state = JSGEN_RUNNING;
        break;

      default:
        JS_ASSERT(op == JSGENOP_CLOSE);
        SetPendingException(cx, JS_GENERATOR_CLOSING);
        gen->state = JSGEN_CLOSING;
        break;
    }

    JSStackFrame *genfp = gen->getFloatingFrame();
    JSBool ok;
    {
        Value *genVp = gen->floatingStack;
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

        Value *vp = frame.getvp();
        JSStackFrame *fp = frame.getFrame();

        /*
         * Copy and rebase stack frame/args/slots. The "floating" flag must
         * only be set on the generator's frame. See args_or_call_trace.
         */
        uintN usedBefore = gen->savedRegs.sp - genVp;
        memcpy(vp, genVp, usedBefore * sizeof(Value));
        fp->flags &= ~JSFRAME_FLOATING_GENERATOR;
        fp->argv = vp + 2;
        gen->savedRegs.sp = fp->slots() + (gen->savedRegs.sp - genfp->slots());
        JS_ASSERT(uintN(gen->savedRegs.sp - fp->slots()) <= fp->script->nslots);

#ifdef DEBUG
        JSObject *callobjBefore = fp->callobj;
        JSObject *argsobjBefore = fp->argsObj();
#endif

        /*
         * Repoint Call, Arguments, Block and With objects to the new live
         * frame. Call and Arguments are done directly because we have
         * pointers to them. Block and With objects are done indirectly through
         * 'liveFrame'. See js_LiveFrameToFloating comment in jsiter.h.
         */
        if (genfp->callobj)
            fp->callobj->setPrivate(fp);
        if (genfp->argsObj())
            fp->argsObj()->setPrivate(fp);
        gen->liveFrame = fp;
        (void)cx->enterGenerator(gen); /* OOM check above. */

        /* Officially push |fp|. |frame|'s destructor pops. */
        cx->stack().pushExecuteFrame(cx, frame, gen->savedRegs, NULL);

        /* Swap the enumerators stack for the generator's stack. */
        JSObject *enumerators = cx->enumerators;
        cx->enumerators = gen->enumerators;

        ok = RunScript(cx, fp->script, fp->fun, &fp->scopeChain.asObject());

        /* Restore the original enumerators stack. */
        gen->enumerators = cx->enumerators;
        cx->enumerators = enumerators;

        /* Restore call/args/block objects. */
        cx->leaveGenerator(gen);
        gen->liveFrame = genfp;
        if (fp->argsObj())
            fp->argsObj()->setPrivate(genfp);
        if (fp->callobj)
            fp->callobj->setPrivate(genfp);

        JS_ASSERT_IF(argsobjBefore, argsobjBefore == fp->argsObj());
        JS_ASSERT_IF(callobjBefore, callobjBefore == fp->callobj);

        /* Copy and rebase stack frame/args/slots. Restore "floating" flag. */
        JS_ASSERT(uintN(gen->savedRegs.sp - fp->slots()) <= fp->script->nslots);
        uintN usedAfter = gen->savedRegs.sp - vp;
        memcpy(genVp, vp, usedAfter * sizeof(Value));
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

    genfp->rval.setUndefined();
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

    return SendToGenerator(cx, JSGENOP_CLOSE, obj, gen, UndefinedTag());
}

/*
 * Common subroutine of generator_(next|send|throw|close) methods.
 */
static JSBool
generator_op(JSContext *cx, JSGeneratorOp op, Value *vp, uintN argc)
{
    JSObject *obj;
    LeaveTrace(cx);

    obj = ComputeThisObjectFromVp(cx, vp);
    if (!InstanceOf(cx, obj, &js_GeneratorClass.base, vp + 2))
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
            if (argc >= 1 && !vp[2].isUndefined()) {
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
            SetPendingException(cx, argc >= 1 ? vp[2] : Value(UndefinedTag()));
            return JS_FALSE;
          default:
            JS_ASSERT(op == JSGENOP_CLOSE);
            return JS_TRUE;
        }
    }

    bool undef = ((op == JSGENOP_SEND || op == JSGENOP_THROW) && argc != 0);
    if (!SendToGenerator(cx, op, obj, gen, undef ? vp[2] : Value(UndefinedTag())))
        return JS_FALSE;
    *vp = gen->getFloatingFrame()->rval;
    return JS_TRUE;
}

static JSBool
generator_send(JSContext *cx, uintN argc, Value *vp)
{
    return generator_op(cx, JSGENOP_SEND, vp, argc);
}

static JSBool
generator_next(JSContext *cx, uintN argc, Value *vp)
{
    return generator_op(cx, JSGENOP_NEXT, vp, argc);
}

static JSBool
generator_throw(JSContext *cx, uintN argc, Value *vp)
{
    return generator_op(cx, JSGENOP_THROW, vp, argc);
}

static JSBool
generator_close(JSContext *cx, uintN argc, Value *vp)
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

    proto = js_InitClass(cx, obj, NULL, &js_IteratorClass.base, Iterator, 2,
                         NULL, iterator_methods, NULL, NULL);
    if (!proto)
        return NULL;

#if JS_HAS_GENERATORS
    /* Initialize the generator internals if configured. */
    if (!js_InitClass(cx, obj, NULL, &js_GeneratorClass.base, NULL, 0,
                      NULL, generator_methods, NULL, NULL)) {
        return NULL;
    }
#endif

    return js_InitClass(cx, obj, NULL, &js_StopIterationClass, NULL, 0,
                        NULL, NULL, NULL, NULL);
}
