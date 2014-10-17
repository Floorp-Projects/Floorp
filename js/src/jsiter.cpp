/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript iterators. */

#include "jsiter.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

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
#include "vm/TypedArrayCommon.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/NativeObject-inl.h"
#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

using namespace js;
using namespace js::gc;
using JS::ForOfIterator;

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
    for (HeapPtrFlatString *str = begin(); str < end(); str++)
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
    JS::AutoValueArray<2> vec(cx);
    vec[0].set(IdToValue(id));
    vec[1].set(val);

    JSObject *aobj = NewDenseCopiedArray(cx, 2, vec.begin());
    if (!aobj)
        return false;
    rval.setObject(*aobj);
    return true;
}

static inline bool
Enumerate(JSContext *cx, HandleObject pobj, jsid id,
          bool enumerable, unsigned flags, IdSet& ht, AutoIdVector *props)
{
    // We implement __proto__ using a property on |Object.prototype|, but
    // because __proto__ is highly deserving of removal, we don't want it to
    // show up in property enumeration, even if only for |Object.prototype|
    // (think introspection by Prototype-like frameworks that add methods to
    // the built-in prototypes).  So exclude __proto__ if the object where the
    // property was found has no [[Prototype]] and might be |Object.prototype|.
    if (MOZ_UNLIKELY(!pobj->getTaggedProto().isObject() && JSID_IS_ATOM(id, cx->names().proto)))
        return true;

    if (!(flags & JSITER_OWNONLY) || pobj->is<ProxyObject>() || pobj->getOps()->enumerate) {
        // If we've already seen this, we definitely won't add it.
        IdSet::AddPtr p = ht.lookupForAdd(id);
        if (MOZ_UNLIKELY(!!p))
            return true;

        // It's not necessary to add properties to the hash table at the end of
        // the prototype chain, but custom enumeration behaviors might return
        // duplicated properties, so always add in such cases.
        if ((pobj->is<ProxyObject>() || pobj->getProto() || pobj->getOps()->enumerate) && !ht.add(p, id))
            return false;
    }

    // Symbol-keyed properties and nonenumerable properties are skipped unless
    // the caller specifically asks for them. A caller can also filter out
    // non-symbols by asking for JSITER_SYMBOLSONLY.
    if (JSID_IS_SYMBOL(id) ? !(flags & JSITER_SYMBOLS) : (flags & JSITER_SYMBOLSONLY))
        return true;
    if (!enumerable && !(flags & JSITER_HIDDEN))
        return true;

    return props->append(id);
}

static bool
EnumerateNativeProperties(JSContext *cx, HandleNativeObject pobj, unsigned flags, IdSet &ht,
                          AutoIdVector *props)
{
    bool enumerateSymbols;
    if (flags & JSITER_SYMBOLSONLY) {
        enumerateSymbols = true;
    } else {
        /* Collect any dense elements from this object. */
        size_t initlen = pobj->getDenseInitializedLength();
        const Value *vp = pobj->getDenseElements();
        for (size_t i = 0; i < initlen; ++i, ++vp) {
            if (!vp->isMagic(JS_ELEMENTS_HOLE)) {
                /* Dense arrays never get so large that i would not fit into an integer id. */
                if (!Enumerate(cx, pobj, INT_TO_JSID(i), /* enumerable = */ true, flags, ht, props))
                    return false;
            }
        }

        /* Collect any typed array or shared typed array elements from this object. */
        if (IsAnyTypedArray(pobj)) {
            size_t len = AnyTypedArrayLength(pobj);
            for (size_t i = 0; i < len; i++) {
                if (!Enumerate(cx, pobj, INT_TO_JSID(i), /* enumerable = */ true, flags, ht, props))
                    return false;
            }
        }

        size_t initialLength = props->length();

        /* Collect all unique property names from this object's shape. */
        bool symbolsFound = false;
        Shape::Range<NoGC> r(pobj->lastProperty());
        for (; !r.empty(); r.popFront()) {
            Shape &shape = r.front();
            jsid id = shape.propid();

            if (JSID_IS_SYMBOL(id)) {
                symbolsFound = true;
                continue;
            }

            if (!Enumerate(cx, pobj, id, shape.enumerable(), flags, ht, props))
                return false;
        }
        ::Reverse(props->begin() + initialLength, props->end());

        enumerateSymbols = symbolsFound && (flags & JSITER_SYMBOLS);
    }

    if (enumerateSymbols) {
        // Do a second pass to collect symbols. ES6 draft rev 25 (2014 May 22)
        // 9.1.12 requires that all symbols appear after all strings in the
        // result.
        size_t initialLength = props->length();
        for (Shape::Range<NoGC> r(pobj->lastProperty()); !r.empty(); r.popFront()) {
            Shape &shape = r.front();
            jsid id = shape.propid();
            if (JSID_IS_SYMBOL(id)) {
                if (!Enumerate(cx, pobj, id, shape.enumerable(), flags, ht, props))
                    return false;
            }
        }
        ::Reverse(props->begin() + initialLength, props->end());
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
        // Pick an arbitrary order on jsids that is as stable as possible
        // across executions.
        if (a == b) {
            *lessOrEqualp = true;
            return true;
        }

        size_t ta = JSID_BITS(a) & JSID_TYPE_MASK;
        size_t tb = JSID_BITS(b) & JSID_TYPE_MASK;
        if (ta != tb) {
            *lessOrEqualp = (ta <= tb);
            return true;
        }

        if (JSID_IS_INT(a)) {
            *lessOrEqualp = (JSID_TO_INT(a) <= JSID_TO_INT(b));
            return true;
        }

        RootedString astr(cx), bstr(cx);
        if (JSID_IS_SYMBOL(a)) {
            MOZ_ASSERT(JSID_IS_SYMBOL(b));
            JS::SymbolCode ca = JSID_TO_SYMBOL(a)->code();
            JS::SymbolCode cb = JSID_TO_SYMBOL(b)->code();
            if (ca != cb) {
                *lessOrEqualp = uint32_t(ca) <= uint32_t(cb);
                return true;
            }
            MOZ_ASSERT(ca == JS::SymbolCode::InSymbolRegistry || ca == JS::SymbolCode::UniqueSymbol);
            astr = JSID_TO_SYMBOL(a)->description();
            bstr = JSID_TO_SYMBOL(b)->description();
            if (!astr || !bstr) {
                *lessOrEqualp = !astr;
                return true;
            }

            // Fall through to string comparison on the descriptions. The sort
            // order is nondeterministic if two different unique symbols have
            // the same description.
        } else {
            astr = IdToString(cx, a);
            if (!astr)
                return false;
            bstr = IdToString(cx, b);
            if (!bstr)
                return false;
        }

        int32_t result;
        if (!CompareStrings(cx, astr, bstr, &result))
            return false;

        *lessOrEqualp = (result <= 0);
        return true;
    }
};

#endif /* JS_MORE_DETERMINISTIC */

static bool
Snapshot(JSContext *cx, HandleObject pobj_, unsigned flags, AutoIdVector *props)
{
    IdSet ht(cx);
    if (!ht.init(32))
        return false;

    RootedObject pobj(cx, pobj_);

    do {
        const Class *clasp = pobj->getClass();
        if (pobj->isNative() &&
            !pobj->getOps()->enumerate &&
            !(clasp->flags & JSCLASS_NEW_ENUMERATE))
        {
            if (!clasp->enumerate(cx, pobj.as<NativeObject>()))
                return false;
            if (!EnumerateNativeProperties(cx, pobj.as<NativeObject>(), flags, ht, props))
                return false;
        } else {
            if (pobj->is<ProxyObject>()) {
                AutoIdVector proxyProps(cx);
                if (flags & JSITER_OWNONLY) {
                    if (flags & JSITER_HIDDEN) {
                        // This gets all property keys, both strings and
                        // symbols.  The call to Enumerate in the loop below
                        // will filter out unwanted keys, per the flags.
                        if (!Proxy::ownPropertyKeys(cx, pobj, proxyProps))
                            return false;
                    } else {
                        if (!Proxy::getOwnEnumerablePropertyKeys(cx, pobj, proxyProps))
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

                // Proxy objects enumerate the prototype on their own, so we're
                // done here.
                break;
            }
            RootedValue state(cx);
            RootedId id(cx);
            JSIterateOp op = (flags & JSITER_HIDDEN) ? JSENUMERATE_INIT_ALL : JSENUMERATE_INIT;
            if (!JSObject::enumerate(cx, pobj, op, &state, &id))
                return false;
            if (state.isMagic(JS_NATIVE_ENUMERATE)) {
                if (!EnumerateNativeProperties(cx, pobj.as<NativeObject>(), flags, ht, props))
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

    } while ((pobj = pobj->getProto()) != nullptr);

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
    JSIdArray *ida = reinterpret_cast<JSIdArray *>(cx->zone()->pod_malloc<uint8_t>(sz));
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
js::GetPropertyKeys(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector *props)
{
    return Snapshot(cx, obj,
                    flags & (JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS | JSITER_SYMBOLSONLY),
                    props);
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
                             -1, val, js::NullPtr(), bytes.ptr());
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
        RootedTypeObject type(cx, cx->getNewType(&PropertyIteratorObject::class_, TaggedProto(nullptr)));
        if (!type)
            return nullptr;

        JSObject *metadata = nullptr;
        if (!NewObjectMetadata(cx, &metadata))
            return nullptr;

        const Class *clasp = &PropertyIteratorObject::class_;
        RootedShape shape(cx, EmptyShape::getInitialShape(cx, clasp, TaggedProto(nullptr), nullptr, metadata,
                                                          ITERATOR_FINALIZE_KIND));
        if (!shape)
            return nullptr;

        NativeObject *obj =
            MaybeNativeObject(JSObject::create(cx, ITERATOR_FINALIZE_KIND,
                                               GetInitialHeap(GenericObject, clasp), shape, type));
        if (!obj)
            return nullptr;

        MOZ_ASSERT(obj->numFixedSlots() == JSObject::ITER_CLASS_NFIXED_SLOTS);
        return &obj->as<PropertyIteratorObject>();
    }

    JSObject *obj = NewBuiltinClassInstance(cx, &PropertyIteratorObject::class_);
    if (!obj)
        return nullptr;

    return &obj->as<PropertyIteratorObject>();
}

NativeIterator *
NativeIterator::allocateIterator(JSContext *cx, uint32_t slength, const AutoIdVector &props)
{
    size_t plength = props.length();
    NativeIterator *ni = cx->zone()->pod_malloc_with_extra<NativeIterator, void *>(plength + slength);
    if (!ni)
        return nullptr;

    AutoValueVector strings(cx);
    ni->props_array = ni->props_cursor = reinterpret_cast<HeapPtrFlatString *>(ni + 1);
    ni->props_end = ni->props_array + plength;
    if (plength) {
        for (size_t i = 0; i < plength; i++) {
            JSFlatString *str = IdToString(cx, props[i]);
            if (!str || !strings.append(StringValue(str)))
                return nullptr;
            ni->props_array[i].init(str);
        }
    }
    ni->next_ = nullptr;
    ni->prev_ = nullptr;
    return ni;
}

NativeIterator *
NativeIterator::allocateSentinel(JSContext *cx)
{
    NativeIterator *ni = js_pod_malloc<NativeIterator>();
    if (!ni)
        return nullptr;

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

        MOZ_ASSERT(!(ni->flags & JSITER_ACTIVE));
        ni->flags |= JSITER_ACTIVE;
    }
}

static inline bool
VectorToKeyIterator(JSContext *cx, HandleObject obj, unsigned flags, AutoIdVector &keys,
                    uint32_t slength, uint32_t key, MutableHandleValue vp)
{
    MOZ_ASSERT(!(flags & JSITER_FOREACH));

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
        MOZ_ASSERT(ind == slength);
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
    MOZ_ASSERT(flags & JSITER_FOREACH);

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
    Vector<Shape *, 8> shapes(cx);
    uint32_t key = 0;

    bool keysOnly = (flags == JSITER_ENUMERATE);

    if (obj) {
        if (JSIteratorOp op = obj->getClass()->ext.iteratorObject) {
            JSObject *iterobj = op(cx, obj, !(flags & JSITER_FOREACH));
            if (!iterobj)
                return false;
            vp.setObject(*iterobj);
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
                    obj->as<NativeObject>().hasEmptyElements() &&
                    obj->lastProperty() == lastni->shapes_array[0])
                {
                    JSObject *proto = obj->getProto();
                    if (proto->isNative() &&
                        proto->as<NativeObject>().hasEmptyElements() &&
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
                        !pobj->as<NativeObject>().hasEmptyElements() ||
                        IsAnyTypedArray(pobj) ||
                        pobj->hasUncacheableProto() ||
                        pobj->getOps()->enumerate ||
                        pobj->getClass()->enumerate != JS_EnumerateStub ||
                        pobj->as<NativeObject>().containsPure(cx->names().iteratorIntrinsic))
                    {
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
        if (obj->is<ProxyObject>())
            return Proxy::iterate(cx, obj, flags, vp);

        if (!GetCustomIterator(cx, obj, flags, vp))
            return false;
        if (!vp.isUndefined())
            return true;
    }

    /* NB: for (var p in null) succeeds by iterating over no properties. */

    AutoIdVector keys(cx);
    if (flags & JSITER_FOREACH) {
        if (MOZ_LIKELY(obj != nullptr) && !Snapshot(cx, obj, flags, &keys))
            return false;
        MOZ_ASSERT(shapes.empty());
        if (!VectorToValueIterator(cx, obj, flags, keys, vp))
            return false;
    } else {
        if (MOZ_LIKELY(obj != nullptr) && !Snapshot(cx, obj, flags, &keys))
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
        return nullptr;
    return &value.toObject();
}

JSObject *
js::CreateItrResultObject(JSContext *cx, HandleValue value, bool done)
{
    // FIXME: We can cache the iterator result object shape somewhere.
    AssertHeapIsIdle(cx);

    RootedObject proto(cx, cx->global()->getOrCreateObjectPrototype(cx));
    if (!proto)
        return nullptr;

    RootedObject obj(cx, NewObjectWithGivenProto(cx, &JSObject::class_, proto, cx->global()));
    if (!obj)
        return nullptr;

    if (!JSObject::defineProperty(cx, obj, cx->names().value, value))
        return nullptr;

    RootedValue doneBool(cx, BooleanValue(done));
    if (!JSObject::defineProperty(cx, obj, cx->names().done, doneBool))
        return nullptr;

    return obj;
}

bool
js::ThrowStopIteration(JSContext *cx)
{
    MOZ_ASSERT(!JS_IsExceptionPending(cx));

    // StopIteration isn't a constructor, but it's stored in GlobalObject
    // as one, out of laziness. Hence the GetBuiltinConstructor call here.
    RootedObject ctor(cx);
    if (GetBuiltinConstructor(cx, JSProto_StopIteration, &ctor))
        cx->setPendingException(ObjectValue(*ctor));
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

MOZ_ALWAYS_INLINE bool
IsIterator(HandleValue v)
{
    return v.isObject() && v.toObject().hasClass(&PropertyIteratorObject::class_);
}

MOZ_ALWAYS_INLINE bool
iterator_next_impl(JSContext *cx, CallArgs args)
{
    MOZ_ASSERT(IsIterator(args.thisv()));

    RootedObject thisObj(cx, &args.thisv().toObject());

    if (!IteratorMore(cx, thisObj, args.rval()))
        return false;

    if (args.rval().isMagic(JS_NO_ITER_VALUE)) {
        ThrowStopIteration(cx);
        return false;
    }

    return true;
}

static bool
iterator_next(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsIterator, iterator_next_impl>(cx, args);
}

static const JSFunctionSpec iterator_methods[] = {
    JS_SELF_HOSTED_FN("@@iterator", "LegacyIteratorShim", 0, 0),
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

const Class PropertyIteratorObject::class_ = {
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
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    trace,
    JS_NULL_CLASS_SPEC,
    {
        nullptr,             /* outerObject    */
        nullptr,             /* innerObject    */
        iterator_iteratorObject,
    }
};

enum {
    ArrayIteratorSlotIteratedObject,
    ArrayIteratorSlotNextIndex,
    ArrayIteratorSlotItemKind,
    ArrayIteratorSlotCount
};

const Class ArrayIteratorObject::class_ = {
    "Array Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(ArrayIteratorSlotCount),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr                  /* finalize    */
};

static const JSFunctionSpec array_iterator_methods[] = {
    JS_SELF_HOSTED_FN("@@iterator", "ArrayIteratorIdentity", 0, 0),
    JS_SELF_HOSTED_FN("next", "ArrayIteratorNext", 0, 0),
    JS_FS_END
};

static const Class StringIteratorPrototypeClass = {
    "String Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS,
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr                  /* finalize    */
};

enum {
    StringIteratorSlotIteratedObject,
    StringIteratorSlotNextIndex,
    StringIteratorSlotCount
};

const Class StringIteratorObject::class_ = {
    "String Iterator",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(StringIteratorSlotCount),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr                  /* finalize    */
};

static const JSFunctionSpec string_iterator_methods[] = {
    JS_SELF_HOSTED_FN("@@iterator", "StringIteratorIdentity", 0, 0),
    JS_SELF_HOSTED_FN("next", "StringIteratorNext", 0, 0),
    JS_FS_END
};

bool
js::ValueToIterator(JSContext *cx, unsigned flags, MutableHandleValue vp)
{
    /* JSITER_KEYVALUE must always come with JSITER_FOREACH */
    MOZ_ASSERT_IF(flags & JSITER_KEYVALUE, flags & JSITER_FOREACH);

    RootedObject obj(cx);
    if (vp.isObject()) {
        /* Common case. */
        obj = &vp.toObject();
    } else {
        /*
         * Enumerating over null and undefined gives an empty enumerator, so
         * that |for (var p in <null or undefined>) <loop>;| never executes
         * <loop>, per ES5 12.6.4.
         */
        if (!(flags & JSITER_ENUMERATE) || !vp.isNullOrUndefined()) {
            obj = ToObject(cx, vp);
            if (!obj)
                return false;
        }
    }

    return GetIterator(cx, obj, flags, vp);
}

bool
js::CloseIterator(JSContext *cx, HandleObject obj)
{
    if (obj->is<PropertyIteratorObject>()) {
        /* Remove enumerators from the active list, which is a stack. */
        NativeIterator *ni = obj->as<PropertyIteratorObject>().getNativeIterator();

        if (ni->flags & JSITER_ENUMERATE) {
            ni->unlink();

            MOZ_ASSERT(ni->flags & JSITER_ACTIVE);
            ni->flags &= ~JSITER_ACTIVE;

            /*
             * Reset the enumerator; it may still be in the cached iterators
             * for this thread, and can be reused.
             */
            ni->props_cursor = ni->props_array;
        }
    } else if (obj->is<LegacyGeneratorObject>()) {
        Rooted<LegacyGeneratorObject*> genObj(cx, &obj->as<LegacyGeneratorObject>());
        if (genObj->isClosed())
            return true;
        if (genObj->isRunning() || genObj->isClosing()) {
            // Nothing sensible to do.
            return true;
        }
        return LegacyGeneratorObject::close(cx, obj);
    }
    return true;
}

bool
js::UnwindIteratorForException(JSContext *cx, HandleObject obj)
{
    RootedValue v(cx);
    bool getOk = cx->getPendingException(&v);
    cx->clearPendingException();
    if (!CloseIterator(cx, obj))
        return false;
    if (!getOk)
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
            HeapPtrFlatString *props_cursor = ni->current();
            HeapPtrFlatString *props_end = ni->end();
            for (HeapPtrFlatString *idp = props_cursor; idp < props_end; ++idp) {
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
                                attrs = GetShapeAttributes(obj2, prop);
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
                        for (HeapPtrFlatString *p = idp; p + 1 != props_end; p++)
                            *p = *(p + 1);
                        ni->props_end = ni->end() - 1;

                        /*
                         * This invokes the pre barrier on this element, since
                         * it's no longer going to be marked, and ensures that
                         * any existing remembered set entry will be dropped.
                         */
                        *ni->props_end = nullptr;
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

namespace {

class SingleStringPredicate {
    Handle<JSFlatString*> str;
public:
    explicit SingleStringPredicate(Handle<JSFlatString*> str) : str(str) {}

    bool operator()(JSFlatString *str) { return EqualStrings(str, this->str); }
    bool matchesAtMostOne() { return true; }
};

} /* anonymous namespace */

bool
js::SuppressDeletedProperty(JSContext *cx, HandleObject obj, jsid id)
{
    if (JSID_IS_SYMBOL(id))
        return true;

    Rooted<JSFlatString*> str(cx, IdToString(cx, id));
    if (!str)
        return false;
    return SuppressDeletedPropertyHelper(cx, obj, SingleStringPredicate(str));
}

bool
js::SuppressDeletedElement(JSContext *cx, HandleObject obj, uint32_t index)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, &id))
        return false;
    return SuppressDeletedProperty(cx, obj, id);
}

namespace {

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

} /* anonymous namespace */

bool
js::SuppressDeletedElements(JSContext *cx, HandleObject obj, uint32_t begin, uint32_t end)
{
    return SuppressDeletedPropertyHelper(cx, obj, IndexRangePredicate(begin, end));
}

bool
js::IteratorMore(JSContext *cx, HandleObject iterobj, MutableHandleValue rval)
{
    /* Fast path for native iterators */
    NativeIterator *ni = nullptr;
    if (iterobj->is<PropertyIteratorObject>()) {
        /* Key iterators are handled by fast-paths. */
        ni = iterobj->as<PropertyIteratorObject>().getNativeIterator();
        if (ni->props_cursor >= ni->props_end) {
            rval.setMagic(JS_NO_ITER_VALUE);
            return true;
        }
        if (ni->isKeyIter()) {
            rval.setString(*ni->current());
            ni->incCursor();
            return true;
        }
    }

    /* We're reentering below and can call anything. */
    JS_CHECK_RECURSION(cx, return false);

    /* Fetch and cache the next value from the iterator. */
    if (ni) {
        MOZ_ASSERT(!ni->isKeyIter());
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
        return true;
    }

    /* Call the iterator object's .next method. */
    if (!JSObject::getProperty(cx, iterobj, iterobj, cx->names().next, rval))
        return false;
    if (!Invoke(cx, ObjectValue(*iterobj), rval, 0, nullptr, rval)) {
        /* Check for StopIteration. */
        if (!cx->isExceptionPending())
            return false;
        RootedValue exception(cx);
        if (!cx->getPendingException(&exception))
            return false;
        if (!JS_IsStopIteration(exception))
            return false;

        cx->clearPendingException();
        rval.setMagic(JS_NO_ITER_VALUE);
        return true;
    }

    return true;
}

static bool
stopiter_hasInstance(JSContext *cx, HandleObject obj, MutableHandleValue v, bool *bp)
{
    *bp = JS_IsStopIteration(v);
    return true;
}

const Class StopIterationObject::class_ = {
    "StopIteration",
    JSCLASS_HAS_CACHED_PROTO(JSProto_StopIteration),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,                 /* finalize    */
    nullptr,                 /* call        */
    stopiter_hasInstance,
    nullptr                  /* construct   */
};

bool
ForOfIterator::init(HandleValue iterable, NonIterableBehavior nonIterableBehavior)
{
    JSContext *cx = cx_;
    RootedObject iterableObj(cx, ToObject(cx, iterable));
    if (!iterableObj)
        return false;

    MOZ_ASSERT(index == NOT_ARRAY);

    // Check the PIC first for a match.
    if (iterableObj->is<ArrayObject>()) {
        ForOfPIC::Chain *stubChain = ForOfPIC::getOrCreate(cx);
        if (!stubChain)
            return false;

        bool optimized;
        if (!stubChain->tryOptimizeArray(cx, iterableObj.as<ArrayObject>(), &optimized))
            return false;

        if (optimized) {
            // Got optimized stub.  Array is optimizable.
            index = 0;
            iterator = iterableObj;
            return true;
        }
    }

    MOZ_ASSERT(index == NOT_ARRAY);

    // The iterator is the result of calling obj[@@iterator]().
    InvokeArgs args(cx);
    if (!args.init(0))
        return false;
    args.setThis(ObjectValue(*iterableObj));

    RootedValue callee(cx);
    if (!JSObject::getProperty(cx, iterableObj, iterableObj, cx->names().std_iterator, &callee))
        return false;

    // If obj[@@iterator] is undefined and we were asked to allow non-iterables,
    // bail out now without setting iterator.  This will make valueIsIterable(),
    // which our caller should check, return false.
    if (nonIterableBehavior == AllowNonIterable && callee.isUndefined())
        return true;

    // Throw if obj[@@iterator] isn't callable.
    // js::Invoke is about to check for this kind of error anyway, but it would
    // throw an inscrutable error message about |method| rather than this nice
    // one about |obj|.
    if (!callee.isObject() || !callee.toObject().isCallable()) {
        char *bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, iterable, NullPtr());
        if (!bytes)
            return false;
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_NOT_ITERABLE, bytes);
        js_free(bytes);
        return false;
    }

    args.setCallee(callee);
    if (!Invoke(cx, args))
        return false;

    iterator = ToObject(cx, args.rval());
    if (!iterator)
        return false;

    return true;
}

bool
ForOfIterator::initWithIterator(HandleValue aIterator)
{
    JSContext *cx = cx_;
    RootedObject iteratorObj(cx, ToObject(cx, aIterator));
    return iterator = iteratorObj;
}

inline bool
ForOfIterator::nextFromOptimizedArray(MutableHandleValue vp, bool *done)
{
    MOZ_ASSERT(index != NOT_ARRAY);

    if (!CheckForInterrupt(cx_))
        return false;

    ArrayObject *arr = &iterator->as<ArrayObject>();

    if (index >= arr->length()) {
        vp.setUndefined();
        *done = true;
        return true;
    }
    *done = false;

    // Try to get array element via direct access.
    if (index < arr->getDenseInitializedLength()) {
        vp.set(arr->getDenseElement(index));
        if (!vp.isMagic(JS_ELEMENTS_HOLE)) {
            ++index;
            return true;
        }
    }

    return JSObject::getElement(cx_, iterator, iterator, index++, vp);
}

bool
ForOfIterator::next(MutableHandleValue vp, bool *done)
{
    MOZ_ASSERT(iterator);

    if (index != NOT_ARRAY) {
        ForOfPIC::Chain *stubChain = ForOfPIC::getOrCreate(cx_);
        if (!stubChain)
            return false;

        if (stubChain->isArrayNextStillSane())
            return nextFromOptimizedArray(vp, done);

        // ArrayIterator.prototype.next changed, materialize a proper
        // ArrayIterator instance and fall through to slowpath case.
        if (!materializeArrayIterator())
            return false;
    }

    RootedValue method(cx_);
    if (!JSObject::getProperty(cx_, iterator, iterator, cx_->names().next, &method))
        return false;

    InvokeArgs args(cx_);
    if (!args.init(1))
        return false;
    args.setCallee(method);
    args.setThis(ObjectValue(*iterator));
    args[0].setUndefined();
    if (!Invoke(cx_, args))
        return false;

    RootedObject resultObj(cx_, ToObject(cx_, args.rval()));
    if (!resultObj)
        return false;
    RootedValue doneVal(cx_);
    if (!JSObject::getProperty(cx_, resultObj, resultObj, cx_->names().done, &doneVal))
        return false;
    *done = ToBoolean(doneVal);
    if (*done) {
        vp.setUndefined();
        return true;
    }
    return JSObject::getProperty(cx_, resultObj, resultObj, cx_->names().value, vp);
}

bool
ForOfIterator::materializeArrayIterator()
{
    MOZ_ASSERT(index != NOT_ARRAY);

    const char *nameString = "ArrayValuesAt";

    RootedAtom name(cx_, Atomize(cx_, nameString, strlen(nameString)));
    if (!name)
        return false;

    RootedValue val(cx_);
    if (!cx_->global()->getSelfHostedFunction(cx_, name, name, 1, &val))
        return false;

    InvokeArgs args(cx_);
    if (!args.init(1))
        return false;
    args.setCallee(val);
    args.setThis(ObjectValue(*iterator));
    args[0].set(Int32Value(index));
    if (!Invoke(cx_, args))
        return false;

    index = NOT_ARRAY;
    // Result of call to ArrayValuesAt must be an object.
    iterator = &args.rval().toObject();
    return true;
}

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
        ni->init(nullptr, nullptr, 0 /* flags */, 0, 0);

        iteratorProto->as<PropertyIteratorObject>().setNativeIterator(ni);

        Rooted<JSFunction*> ctor(cx);
        ctor = global->createConstructor(cx, IteratorConstructor, cx->names().Iterator, 2);
        if (!ctor)
            return false;
        if (!LinkConstructorAndPrototype(cx, ctor, iteratorProto))
            return false;
        if (!DefinePropertiesAndFunctions(cx, iteratorProto, nullptr, iterator_methods))
            return false;
        if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_Iterator,
                                                  ctor, iteratorProto))
        {
            return false;
        }
    }

    RootedObject proto(cx);
    if (global->getSlot(ARRAY_ITERATOR_PROTO).isUndefined()) {
        const Class *cls = &ArrayIteratorObject::class_;
        proto = global->createBlankPrototypeInheriting(cx, cls, *iteratorProto);
        if (!proto || !DefinePropertiesAndFunctions(cx, proto, nullptr, array_iterator_methods))
            return false;
        global->setReservedSlot(ARRAY_ITERATOR_PROTO, ObjectValue(*proto));
    }

    if (global->getSlot(STRING_ITERATOR_PROTO).isUndefined()) {
        const Class *cls = &StringIteratorPrototypeClass;
        proto = global->createBlankPrototype(cx, cls);
        if (!proto || !DefinePropertiesAndFunctions(cx, proto, nullptr, string_iterator_methods))
            return false;
        global->setReservedSlot(STRING_ITERATOR_PROTO, ObjectValue(*proto));
    }

    return GlobalObject::initStopIterationClass(cx, global);
}

/* static */ bool
GlobalObject::initStopIterationClass(JSContext *cx, Handle<GlobalObject *> global)
{
    if (!global->getPrototype(JSProto_StopIteration).isUndefined())
        return true;

    RootedObject proto(cx, global->createBlankPrototype(cx, &StopIterationObject::class_));
    if (!proto || !JSObject::freeze(cx, proto))
        return false;

    // This should use a non-JSProtoKey'd slot, but this is easier for now.
    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_StopIteration, proto, proto))
        return false;

    global->setConstructor(JSProto_StopIteration, ObjectValue(*proto));

    return true;
}

JSObject *
js_InitIteratorClasses(JSContext *cx, HandleObject obj)
{
    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());
    if (!GlobalObject::initIteratorClasses(cx, global))
        return nullptr;
    if (!GlobalObject::initGeneratorClasses(cx, global))
        return nullptr;
    return global->getIteratorPrototype();
}
