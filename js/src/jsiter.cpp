/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JavaScript iterators.
 */
#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsutil.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
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
#include "jsproxy.h"
#include "jsscope.h"
#include "jsscript.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#include "ds/Sort.h"
#include "frontend/TokenStream.h"
#include "gc/Marking.h"
#include "vm/GlobalObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "builtin/Iterator-inl.h"
#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace mozilla;
using namespace js;
using namespace js::gc;

static const gc::AllocKind ITERATOR_FINALIZE_KIND = gc::FINALIZE_OBJECT2;

void
NativeIterator::mark(JSTracer *trc)
{
    for (HeapPtr<JSFlatString> *str = begin(); str < end(); str++)
        MarkString(trc, str, "prop");
    if (obj)
        MarkObject(trc, &obj, "obj");
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
    AutoArrayRooter tvr(cx, ArrayLength(vec), vec);

    JSObject *aobj = NewDenseCopiedArray(cx, 2, vec);
    if (!aobj)
        return false;
    rval->setObject(*aobj);
    return true;
}

static inline bool
Enumerate(JSContext *cx, JSObject *obj, JSObject *pobj, jsid id,
          bool enumerable, unsigned flags, IdSet& ht, AutoIdVector *props)
{
    JS_ASSERT_IF(flags & JSITER_OWNONLY, obj == pobj);

    /*
     * We implement __proto__ using a property on |Object.prototype|, but
     * because __proto__ is highly deserving of removal, we don't want it to
     * show up in property enumeration, even if only for |Object.prototype|
     * (think introspection by Prototype-like frameworks that add methods to
     * the built-in prototypes).  So exclude __proto__ if the object where the
     * property was found has no [[Prototype]] and might be |Object.prototype|.
     */
    if (JS_UNLIKELY(!pobj->getProto() && JSID_IS_ATOM(id, cx->runtime->atomState.protoAtom)))
        return true;

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
EnumerateNativeProperties(JSContext *cx, JSObject *obj_, JSObject *pobj_, unsigned flags, IdSet &ht,
                          AutoIdVector *props)
{
    RootedObject obj(cx, obj_), pobj(cx, pobj_);

    size_t initialLength = props->length();

    /* Collect all unique properties from this object's scope. */
    Shape::Range r = pobj->lastProperty()->all();
    Shape::Range::AutoRooter root(cx, &r);
    for (; !r.empty(); r.popFront()) {
        Shape &shape = r.front();

        if (!JSID_IS_DEFAULT_XML_NAMESPACE(shape.propid()) &&
            !Enumerate(cx, obj, pobj, shape.propid(), shape.enumerable(), flags, ht, props))
        {
            return false;
        }
    }

    ::Reverse(props->begin() + initialLength, props->end());
    return true;
}

static bool
EnumerateDenseArrayProperties(JSContext *cx, JSObject *obj, JSObject *pobj, unsigned flags,
                              IdSet &ht, AutoIdVector *props)
{
    if (!Enumerate(cx, obj, pobj, NameToId(cx->runtime->atomState.lengthAtom), false,
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

#ifdef JS_MORE_DETERMINISTIC

struct SortComparatorIds
{
    JSContext   *const cx;

    SortComparatorIds(JSContext *cx)
      : cx(cx) {}

    bool operator()(jsid a, jsid b, bool *lessOrEqualp)
    {
        /* Pick an arbitrary total order on jsids that is stable across executions. */
        JSString *astr = IdToString(cx, a);
	if (!astr)
	    return false;
        JSString *bstr = IdToString(cx, b);
        if (!bstr)
            return false;

        int32_t result;
        if (!CompareStrings(cx, astr, bstr, &result))
            return false;

        *lessOrEqualp = (result <= 0);
        return true;
    }
};

#endif /* JS_MORE_DETERMINISTIC */

static bool
Snapshot(JSContext *cx, JSObject *obj_, unsigned flags, AutoIdVector *props)
{
    IdSet ht(cx);
    if (!ht.init(32))
        return false;

    RootedObject obj(cx, obj_), pobj(cx);
    pobj = obj;

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
                        if (!Proxy::getOwnPropertyNames(cx, pobj, proxyProps))
                            return false;
                    } else {
                        if (!Proxy::keys(cx, pobj, proxyProps))
                            return false;
                    }
                } else {
                    if (!Proxy::enumerate(cx, pobj, proxyProps))
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

        if (flags & JSITER_OWNONLY)
            break;

#if JS_HAS_XML_SUPPORT
        if (pobj->isXML())
            break;
#endif
    } while ((pobj = pobj->getProto()) != NULL);

#ifdef JS_MORE_DETERMINISTIC

    /*
     * In some cases the enumeration order for an object depends on the
     * execution mode (interpreter vs. JIT), especially for native objects
     * with a class enumerate hook (where resolving a property changes the
     * resulting enumeration order). These aren't really bugs, but the
     * differences can change the generated output and confuse correctness
     * fuzzers, so we sort the ids if such a fuzzer is running.
     *
     * We don't do this in the general case because (a) doing so is slow,
     * and (b) it also breaks the web, which expects enumeration order to
     * follow the order in which properties are added, in certain cases.
     * Since ECMA does not specify an enumeration order for objects, both
     * behaviors are technically correct to do.
     */

    jsid *ids = props->begin();
    size_t n = props->length();

    Vector<jsid> tmp(cx);
    if (!tmp.resizeUninitialized(n))
        return false;

    if (!MergeSort(ids, n, tmp.begin(), SortComparatorIds(cx)))
        return false;

#endif /* JS_MORE_DETERMINISTIC */

    return true;
}

bool
js::VectorToIdArray(JSContext *cx, AutoIdVector &props, JSIdArray **idap)
{
    JS_STATIC_ASSERT(sizeof(JSIdArray) > sizeof(jsid));
    size_t len = props.length();
    size_t idsz = len * sizeof(jsid);
    size_t sz = (sizeof(JSIdArray) - sizeof(jsid)) + idsz;
    JSIdArray *ida = static_cast<JSIdArray *>(cx->malloc_(sz));
    if (!ida)
        return false;

    ida->length = static_cast<int>(len);
    jsid *v = props.begin();
    for (int i = 0; i < ida->length; i++)
        ida->vector[i].init(v[i]);
    *idap = ida;
    return true;
}

JS_FRIEND_API(bool)
js::GetPropertyNames(JSContext *cx, JSObject *obj, unsigned flags, AutoIdVector *props)
{
    return Snapshot(cx, obj, flags & (JSITER_OWNONLY | JSITER_HIDDEN), props);
}

size_t sCustomIteratorCount = 0;

static inline bool
GetCustomIterator(JSContext *cx, HandleObject obj, unsigned flags, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);

    /* Check whether we have a valid __iterator__ method. */
    PropertyName *name = cx->runtime->atomState.iteratorIntrinsicAtom;
    if (!GetMethod(cx, obj, name, 0, vp))
        return false;

    /* If there is no custom __iterator__ method, we are done here. */
    if (!vp->isObject()) {
        vp->setUndefined();
        return true;
    }

    if (!cx->runningWithTrustedPrincipals())
        ++sCustomIteratorCount;

    /* Otherwise call it and return that object. */
    Value arg = BooleanValue((flags & JSITER_FOREACH) == 0);
    if (!Invoke(cx, ObjectValue(*obj), *vp, 1, &arg, vp))
        return false;
    if (vp->isPrimitive()) {
        /*
         * We are always coming from js::ValueToIterator, and we are no longer on
         * trace, so the object we are iterating over is on top of the stack (-1).
         */
        JSAutoByteString bytes;
        if (!js_AtomToPrintableString(cx, name, &bytes))
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

static inline PropertyIteratorObject *
NewPropertyIteratorObject(JSContext *cx, unsigned flags)
{
    if (flags & JSITER_ENUMERATE) {
        RootedTypeObject type(cx, cx->compartment->getEmptyType(cx));
        if (!type)
            return NULL;

        RootedShape shape(cx, EmptyShape::getInitialShape(cx, &PropertyIteratorObject::class_,
                                                          NULL, NULL, ITERATOR_FINALIZE_KIND));
        if (!shape)
            return NULL;

        JSObject *obj = JSObject::create(cx, ITERATOR_FINALIZE_KIND, shape, type, NULL);
        if (!obj)
            return NULL;

        JS_ASSERT(obj->numFixedSlots() == JSObject::ITER_CLASS_NFIXED_SLOTS);
        return &obj->asPropertyIterator();
    }

    return &NewBuiltinClassInstance(cx, &PropertyIteratorObject::class_)->asPropertyIterator();
}

NativeIterator *
NativeIterator::allocateIterator(JSContext *cx, uint32_t slength, const AutoIdVector &props)
{
    size_t plength = props.length();
    NativeIterator *ni = (NativeIterator *)
        cx->malloc_(sizeof(NativeIterator)
                    + plength * sizeof(JSString *)
                    + slength * sizeof(Shape *));
    if (!ni)
        return NULL;
    AutoValueVector strings(cx);
    ni->props_array = ni->props_cursor = (HeapPtr<JSFlatString> *) (ni + 1);
    ni->props_end = ni->props_array + plength;
    if (plength) {
        for (size_t i = 0; i < plength; i++) {
            JSFlatString *str = IdToString(cx, props[i]);
            if (!str || !strings.append(StringValue(str)))
                return NULL;
            ni->props_array[i].init(str);
        }
    }
    return ni;
}

inline void
NativeIterator::init(JSObject *obj, unsigned flags, uint32_t slength, uint32_t key)
{
    this->obj.init(obj);
    this->flags = flags;
    this->shapes_array = (Shape **) this->props_end;
    this->shapes_length = slength;
    this->shapes_key = key;
}

static inline void
RegisterEnumerator(JSContext *cx, PropertyIteratorObject *iterobj, NativeIterator *ni)
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
VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &keys,
                    uint32_t slength, uint32_t key, Value *vp)
{
    JS_ASSERT(!(flags & JSITER_FOREACH));

    if (obj) {
        if (obj->hasSingletonType() && !obj->setIteratedSingleton(cx))
            return false;
        types::MarkTypeObjectFlags(cx, obj, types::OBJECT_FLAG_ITERATED);
    }

    Rooted<PropertyIteratorObject *> iterobj(cx, NewPropertyIteratorObject(cx, flags));
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
            ni->shapes_array[ind++] = pobj->lastProperty();
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
VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props, Value *vp)
{
    return VectorToKeyIterator(cx, obj, flags, props, 0, 0, vp);
}

bool
VectorToValueIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &keys,
                      Value *vp)
{
    JS_ASSERT(flags & JSITER_FOREACH);

    if (obj) {
        if (obj->hasSingletonType() && !obj->setIteratedSingleton(cx))
            return false;
        types::MarkTypeObjectFlags(cx, obj, types::OBJECT_FLAG_ITERATED);
    }

    Rooted<PropertyIteratorObject*> iterobj(cx, NewPropertyIteratorObject(cx, flags));
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
EnumeratedIdVectorToIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props, Value *vp)
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
GetIterator(JSContext *cx, HandleObject obj, unsigned flags, Value *vp)
{
    if (flags == JSITER_FOR_OF) {
        // for-of loop. The iterator is simply |obj.iterator()|.
        Value method;
        if (!obj->getProperty(cx, obj, cx->runtime->atomState.iteratorAtom, &method))
            return false;

        // Throw if obj.iterator isn't callable. js::Invoke is about to check
        // for this kind of error anyway, but it would throw an inscrutable
        // error message about |method| rather than this nice one about |obj|.
        if (!method.isObject() || !method.toObject().isCallable()) {
            char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, ObjectOrNullValue(obj), NULL);
            if (!bytes)
                return false;
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_ITERABLE, bytes);
            cx->free_(bytes);
            return false;
        }

        if (!Invoke(cx, ObjectOrNullValue(obj), method, 0, NULL, vp))
            return false;

        if (!ToObject(cx, vp))
            return false;
        return true;
    }

    Vector<Shape *, 8> shapes(cx);
    uint32_t key = 0;

    bool keysOnly = (flags == JSITER_ENUMERATE);

    if (obj) {
        if (JSIteratorOp op = obj->getClass()->ext.iteratorObject) {
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
            PropertyIteratorObject *last = cx->runtime->nativeIterCache.last;
            JSObject *proto = obj->getProto();
            if (last) {
                NativeIterator *lastni = last->getNativeIterator();
                if (!(lastni->flags & (JSITER_ACTIVE|JSITER_UNREUSABLE)) &&
                    obj->isNative() &&
                    obj->lastProperty() == lastni->shapes_array[0] &&
                    proto && proto->isNative() &&
                    proto->lastProperty() == lastni->shapes_array[1] &&
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
                    pobj->hasUncacheableProto() ||
                    obj->getOps()->enumerate ||
                    pobj->getClass()->enumerate != JS_EnumerateStub) {
                    shapes.clear();
                    goto miss;
                }
                Shape *shape = pobj->lastProperty();
                key = (key + (key << 16)) ^ (uintptr_t(shape) >> 3);
                if (!shapes.append((Shape *) shape))
                    return false;
                pobj = pobj->getProto();
            } while (pobj);

            PropertyIteratorObject *iterobj = cx->runtime->nativeIterCache.get(key);
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
                        cx->runtime->nativeIterCache.last = iterobj;
                    return true;
                }
            }
        }

      miss:
        if (obj->isProxy()) {
            types::MarkIteratorUnknown(cx);
            return Proxy::iterate(cx, obj, flags, vp);
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

    PropertyIteratorObject *iterobj = &vp->toObject().asPropertyIterator();

    /* Cache the iterator object if possible. */
    if (shapes.length())
        cx->runtime->nativeIterCache.set(key, iterobj);

    if (shapes.length() == 2)
        cx->runtime->nativeIterCache.last = iterobj;
    return true;
}

}

JSBool
js_ThrowStopIteration(JSContext *cx)
{
    JS_ASSERT(!JS_IsExceptionPending(cx));
    RootedValue v(cx);
    if (js_FindClassObject(cx, NullPtr(), JSProto_StopIteration, &v))
        cx->setPendingException(v);
    return JS_FALSE;
}

/*** Iterator objects ****************************************************************************/

static JSBool
Iterator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() == 0) {
        js_ReportMissingArg(cx, args.calleev(), 0);
        return false;
    }

    bool keyonly = false;
    if (args.length() >= 2)
        keyonly = ToBoolean(args[1]);
    unsigned flags = JSITER_OWNONLY | (keyonly ? 0 : (JSITER_FOREACH | JSITER_KEYVALUE));

    if (!ValueToIterator(cx, flags, &args[0]))
        return false;
    args.rval() = args[0];
    return true;
}

static bool
IsIterator(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&PropertyIteratorObject::class_);
}

static bool
iterator_next_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsIterator(args.thisv()));

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());

    if (!js_IteratorMore(cx, thisObj, &args.rval()))
        return false;

    if (!args.rval().toBoolean()) {
        js_ThrowStopIteration(cx);
        return false;
    }

    return js_IteratorNext(cx, thisObj, &args.rval());
}

static JSBool
iterator_iterator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval() = args.thisv();
    return true;
}

static JSBool
iterator_next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsIterator, iterator_next_impl, args);
}

static JSFunctionSpec iterator_methods[] = {
    JS_FN("iterator",  iterator_iterator,   0, 0),
    JS_FN("next",      iterator_next,       0, 0),
    JS_FS_END
};

static JSObject *
iterator_iteratorObject(JSContext *cx, HandleObject obj, JSBool keysonly)
{
    return obj;
}

void
PropertyIteratorObject::trace(JSTracer *trc, JSObject *obj)
{
    if (NativeIterator *ni = obj->asPropertyIterator().getNativeIterator())
        ni->mark(trc);
}

void
PropertyIteratorObject::finalize(FreeOp *fop, JSObject *obj)
{
    if (NativeIterator *ni = obj->asPropertyIterator().getNativeIterator()) {
        obj->asPropertyIterator().setNativeIterator(NULL);
        fop->free_(ni);
    }
}

Class PropertyIteratorObject::class_ = {
    "Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Iterator) |
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    trace,
    {
        NULL,                /* equality       */
        NULL,                /* outerObject    */
        NULL,                /* innerObject    */
        iterator_iteratorObject,
        NULL                 /* unused  */
    }
};

const uint32_t CLOSED_INDEX = UINT32_MAX;

JSObject *
ElementIteratorObject::create(JSContext *cx, Handle<Value> target)
{
    GlobalObject *global = GetCurrentGlobal(cx);
    Rooted<JSObject*> proto(cx, global->getOrCreateElementIteratorPrototype(cx));
    if (!proto)
        return NULL;
    JSObject *iterobj = NewObjectWithGivenProto(cx, &ElementIteratorClass, proto, global);
    if (iterobj) {
        iterobj->setReservedSlot(TargetSlot, target);
        iterobj->setReservedSlot(IndexSlot, Int32Value(0));
    }
    return iterobj;
}

static bool
IsElementIterator(const Value &v)
{
    return v.isObject() && v.toObject().isElementIterator();
}

JSBool
ElementIteratorObject::next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsElementIterator, next_impl, args);
}

bool
ElementIteratorObject::next_impl(JSContext *cx, CallArgs args)
{
    JSObject *iterobj = &args.thisv().toObject();
    uint32_t i, length;
    Value target = iterobj->getReservedSlot(TargetSlot);
    Rooted<JSObject*> obj(cx);

    // Get target.length.
    if (target.isString()) {
        length = uint32_t(target.toString()->length());
    } else {
        obj = ValueToObject(cx, target);
        if (!obj)
            goto close;
        if (!js_GetLengthProperty(cx, obj, &length))
            goto close;
    }

    // Check target.length.
    i = uint32_t(iterobj->getReservedSlot(IndexSlot).toInt32());
    if (i >= length) {
        js_ThrowStopIteration(cx);
        goto close;
    }

    // Get target[i].
    JS_ASSERT(i + 1 > i);
    if (target.isString()) {
        JSString *c = cx->runtime->staticStrings.getUnitStringForElement(cx, target.toString(), i);
        if (!c)
            goto close;
        args.rval().setString(c);
    } else {
        if (!obj->getElement(cx, obj, i, &args.rval()))
            goto close;
    }

    // On success, bump the index.
    iterobj->setReservedSlot(IndexSlot, Int32Value(int32_t(i + 1)));
    return true;

  close:
    // Close the iterator. The TargetSlot will never be used again, so don't keep a
    // reference to it.
    iterobj->setReservedSlot(TargetSlot, UndefinedValue());
    iterobj->setReservedSlot(IndexSlot, Int32Value(int32_t(CLOSED_INDEX)));
    return false;
}

Class js::ElementIteratorClass = {
    "Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(ElementIteratorObject::NumSlots),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL                     /* finalize    */
};

JSFunctionSpec ElementIteratorObject::methods[] = {
    JS_FN("next", next, 0, 0),
    JS_FS_END
};

#if JS_HAS_GENERATORS
static JSBool
CloseGenerator(JSContext *cx, JSObject *genobj);
#endif

bool
js::ValueToIterator(JSContext *cx, unsigned flags, Value *vp)
{
    /* JSITER_KEYVALUE must always come with JSITER_FOREACH */
    JS_ASSERT_IF(flags & JSITER_KEYVALUE, flags & JSITER_FOREACH);

    /*
     * Make sure the more/next state machine doesn't get stuck. A value might be
     * left in iterValue when a trace is left due to an operation time-out after
     * JSOP_MOREITER but before the value is picked up by FOR*.
     */
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    RootedObject obj(cx);
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
        if (flags & JSITER_ENUMERATE) {
            if (!js_ValueToObjectOrNull(cx, *vp, obj.address()))
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

bool
js::CloseIterator(JSContext *cx, JSObject *obj)
{
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    if (obj->isPropertyIterator()) {
        /* Remove enumerators from the active list, which is a stack. */
        NativeIterator *ni = obj->asPropertyIterator().getNativeIterator();

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

bool
js::UnwindIteratorForException(JSContext *cx, JSObject *obj)
{
    RootedValue v(cx, cx->getPendingException());
    cx->clearPendingException();
    if (!CloseIterator(cx, obj))
        return false;
    cx->setPendingException(v);
    return true;
}

void
js::UnwindIteratorForUncatchableException(JSContext *cx, JSObject *obj)
{
    if (obj->isPropertyIterator()) {
        NativeIterator *ni = obj->asPropertyIterator().getNativeIterator();
        if (ni->flags & JSITER_ENUMERATE) {
            JS_ASSERT(cx->enumerators == obj);
            cx->enumerators = ni->next;
        }
    }
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
template<typename StringPredicate>
static bool
SuppressDeletedPropertyHelper(JSContext *cx, HandleObject obj, StringPredicate predicate)
{
    PropertyIteratorObject *iterobj = cx->enumerators;
    while (iterobj) {
      again:
        NativeIterator *ni = iterobj->getNativeIterator();
        /* This only works for identified surpressed keys, not values. */
        if (ni->isKeyIter() && ni->obj == obj && ni->props_cursor < ni->props_end) {
            /* Check whether id is still to come. */
            HeapPtr<JSFlatString> *props_cursor = ni->current();
            HeapPtr<JSFlatString> *props_end = ni->end();
            for (HeapPtr<JSFlatString> *idp = props_cursor; idp < props_end; ++idp) {
                if (predicate(*idp)) {
                    /*
                     * Check whether another property along the prototype chain
                     * became visible as a result of this deletion.
                     */
                    if (obj->getProto()) {
                        JSObject *proto = obj->getProto();
                        RootedObject obj2(cx);
                        RootedShape prop(cx);
                        RootedId id(cx);
                        if (!ValueToId(cx, StringValue(*idp), id.address()))
                            return false;
                        if (!proto->lookupGeneric(cx, id, &obj2, &prop))
                            return false;
                        if (prop) {
                            unsigned attrs;
                            if (obj2->isNative())
                                attrs = prop->attributes();
                            else if (!obj2->getGenericAttributes(cx, id, &attrs))
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
                        for (HeapPtr<JSFlatString> *p = idp; p + 1 != props_end; p++)
                            *p = *(p + 1);
                        ni->props_end = ni->end() - 1;

                        /*
                         * This invokes the pre barrier on this element, since
                         * it's no longer going to be marked, and ensures that
                         * any existing remembered set entry will be dropped.
                         */
                        *ni->props_end = NULL;
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

class SingleStringPredicate {
    Handle<JSFlatString*> str;
public:
    SingleStringPredicate(JSContext *cx, Handle<JSFlatString*> str) : str(str) {}

    bool operator()(JSFlatString *str) { return EqualStrings(str, this->str); }
    bool matchesAtMostOne() { return true; }
};

bool
js_SuppressDeletedProperty(JSContext *cx, HandleObject obj, jsid id)
{
    Rooted<JSFlatString*> str(cx, IdToString(cx, id));
    if (!str)
        return false;
    return SuppressDeletedPropertyHelper(cx, obj, SingleStringPredicate(cx, str));
}

bool
js_SuppressDeletedElement(JSContext *cx, HandleObject obj, uint32_t index)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return js_SuppressDeletedProperty(cx, obj, id);
}

class IndexRangePredicate {
    uint32_t begin, end;

  public:
    IndexRangePredicate(uint32_t begin, uint32_t end) : begin(begin), end(end) {}

    bool operator()(JSFlatString *str) {
        uint32_t index;
        return str->isIndex(&index) && begin <= index && index < end;
    }

    bool matchesAtMostOne() { return false; }
};

bool
js_SuppressDeletedElements(JSContext *cx, HandleObject obj, uint32_t begin, uint32_t end)
{
    return SuppressDeletedPropertyHelper(cx, obj, IndexRangePredicate(begin, end));
}

JSBool
js_IteratorMore(JSContext *cx, HandleObject iterobj, Value *rval)
{
    /* Fast path for native iterators */
    NativeIterator *ni = NULL;
    if (iterobj->isPropertyIterator()) {
        /* Key iterators are handled by fast-paths. */
        ni = iterobj->asPropertyIterator().getNativeIterator();
        bool more = ni->props_cursor < ni->props_end;
        if (ni->isKeyIter() || !more) {
            rval->setBoolean(more);
            return true;
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
    if (ni) {
        JS_ASSERT(!ni->isKeyIter());
        RootedId id(cx);
        if (!ValueToId(cx, StringValue(*ni->current()), id.address()))
            return false;
        ni->incCursor();
        if (!ni->obj->getGeneric(cx, id, rval))
            return false;
        if ((ni->flags & JSITER_KEYVALUE) && !NewKeyValuePair(cx, id, *rval, rval))
            return false;
    } else {
        /* Call the iterator object's .next method. */
        if (!GetMethod(cx, iterobj, cx->runtime->atomState.nextAtom, 0, rval))
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
    if (iterobj->isPropertyIterator()) {
        /*
         * Implement next directly as all the methods of the native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->asPropertyIterator().getNativeIterator();
        if (ni->isKeyIter()) {
            JS_ASSERT(ni->props_cursor < ni->props_end);
            *rval = StringValue(*ni->current());
            ni->incCursor();

            if (rval->isString())
                return true;

            JSString *str;
            int i;
            if (rval->isInt32() && StaticStrings::hasInt(i = rval->toInt32())) {
                str = cx->runtime->staticStrings.getInt(i);
            } else {
                str = ToString(cx, *rval);
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
stopiter_hasInstance(JSContext *cx, HandleObject obj, const Value *v, JSBool *bp)
{
    *bp = IsStopIteration(*v);
    return JS_TRUE;
}

Class js::StopIterationClass = {
    "StopIteration",
    JSCLASS_HAS_CACHED_PROTO(JSProto_StopIteration) |
    JSCLASS_FREEZE_PROTO,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    stopiter_hasInstance,
    NULL                     /* construct   */
};

/*** Generators **********************************************************************************/

#if JS_HAS_GENERATORS

static void
generator_finalize(FreeOp *fop, JSObject *obj)
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
    JS_POISON(gen->fp, JS_FREE_PATTERN, sizeof(StackFrame));
    JS_POISON(gen, JS_FREE_PATTERN, sizeof(JSGenerator));
    fop->free_(gen);
}

static void
MarkGeneratorFrame(JSTracer *trc, JSGenerator *gen)
{
    MarkValueRange(trc,
                   HeapValueify(gen->fp->generatorArgsSnapshotBegin()),
                   HeapValueify(gen->fp->generatorArgsSnapshotEnd()),
                   "Generator Floating Args");
    gen->fp->mark(trc);
    MarkValueRange(trc,
                   HeapValueify(gen->fp->generatorSlotsSnapshotBegin()),
                   HeapValueify(gen->regs.sp),
                   "Generator Floating Stack");
}

static void
GeneratorWriteBarrierPre(JSContext *cx, JSGenerator *gen)
{
    JSCompartment *comp = cx->compartment;
    if (comp->needsBarrier())
        MarkGeneratorFrame(comp->barrierTracer(), gen);
}

/*
 * Only mark generator frames/slots when the generator is not active on the
 * stack or closed. Barriers when copying onto the stack or closing preserve
 * gc invariants.
 */
bool
js::GeneratorHasMarkableFrame(JSGenerator *gen)
{
    return gen->state == JSGEN_NEWBORN || gen->state == JSGEN_OPEN;
}

/*
 * When a generator is closed, the GC things reachable from the contained frame
 * and slots become unreachable and thus require a write barrier.
 */
static void
SetGeneratorClosed(JSContext *cx, JSGenerator *gen)
{
    JS_ASSERT(gen->state != JSGEN_CLOSED);
    if (GeneratorHasMarkableFrame(gen))
        GeneratorWriteBarrierPre(cx, gen);
    gen->state = JSGEN_CLOSED;
}

static void
generator_trace(JSTracer *trc, JSObject *obj)
{
    JSGenerator *gen = (JSGenerator *) obj->getPrivate();
    if (!gen)
        return;

    if (GeneratorHasMarkableFrame(gen))
        MarkGeneratorFrame(trc, gen);
}

Class js::GeneratorClass = {
    "Generator",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    generator_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    generator_trace,
    {
        NULL,                /* equality       */
        NULL,                /* outerObject    */
        NULL,                /* innerObject    */
        iterator_iteratorObject,
        NULL                 /* unused */
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
JSObject *
js_NewGenerator(JSContext *cx)
{
    FrameRegs &stackRegs = cx->regs();
    JS_ASSERT(stackRegs.stackDepth() == 0);
    StackFrame *stackfp = stackRegs.fp();

    Rooted<GlobalObject*> global(cx, &stackfp->global());
    JSObject *proto = global->getOrCreateGeneratorPrototype(cx);
    if (!proto)
        return NULL;
    JSObject *obj = NewObjectWithGivenProto(cx, &GeneratorClass, proto, global);
    if (!obj)
        return NULL;

    /* Load and compute stack slot counts. */
    Value *stackvp = stackfp->generatorArgsSnapshotBegin();
    unsigned vplen = stackfp->generatorArgsSnapshotEnd() - stackvp;

    /* Compute JSGenerator size. */
    unsigned nbytes = sizeof(JSGenerator) +
                   (-1 + /* one Value included in JSGenerator */
                    vplen +
                    VALUES_PER_STACK_FRAME +
                    stackfp->script()->nslots) * sizeof(HeapValue);

    JS_ASSERT(nbytes % sizeof(Value) == 0);
    JS_STATIC_ASSERT(sizeof(StackFrame) % sizeof(HeapValue) == 0);

    JSGenerator *gen = (JSGenerator *) cx->malloc_(nbytes);
    if (!gen)
        return NULL;
    SetValueRangeToUndefined((Value *)gen, nbytes / sizeof(Value));

    /* Cut up floatingStack space. */
    HeapValue *genvp = gen->stackSnapshot;
    StackFrame *genfp = reinterpret_cast<StackFrame *>(genvp + vplen);

    /* Initialize JSGenerator. */
    gen->obj.init(obj);
    gen->state = JSGEN_NEWBORN;
    gen->enumerators = NULL;
    gen->fp = genfp;
    gen->prevGenerator = NULL;

    /* Copy from the stack to the generator's floating frame. */
    gen->regs.rebaseFromTo(stackRegs, *genfp);
    genfp->copyFrameAndValues<StackFrame::DoPostBarrier>(cx, (Value *)genvp, stackfp,
                                                         stackvp, stackRegs.sp);

    obj->setPrivate(gen);
    return obj;
}

static void
SetGeneratorClosed(JSContext *cx, JSGenerator *gen);

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
static JSBool
SendToGenerator(JSContext *cx, JSGeneratorOp op, JSObject *obj,
                JSGenerator *gen, const Value &arg)
{
    if (gen->state == JSGEN_RUNNING || gen->state == JSGEN_CLOSING) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NESTING_GENERATOR);
        return JS_FALSE;
    }

    /*
     * Write barrier is needed since the generator stack can be updated,
     * and it's not barriered in any other way. We need to do it before
     * gen->state changes, which can cause us to trace the generator
     * differently.
     *
     * We could optimize this by setting a bit on the generator to signify
     * that it has been marked. If this bit has already been set, there is no
     * need to mark again. The bit would have to be reset before the next GC,
     * or else some kind of epoch scheme would have to be used.
     */
    GeneratorWriteBarrierPre(cx, gen);

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

    JSBool ok;
    {
        GeneratorFrameGuard gfg;
        if (!cx->stack.pushGeneratorFrame(cx, gen, &gfg)) {
            SetGeneratorClosed(cx, gen);
            return JS_FALSE;
        }

        StackFrame *fp = gfg.fp();
        gen->regs = cx->regs();

        cx->enterGenerator(gen);   /* OOM check above. */
        PropertyIteratorObject *enumerators = cx->enumerators;
        cx->enumerators = gen->enumerators;

        ok = RunScript(cx, fp->script(), fp);

        gen->enumerators = cx->enumerators;
        cx->enumerators = enumerators;
        cx->leaveGenerator(gen);
    }

    if (gen->fp->isYielding()) {
        /* Yield cannot fail, throw or be called on closing. */
        JS_ASSERT(ok);
        JS_ASSERT(!cx->isExceptionPending());
        JS_ASSERT(gen->state == JSGEN_RUNNING);
        JS_ASSERT(op != JSGENOP_CLOSE);
        gen->fp->clearYielding();
        gen->state = JSGEN_OPEN;
        return JS_TRUE;
    }

    gen->fp->clearReturnValue();
    SetGeneratorClosed(cx, gen);
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

static JSBool
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

static bool
IsGenerator(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&GeneratorClass);
}

static bool
generator_send_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsGenerator(args.thisv()));

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = (JSGenerator *) thisObj->getPrivate();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        return js_ThrowStopIteration(cx);
    }

    if (gen->state == JSGEN_NEWBORN && args.hasDefined(0)) {
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_SEND,
                            JSDVG_SEARCH_STACK, args[0], NULL);
        return false;
    }

    if (!SendToGenerator(cx, JSGENOP_SEND, thisObj, gen,
                         args.length() > 0 ? args[0] : UndefinedValue()))
    {
        return false;
    }

    args.rval() = gen->fp->returnValue();
    return true;

}

static JSBool
generator_send(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsGenerator, generator_send_impl, args);
}

static bool
generator_next_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsGenerator(args.thisv()));

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = (JSGenerator *) thisObj->getPrivate();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        return js_ThrowStopIteration(cx);
    }

    if (!SendToGenerator(cx, JSGENOP_NEXT, thisObj, gen, UndefinedValue()))
        return false;

    args.rval() = gen->fp->returnValue();
    return true;
}

static JSBool
generator_next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsGenerator, generator_next_impl, args);
}

static bool
generator_throw_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsGenerator(args.thisv()));

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = (JSGenerator *) thisObj->getPrivate();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        cx->setPendingException(args.length() >= 1 ? args[0] : UndefinedValue());
        return false;
    }

    if (!SendToGenerator(cx, JSGENOP_THROW, thisObj, gen,
                         args.length() > 0 ? args[0] : UndefinedValue()))
    {
        return false;
    }

    args.rval() = gen->fp->returnValue();
    return true;

}

static JSBool
generator_throw(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsGenerator, generator_throw_impl, args);
}

static bool
generator_close_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsGenerator(args.thisv()));

    Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = (JSGenerator *) thisObj->getPrivate();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        args.rval().setUndefined();
        return true;
    }

    if (gen->state == JSGEN_NEWBORN) {
        SetGeneratorClosed(cx, gen);
        args.rval().setUndefined();
        return true;
    }

    if (!SendToGenerator(cx, JSGENOP_CLOSE, thisObj, gen, UndefinedValue()))
        return false;

    args.rval() = gen->fp->returnValue();
    return true;
}

static JSBool
generator_close(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsGenerator, generator_close_impl, args);
}

#define JSPROP_ROPERM   (JSPROP_READONLY | JSPROP_PERMANENT)

static JSFunctionSpec generator_methods[] = {
    JS_FN("iterator",  iterator_iterator,  0, 0),
    JS_FN("next",      generator_next,     0,JSPROP_ROPERM),
    JS_FN("send",      generator_send,     1,JSPROP_ROPERM),
    JS_FN("throw",     generator_throw,    1,JSPROP_ROPERM),
    JS_FN("close",     generator_close,    0,JSPROP_ROPERM),
    JS_FS_END
};

#endif /* JS_HAS_GENERATORS */

/* static */ bool
GlobalObject::initIteratorClasses(JSContext *cx, Handle<GlobalObject *> global)
{
    Rooted<JSObject*> iteratorProto(cx);
    Value iteratorProtoVal = global->getPrototype(JSProto_Iterator);
    if (iteratorProtoVal.isObject()) {
        iteratorProto = &iteratorProtoVal.toObject();
    } else {
        iteratorProto = global->createBlankPrototype(cx, &PropertyIteratorObject::class_);
        if (!iteratorProto)
            return false;

        AutoIdVector blank(cx);
        NativeIterator *ni = NativeIterator::allocateIterator(cx, 0, blank);
        if (!ni)
            return false;
        ni->init(NULL, 0 /* flags */, 0, 0);

        iteratorProto->asPropertyIterator().setNativeIterator(ni);

        Rooted<JSFunction*> ctor(cx, global->createConstructor(cx, Iterator, CLASS_NAME(cx, Iterator), 2));
        if (!ctor)
            return false;
        if (!LinkConstructorAndPrototype(cx, ctor, iteratorProto))
            return false;
        if (!DefinePropertiesAndBrand(cx, iteratorProto, NULL, iterator_methods))
            return false;
        if (!DefineConstructorAndPrototype(cx, global, JSProto_Iterator, ctor, iteratorProto))
            return false;
    }

    Rooted<JSObject*> proto(cx);
    if (global->getSlot(ELEMENT_ITERATOR_PROTO).isUndefined()) {
        Class *cls = &ElementIteratorClass;
        proto = global->createBlankPrototypeInheriting(cx, cls, *iteratorProto);
        if (!proto || !DefinePropertiesAndBrand(cx, proto, NULL, ElementIteratorObject::methods))
            return false;
        global->setReservedSlot(ELEMENT_ITERATOR_PROTO, ObjectValue(*proto));
    }

#if JS_HAS_GENERATORS
    if (global->getSlot(GENERATOR_PROTO).isUndefined()) {
        proto = global->createBlankPrototype(cx, &GeneratorClass);
        if (!proto || !DefinePropertiesAndBrand(cx, proto, NULL, generator_methods))
            return false;
        global->setReservedSlot(GENERATOR_PROTO, ObjectValue(*proto));
    }
#endif

    if (global->getPrototype(JSProto_StopIteration).isUndefined()) {
        proto = global->createBlankPrototype(cx, &StopIterationClass);
        if (!proto || !proto->freeze(cx))
            return false;

        /* This should use a non-JSProtoKey'd slot, but this is easier for now. */
        if (!DefineConstructorAndPrototype(cx, global, JSProto_StopIteration, proto, proto))
            return false;

        MarkStandardClassInitializedNoProto(global, &StopIterationClass);
    }

    return true;
}

JSObject *
js_InitIteratorClasses(JSContext *cx, JSObject *obj)
{
    Rooted<GlobalObject*> global(cx, &obj->asGlobal());
    if (!GlobalObject::initIteratorClasses(cx, global))
        return NULL;
    return global->getIteratorPrototype();
}
