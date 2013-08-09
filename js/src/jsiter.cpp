/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript iterators. */

#include "jsiter.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Util.h"

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"

#include "ds/Sort.h"
#include "gc/Marking.h"
#include "vm/GeneratorObject.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Shape.h"
#include "vm/StopIterationObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "builtin/Iterator-inl.h"
#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;

using mozilla::ArrayLength;
#ifdef JS_MORE_DETERMINISTIC
using mozilla::PodCopy;
#endif
using mozilla::PodZero;

typedef Rooted<PropertyIteratorObject*> RootedPropertyIteratorObject;

static const gc::AllocKind ITERATOR_FINALIZE_KIND = gc::FINALIZE_OBJECT2_BACKGROUND;

void
NativeIterator::mark(JSTracer *trc)
{
    for (HeapPtr<JSFlatString> *str = begin(); str < end(); str++)
        MarkString(trc, str, "prop");
    if (obj)
        MarkObject(trc, &obj, "obj");

    // The SuppressDeletedPropertyHelper loop can GC, so make sure that if the
    // GC removes any elements from the list, it won't remove this one.
    if (iterObj_)
        MarkObjectUnbarriered(trc, &iterObj_, "iterObj");
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
NewKeyValuePair(JSContext *cx, jsid id, const Value &val, MutableHandleValue rval)
{
    Value vec[2] = { IdToValue(id), val };
    AutoArrayRooter tvr(cx, ArrayLength(vec), vec);

    JSObject *aobj = NewDenseCopiedArray(cx, 2, vec);
    if (!aobj)
        return false;
    rval.setObject(*aobj);
    return true;
}

static inline bool
Enumerate(JSContext *cx, HandleObject pobj, jsid id,
          bool enumerable, unsigned flags, IdSet& ht, AutoIdVector *props)
{
    /*
     * We implement __proto__ using a property on |Object.prototype|, but
     * because __proto__ is highly deserving of removal, we don't want it to
     * show up in property enumeration, even if only for |Object.prototype|
     * (think introspection by Prototype-like frameworks that add methods to
     * the built-in prototypes).  So exclude __proto__ if the object where the
     * property was found has no [[Prototype]] and might be |Object.prototype|.
     */
    if (JS_UNLIKELY(!pobj->getTaggedProto().isObject() && JSID_IS_ATOM(id, cx->names().proto)))
        return true;

    if (!(flags & JSITER_OWNONLY) || pobj->is<ProxyObject>() || pobj->getOps()->enumerate) {
        /* If we've already seen this, we definitely won't add it. */
        IdSet::AddPtr p = ht.lookupForAdd(id);
        if (JS_UNLIKELY(!!p))
            return true;

        /*
         * It's not necessary to add properties to the hash table at the end of
         * the prototype chain, but custom enumeration behaviors might return
         * duplicated properties, so always add in such cases.
         */
        if ((pobj->is<ProxyObject>() || pobj->getProto() || pobj->getOps()->enumerate) && !ht.add(p, id))
            return false;
    }

    if (enumerable || (flags & JSITER_HIDDEN))
        return props->append(id);

    return true;
}

static bool
EnumerateNativeProperties(JSContext *cx, HandleObject pobj, unsigned flags, IdSet &ht,
                          AutoIdVector *props)
{
    /* Collect any elements from this object. */
    size_t initlen = pobj->getDenseInitializedLength();
    const Value *vp = pobj->getDenseElements();
    for (size_t i = 0; i < initlen; ++i, ++vp) {
        if (!vp->isMagic(JS_ELEMENTS_HOLE)) {
            /* Dense arrays never get so large that i would not fit into an integer id. */
            if (!Enumerate(cx, pobj, INT_TO_JSID(i), true, flags, ht, props))
                return false;
        }
    }

    size_t initialLength = props->length();

    /* Collect all unique properties from this object's scope. */
    Shape::Range<NoGC> r(pobj->lastProperty());
    for (; !r.empty(); r.popFront()) {
        Shape &shape = r.front();

        if (!Enumerate(cx, pobj, shape.propid(), shape.enumerable(), flags, ht, props))
            return false;
    }

    ::Reverse(props->begin() + initialLength, props->end());
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
Snapshot(JSContext *cx, JSObject *pobj_, unsigned flags, AutoIdVector *props)
{
    IdSet ht(cx);
    if (!ht.init(32))
        return false;

    RootedObject pobj(cx, pobj_);

    do {
        Class *clasp = pobj->getClass();
        if (pobj->isNative() &&
            !pobj->getOps()->enumerate &&
            !(clasp->flags & JSCLASS_NEW_ENUMERATE)) {
            if (!clasp->enumerate(cx, pobj))
                return false;
            if (!EnumerateNativeProperties(cx, pobj, flags, ht, props))
                return false;
        } else {
            if (pobj->is<ProxyObject>()) {
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
                    if (!Enumerate(cx, pobj, proxyProps[n], true, flags, ht, props))
                        return false;
                }
                /* Proxy objects enumerate the prototype on their own, so we are done here. */
                break;
            }
            RootedValue state(cx);
            RootedId id(cx);
            JSIterateOp op = (flags & JSITER_HIDDEN) ? JSENUMERATE_INIT_ALL : JSENUMERATE_INIT;
            if (!JSObject::enumerate(cx, pobj, op, &state, &id))
                return false;
            if (state.isMagic(JS_NATIVE_ENUMERATE)) {
                if (!EnumerateNativeProperties(cx, pobj, flags, ht, props))
                    return false;
            } else {
                while (true) {
                    RootedId id(cx);
                    if (!JSObject::enumerate(cx, pobj, JSENUMERATE_NEXT, &state, &id))
                        return false;
                    if (state.isNull())
                        break;
                    if (!Enumerate(cx, pobj, id, true, flags, ht, props))
                        return false;
                }
            }
        }

        if (flags & JSITER_OWNONLY)
            break;

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

    AutoIdVector tmp(cx);
    if (!tmp.resize(n))
        return false;
    PodCopy(tmp.begin(), ids, n);

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
GetCustomIterator(JSContext *cx, HandleObject obj, unsigned flags, MutableHandleValue vp)
{
    JS_CHECK_RECURSION(cx, return false);

    /* Check whether we have a valid __iterator__ method. */
    HandlePropertyName name = cx->names().iteratorIntrinsic;
    if (!JSObject::getProperty(cx, obj, obj, name, vp))
        return false;

    /* If there is no custom __iterator__ method, we are done here. */
    if (!vp.isObject()) {
        vp.setUndefined();
        return true;
    }

    if (!cx->runningWithTrustedPrincipals())
        ++sCustomIteratorCount;

    /* Otherwise call it and return that object. */
    Value arg = BooleanValue((flags & JSITER_FOREACH) == 0);
    if (!Invoke(cx, ObjectValue(*obj), vp, 1, &arg, vp))
        return false;
    if (vp.isPrimitive()) {
        /*
         * We are always coming from js::ValueToIterator, and we are no longer on
         * trace, so the object we are iterating over is on top of the stack (-1).
         */
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, name, &bytes))
            return false;
        RootedValue val(cx, ObjectValue(*obj));
        js_ReportValueError2(cx, JSMSG_BAD_TRAP_RETURN_VALUE,
                             -1, val, NullPtr(), bytes.ptr());
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
        RootedTypeObject type(cx, cx->getNewType(&PropertyIteratorObject::class_, NULL));
        if (!type)
            return NULL;

        JSObject *metadata = NULL;
        if (!NewObjectMetadata(cx, &metadata))
            return NULL;

        Class *clasp = &PropertyIteratorObject::class_;
        RootedShape shape(cx, EmptyShape::getInitialShape(cx, clasp, NULL, NULL, metadata,
                                                          ITERATOR_FINALIZE_KIND));
        if (!shape)
            return NULL;

        JSObject *obj = JSObject::create(cx, ITERATOR_FINALIZE_KIND,
                                         GetInitialHeap(GenericObject, clasp), shape, type);
        if (!obj)
            return NULL;

        JS_ASSERT(obj->numFixedSlots() == JSObject::ITER_CLASS_NFIXED_SLOTS);
        return &obj->as<PropertyIteratorObject>();
    }

    return &NewBuiltinClassInstance(cx, &PropertyIteratorObject::class_)->as<PropertyIteratorObject>();
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
    ni->next_ = NULL;
    ni->prev_ = NULL;
    return ni;
}

NativeIterator *
NativeIterator::allocateSentinel(JSContext *cx)
{
    NativeIterator *ni = (NativeIterator *)js_malloc(sizeof(NativeIterator));
    if (!ni)
        return NULL;

    PodZero(ni);

    ni->next_ = ni;
    ni->prev_ = ni;
    return ni;
}

inline void
NativeIterator::init(JSObject *obj, JSObject *iterObj, unsigned flags, uint32_t slength, uint32_t key)
{
    this->obj.init(obj);
    this->iterObj_ = iterObj;
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
        ni->link(cx->compartment()->enumerators);

        JS_ASSERT(!(ni->flags & JSITER_ACTIVE));
        ni->flags |= JSITER_ACTIVE;
    }
}

static inline bool
VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &keys,
                    uint32_t slength, uint32_t key, MutableHandleValue vp)
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
    ni->init(obj, iterobj, flags, slength, key);

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
    vp.setObject(*iterobj);

    RegisterEnumerator(cx, iterobj, ni);
    return true;
}

bool
js::VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &props,
                        MutableHandleValue vp)
{
    return VectorToKeyIterator(cx, obj, flags, props, 0, 0, vp);
}

bool
js::VectorToValueIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &keys,
                          MutableHandleValue vp)
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
    ni->init(obj, iterobj, flags, 0, 0);

    iterobj->setNativeIterator(ni);
    vp.setObject(*iterobj);

    RegisterEnumerator(cx, iterobj, ni);
    return true;
}

bool
js::EnumeratedIdVectorToIterator(JSContext *cx, HandleObject obj, unsigned flags,
                                 AutoIdVector &props, MutableHandleValue vp)
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
js::GetIterator(JSContext *cx, HandleObject obj, unsigned flags, MutableHandleValue vp)
{
    if (flags == JSITER_FOR_OF) {
        // for-of loop. The iterator is simply |obj.iterator()|.
        RootedValue method(cx);
        if (!JSObject::getProperty(cx, obj, obj, cx->names().iterator, &method))
            return false;

        // Throw if obj.iterator isn't callable. js::Invoke is about to check
        // for this kind of error anyway, but it would throw an inscrutable
        // error message about |method| rather than this nice one about |obj|.
        if (!method.isObject() || !method.toObject().isCallable()) {
            RootedValue val(cx, ObjectOrNullValue(obj));
            char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, val, NullPtr());
            if (!bytes)
                return false;
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_ITERABLE, bytes);
            js_free(bytes);
            return false;
        }

        if (!Invoke(cx, ObjectOrNullValue(obj), method, 0, NULL, vp))
            return false;

        JSObject *resultObj = ToObject(cx, vp);
        if (!resultObj)
            return false;
        vp.setObject(*resultObj);
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
            vp.setObject(*iterobj);
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
            PropertyIteratorObject *last = cx->runtime()->nativeIterCache.last;
            if (last) {
                NativeIterator *lastni = last->getNativeIterator();
                if (!(lastni->flags & (JSITER_ACTIVE|JSITER_UNREUSABLE)) &&
                    obj->isNative() &&
                    obj->hasEmptyElements() &&
                    obj->lastProperty() == lastni->shapes_array[0])
                {
                    JSObject *proto = obj->getProto();
                    if (proto->isNative() &&
                        proto->hasEmptyElements() &&
                        proto->lastProperty() == lastni->shapes_array[1] &&
                        !proto->getProto())
                    {
                        vp.setObject(*last);
                        UpdateNativeIterator(lastni, obj);
                        RegisterEnumerator(cx, last, lastni);
                        return true;
                    }
                }
            }

            /*
             * The iterator object for JSITER_ENUMERATE never escapes, so we
             * don't care for the proper parent/proto to be set. This also
             * allows us to re-use a previous iterator object that is not
             * currently active.
             */
            {
                JSObject *pobj = obj;
                do {
                    if (!pobj->isNative() ||
                        !pobj->hasEmptyElements() ||
                        pobj->hasUncacheableProto() ||
                        obj->getOps()->enumerate ||
                        pobj->getClass()->enumerate != JS_EnumerateStub) {
                        shapes.clear();
                        goto miss;
                    }
                    Shape *shape = pobj->lastProperty();
                    key = (key + (key << 16)) ^ (uintptr_t(shape) >> 3);
                    if (!shapes.append(shape))
                        return false;
                    pobj = pobj->getProto();
                } while (pobj);
            }

            PropertyIteratorObject *iterobj = cx->runtime()->nativeIterCache.get(key);
            if (iterobj) {
                NativeIterator *ni = iterobj->getNativeIterator();
                if (!(ni->flags & (JSITER_ACTIVE|JSITER_UNREUSABLE)) &&
                    ni->shapes_key == key &&
                    ni->shapes_length == shapes.length() &&
                    Compare(ni->shapes_array, shapes.begin(), ni->shapes_length)) {
                    vp.setObject(*iterobj);

                    UpdateNativeIterator(ni, obj);
                    RegisterEnumerator(cx, iterobj, ni);
                    if (shapes.length() == 2)
                        cx->runtime()->nativeIterCache.last = iterobj;
                    return true;
                }
            }
        }

      miss:
        if (obj->is<ProxyObject>()) {
            types::MarkIteratorUnknown(cx);
            return Proxy::iterate(cx, obj, flags, vp);
        }
        if (!GetCustomIterator(cx, obj, flags, vp))
            return false;
        if (!vp.isUndefined()) {
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

    PropertyIteratorObject *iterobj = &vp.toObject().as<PropertyIteratorObject>();

    /* Cache the iterator object if possible. */
    if (shapes.length())
        cx->runtime()->nativeIterCache.set(key, iterobj);

    if (shapes.length() == 2)
        cx->runtime()->nativeIterCache.last = iterobj;
    return true;
}

JSObject *
js::GetIteratorObject(JSContext *cx, HandleObject obj, uint32_t flags)
{
    RootedValue value(cx);
    if (!GetIterator(cx, obj, flags, &value))
        return NULL;
    return &value.toObject();
}

bool
js_ThrowStopIteration(JSContext *cx)
{
    JS_ASSERT(!JS_IsExceptionPending(cx));
    RootedValue v(cx);
    if (js_FindClassObject(cx, JSProto_StopIteration, &v))
        cx->setPendingException(v);
    return false;
}

/*** Iterator objects ****************************************************************************/

bool
js::IteratorConstructor(JSContext *cx, unsigned argc, Value *vp)
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

    if (!ValueToIterator(cx, flags, args[0]))
        return false;
    args.rval().set(args[0]);
    return true;
}

JS_ALWAYS_INLINE bool
IsIterator(const Value &v)
{
    return v.isObject() && v.toObject().hasClass(&PropertyIteratorObject::class_);
}

JS_ALWAYS_INLINE bool
iterator_next_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsIterator(args.thisv()));

    RootedObject thisObj(cx, &args.thisv().toObject());

    if (!js_IteratorMore(cx, thisObj, args.rval()))
        return false;

    if (!args.rval().toBoolean()) {
        js_ThrowStopIteration(cx);
        return false;
    }

    return js_IteratorNext(cx, thisObj, args.rval());
}

static bool
iterator_iterator(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(args.thisv());
    return true;
}

bool
iterator_next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsIterator, iterator_next_impl>(cx, args);
}

static const JSFunctionSpec iterator_methods[] = {
    JS_FN("iterator",  iterator_iterator,   0, 0),
    JS_FN("next",      iterator_next,       0, 0),
    JS_FS_END
};

static JSObject *
iterator_iteratorObject(JSContext *cx, HandleObject obj, bool keysonly)
{
    return obj;
}

size_t
PropertyIteratorObject::sizeOfMisc(mozilla::MallocSizeOf mallocSizeOf) const
{
    return mallocSizeOf(getPrivate());
}

void
PropertyIteratorObject::trace(JSTracer *trc, JSObject *obj)
{
    if (NativeIterator *ni = obj->as<PropertyIteratorObject>().getNativeIterator())
        ni->mark(trc);
}

void
PropertyIteratorObject::finalize(FreeOp *fop, JSObject *obj)
{
    if (NativeIterator *ni = obj->as<PropertyIteratorObject>().getNativeIterator())
        fop->free_(ni);
}

Class PropertyIteratorObject::class_ = {
    "Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Iterator) |
    JSCLASS_HAS_PRIVATE |
    JSCLASS_BACKGROUND_FINALIZE,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    trace,
    {
        NULL,                /* outerObject    */
        NULL,                /* innerObject    */
        iterator_iteratorObject,
    }
};

const uint32_t CLOSED_INDEX = UINT32_MAX;

JSObject *
ElementIteratorObject::create(JSContext *cx, Handle<Value> target)
{
    RootedObject proto(cx, cx->global()->getOrCreateElementIteratorPrototype(cx));
    if (!proto)
        return NULL;
    RootedObject iterobj(cx, NewObjectWithGivenProto(cx, &class_, proto, cx->global()));
    if (iterobj) {
        iterobj->setReservedSlot(TargetSlot, target);
        iterobj->setReservedSlot(IndexSlot, Int32Value(0));
    }
    return iterobj;
}

static bool
IsElementIterator(const Value &v)
{
    return v.isObject() && v.toObject().is<ElementIteratorObject>();
}

bool
ElementIteratorObject::next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod(cx, IsElementIterator, next_impl, args);
}

bool
ElementIteratorObject::next_impl(JSContext *cx, CallArgs args)
{
    RootedObject iterobj(cx, &args.thisv().toObject());
    uint32_t i, length;
    RootedValue target(cx, iterobj->getReservedSlot(TargetSlot));
    RootedObject obj(cx);

    // Get target.length.
    if (target.isString()) {
        length = uint32_t(target.toString()->length());
    } else {
        obj = ToObjectFromStack(cx, target);
        if (!obj)
            goto close;
        if (!GetLengthProperty(cx, obj, &length))
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
        JSString *c = cx->runtime()->staticStrings.getUnitStringForElement(cx, target.toString(), i);
        if (!c)
            goto close;
        args.rval().setString(c);
    } else {
        if (!JSObject::getElement(cx, obj, obj, i, args.rval()))
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

Class ElementIteratorObject::class_ = {
    "Array Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(ElementIteratorObject::NumSlots),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL                     /* finalize    */
};

const JSFunctionSpec ElementIteratorObject::methods[] = {
    JS_FN("next", next, 0, 0),
    JS_FS_END
};

static bool
CloseGenerator(JSContext *cx, HandleObject genobj);

bool
js::ValueToIterator(JSContext *cx, unsigned flags, MutableHandleValue vp)
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
    if (vp.isObject()) {
        /* Common case. */
        obj = &vp.toObject();
    } else {
        /*
         * Enumerating over null and undefined gives an empty enumerator.
         * This is contrary to ECMA-262 9.9 ToObject, invoked from step 3 of
         * the first production in 12.6.4 and step 4 of the second production,
         * but it's "web JS" compatible. ES5 fixed for-in to match this de-facto
         * standard.
         */
        if (flags & JSITER_ENUMERATE) {
            if (!js_ValueToObjectOrNull(cx, vp, &obj))
                return false;
            /* fall through */
        } else {
            obj = js_ValueToNonNullObject(cx, vp);
            if (!obj)
                return false;
        }
    }

    return GetIterator(cx, obj, flags, vp);
}

bool
js::CloseIterator(JSContext *cx, HandleObject obj)
{
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    if (obj->is<PropertyIteratorObject>()) {
        /* Remove enumerators from the active list, which is a stack. */
        NativeIterator *ni = obj->as<PropertyIteratorObject>().getNativeIterator();

        if (ni->flags & JSITER_ENUMERATE) {
            ni->unlink();

            JS_ASSERT(ni->flags & JSITER_ACTIVE);
            ni->flags &= ~JSITER_ACTIVE;

            /*
             * Reset the enumerator; it may still be in the cached iterators
             * for this thread, and can be reused.
             */
            ni->props_cursor = ni->props_array;
        }
    } else if (obj->is<GeneratorObject>()) {
        // FIXME: Only close legacy generators.
        return CloseGenerator(cx, obj);
    }
    return true;
}

bool
js::UnwindIteratorForException(JSContext *cx, HandleObject obj)
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
    if (obj->is<PropertyIteratorObject>()) {
        NativeIterator *ni = obj->as<PropertyIteratorObject>().getNativeIterator();
        if (ni->flags & JSITER_ENUMERATE)
            ni->unlink();
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
    NativeIterator *enumeratorList = cx->compartment()->enumerators;
    NativeIterator *ni = enumeratorList->next();

    while (ni != enumeratorList) {
      again:
        /* This only works for identified suppressed keys, not values. */
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
                    RootedObject proto(cx);
                    if (!JSObject::getProto(cx, obj, &proto))
                        return false;
                    if (proto) {
                        RootedObject obj2(cx);
                        RootedShape prop(cx);
                        RootedId id(cx);
                        RootedValue idv(cx, StringValue(*idp));
                        if (!ValueToId<CanGC>(cx, idv, &id))
                            return false;
                        if (!JSObject::lookupGeneric(cx, proto, id, &obj2, &prop))
                            return false;
                        if (prop) {
                            unsigned attrs;
                            if (obj2->isNative())
                                attrs = GetShapeAttributes(prop);
                            else if (!JSObject::getGenericAttributes(cx, obj2, id, &attrs))
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
        ni = ni->next();
    }
    return true;
}

class SingleStringPredicate {
    Handle<JSFlatString*> str;
public:
    SingleStringPredicate(Handle<JSFlatString*> str) : str(str) {}

    bool operator()(JSFlatString *str) { return EqualStrings(str, this->str); }
    bool matchesAtMostOne() { return true; }
};

bool
js_SuppressDeletedProperty(JSContext *cx, HandleObject obj, jsid id)
{
    Rooted<JSFlatString*> str(cx, IdToString(cx, id));
    if (!str)
        return false;
    return SuppressDeletedPropertyHelper(cx, obj, SingleStringPredicate(str));
}

bool
js_SuppressDeletedElement(JSContext *cx, HandleObject obj, uint32_t index)
{
    RootedId id(cx);
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

static inline bool
IsStopIteration(const js::Value &v)
{
    return v.isObject() && v.toObject().is<StopIterationObject>();
}

bool
js_IteratorMore(JSContext *cx, HandleObject iterobj, MutableHandleValue rval)
{
    /* Fast path for native iterators */
    NativeIterator *ni = NULL;
    if (iterobj->is<PropertyIteratorObject>()) {
        /* Key iterators are handled by fast-paths. */
        ni = iterobj->as<PropertyIteratorObject>().getNativeIterator();
        bool more = ni->props_cursor < ni->props_end;
        if (ni->isKeyIter() || !more) {
            rval.setBoolean(more);
            return true;
        }
    }

    /* We might still have a pending value. */
    if (!cx->iterValue.isMagic(JS_NO_ITER_VALUE)) {
        rval.setBoolean(true);
        return true;
    }

    /* We're reentering below and can call anything. */
    JS_CHECK_RECURSION(cx, return false);

    /* Fetch and cache the next value from the iterator. */
    if (ni) {
        JS_ASSERT(!ni->isKeyIter());
        RootedId id(cx);
        RootedValue current(cx, StringValue(*ni->current()));
        if (!ValueToId<CanGC>(cx, current, &id))
            return false;
        ni->incCursor();
        RootedObject obj(cx, ni->obj);
        if (!JSObject::getGeneric(cx, obj, obj, id, rval))
            return false;
        if ((ni->flags & JSITER_KEYVALUE) && !NewKeyValuePair(cx, id, rval, rval))
            return false;
    } else {
        /* Call the iterator object's .next method. */
        if (!JSObject::getProperty(cx, iterobj, iterobj, cx->names().next, rval))
            return false;
        if (!Invoke(cx, ObjectValue(*iterobj), rval, 0, NULL, rval)) {
            /* Check for StopIteration. */
            if (!cx->isExceptionPending() || !IsStopIteration(cx->getPendingException()))
                return false;

            cx->clearPendingException();
            cx->iterValue.setMagic(JS_NO_ITER_VALUE);
            rval.setBoolean(false);
            return true;
        }
    }

    /* Cache the value returned by iterobj.next() so js_IteratorNext() can find it. */
    JS_ASSERT(!rval.isMagic(JS_NO_ITER_VALUE));
    cx->iterValue = rval;
    rval.setBoolean(true);
    return true;
}

bool
js_IteratorNext(JSContext *cx, HandleObject iterobj, MutableHandleValue rval)
{
    /* Fast path for native iterators */
    if (iterobj->is<PropertyIteratorObject>()) {
        /*
         * Implement next directly as all the methods of the native iterator are
         * read-only and permanent.
         */
        NativeIterator *ni = iterobj->as<PropertyIteratorObject>().getNativeIterator();
        if (ni->isKeyIter()) {
            JS_ASSERT(ni->props_cursor < ni->props_end);
            rval.setString(*ni->current());
            ni->incCursor();
            return true;
        }
    }

    JS_ASSERT(!cx->iterValue.isMagic(JS_NO_ITER_VALUE));
    rval.set(cx->iterValue);
    cx->iterValue.setMagic(JS_NO_ITER_VALUE);

    return true;
}

static bool
stopiter_hasInstance(JSContext *cx, HandleObject obj, MutableHandleValue v, bool *bp)
{
    *bp = IsStopIteration(v);
    return true;
}

Class StopIterationObject::class_ = {
    "StopIteration",
    JSCLASS_HAS_CACHED_PROTO(JSProto_StopIteration) |
    JSCLASS_FREEZE_PROTO,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
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

static void
generator_finalize(FreeOp *fop, JSObject *obj)
{
    JSGenerator *gen = obj->as<GeneratorObject>().getGenerator();
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
    JS::Zone *zone = cx->zone();
    if (zone->needsBarrier())
        MarkGeneratorFrame(zone->barrierTracer(), gen);
}

static void
GeneratorWriteBarrierPost(JSContext *cx, JSGenerator *gen)
{
#ifdef JSGC_GENERATIONAL
    cx->runtime()->gcStoreBuffer.putWholeCell(gen->obj);
#endif
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

GeneratorState::GeneratorState(JSContext *cx, JSGenerator *gen, JSGeneratorState futureState)
  : RunState(cx, Generator, gen->fp->script()),
    cx_(cx),
    gen_(gen),
    futureState_(futureState),
    entered_(false)
{ }

GeneratorState::~GeneratorState()
{
    gen_->fp->setSuspended();

    if (entered_)
        cx_->leaveGenerator(gen_);
}

StackFrame *
GeneratorState::pushInterpreterFrame(JSContext *cx, FrameGuard *)
{
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
    GeneratorWriteBarrierPre(cx, gen_);
    gen_->state = futureState_;

    gen_->fp->clearSuspended();

    cx->enterGenerator(gen_);   /* OOM check above. */
    entered_ = true;
    return gen_->fp;
}

static void
generator_trace(JSTracer *trc, JSObject *obj)
{
    JSGenerator *gen = obj->as<GeneratorObject>().getGenerator();
    if (!gen)
        return;

    if (GeneratorHasMarkableFrame(gen))
        MarkGeneratorFrame(trc, gen);
}

Class GeneratorObject::class_ = {
    "Generator",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    generator_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    generator_trace,
    {
        NULL,                /* outerObject    */
        NULL,                /* innerObject    */
        iterator_iteratorObject,
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
js_NewGenerator(JSContext *cx, const FrameRegs &stackRegs)
{
    JS_ASSERT(stackRegs.stackDepth() == 0);
    StackFrame *stackfp = stackRegs.fp();

    Rooted<GlobalObject*> global(cx, &stackfp->global());
    RootedObject obj(cx);
    {
        JSObject *proto = global->getOrCreateGeneratorPrototype(cx);
        if (!proto)
            return NULL;
        obj = NewObjectWithGivenProto(cx, &GeneratorObject::class_, proto, global);
    }
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

    JSGenerator *gen = (JSGenerator *) cx->calloc_(nbytes);
    if (!gen)
        return NULL;

    /* Cut up floatingStack space. */
    HeapValue *genvp = gen->stackSnapshot;
    SetValueRangeToUndefined((Value *)genvp, vplen);

    StackFrame *genfp = reinterpret_cast<StackFrame *>(genvp + vplen);

    /* Initialize JSGenerator. */
    gen->obj.init(obj);
    gen->state = JSGEN_NEWBORN;
    gen->fp = genfp;
    gen->prevGenerator = NULL;

    /* Copy from the stack to the generator's floating frame. */
    gen->regs.rebaseFromTo(stackRegs, *genfp);
    genfp->copyFrameAndValues<StackFrame::DoPostBarrier>(cx, (Value *)genvp, stackfp,
                                                         stackvp, stackRegs.sp);
    genfp->setSuspended();
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
static bool
SendToGenerator(JSContext *cx, JSGeneratorOp op, HandleObject obj,
                JSGenerator *gen, HandleValue arg)
{
    if (gen->state == JSGEN_RUNNING || gen->state == JSGEN_CLOSING) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NESTING_GENERATOR);
        return false;
    }

    JSGeneratorState futureState;
    JS_ASSERT(gen->state == JSGEN_NEWBORN || gen->state == JSGEN_OPEN);
    switch (op) {
      case JSGENOP_NEXT:
      case JSGENOP_SEND:
        if (gen->state == JSGEN_OPEN) {
            /*
             * Store the argument to send as the result of the yield
             * expression. The generator stack is not barriered, so we need
             * write barriers here.
             */
            HeapValue::writeBarrierPre(gen->regs.sp[-1]);
            gen->regs.sp[-1] = arg;
            HeapValue::writeBarrierPost(cx->runtime(), gen->regs.sp[-1], &gen->regs.sp[-1]);
        }
        futureState = JSGEN_RUNNING;
        break;

      case JSGENOP_THROW:
        cx->setPendingException(arg);
        futureState = JSGEN_RUNNING;
        break;

      default:
        JS_ASSERT(op == JSGENOP_CLOSE);
        cx->setPendingException(MagicValue(JS_GENERATOR_CLOSING));
        futureState = JSGEN_CLOSING;
        break;
    }

    bool ok;
    {
        GeneratorState state(cx, gen, futureState);
        ok = RunScript(cx, state);
        if (!ok && gen->state == JSGEN_CLOSED)
            return false;
    }

    if (gen->fp->isYielding()) {
        /*
         * Yield is ordinarily infallible, but ok can be false here if a
         * Debugger.Frame.onPop hook fails.
         */
        JS_ASSERT(gen->state == JSGEN_RUNNING);
        JS_ASSERT(op != JSGENOP_CLOSE);
        gen->fp->clearYielding();
        gen->state = JSGEN_OPEN;
        GeneratorWriteBarrierPost(cx, gen);
        return ok;
    }

    gen->fp->clearReturnValue();
    SetGeneratorClosed(cx, gen);
    if (ok) {
        /* Returned, explicitly or by falling off the end. */
        if (op == JSGENOP_CLOSE)
            return true;
        return js_ThrowStopIteration(cx);
    }

    /*
     * An error, silent termination by operation callback or an exception.
     * Propagate the condition to the caller.
     */
    return false;
}

static bool
CloseGenerator(JSContext *cx, HandleObject obj)
{
    JSGenerator *gen = obj->as<GeneratorObject>().getGenerator();
    if (!gen) {
        /* Generator prototype object. */
        return true;
    }

    // FIXME: Assert that gen is a legacy generator.

    if (gen->state == JSGEN_CLOSED)
        return true;

    return SendToGenerator(cx, JSGENOP_CLOSE, obj, gen, JS::UndefinedHandleValue);
}

JS_ALWAYS_INLINE bool
IsGenerator(const Value &v)
{
    return v.isObject() && v.toObject().is<GeneratorObject>();
}

JS_ALWAYS_INLINE bool
generator_send_impl(JSContext *cx, CallArgs args)
{
    // FIXME: Change assertion to IsLegacyGenerator().
    JS_ASSERT(IsGenerator(args.thisv()));

    RootedObject thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = thisObj->as<GeneratorObject>().getGenerator();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        return js_ThrowStopIteration(cx);
    }

    if (gen->state == JSGEN_NEWBORN && args.hasDefined(0)) {
        RootedValue val(cx, args[0]);
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_SEND,
                            JSDVG_SEARCH_STACK, val, NullPtr());
        return false;
    }

    // FIXME: next() takes the send value as an optional argument in ES6
    // generator objects.
    if (!SendToGenerator(cx, JSGENOP_SEND, thisObj, gen, args.get(0)))
        return false;

    args.rval().set(gen->fp->returnValue());
    return true;
}

bool
generator_send(JSContext *cx, unsigned argc, Value *vp)
{
    // FIXME: send() is only a method on legacy generator objects.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsGenerator, generator_send_impl>(cx, args);
}

JS_ALWAYS_INLINE bool
generator_next_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsGenerator(args.thisv()));

    RootedObject thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = thisObj->as<GeneratorObject>().getGenerator();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        return js_ThrowStopIteration(cx);
    }

    if (!SendToGenerator(cx, JSGENOP_NEXT, thisObj, gen, JS::UndefinedHandleValue))
        return false;

    args.rval().set(gen->fp->returnValue());
    return true;
}

bool
generator_next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsGenerator, generator_next_impl>(cx, args);
}

JS_ALWAYS_INLINE bool
generator_throw_impl(JSContext *cx, CallArgs args)
{
    JS_ASSERT(IsGenerator(args.thisv()));

    RootedObject thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = thisObj->as<GeneratorObject>().getGenerator();
    if (!gen || gen->state == JSGEN_CLOSED) {
        /* This happens when obj is the generator prototype. See bug 352885. */
        cx->setPendingException(args.length() >= 1 ? args[0] : UndefinedValue());
        return false;
    }

    if (!SendToGenerator(cx, JSGENOP_THROW, thisObj, gen, args.get(0)))
        return false;

    args.rval().set(gen->fp->returnValue());
    return true;
}

bool
generator_throw(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsGenerator, generator_throw_impl>(cx, args);
}

JS_ALWAYS_INLINE bool
generator_close_impl(JSContext *cx, CallArgs args)
{
    // FIXME: Change assertion to IsLegacyGenerator().
    JS_ASSERT(IsGenerator(args.thisv()));

    RootedObject thisObj(cx, &args.thisv().toObject());

    JSGenerator *gen = thisObj->as<GeneratorObject>().getGenerator();
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

    if (!SendToGenerator(cx, JSGENOP_CLOSE, thisObj, gen, JS::UndefinedHandleValue))
        return false;

    args.rval().set(gen->fp->returnValue());
    return true;
}

bool
generator_close(JSContext *cx, unsigned argc, Value *vp)
{
    // FIXME: close() is only a method on legacy generator objects.
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsGenerator, generator_close_impl>(cx, args);
}

#define JSPROP_ROPERM   (JSPROP_READONLY | JSPROP_PERMANENT)

static const JSFunctionSpec generator_methods[] = {
    JS_FN("iterator",  iterator_iterator,  0, 0),
    JS_FN("next",      generator_next,     0,JSPROP_ROPERM),
    JS_FN("send",      generator_send,     1,JSPROP_ROPERM),
    JS_FN("throw",     generator_throw,    1,JSPROP_ROPERM),
    JS_FN("close",     generator_close,    0,JSPROP_ROPERM),
    JS_FS_END
};

/* static */ bool
GlobalObject::initIteratorClasses(JSContext *cx, Handle<GlobalObject *> global)
{
    RootedObject iteratorProto(cx);
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
        ni->init(NULL, NULL, 0 /* flags */, 0, 0);

        iteratorProto->as<PropertyIteratorObject>().setNativeIterator(ni);

        Rooted<JSFunction*> ctor(cx);
        ctor = global->createConstructor(cx, IteratorConstructor, cx->names().Iterator, 2);
        if (!ctor)
            return false;
        if (!LinkConstructorAndPrototype(cx, ctor, iteratorProto))
            return false;
        if (!DefinePropertiesAndBrand(cx, iteratorProto, NULL, iterator_methods))
            return false;
        if (!DefineConstructorAndPrototype(cx, global, JSProto_Iterator, ctor, iteratorProto))
            return false;
    }

    RootedObject proto(cx);
    if (global->getSlot(ELEMENT_ITERATOR_PROTO).isUndefined()) {
        Class *cls = &ElementIteratorObject::class_;
        proto = global->createBlankPrototypeInheriting(cx, cls, *iteratorProto);
        if (!proto || !DefinePropertiesAndBrand(cx, proto, NULL, ElementIteratorObject::methods))
            return false;
        global->setReservedSlot(ELEMENT_ITERATOR_PROTO, ObjectValue(*proto));
    }

    if (global->getSlot(GENERATOR_PROTO).isUndefined()) {
        proto = global->createBlankPrototype(cx, &GeneratorObject::class_);
        if (!proto || !DefinePropertiesAndBrand(cx, proto, NULL, generator_methods))
            return false;
        global->setReservedSlot(GENERATOR_PROTO, ObjectValue(*proto));
    }

    if (global->getPrototype(JSProto_StopIteration).isUndefined()) {
        proto = global->createBlankPrototype(cx, &StopIterationObject::class_);
        if (!proto || !JSObject::freeze(cx, proto))
            return false;

        /* This should use a non-JSProtoKey'd slot, but this is easier for now. */
        if (!DefineConstructorAndPrototype(cx, global, JSProto_StopIteration, proto, proto))
            return false;

        MarkStandardClassInitializedNoProto(global, &StopIterationObject::class_);
    }

    return true;
}

JSObject *
js_InitIteratorClasses(JSContext *cx, HandleObject obj)
{
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    if (!GlobalObject::initIteratorClasses(cx, global))
        return NULL;
    return global->getIteratorPrototype();
}
