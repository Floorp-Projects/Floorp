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
#include "jsgcmark.h"
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
#include "jsvector.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "vm/GlobalObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

static void iterator_finalize(JSContext *cx, JSObject *obj);
static void iterator_trace(JSTracer *trc, JSObject *obj);
static JSObject *iterator_iterator(JSContext *cx, JSObject *obj, JSBool keysonly);

Class js::IteratorClass = {
    "Iterator",
    JSCLASS_HAS_PRIVATE |
    JSCLASS_CONCURRENT_FINALIZER |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Iterator),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    iterator_finalize,
    NULL,                 /* reserved    */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    iterator_trace,
    {
        NULL,             /* equality       */
        NULL,             /* outerObject    */
        NULL,             /* innerObject    */
        iterator_iterator,
        NULL              /* unused  */
    }
};

void
NativeIterator::mark(JSTracer *trc)
{
    MarkIdRange(trc, begin(), end(), "props");
    if (obj)
        MarkObject(trc, *obj, "obj");
}

static void
iterator_finalize(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isIterator());

    NativeIterator *ni = obj->getNativeIterator();
    if (ni) {
        cx->free_(ni);
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

struct IdHashPolicy {
    typedef jsid Lookup;
    static HashNumber hash(jsid id) {
        return JSID_BITS(id);
    }
    static bool match(jsid id1, jsid id2) {
        return id1 == id2;
    }
};

typedef HashSet<jsid, IdHashPolicy> IdSet;

static inline bool
NewKeyValuePair(JSContext *cx, jsid id, const Value &val, Value *rval)
{
    Value vec[2] = { IdToValue(id), val };
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(vec), vec);

    JSObject *aobj = NewDenseCopiedArray(cx, 2, vec);
    if (!aobj)
        return false;
    rval->setObject(*aobj);
    return true;
}

static inline bool
Enumerate(JSContext *cx, JSObject *obj, JSObject *pobj, jsid id,
          bool enumerable, uintN flags, IdSet& ht, AutoIdVector *props)
{
    JS_ASSERT_IF(flags & JSITER_OWNONLY, obj == pobj);

    if (!(flags & JSITER_OWNONLY) || pobj->isProxy() || pobj->getOps()->enumerate) {
        /* If we've already seen this, we definitely won't add it. */
        IdSet::AddPtr p = ht.lookupForAdd(id);
        if (JS_UNLIKELY(!!p))
            return true;

        /*
         * It's not necessary to add properties to the hash table at the end of
         * the prototype chain, but custom enumeration behaviors might return
         * duplicated properties, so always add in such cases.
         */
        if ((pobj->getProto() || pobj->isProxy() || pobj->getOps()->enumerate) && !ht.add(p, id))
            return false;
    }

    if (enumerable || (flags & JSITER_HIDDEN))
        return props->append(id);

    return true;
}

static bool
EnumerateNativeProperties(JSContext *cx, JSObject *obj, JSObject *pobj, uintN flags, IdSet &ht,
                          AutoIdVector *props)
{
    size_t initialLength = props->length();

    /* Collect all unique properties from this object's scope. */
    for (Shape::Range r = pobj->lastProperty()->all(); !r.empty(); r.popFront()) {
        const Shape &shape = r.front();

        if (!JSID_IS_DEFAULT_XML_NAMESPACE(shape.propid) &&
            !Enumerate(cx, obj, pobj, shape.propid, shape.enumerable(), flags, ht, props))
        {
            return false;
        }
    }

    ::Reverse(props->begin() + initialLength, props->end());
    return true;
}

static bool
EnumerateDenseArrayProperties(JSContext *cx, JSObject *obj, JSObject *pobj, uintN flags,
                              IdSet &ht, AutoIdVector *props)
{
    if (!Enumerate(cx, obj, pobj, ATOM_TO_JSID(cx->runtime->atomState.lengthAtom), false,
                   flags, ht, props)) {
        return false;
    }

    if (pobj->getArrayLength() > 0) {
        size_t initlen = pobj->getDenseArrayInitializedLength();
        const Value *vp = pobj->getDenseArrayElements();
        for (size_t i = 0; i < initlen; ++i, ++vp) {
            if (!vp->isMagic(JS_ARRAY_HOLE)) {
                /* Dense arrays never get so large that i would not fit into an integer id. */
                if (!Enumerate(cx, obj, pobj, INT_TO_JSID(i), true, flags, ht, props))
                    return false;
            }
        }
    }

    return true;
}

static bool
Snapshot(JSContext *cx, JSObject *obj, uintN flags, AutoIdVector *props)
{
    IdSet ht(cx);
    if (!ht.init(32))
        return NULL;

    JSObject *pobj = obj;
    do {
        Class *clasp = pobj->getClass();
        if (pobj->isNative() &&
            !pobj->getOps()->enumerate &&
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
                AutoIdVector proxyProps(cx);
                if (flags & JSITER_OWNONLY) {
                    if (flags & JSITER_HIDDEN) {
                        if (!JSProxy::getOwnPropertyNames(cx, pobj, proxyProps))
                            return false;
                    } else {
                        if (!JSProxy::keys(cx, pobj, proxyProps))
                            return false;
                    }
                } else {
                    if (!JSProxy::enumerate(cx, pobj, proxyProps))
                        return false;
                }
                for (size_t n = 0, len = proxyProps.length(); n < len; n++) {
                    if (!Enumerate(cx, obj, pobj, proxyProps[n], true, flags, ht, props))
                        return false;
                }
                /* Proxy objects enumerate the prototype on their own, so we are done here. */
                break;
            }
            Value state;
            JSIterateOp op = (flags & JSITER_HIDDEN) ? JSENUMERATE_INIT_ALL : JSENUMERATE_INIT;
            if (!pobj->enumerate(cx, op, &state, NULL))
                return false;
            if (state.isMagic(JS_NATIVE_ENUMERATE)) {
                if (!EnumerateNativeProperties(cx, obj, pobj, flags, ht, props))
                    return false;
            } else {
                while (true) {
                    jsid id;
                    if (!pobj->enumerate(cx, JSENUMERATE_NEXT, &state, &id))
                        return false;
                    if (state.isNull())
                        break;
                    if (!Enumerate(cx, obj, pobj, id, true, flags, ht, props))
                        return false;
                }
            }
        }

        if ((flags & JSITER_OWNONLY) || pobj->isXML())
            break;
    } while ((pobj = pobj->getProto()) != NULL);

    return true;
}

namespace js {

bool
VectorToIdArray(JSContext *cx, AutoIdVector &props, JSIdArray **idap)
{
    JS_STATIC_ASSERT(sizeof(JSIdArray) > sizeof(jsid));
    size_t len = props.length();
    size_t idsz = len * sizeof(jsid);
    size_t sz = (sizeof(JSIdArray) - sizeof(jsid)) + idsz;
    JSIdArray *ida = static_cast<JSIdArray *>(cx->malloc_(sz));
    if (!ida)
        return false;

    ida->length = static_cast<jsint>(len);
    memcpy(ida->vector, props.begin(), idsz);
    *idap = ida;
    return true;
}

JS_FRIEND_API(bool)
GetPropertyNames(JSContext *cx, JSObject *obj, uintN flags, AutoIdVector *props)
{
    return Snapshot(cx, obj, flags & (JSITER_OWNONLY | JSITER_HIDDEN), props);
}

}

size_t sCustomIteratorCount = 0;

static inline bool
GetCustomIterator(JSContext *cx, JSObject *obj, uintN flags, Value *vp)
{
    /* Check whether we have a valid __iterator__ method. */
    JSAtom *atom = cx->runtime->atomState.iteratorAtom;
    if (!js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, vp))
        return false;

    /* If there is no custom __iterator__ method, we are done here. */
    if (!vp->isObject()) {
        vp->setUndefined();
        return true;
    }

    if (!cx->runningWithTrustedPrincipals())
        ++sCustomIteratorCount;

    /* Otherwise call it and return that object. */
    LeaveTrace(cx);
    Value arg = BooleanValue((flags & JSITER_FOREACH) == 0);
    if (!Invoke(cx, ObjectValue(*obj), *vp, 1, &arg, vp))
        return false;
    if (vp->isPrimitive()) {
        /*
         * We are always coming from js_ValueToIterator, and we are no longer on
         * trace, so the object we are iterating over is on top of the stack (-1).
         */
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, atom, &bytes))
            return false;
        js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                             -1, ObjectValue(*obj), NULL, bytes.ptr());
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

static inline JSObject *
NewIteratorObject(JSContext *cx, uintN flags)
{
    if (flags & JSITER_ENUMERATE) {
        /*
         * Non-escaping native enumerator objects do not need map, proto, or
         * parent. However, code in jstracer.cpp and elsewhere may find such a
         * native enumerator object via the stack and (as for all objects that
         * are not stillborn, with the exception of "NoSuchMethod" internal
         * helper objects) expect it to have a non-null map pointer, so we
         * share an empty Enumerator scope in the runtime.
         */
        JSObject *obj = js_NewGCObject(cx, FINALIZE_OBJECT0);
        if (!obj)
            return NULL;

        EmptyShape *emptyEnumeratorShape = EmptyShape::getEmptyEnumeratorShape(cx);
        if (!emptyEnumeratorShape)
            return NULL;
        obj->init(cx, &IteratorClass, &types::emptyTypeObject, NULL, NULL, false);
        obj->setMap(emptyEnumeratorShape);
        return obj;
    }

    return NewBuiltinClassInstance(cx, &IteratorClass);
}

NativeIterator *
NativeIterator::allocateIterator(JSContext *cx, uint32 slength, const AutoIdVector &props)
{
    size_t plength = props.length();
    NativeIterator *ni = (NativeIterator *)
        cx->malloc_(sizeof(NativeIterator) + plength * sizeof(jsid) + slength * sizeof(uint32));
    if (!ni)
        return NULL;
    ni->props_array = ni->props_cursor = (jsid *) (ni + 1);
    ni->props_end = (jsid *)ni->props_array + plength;
    if (plength)
        memcpy(ni->props_array, props.begin(), plength * sizeof(jsid));
    return ni;
}

inline void
NativeIterator::init(JSObject *obj, uintN flags, uint32 slength, uint32 key)
{
    this->obj = obj;
    this->flags = flags;
    this->shapes_array = (uint32 *) this->props_end;
    this->shapes_length = slength;
    this->shapes_key = key;
}

static inline void
RegisterEnumerator(JSContext *cx, JSObject *iterobj, NativeIterator *ni)
{
    /* Register non-escaping native enumerators (for-in) with the current context. */
    if (ni->flags & JSITER_ENUMERATE) {
        ni->next = cx->enumerators;
        cx->enumerators = iterobj;

        JS_ASSERT(!(ni->flags & JSITER_ACTIVE));
        ni->flags |= JSITER_ACTIVE;
    }
}

static inline bool
VectorToKeyIterator(JSContext *cx, JSObject *obj, uintN flags, AutoIdVector &keys,
                    uint32 slength, uint32 key, Value *vp)
{
    JS_ASSERT(!(flags & JSITER_FOREACH));

    if (obj) {
        obj->flags |= JSObject::ITERATED;
        types::MarkTypeObjectFlags(cx, obj, types::OBJECT_FLAG_ITERATED);
    }

    JSObject *iterobj = NewIteratorObject(cx, flags);
    if (!iterobj)
        return false;

    NativeIterator *ni = NativeIterator::allocateIterator(cx, slength, keys);
    if (!ni)
        return false;
    ni->init(obj, flags, slength, key);

    if (slength) {
        /*
         * Fill in the shape array from scratch.  We can't use the array that was
         * computed for the cache lookup earlier, as constructing iterobj could
         * have triggered a shape-regenerating GC.  Don't bother with regenerating
         * the shape key; if such a GC *does* occur, we can only get hits through
         * the one-slot lastNativeIterator cache.
         */
        JSObject *pobj = obj;
        size_t ind = 0;
        do {
            ni->shapes_array[ind++] = pobj->shape();
            pobj = pobj->getProto();
        } while (pobj);
        JS_ASSERT(ind == slength);
    }

    iterobj->setNativeIterator(ni);
    vp->setObject(*iterobj);

    RegisterEnumerator(cx, iterobj, ni);
    return true;
}

namespace js {

bool
VectorToKeyIterator(JSContext *cx, JSObject *obj, uintN flags, AutoIdVector &props, Value *vp)
{
    return VectorToKeyIterator(cx, obj, flags, props, 0, 0, vp);
}

bool
VectorToValueIterator(JSContext *cx, JSObject *obj, uintN flags, AutoIdVector &keys,
                      Value *vp)
{
    JS_ASSERT(flags & JSITER_FOREACH);

    if (obj) {
        obj->flags |= JSObject::ITERATED;
        types::MarkTypeObjectFlags(cx, obj, types::OBJECT_FLAG_ITERATED);
    }

    JSObject *iterobj = NewIteratorObject(cx, flags);
    if (!iterobj)
        return false;

    NativeIterator *ni = NativeIterator::allocateIterator(cx, 0, keys);
    if (!ni)
        return false;
    ni->init(obj, flags, 0, 0);

    iterobj->setNativeIterator(ni);
    vp->setObject(*iterobj);

    RegisterEnumerator(cx, iterobj, ni);
    return true;
}

bool
EnumeratedIdVectorToIterator(JSContext *cx, JSObject *obj, uintN flags, AutoIdVector &props, Value *vp)
{
    if (!(flags & JSITER_FOREACH))
        return VectorToKeyIterator(cx, obj, flags, props, vp);

    return VectorToValueIterator(cx, obj, flags, props, vp);
}

static inline void
UpdateNativeIterator(NativeIterator *ni, JSObject *obj)
{
    // Update the object for which the native iterator is associated, so
    // SuppressDeletedPropertyHelper will recognize the iterator as a match.
    ni->obj = obj;
}

bool
GetIterator(JSContext *cx, JSObject *obj, uintN flags, Value *vp)
{
    Vector<uint32, 8> shapes(cx);
    uint32 key = 0;

    bool keysOnly = (flags == JSITER_ENUMERATE);

    if (obj) {
        /* Enumerate Iterator.prototype directly. */
        JSIteratorOp op = obj->getClass()->ext.iteratorObject;
        if (op && (obj->getClass() != &IteratorClass || obj->getNativeIterator())) {
            JSObject *iterobj = op(cx, obj, !(flags & JSITER_FOREACH));
            if (!iterobj)
                return false;
            vp->setObject(*iterobj);
            types::MarkIteratorUnknown(cx);
            return true;
        }

        if (keysOnly) {
            /*
             * Check to see if this is the same as the most recent object which
             * was iterated over.  We don't explicitly check for shapeless
             * objects here, as they are not inserted into the cache and
             * will result in a miss.
             */
            JSObject *last = cx->compartment->nativeIterCache.last;
            JSObject *proto = obj->getProto();
            if (last) {
                NativeIterator *lastni = last->getNativeIterator();
                if (!(lastni->flags & (JSITER_ACTIVE|JSITER_UNREUSABLE)) &&
                    obj->isNative() &&
                    obj->shape() == lastni->shapes_array[0] &&
                    proto && proto->isNative() &&
                    proto->shape() == lastni->shapes_array[1] &&
                    !proto->getProto()) {
                    vp->setObject(*last);
                    UpdateNativeIterator(lastni, obj);
                    RegisterEnumerator(cx, last, lastni);
                    return true;
                }
            }

            /*
             * The iterator object for JSITER_ENUMERATE never escapes, so we
             * don't care for the proper parent/proto to be set. This also
             * allows us to re-use a previous iterator object that is not
             * currently active.
             */
            JSObject *pobj = obj;
            do {
                if (!pobj->isNative() ||
                    obj->getOps()->enumerate ||
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

            JSObject *iterobj = cx->compartment->nativeIterCache.get(key);
            if (iterobj) {
                NativeIterator *ni = iterobj->getNativeIterator();
                if (!(ni->flags & (JSITER_ACTIVE|JSITER_UNREUSABLE)) &&
                    ni->shapes_key == key &&
                    ni->shapes_length == shapes.length() &&
                    Compare(ni->shapes_array, shapes.begin(), ni->shapes_length)) {
                    vp->setObject(*iterobj);

                    UpdateNativeIterator(ni, obj);
                    RegisterEnumerator(cx, iterobj, ni);
                    if (shapes.length() == 2)
                        cx->compartment->nativeIterCache.last = iterobj;
                    return true;
                }
            }
        }

      miss:
        if (obj->isProxy()) {
            types::MarkIteratorUnknown(cx);
            return JSProxy::iterate(cx, obj, flags, vp);
        }
        if (!GetCustomIterator(cx, obj, flags, vp))
            return false;
        if (!vp->isUndefined()) {
            types::MarkIteratorUnknown(cx);
            return true;
        }
    }

    /* NB: for (var p in null) succeeds by iterating over no properties. */

    AutoIdVector keys(cx);
    if (flags & JSITER_FOREACH) {
        if (JS_LIKELY(obj != NULL) && !Snapshot(cx, obj, flags, &keys))
            return false;
        JS_ASSERT(shapes.empty());
        if (!VectorToValueIterator(cx, obj, flags, keys, vp))
            return false;
    } else {
        if (JS_LIKELY(obj != NULL) && !Snapshot(cx, obj, flags, &keys))
            return false;
        if (!VectorToKeyIterator(cx, obj, flags, keys, shapes.length(), key, vp))
            return false;
    }

    JSObject *iterobj = &vp->toObject();

    /* Cache the iterator object if possible. */
    if (shapes.length())
        cx->compartment->nativeIterCache.set(key, iterobj);

    if (shapes.length() == 2)
        cx->compartment->nativeIterCache.last = iterobj;
    return true;
}

}

static JSObject *
iterator_iterator(JSContext *cx, JSObject *obj, JSBool keysonly)
{
    return obj;
}

static JSBool
Iterator(JSContext *cx, uintN argc, Value *vp)
{
    Value *argv = JS_ARGV(cx, vp);
    bool keyonly = argc >= 2 ? js_ValueToBoolean(argv[1]) : false;
    uintN flags = JSITER_OWNONLY | (keyonly ? 0 : (JSITER_FOREACH | JSITER_KEYVALUE));
    *vp = argc >= 1 ? argv[0] : UndefinedValue();
    return js_ValueToIterator(cx, flags, vp);
}

JSBool
js_ThrowStopIteration(JSContext *cx)
{
    Value v;

    JS_ASSERT(!JS_IsExceptionPending(cx));
    if (js_FindClassObject(cx, NULL, JSProto_StopIteration, &v))
        cx->setPendingException(v);
    return JS_FALSE;
}

static JSBool
iterator_next(JSContext *cx, uintN argc, Value *vp)
{
    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->isIterator()) {
        ReportIncompatibleMethod(cx, vp, &IteratorClass);
        return false;
    }

    if (!js_IteratorMore(cx, obj, vp))
        return false;
    if (!vp->toBoolean()) {
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

    JSObject *obj;
    if (vp->isObject()) {
        /* Common case. */
        obj = &vp->toObject();
    } else {
        /*
         * Enumerating over null and undefined gives an empty enumerator.
         * This is contrary to ECMA-262 9.9 ToObject, invoked from step 3 of
         * the first production in 12.6.4 and step 4 of the second production,
         * but it's "web JS" compatible. ES5 fixed for-in to match this de-facto
         * standard.
         */
        if ((flags & JSITER_ENUMERATE)) {
            if (!js_ValueToObjectOrNull(cx, *vp, &obj))
                return false;
            /* fall through */
        } else {
            obj = js_ValueToNonNullObject(cx, *vp);
            if (!obj)
                return false;
        }
    }

    return GetIterator(cx, obj, flags, vp);
}

#if JS_HAS_GENERATORS
static JS_REQUIRES_STACK JSBool
CloseGenerator(JSContext *cx, JSObject *genobj);
#endif

JS_FRIEND_API(JSBool)
js_CloseIterator(JSContext *cx, JSObject *obj)
{
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    if (obj->isIterator()) {
        /* Remove enumerators from the active list, which is a stack. */
        NativeIterator *ni = obj->getNativeIterator();

        if (ni->flags & JSITER_ENUMERATE) {
            JS_ASSERT(cx->enumerators == obj);
            cx->enumerators = ni->next;

            JS_ASSERT(ni->flags & JSITER_ACTIVE);
            ni->flags &= ~JSITER_ACTIVE;

            /*
             * Reset the enumerator; it may still be in the cached iterators
             * for this thread, and can be reused.
             */
            ni->props_cursor = ni->props_array;
        }
    }
#if JS_HAS_GENERATORS
    else if (obj->isGenerator()) {
        return CloseGenerator(cx, obj);
    }
#endif
    return JS_TRUE;
}

/*
 * Suppress enumeration of deleted properties. This function must be called
 * when a property is deleted and there might be active enumerators.
 *
 * We maintain a list of active non-escaping for-in enumerators. To suppress
 * a property, we check whether each active enumerator contains the (obj, id)
 * pair and has not yet enumerated |id|. If so, and |id| is the next property,
 * we simply advance the cursor. Otherwise, we delete |id| from the list.
 *
 * We do not suppress enumeration of a property deleted along an object's
 * prototype chain. Only direct deletions on the object are handled.
 *
 * This function can suppress multiple properties at once. The |predicate|
 * argument is an object which can be called on an id and returns true or
 * false. It also must have a method |matchesAtMostOne| which allows us to
 * stop searching after the first deletion if true.
 */
template<typename IdPredicate>
static bool
SuppressDeletedPropertyHelper(JSContext *cx, JSObject *obj, IdPredicate predicate)
{
    JSObject *iterobj = cx->enumerators;
    while (iterobj) {
      again:
        NativeIterator *ni = iterobj->getNativeIterator();
        /* This only works for identified surpressed keys, not values. */
        if (ni->isKeyIter() && ni->obj == obj && ni->props_cursor < ni->props_end) {
            /* Check whether id is still to come. */
            jsid *props_cursor = ni->current();
            jsid *props_end = ni->end();
            for (jsid *idp = props_cursor; idp < props_end; ++idp) {
                if (predicate(*idp)) {
                    /*
                     * Check whether another property along the prototype chain
                     * became visible as a result of this deletion.
                     */
                    if (obj->getProto()) {
                        AutoObjectRooter proto(cx, obj->getProto());
                        AutoObjectRooter obj2(cx);
                        JSProperty *prop;
                        if (!proto.object()->lookupProperty(cx, *idp, obj2.addr(), &prop))
                            return false;
                        if (prop) {
                            uintN attrs;
                            if (obj2.object()->isNative())
                                attrs = ((Shape *) prop)->attributes();
                            else if (!obj2.object()->getAttributes(cx, *idp, &attrs))
                                return false;

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
                     * No property along the prototype chain stepped in to take the
                     * property's place, so go ahead and delete id from the list.
                     * If it is the next property to be enumerated, just skip it.
                     */
                    if (idp == props_cursor) {
                        ni->incCursor();
                    } else {
                        memmove(idp, idp + 1, (props_end - (idp + 1)) * sizeof(jsid));
                        ni->props_end = ni->end() - 1;
                    }

                    /* Don't reuse modified native iterators. */
                    ni->flags |= JSITER_UNREUSABLE;

                    if (predicate.matchesAtMostOne())
                        break;
                }
            }
        }
        iterobj = ni->next;
    }
    return true;
}

class SingleIdPredicate {
    jsid id;
public:
    SingleIdPredicate(jsid id) : id(id) {}

    bool operator()(jsid id) { return id == this->id; }
    bool matchesAtMostOne() { return true; }
};

bool
js_SuppressDeletedProperty(JSContext *cx, JSObject *obj, jsid id)
{
    id = js_CheckForStringIndex(id);
    return SuppressDeletedPropertyHelper(cx, obj, SingleIdPredicate(id));
}

bool
js_SuppressDeletedElement(JSContext *cx, JSObject *obj, uint32 index)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    JS_ASSERT(id == js_CheckForStringIndex(id));
    return SuppressDeletedPropertyHelper(cx, obj, SingleIdPredicate(id));
}

class IndexRangePredicate {
    jsint begin, end;
public:
    IndexRangePredicate(jsint begin, jsint end) : begin(begin), end(end) {}

    bool operator()(jsid id) {
        return JSID_IS_INT(id) && begin <= JSID_TO_INT(id) && JSID_TO_INT(id) < end;
    }
    bool matchesAtMostOne() { return false; }
};

bool
js_SuppressDeletedIndexProperties(JSContext *cx, JSObject *obj, jsint begin, jsint end)
{
    return SuppressDeletedPropertyHelper(cx, obj, IndexRangePredicate(begin, end));
}

JSBool
js_IteratorMore(JSContext *cx, JSObject *iterobj, Value *rval)
{
    /* Fast path for native iterators */
    NativeIterator *ni = NULL;
    if (iterobj->isIterator()) {
        /* Key iterators are handled by fast-paths. */
        ni = iterobj->getNativeIterator();
        if (ni) {
            bool more = ni->props_cursor < ni->props_end;
            if (ni->isKeyIter() || !more) {
                rval->setBoolean(more);
                return true;
            }
        }
    }

    /* We might still have a pending value. */
    if (!cx->iterValue.isMagic(JS_NO_ITER_VALUE)) {
        rval->setBoolean(true);
        return true;
    }

    /* We're reentering below and can call anything. */
    JS_CHECK_RECURSION(cx, return false);

    /* Fetch and cache the next value from the iterator. */
    if (!ni) {
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.nextAtom);
        if (!js_GetMethod(cx, iterobj, id, JSGET_METHOD_BARRIER, rval))
            return false;
        if (!Invoke(cx, ObjectValue(*iterobj), *rval, 0, NULL, rval)) {
            /* Check for StopIteration. */
            if (!cx->isExceptionPending() || !IsStopIteration(cx->getPendingException()))
                return false;

            cx->clearPendingException();
            cx->iterValue.setMagic(JS_NO_ITER_VALUE);
            rval->setBoolean(false);
            return true;
        }
    } else {
        JS_ASSERT(!ni->isKeyIter());
        jsid id = *ni->current();
        ni->incCursor();
        if (!ni->obj->getProperty(cx, id, rval))
            return false;
        if ((ni->flags & JSITER_KEYVALUE) && !NewKeyValuePair(cx, id, *rval, rval))
            return false;
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
    if (iterobj->isIterator()) {
        /*
         * Implement next directly as all the methods of the native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->getNativeIterator();
        if (ni && ni->isKeyIter()) {
            JS_ASSERT(ni->props_cursor < ni->props_end);
            *rval = IdToValue(*ni->current());
            ni->incCursor();

            if (rval->isString())
                return true;

            JSString *str;
            jsint i;
            if (rval->isInt32() && JSAtom::hasIntStatic(i = rval->toInt32())) {
                str = &JSAtom::intStatic(i);
            } else {
                str = js_ValueToString(cx, *rval);
                if (!str)
                    return false;
            }

            rval->setString(str);
            return true;
        }
    }

    JS_ASSERT(!cx->iterValue.isMagic(JS_NO_ITER_VALUE));
    *rval = cx->iterValue;
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    return true;
}

static JSBool
stopiter_hasInstance(JSContext *cx, JSObject *obj, const Value *v, JSBool *bp)
{
    *bp = IsStopIteration(*v);
    return JS_TRUE;
}

Class js::StopIterationClass = {
    js_StopIteration_str,
    JSCLASS_HAS_CACHED_PROTO(JSProto_StopIteration) |
    JSCLASS_FREEZE_PROTO,
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    NULL,                 /* finalize    */
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    stopiter_hasInstance
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
    cx->free_(gen);
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

    StackFrame *fp = gen->floatingFrame();
    JS_ASSERT(gen->liveFrame() == fp);

    /*
     * Currently, generators are not mjitted. Still, (overflow) args can be
     * pushed by the mjit and need to be conservatively marked. Technically, the
     * formal args and generator slots are safe for exact marking, but since the
     * plan is to eventually mjit generators, it makes sense to future-proof
     * this code and save someone an hour later.
     */
    MarkStackRangeConservatively(trc, gen->floatingStack, fp->formalArgsEnd());
    js_TraceStackFrame(trc, fp);
    MarkStackRangeConservatively(trc, fp->slots(), gen->regs.sp);
}

Class js::GeneratorClass = {
    "Generator",
    JSCLASS_HAS_PRIVATE,
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    generator_finalize,
    NULL,                 /* reserved    */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    generator_trace,
    {
        NULL,             /* equality       */
        NULL,             /* outerObject    */
        NULL,             /* innerObject    */
        iterator_iterator,
        NULL              /* unused */
    }
};

/*
 * Called from the JSOP_GENERATOR case in the interpreter, with fp referring
 * to the frame by which the generator function was activated.  Create a new
 * JSGenerator object, which contains its own StackFrame that we populate
 * from *fp.  We know that upon return, the JSOP_GENERATOR opcode will return
 * from the activation in fp, so we can steal away fp->callobj and fp->argsobj
 * if they are non-null.
 */
JS_REQUIRES_STACK JSObject *
js_NewGenerator(JSContext *cx)
{
    FrameRegs &stackRegs = cx->regs();
    StackFrame *stackfp = stackRegs.fp();
    JS_ASSERT(stackfp->base() == cx->regs().sp);
    JS_ASSERT(stackfp->actualArgs() <= stackfp->formalArgs());

    GlobalObject *global = stackfp->scopeChain().getGlobal();
    JSObject *proto = global->getOrCreateGeneratorPrototype(cx);
    if (!proto)
        return NULL;
    JSObject *obj = NewNonFunction<WithProto::Given>(cx, &GeneratorClass, proto, global);
    if (!obj)
        return NULL;

    /* Load and compute stack slot counts. */
    Value *stackvp = stackfp->actualArgs() - 2;
    uintN vplen = stackfp->formalArgsEnd() - stackvp;

    /* Compute JSGenerator size. */
    uintN nbytes = sizeof(JSGenerator) +
                   (-1 + /* one Value included in JSGenerator */
                    vplen +
                    VALUES_PER_STACK_FRAME +
                    stackfp->numSlots()) * sizeof(Value);

    JSGenerator *gen = (JSGenerator *) cx->malloc_(nbytes);
    if (!gen)
        return NULL;

    /* Cut up floatingStack space. */
    Value *genvp = gen->floatingStack;
    StackFrame *genfp = reinterpret_cast<StackFrame *>(genvp + vplen);

    /* Initialize JSGenerator. */
    gen->obj = obj;
    gen->state = JSGEN_NEWBORN;
    gen->enumerators = NULL;
    gen->floating = genfp;

    /* Copy from the stack to the generator's floating frame. */
    gen->regs.rebaseFromTo(stackRegs, *genfp);
    genfp->stealFrameAndSlots(genvp, stackfp, stackvp, stackRegs.sp);
    genfp->initFloatingGenerator();

    obj->setPrivate(gen);
    return obj;
}

JSGenerator *
js_FloatingFrameToGenerator(StackFrame *fp)
{
    JS_ASSERT(fp->isGeneratorFrame() && fp->isFloatingGenerator());
    char *floatingStackp = (char *)(fp->actualArgs() - 2);
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
                            JSDVG_SEARCH_STACK, ObjectOrNullValue(obj),
                            JS_GetFunctionId(gen->floatingFrame()->fun()));
        return JS_FALSE;
    }

    /* Check for OOM errors here, where we can fail easily. */
    if (!cx->ensureGeneratorStackSpace())
        return JS_FALSE;

    JS_ASSERT(gen->state == JSGEN_NEWBORN || gen->state == JSGEN_OPEN);
    switch (op) {
      case JSGENOP_NEXT:
      case JSGENOP_SEND:
        if (gen->state == JSGEN_OPEN) {
            /*
             * Store the argument to send as the result of the yield
             * expression.
             */
            gen->regs.sp[-1] = arg;
        }
        gen->state = JSGEN_RUNNING;
        break;

      case JSGENOP_THROW:
        cx->setPendingException(arg);
        gen->state = JSGEN_RUNNING;
        break;

      default:
        JS_ASSERT(op == JSGENOP_CLOSE);
        cx->setPendingException(MagicValue(JS_GENERATOR_CLOSING));
        gen->state = JSGEN_CLOSING;
        break;
    }

    StackFrame *genfp = gen->floatingFrame();

    JSBool ok;
    {
        GeneratorFrameGuard gfg;
        if (!cx->stack.pushGeneratorFrame(cx, gen, &gfg)) {
            gen->state = JSGEN_CLOSED;
            return JS_FALSE;
        }

        StackFrame *fp = gfg.fp();
        gen->regs = cx->regs();
        JS_ASSERT(gen->liveFrame() == fp);

        cx->enterGenerator(gen);   /* OOM check above. */
        JSObject *enumerators = cx->enumerators;
        cx->enumerators = gen->enumerators;

        ok = RunScript(cx, fp->script(), fp);

        gen->enumerators = cx->enumerators;
        cx->enumerators = enumerators;
        cx->leaveGenerator(gen);
    }

    if (gen->floatingFrame()->isYielding()) {
        /* Yield cannot fail, throw or be called on closing. */
        JS_ASSERT(ok);
        JS_ASSERT(!cx->isExceptionPending());
        JS_ASSERT(gen->state == JSGEN_RUNNING);
        JS_ASSERT(op != JSGENOP_CLOSE);
        genfp->clearYielding();
        gen->state = JSGEN_OPEN;
        return JS_TRUE;
    }

    genfp->clearReturnValue();
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
    JS_ASSERT(obj->isGenerator());

    JSGenerator *gen = (JSGenerator *) obj->getPrivate();
    if (!gen) {
        /* Generator prototype object. */
        return JS_TRUE;
    }

    if (gen->state == JSGEN_CLOSED)
        return JS_TRUE;

    return SendToGenerator(cx, JSGENOP_CLOSE, obj, gen, UndefinedValue());
}

/*
 * Common subroutine of generator_(next|send|throw|close) methods.
 */
static JSBool
generator_op(JSContext *cx, JSGeneratorOp op, Value *vp, uintN argc)
{
    LeaveTrace(cx);

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return JS_FALSE;
    if (!obj->isGenerator()) {
        ReportIncompatibleMethod(cx, vp, &GeneratorClass);
        return JS_FALSE;
    }

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
            JS_SET_RVAL(cx, vp, UndefinedValue());
            return JS_TRUE;
        }
    } else if (gen->state == JSGEN_CLOSED) {
      closed_generator:
        switch (op) {
          case JSGENOP_NEXT:
          case JSGENOP_SEND:
            return js_ThrowStopIteration(cx);
          case JSGENOP_THROW:
            cx->setPendingException(argc >= 1 ? vp[2] : UndefinedValue());
            return JS_FALSE;
          default:
            JS_ASSERT(op == JSGENOP_CLOSE);
            JS_SET_RVAL(cx, vp, UndefinedValue());
            return JS_TRUE;
        }
    }

    bool undef = ((op == JSGENOP_SEND || op == JSGENOP_THROW) && argc != 0);
    if (!SendToGenerator(cx, op, obj, gen, undef ? vp[2] : UndefinedValue()))
        return JS_FALSE;

    JS_SET_RVAL(cx, vp, gen->floatingFrame()->returnValue());
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

static bool
InitIteratorClass(JSContext *cx, GlobalObject *global)
{
    JSObject *iteratorProto = global->createBlankPrototype(cx, &IteratorClass);
    if (!iteratorProto)
        return false;

    JSFunction *ctor = global->createConstructor(cx, Iterator, &IteratorClass,
                                                 CLASS_ATOM(cx, Iterator), 2);
    if (!ctor)
        return false;

    if (!LinkConstructorAndPrototype(cx, ctor, iteratorProto))
        return false;

    if (!DefinePropertiesAndBrand(cx, iteratorProto, NULL, iterator_methods))
        return false;

    return DefineConstructorAndPrototype(cx, global, JSProto_Iterator, ctor, iteratorProto);
}

bool
GlobalObject::initGeneratorClass(JSContext *cx)
{
#if JS_HAS_GENERATORS
    JSObject *proto = createBlankPrototype(cx, &GeneratorClass);
    if (!proto || !DefinePropertiesAndBrand(cx, proto, NULL, generator_methods))
        return false;
    setReservedSlot(GENERATOR_PROTO, ObjectValue(*proto));
#endif
    return true;
}

static JSObject *
InitStopIterationClass(JSContext *cx, GlobalObject *global)
{
    JSObject *proto = global->createBlankPrototype(cx, &StopIterationClass);
    if (!proto || !proto->freeze(cx))
        return NULL;

    /* This should use a non-JSProtoKey'd slot, but this is easier for now. */
    if (!DefineConstructorAndPrototype(cx, global, JSProto_StopIteration, proto, proto))
        return NULL;

    MarkStandardClassInitializedNoProto(global, &StopIterationClass);

    return proto;
}

JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isNative());

    GlobalObject *global = obj->asGlobal();

    /*
     * Bail if Iterator has already been initialized.  We test for Iterator
     * rather than for StopIteration because if js_InitIteratorClasses recurs,
     * as happens when the StopIteration object is frozen, initializing the
     * Iterator class a second time will assert.
     */
    JSObject *iter;
    if (!js_GetClassObject(cx, global, JSProto_Iterator, &iter))
        return NULL;
    if (iter)
        return iter;

    if (!InitIteratorClass(cx, global) || !global->initGeneratorClass(cx))
        return NULL;
    return InitStopIterationClass(cx, global);
}
