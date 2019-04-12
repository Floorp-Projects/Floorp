/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript iterators. */

#include "vm/Iteration.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Likely.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Unused.h"

#include <algorithm>
#include <new>

#include "jstypes.h"
#include "jsutil.h"

#include "builtin/Array.h"
#include "builtin/SelfHostingDefines.h"
#include "ds/Sort.h"
#include "gc/FreeOp.h"
#include "gc/Marking.h"
#include "js/PropertySpec.h"
#include "js/Proxy.h"
#include "vm/BytecodeUtil.h"
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/JSAtom.h"
#include "vm/JSContext.h"
#include "vm/JSObject.h"
#include "vm/JSScript.h"
#include "vm/Shape.h"
#include "vm/TypedArrayObject.h"

#include "vm/Compartment-inl.h"
#include "vm/JSScript-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/ReceiverGuard-inl.h"
#include "vm/Stack-inl.h"
#include "vm/StringType-inl.h"

using namespace js;

using mozilla::ArrayEqual;
using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::PodCopy;

typedef Rooted<PropertyIteratorObject*> RootedPropertyIteratorObject;

static const gc::AllocKind ITERATOR_FINALIZE_KIND =
    gc::AllocKind::OBJECT2_BACKGROUND;

// Beware!  This function may have to trace incompletely-initialized
// |NativeIterator| allocations if the |IdToString| in that constructor recurs
// into this code.
void NativeIterator::trace(JSTracer* trc) {
  TraceNullableEdge(trc, &objectBeingIterated_, "objectBeingIterated_");

  // The SuppressDeletedPropertyHelper loop can GC, so make sure that if the
  // GC removes any elements from the list, it won't remove this one.
  if (iterObj_) {
    TraceManuallyBarrieredEdge(trc, &iterObj_, "iterObj");
  }

  // The limits below are correct at every instant of |NativeIterator|
  // initialization, with the end-pointer incremented as each new guard is
  // created, so they're safe to use here.
  std::for_each(guardsBegin(), guardsEnd(),
                [trc](HeapReceiverGuard& guard) { guard.trace(trc); });

  // But as properties must be created *before* guards, |propertiesBegin()|
  // that depends on |guardsEnd()| having its final value can't safely be
  // used.  Until this is fully initialized, use |propertyCursor_| instead,
  // which points at the start of properties even in partially initialized
  // |NativeIterator|s.  (|propertiesEnd()| is safe at all times with respect
  // to the properly-chosen beginning.)
  //
  // Note that we must trace all properties (not just those not yet visited,
  // or just visited, due to |NativeIterator::previousPropertyWas|) for
  // |NativeIterator|s to be reusable.
  GCPtrFlatString* begin =
      MOZ_LIKELY(isInitialized()) ? propertiesBegin() : propertyCursor_;
  std::for_each(begin, propertiesEnd(), [trc](GCPtrFlatString& prop) {
    // Properties begin life non-null and never *become*
    // null.  (Deletion-suppression will shift trailing
    // properties over a deleted property in the properties
    // array, but it doesn't null them out.)
    TraceEdge(trc, &prop, "prop");
  });
}

using IdSet = GCHashSet<jsid, DefaultHasher<jsid>>;

template <bool CheckForDuplicates>
static inline bool Enumerate(JSContext* cx, HandleObject pobj, jsid id,
                             bool enumerable, unsigned flags,
                             MutableHandle<IdSet> visited,
                             MutableHandleIdVector props) {
  if (CheckForDuplicates) {
    // If we've already seen this, we definitely won't add it.
    IdSet::AddPtr p = visited.lookupForAdd(id);
    if (MOZ_UNLIKELY(!!p)) {
      return true;
    }

    // It's not necessary to add properties to the hash set at the end of
    // the prototype chain, but custom enumeration behaviors might return
    // duplicated properties, so always add in such cases.
    if (pobj->is<ProxyObject>() || pobj->staticPrototype() ||
        pobj->getClass()->getNewEnumerate()) {
      if (!visited.add(p, id)) {
        return false;
      }
    }
  }

  if (!enumerable && !(flags & JSITER_HIDDEN)) {
    return true;
  }

  // Symbol-keyed properties and nonenumerable properties are skipped unless
  // the caller specifically asks for them. A caller can also filter out
  // non-symbols by asking for JSITER_SYMBOLSONLY.
  if (JSID_IS_SYMBOL(id) ? !(flags & JSITER_SYMBOLS)
                         : (flags & JSITER_SYMBOLSONLY)) {
    return true;
  }

  return props.append(id);
}

static bool EnumerateExtraProperties(JSContext* cx, HandleObject obj,
                                     unsigned flags,
                                     MutableHandle<IdSet> visited,
                                     MutableHandleIdVector props) {
  MOZ_ASSERT(obj->getClass()->getNewEnumerate());

  RootedIdVector properties(cx);
  bool enumerableOnly = !(flags & JSITER_HIDDEN);
  if (!obj->getClass()->getNewEnumerate()(cx, obj, &properties,
                                          enumerableOnly)) {
    return false;
  }

  RootedId id(cx);
  for (size_t n = 0; n < properties.length(); n++) {
    id = properties[n];

    // The enumerate hook does not indicate whether the properties
    // it returns are enumerable or not. Since we already passed
    // `enumerableOnly` to the hook to filter out non-enumerable
    // properties, it doesn't really matter what we pass here.
    bool enumerable = true;
    if (!Enumerate<true>(cx, obj, id, enumerable, flags, visited, props)) {
      return false;
    }
  }

  return true;
}

static bool SortComparatorIntegerIds(jsid a, jsid b, bool* lessOrEqualp) {
  uint32_t indexA, indexB;
  MOZ_ALWAYS_TRUE(IdIsIndex(a, &indexA));
  MOZ_ALWAYS_TRUE(IdIsIndex(b, &indexB));
  *lessOrEqualp = (indexA <= indexB);
  return true;
}

template <bool CheckForDuplicates>
static bool EnumerateNativeProperties(JSContext* cx, HandleNativeObject pobj,
                                      unsigned flags,
                                      MutableHandle<IdSet> visited,
                                      MutableHandleIdVector props) {
  bool enumerateSymbols;
  if (flags & JSITER_SYMBOLSONLY) {
    enumerateSymbols = true;
  } else {
    // Collect any dense elements from this object.
    size_t firstElemIndex = props.length();
    size_t initlen = pobj->getDenseInitializedLength();
    const Value* vp = pobj->getDenseElements();
    bool hasHoles = false;
    for (size_t i = 0; i < initlen; ++i, ++vp) {
      if (vp->isMagic(JS_ELEMENTS_HOLE)) {
        hasHoles = true;
      } else {
        // Dense arrays never get so large that i would not fit into an
        // integer id.
        if (!Enumerate<CheckForDuplicates>(cx, pobj, INT_TO_JSID(i),
                                           /* enumerable = */ true, flags,
                                           visited, props)) {
          return false;
        }
      }
    }

    // Collect any typed array or shared typed array elements from this
    // object.
    if (pobj->is<TypedArrayObject>()) {
      size_t len = pobj->as<TypedArrayObject>().length();
      for (size_t i = 0; i < len; i++) {
        if (!Enumerate<CheckForDuplicates>(cx, pobj, INT_TO_JSID(i),
                                           /* enumerable = */ true, flags,
                                           visited, props)) {
          return false;
        }
      }
    }

    // Collect any sparse elements from this object.
    bool isIndexed = pobj->isIndexed();
    if (isIndexed) {
      // If the dense elements didn't have holes, we don't need to include
      // them in the sort.
      if (!hasHoles) {
        firstElemIndex = props.length();
      }

      for (Shape::Range<NoGC> r(pobj->lastProperty()); !r.empty();
           r.popFront()) {
        Shape& shape = r.front();
        jsid id = shape.propid();
        uint32_t dummy;
        if (IdIsIndex(id, &dummy)) {
          if (!Enumerate<CheckForDuplicates>(cx, pobj, id, shape.enumerable(),
                                             flags, visited, props)) {
            return false;
          }
        }
      }

      MOZ_ASSERT(firstElemIndex <= props.length());

      jsid* ids = props.begin() + firstElemIndex;
      size_t n = props.length() - firstElemIndex;

      RootedIdVector tmp(cx);
      if (!tmp.resize(n)) {
        return false;
      }
      PodCopy(tmp.begin(), ids, n);

      if (!MergeSort(ids, n, tmp.begin(), SortComparatorIntegerIds)) {
        return false;
      }
    }

    size_t initialLength = props.length();

    /* Collect all unique property names from this object's shape. */
    bool symbolsFound = false;
    Shape::Range<NoGC> r(pobj->lastProperty());
    for (; !r.empty(); r.popFront()) {
      Shape& shape = r.front();
      jsid id = shape.propid();

      if (JSID_IS_SYMBOL(id)) {
        symbolsFound = true;
        continue;
      }

      uint32_t dummy;
      if (isIndexed && IdIsIndex(id, &dummy)) {
        continue;
      }

      if (!Enumerate<CheckForDuplicates>(cx, pobj, id, shape.enumerable(),
                                         flags, visited, props)) {
        return false;
      }
    }
    ::Reverse(props.begin() + initialLength, props.end());

    enumerateSymbols = symbolsFound && (flags & JSITER_SYMBOLS);
  }

  if (enumerateSymbols) {
    // Do a second pass to collect symbols. ES6 draft rev 25 (2014 May 22)
    // 9.1.12 requires that all symbols appear after all strings in the
    // result.
    size_t initialLength = props.length();
    for (Shape::Range<NoGC> r(pobj->lastProperty()); !r.empty(); r.popFront()) {
      Shape& shape = r.front();
      jsid id = shape.propid();
      if (JSID_IS_SYMBOL(id)) {
        if (!Enumerate<CheckForDuplicates>(cx, pobj, id, shape.enumerable(),
                                           flags, visited, props)) {
          return false;
        }
      }
    }
    ::Reverse(props.begin() + initialLength, props.end());
  }

  return true;
}

static bool EnumerateNativeProperties(JSContext* cx, HandleNativeObject pobj,
                                      unsigned flags,
                                      MutableHandle<IdSet> visited,
                                      MutableHandleIdVector props,
                                      bool checkForDuplicates) {
  if (checkForDuplicates) {
    return EnumerateNativeProperties<true>(cx, pobj, flags, visited, props);
  }
  return EnumerateNativeProperties<false>(cx, pobj, flags, visited, props);
}

template <bool CheckForDuplicates>
static bool EnumerateProxyProperties(JSContext* cx, HandleObject pobj,
                                     unsigned flags,
                                     MutableHandle<IdSet> visited,
                                     MutableHandleIdVector props) {
  MOZ_ASSERT(pobj->is<ProxyObject>());

  RootedIdVector proxyProps(cx);

  if (flags & JSITER_HIDDEN || flags & JSITER_SYMBOLS) {
    // This gets all property keys, both strings and symbols. The call to
    // Enumerate in the loop below will filter out unwanted keys, per the
    // flags.
    if (!Proxy::ownPropertyKeys(cx, pobj, &proxyProps)) {
      return false;
    }

    Rooted<PropertyDescriptor> desc(cx);
    for (size_t n = 0, len = proxyProps.length(); n < len; n++) {
      bool enumerable = false;

      // We need to filter, if the caller just wants enumerable symbols.
      if (!(flags & JSITER_HIDDEN)) {
        if (!Proxy::getOwnPropertyDescriptor(cx, pobj, proxyProps[n], &desc)) {
          return false;
        }
        enumerable = desc.enumerable();
      }

      if (!Enumerate<CheckForDuplicates>(cx, pobj, proxyProps[n], enumerable,
                                         flags, visited, props)) {
        return false;
      }
    }

    return true;
  }

  // Returns enumerable property names (no symbols).
  if (!Proxy::getOwnEnumerablePropertyKeys(cx, pobj, &proxyProps)) {
    return false;
  }

  for (size_t n = 0, len = proxyProps.length(); n < len; n++) {
    if (!Enumerate<CheckForDuplicates>(cx, pobj, proxyProps[n], true, flags,
                                       visited, props)) {
      return false;
    }
  }

  return true;
}

#ifdef JS_MORE_DETERMINISTIC

struct SortComparatorIds {
  JSContext* const cx;

  SortComparatorIds(JSContext* cx) : cx(cx) {}

  bool operator()(jsid a, jsid b, bool* lessOrEqualp) {
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
      MOZ_ASSERT(ca == JS::SymbolCode::InSymbolRegistry ||
                 ca == JS::SymbolCode::UniqueSymbol);
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
      if (!astr) {
        return false;
      }
      bstr = IdToString(cx, b);
      if (!bstr) {
        return false;
      }
    }

    int32_t result;
    if (!CompareStrings(cx, astr, bstr, &result)) {
      return false;
    }

    *lessOrEqualp = (result <= 0);
    return true;
  }
};

#endif /* JS_MORE_DETERMINISTIC */

static bool Snapshot(JSContext* cx, HandleObject pobj_, unsigned flags,
                     MutableHandleIdVector props) {
  Rooted<IdSet> visited(cx, IdSet(cx));
  RootedObject pobj(cx, pobj_);

  // Don't check for duplicates if we're only interested in own properties.
  // This does the right thing for most objects: native objects don't have
  // duplicate property ids and we allow the [[OwnPropertyKeys]] proxy trap to
  // return duplicates.
  //
  // The only special case is when the object has a newEnumerate hook: it
  // can return duplicate properties and we have to filter them. This is
  // handled below.
  bool checkForDuplicates = !(flags & JSITER_OWNONLY);

  do {
    if (pobj->getClass()->getNewEnumerate()) {
      if (!EnumerateExtraProperties(cx, pobj, flags, &visited, props)) {
        return false;
      }

      if (pobj->isNative()) {
        if (!EnumerateNativeProperties(cx, pobj.as<NativeObject>(), flags,
                                       &visited, props, true)) {
          return false;
        }
      }

    } else if (pobj->isNative()) {
      // Give the object a chance to resolve all lazy properties
      if (JSEnumerateOp enumerate = pobj->getClass()->getEnumerate()) {
        if (!enumerate(cx, pobj.as<NativeObject>())) {
          return false;
        }
      }
      if (!EnumerateNativeProperties(cx, pobj.as<NativeObject>(), flags,
                                     &visited, props, checkForDuplicates)) {
        return false;
      }
    } else if (pobj->is<ProxyObject>()) {
      if (checkForDuplicates) {
        if (!EnumerateProxyProperties<true>(cx, pobj, flags, &visited, props)) {
          return false;
        }
      } else {
        if (!EnumerateProxyProperties<false>(cx, pobj, flags, &visited,
                                             props)) {
          return false;
        }
      }
    } else {
      MOZ_CRASH("non-native objects must have an enumerate op");
    }

    if (flags & JSITER_OWNONLY) {
      break;
    }

    if (!GetPrototype(cx, pobj, &pobj)) {
      return false;
    }

    // The [[Prototype]] chain might be cyclic.
    if (!CheckForInterrupt(cx)) {
      return false;
    }
  } while (pobj != nullptr);

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

  jsid* ids = props.begin();
  size_t n = props.length();

  RootedIdVector tmp(cx);
  if (!tmp.resize(n)) {
    return false;
  }
  PodCopy(tmp.begin(), ids, n);

  if (!MergeSort(ids, n, tmp.begin(), SortComparatorIds(cx))) {
    return false;
  }

#endif /* JS_MORE_DETERMINISTIC */

  return true;
}

JS_FRIEND_API bool js::GetPropertyKeys(JSContext* cx, HandleObject obj,
                                       unsigned flags,
                                       MutableHandleIdVector props) {
  return Snapshot(cx, obj,
                  flags & (JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS |
                           JSITER_SYMBOLSONLY),
                  props);
}

static inline void RegisterEnumerator(ObjectRealm& realm, NativeIterator* ni) {
  // Register non-escaping native enumerators (for-in) with the current
  // context.
  ni->link(realm.enumerators);

  MOZ_ASSERT(!ni->isActive());
  ni->markActive();
}

static PropertyIteratorObject* NewPropertyIteratorObject(JSContext* cx) {
  RootedObjectGroup group(
      cx, ObjectGroup::defaultNewGroup(cx, &PropertyIteratorObject::class_,
                                       TaggedProto(nullptr)));
  if (!group) {
    return nullptr;
  }

  const Class* clasp = &PropertyIteratorObject::class_;
  RootedShape shape(cx,
                    EmptyShape::getInitialShape(cx, clasp, TaggedProto(nullptr),
                                                ITERATOR_FINALIZE_KIND));
  if (!shape) {
    return nullptr;
  }

  JSObject* obj;
  JS_TRY_VAR_OR_RETURN_NULL(
      cx, obj,
      NativeObject::create(cx, ITERATOR_FINALIZE_KIND,
                           GetInitialHeap(GenericObject, group), shape, group));

  PropertyIteratorObject* res = &obj->as<PropertyIteratorObject>();

  // CodeGenerator::visitIteratorStartO assumes the iterator object is not
  // inside the nursery when deciding whether a barrier is necessary.
  MOZ_ASSERT(!js::gc::IsInsideNursery(res));

  MOZ_ASSERT(res->numFixedSlots() == PropertyIteratorObject::NUM_FIXED_SLOTS);
  return res;
}

static PropertyIteratorObject* CreatePropertyIterator(
    JSContext* cx, Handle<JSObject*> objBeingIterated, HandleIdVector props,
    uint32_t numGuards, uint32_t guardKey) {
  Rooted<PropertyIteratorObject*> propIter(cx, NewPropertyIteratorObject(cx));
  if (!propIter) {
    return nullptr;
  }

  static_assert(sizeof(ReceiverGuard) == 2 * sizeof(GCPtrFlatString),
                "NativeIterators are allocated in space for 1) themselves, "
                "2) the properties a NativeIterator iterates (as "
                "GCPtrFlatStrings), and 3) |numGuards| HeapReceiverGuard "
                "objects; the additional-length calculation below assumes "
                "this size-relationship when determining the extra space to "
                "allocate");

  size_t extraCount = props.length() + numGuards * 2;
  void* mem =
      cx->pod_malloc_with_extra<NativeIterator, GCPtrFlatString>(extraCount);
  if (!mem) {
    return nullptr;
  }

  // This also registers |ni| with |propIter|.
  bool hadError = false;
  NativeIterator* ni = new (mem) NativeIterator(
      cx, propIter, objBeingIterated, props, numGuards, guardKey, &hadError);
  if (hadError) {
    return nullptr;
  }

  ObjectRealm& realm = objBeingIterated ? ObjectRealm::get(objBeingIterated)
                                        : ObjectRealm::get(propIter);
  RegisterEnumerator(realm, ni);

  return propIter;
}

/**
 * Initialize a sentinel NativeIterator whose purpose is only to act as the
 * start/end of the circular linked list of NativeIterators in
 * ObjectRealm::enumerators.
 */
NativeIterator::NativeIterator() {
  // Do our best to enforce that nothing in |this| except the two fields set
  // below is ever observed.
  AlwaysPoison(static_cast<void*>(this), 0xCC, sizeof(*this),
               MemCheckKind::MakeUndefined);

  // These are the only two fields in sentinel NativeIterators that are
  // examined, in ObjectRealm::sweepNativeIterators.  Everything else is
  // only examined *if* it's a NativeIterator being traced by a
  // PropertyIteratorObject that owns it, and nothing owns this iterator.
  prev_ = next_ = this;
}

NativeIterator* NativeIterator::allocateSentinel(JSContext* cx) {
  NativeIterator* ni = js_new<NativeIterator>();
  if (!ni) {
    ReportOutOfMemory(cx);
    return nullptr;
  }

  return ni;
}

/**
 * Initialize a fresh NativeIterator.
 *
 * This definition is a bit tricky: some parts of initializing are fallible, so
 * as we initialize, we must carefully keep this in GC-safe state (see
 * NativeIterator::trace).
 */
NativeIterator::NativeIterator(JSContext* cx,
                               Handle<PropertyIteratorObject*> propIter,
                               Handle<JSObject*> objBeingIterated,
                               HandleIdVector props, uint32_t numGuards,
                               uint32_t guardKey, bool* hadError)
    : objectBeingIterated_(objBeingIterated),
      iterObj_(propIter),
      // NativeIterator initially acts (before full initialization) as if it
      // contains no guards...
      guardsEnd_(guardsBegin()),
      // ...and no properties.
      propertyCursor_(
          reinterpret_cast<GCPtrFlatString*>(guardsBegin() + numGuards)),
      propertiesEnd_(propertyCursor_),
      guardKey_(guardKey),
      flags_(0)  // note: no Flags::Initialized
{
  MOZ_ASSERT(!*hadError);

  // NOTE: This must be done first thing: PropertyIteratorObject::finalize
  //       can only free |this| (and not leak it) if this has happened.
  propIter->setNativeIterator(this);

  for (size_t i = 0, len = props.length(); i < len; i++) {
    JSFlatString* str = IdToString(cx, props[i]);
    if (!str) {
      *hadError = true;
      return;
    }

    // Placement-new the next property string at the end of the currently
    // computed property strings.
    GCPtrFlatString* loc = propertiesEnd_;

    // Increase the overall property string count before initializing the
    // property string, so this construction isn't on a location not known
    // to the GC yet.
    propertiesEnd_++;

    new (loc) GCPtrFlatString(str);
  }

  if (numGuards > 0) {
    // Construct guards into the guard array.  Also recompute the guard key,
    // which incorporates Shape* and ObjectGroup* addresses that could have
    // changed during a GC triggered in (among other places) |IdToString|
    //. above.
    JSObject* pobj = objBeingIterated;
#ifdef DEBUG
    uint32_t i = 0;
#endif
    uint32_t key = 0;
    do {
      ReceiverGuard guard(pobj);

      // Placement-new the next HeapReceiverGuard at the end of the
      // currently initialized HeapReceiverGuards.
      HeapReceiverGuard* loc = guardsEnd_;

      // Increase the overall guard-count before initializing the
      // HeapReceiverGuard, so this construction isn't on a location not
      // known to the GC.
      guardsEnd_++;
#ifdef DEBUG
      i++;
#endif

      new (loc) HeapReceiverGuard(guard);

      key = mozilla::AddToHash(key, guard.hash());

      // The one caller of this method that passes |numGuards > 0|, does
      // so only if the entire chain consists of cacheable objects (that
      // necessarily have static prototypes).
      pobj = pobj->staticPrototype();
    } while (pobj);

    guardKey_ = key;
    MOZ_ASSERT(i == numGuards);
  }

  // |guardsEnd_| is now guaranteed to point at the start of properties, so
  // we can mark this initialized.
  MOZ_ASSERT(static_cast<void*>(guardsEnd_) == propertyCursor_);
  markInitialized();

  MOZ_ASSERT(!*hadError);
}

/* static */
bool IteratorHashPolicy::match(PropertyIteratorObject* obj,
                               const Lookup& lookup) {
  NativeIterator* ni = obj->getNativeIterator();
  if (ni->guardKey() != lookup.key || ni->guardCount() != lookup.numGuards) {
    return false;
  }

  return ArrayEqual(reinterpret_cast<ReceiverGuard*>(ni->guardsBegin()),
                    lookup.guards, ni->guardCount());
}

static inline bool CanCompareIterableObjectToCache(JSObject* obj) {
  if (obj->isNative()) {
    return obj->as<NativeObject>().getDenseInitializedLength() == 0;
  }
  return false;
}

using ReceiverGuardVector = Vector<ReceiverGuard, 8>;

static MOZ_ALWAYS_INLINE PropertyIteratorObject* LookupInIteratorCache(
    JSContext* cx, JSObject* obj, uint32_t* numGuards) {
  MOZ_ASSERT(*numGuards == 0);

  ReceiverGuardVector guards(cx);
  uint32_t key = 0;
  JSObject* pobj = obj;
  do {
    if (!CanCompareIterableObjectToCache(pobj)) {
      return nullptr;
    }

    ReceiverGuard guard(pobj);
    key = mozilla::AddToHash(key, guard.hash());

    if (MOZ_UNLIKELY(!guards.append(guard))) {
      cx->recoverFromOutOfMemory();
      return nullptr;
    }

    pobj = pobj->staticPrototype();
  } while (pobj);

  MOZ_ASSERT(!guards.empty());
  *numGuards = guards.length();

  IteratorHashPolicy::Lookup lookup(guards.begin(), guards.length(), key);
  auto p = ObjectRealm::get(obj).iteratorCache.lookup(lookup);
  if (!p) {
    return nullptr;
  }

  PropertyIteratorObject* iterobj = *p;
  MOZ_ASSERT(iterobj->compartment() == cx->compartment());

  NativeIterator* ni = iterobj->getNativeIterator();
  if (!ni->isReusable()) {
    return nullptr;
  }

  return iterobj;
}

static bool CanStoreInIteratorCache(JSObject* obj) {
  do {
    MOZ_ASSERT(obj->isNative());

    MOZ_ASSERT(obj->as<NativeObject>().getDenseInitializedLength() == 0);

    // Typed arrays have indexed properties not captured by the Shape guard.
    // Enumerate hooks may add extra properties.
    const Class* clasp = obj->getClass();
    if (MOZ_UNLIKELY(IsTypedArrayClass(clasp))) {
      return false;
    }
    if (MOZ_UNLIKELY(clasp->getNewEnumerate() || clasp->getEnumerate())) {
      return false;
    }

    obj = obj->staticPrototype();
  } while (obj);

  return true;
}

static MOZ_MUST_USE bool StoreInIteratorCache(JSContext* cx, JSObject* obj,
                                              PropertyIteratorObject* iterobj) {
  MOZ_ASSERT(CanStoreInIteratorCache(obj));

  NativeIterator* ni = iterobj->getNativeIterator();
  MOZ_ASSERT(ni->guardCount() > 0);

  IteratorHashPolicy::Lookup lookup(
      reinterpret_cast<ReceiverGuard*>(ni->guardsBegin()), ni->guardCount(),
      ni->guardKey());

  ObjectRealm::IteratorCache& cache = ObjectRealm::get(obj).iteratorCache;
  bool ok;
  auto p = cache.lookupForAdd(lookup);
  if (MOZ_LIKELY(!p)) {
    ok = cache.add(p, iterobj);
  } else {
    // If we weren't able to use an existing cached iterator, just
    // replace it.
    cache.remove(p);
    ok = cache.relookupOrAdd(p, lookup, iterobj);
  }
  if (!ok) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

bool js::EnumerateProperties(JSContext* cx, HandleObject obj,
                             MutableHandleIdVector props) {
  MOZ_ASSERT(props.empty());

  if (MOZ_UNLIKELY(obj->is<ProxyObject>())) {
    return Proxy::enumerate(cx, obj, props);
  }

  return Snapshot(cx, obj, 0, props);
}

static JSObject* GetIterator(JSContext* cx, HandleObject obj) {
  MOZ_ASSERT(!obj->is<PropertyIteratorObject>());
  MOZ_ASSERT(cx->compartment() == obj->compartment(),
             "We may end up allocating shapes in the wrong zone!");

  uint32_t numGuards = 0;
  if (PropertyIteratorObject* iterobj =
          LookupInIteratorCache(cx, obj, &numGuards)) {
    NativeIterator* ni = iterobj->getNativeIterator();
    ni->changeObjectBeingIterated(*obj);
    RegisterEnumerator(ObjectRealm::get(obj), ni);
    return iterobj;
  }

  if (numGuards > 0 && !CanStoreInIteratorCache(obj)) {
    numGuards = 0;
  }

  RootedIdVector keys(cx);
  if (!EnumerateProperties(cx, obj, &keys)) {
    return nullptr;
  }

  if (obj->isSingleton() && !JSObject::setIteratedSingleton(cx, obj)) {
    return nullptr;
  }
  MarkObjectGroupFlags(cx, obj, OBJECT_FLAG_ITERATED);

  PropertyIteratorObject* iterobj =
      CreatePropertyIterator(cx, obj, keys, numGuards, 0);
  if (!iterobj) {
    return nullptr;
  }

  cx->check(iterobj);

  // Cache the iterator object.
  if (numGuards > 0) {
    if (!StoreInIteratorCache(cx, obj, iterobj)) {
      return nullptr;
    }
  }

  return iterobj;
}

PropertyIteratorObject* js::LookupInIteratorCache(JSContext* cx,
                                                  HandleObject obj) {
  uint32_t numGuards = 0;
  return LookupInIteratorCache(cx, obj, &numGuards);
}

// ES 2017 draft 7.4.7.
JSObject* js::CreateIterResultObject(JSContext* cx, HandleValue value,
                                     bool done) {
  // Step 1 (implicit).

  // Step 2.
  RootedObject templateObject(
      cx, cx->realm()->getOrCreateIterResultTemplateObject(cx));
  if (!templateObject) {
    return nullptr;
  }

  NativeObject* resultObj;
  JS_TRY_VAR_OR_RETURN_NULL(
      cx, resultObj, NativeObject::createWithTemplate(cx, templateObject));

  // Step 3.
  resultObj->setSlot(Realm::IterResultObjectValueSlot, value);

  // Step 4.
  resultObj->setSlot(Realm::IterResultObjectDoneSlot,
                     done ? TrueHandleValue : FalseHandleValue);

  // Step 5.
  return resultObj;
}

NativeObject* Realm::getOrCreateIterResultTemplateObject(JSContext* cx) {
  MOZ_ASSERT(cx->realm() == this);

  if (iterResultTemplate_) {
    return iterResultTemplate_;
  }

  NativeObject* templateObj =
      createIterResultTemplateObject(cx, WithObjectPrototype::Yes);
  iterResultTemplate_.set(templateObj);
  return iterResultTemplate_;
}

NativeObject* Realm::getOrCreateIterResultWithoutPrototypeTemplateObject(
    JSContext* cx) {
  MOZ_ASSERT(cx->realm() == this);

  if (iterResultWithoutPrototypeTemplate_) {
    return iterResultWithoutPrototypeTemplate_;
  }

  NativeObject* templateObj =
      createIterResultTemplateObject(cx, WithObjectPrototype::No);
  iterResultWithoutPrototypeTemplate_.set(templateObj);
  return iterResultWithoutPrototypeTemplate_;
}

NativeObject* Realm::createIterResultTemplateObject(
    JSContext* cx, WithObjectPrototype withProto) {
  // Create template plain object
  RootedNativeObject templateObject(
      cx, withProto == WithObjectPrototype::Yes
              ? NewBuiltinClassInstance<PlainObject>(cx, TenuredObject)
              : NewObjectWithNullTaggedProto<PlainObject>(cx));
  if (!templateObject) {
    return nullptr;
  }

  // Create a new group for the template.
  Rooted<TaggedProto> proto(cx, templateObject->taggedProto());
  RootedObjectGroup group(
      cx, ObjectGroupRealm::makeGroup(cx, templateObject->realm(),
                                      templateObject->getClass(), proto));
  if (!group) {
    return nullptr;
  }
  templateObject->setGroup(group);

  // Set dummy `value` property
  if (!NativeDefineDataProperty(cx, templateObject, cx->names().value,
                                UndefinedHandleValue, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  // Set dummy `done` property
  if (!NativeDefineDataProperty(cx, templateObject, cx->names().done,
                                TrueHandleValue, JSPROP_ENUMERATE)) {
    return nullptr;
  }

  AutoSweepObjectGroup sweep(group);
  if (!group->unknownProperties(sweep)) {
    // Update `value` property typeset, since it can be any value.
    HeapTypeSet* types =
        group->maybeGetProperty(sweep, NameToId(cx->names().value));
    MOZ_ASSERT(types);
    {
      AutoEnterAnalysis enter(cx);
      types->makeUnknown(sweep, cx);
    }
  }

  // Make sure that the properties are in the right slots.
  DebugOnly<Shape*> shape = templateObject->lastProperty();
  MOZ_ASSERT(shape->previous()->slot() == Realm::IterResultObjectValueSlot &&
             shape->previous()->propidRef() == NameToId(cx->names().value));
  MOZ_ASSERT(shape->slot() == Realm::IterResultObjectDoneSlot &&
             shape->propidRef() == NameToId(cx->names().done));

  return templateObject;
}

/*** Iterator objects *******************************************************/

size_t PropertyIteratorObject::sizeOfMisc(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return mallocSizeOf(getPrivate());
}

void PropertyIteratorObject::trace(JSTracer* trc, JSObject* obj) {
  if (NativeIterator* ni =
          obj->as<PropertyIteratorObject>().getNativeIterator()) {
    ni->trace(trc);
  }
}

void PropertyIteratorObject::finalize(FreeOp* fop, JSObject* obj) {
  if (NativeIterator* ni =
          obj->as<PropertyIteratorObject>().getNativeIterator()) {
    fop->free_(ni);
  }
}

const ClassOps PropertyIteratorObject::classOps_ = {nullptr, /* addProperty */
                                                    nullptr, /* delProperty */
                                                    nullptr, /* enumerate */
                                                    nullptr, /* newEnumerate */
                                                    nullptr, /* resolve */
                                                    nullptr, /* mayResolve */
                                                    finalize,
                                                    nullptr, /* call        */
                                                    nullptr, /* hasInstance */
                                                    nullptr, /* construct   */
                                                    trace};

const Class PropertyIteratorObject::class_ = {
    "Iterator", JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
    &PropertyIteratorObject::classOps_};

static const Class ArrayIteratorPrototypeClass = {"Array Iterator", 0};

enum {
  ArrayIteratorSlotIteratedObject,
  ArrayIteratorSlotNextIndex,
  ArrayIteratorSlotItemKind,
  ArrayIteratorSlotCount
};

const Class ArrayIteratorObject::class_ = {
    "Array Iterator", JSCLASS_HAS_RESERVED_SLOTS(ArrayIteratorSlotCount)};

ArrayIteratorObject* js::NewArrayIteratorObject(JSContext* cx,
                                                NewObjectKind newKind) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateArrayIteratorPrototype(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  return NewObjectWithGivenProto<ArrayIteratorObject>(cx, proto, newKind);
}

static const JSFunctionSpec array_iterator_methods[] = {
    JS_SELF_HOSTED_FN("next", "ArrayIteratorNext", 0, 0), JS_FS_END};

static const Class StringIteratorPrototypeClass = {"String Iterator", 0};

enum {
  StringIteratorSlotIteratedObject,
  StringIteratorSlotNextIndex,
  StringIteratorSlotCount
};

const Class StringIteratorObject::class_ = {
    "String Iterator", JSCLASS_HAS_RESERVED_SLOTS(StringIteratorSlotCount)};

static const JSFunctionSpec string_iterator_methods[] = {
    JS_SELF_HOSTED_FN("next", "StringIteratorNext", 0, 0), JS_FS_END};

StringIteratorObject* js::NewStringIteratorObject(JSContext* cx,
                                                  NewObjectKind newKind) {
  RootedObject proto(
      cx, GlobalObject::getOrCreateStringIteratorPrototype(cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  return NewObjectWithGivenProto<StringIteratorObject>(cx, proto, newKind);
}

static const Class RegExpStringIteratorPrototypeClass = {
    "RegExp String Iterator", 0};

enum {
  // The regular expression used for iteration. May hold the original RegExp
  // object when it is reused instead of a new RegExp object.
  RegExpStringIteratorSlotRegExp,

  // The String value being iterated upon.
  RegExpStringIteratorSlotString,

  // The source string of the original RegExp object. Used to validate we can
  // reuse the original RegExp object for matching.
  RegExpStringIteratorSlotSource,

  // The flags of the original RegExp object.
  RegExpStringIteratorSlotFlags,

  // When non-negative, this slot holds the current lastIndex position when
  // reusing the original RegExp object for matching. When set to |-1|, the
  // iterator has finished. When set to any other negative value, the
  // iterator is not yet exhausted and we're not on the fast path and we're
  // not reusing the input RegExp object.
  RegExpStringIteratorSlotLastIndex,

  RegExpStringIteratorSlotCount
};

static_assert(RegExpStringIteratorSlotRegExp ==
                  REGEXP_STRING_ITERATOR_REGEXP_SLOT,
              "RegExpStringIteratorSlotRegExp must match self-hosting define "
              "for regexp slot.");
static_assert(RegExpStringIteratorSlotString ==
                  REGEXP_STRING_ITERATOR_STRING_SLOT,
              "RegExpStringIteratorSlotString must match self-hosting define "
              "for string slot.");
static_assert(RegExpStringIteratorSlotSource ==
                  REGEXP_STRING_ITERATOR_SOURCE_SLOT,
              "RegExpStringIteratorSlotString must match self-hosting define "
              "for source slot.");
static_assert(RegExpStringIteratorSlotFlags ==
                  REGEXP_STRING_ITERATOR_FLAGS_SLOT,
              "RegExpStringIteratorSlotFlags must match self-hosting define "
              "for flags slot.");
static_assert(RegExpStringIteratorSlotLastIndex ==
                  REGEXP_STRING_ITERATOR_LASTINDEX_SLOT,
              "RegExpStringIteratorSlotLastIndex must match self-hosting "
              "define for lastIndex slot.");

const Class RegExpStringIteratorObject::class_ = {
    "RegExp String Iterator",
    JSCLASS_HAS_RESERVED_SLOTS(RegExpStringIteratorSlotCount)};

static const JSFunctionSpec regexp_string_iterator_methods[] = {
    JS_SELF_HOSTED_FN("next", "RegExpStringIteratorNext", 0, 0),

    JS_FS_END};

RegExpStringIteratorObject* js::NewRegExpStringIteratorObject(
    JSContext* cx, NewObjectKind newKind) {
  RootedObject proto(cx, GlobalObject::getOrCreateRegExpStringIteratorPrototype(
                             cx, cx->global()));
  if (!proto) {
    return nullptr;
  }

  return NewObjectWithGivenProto<RegExpStringIteratorObject>(cx, proto,
                                                             newKind);
}

JSObject* js::ValueToIterator(JSContext* cx, HandleValue vp) {
  RootedObject obj(cx);
  if (vp.isObject()) {
    /* Common case. */
    obj = &vp.toObject();
  } else if (vp.isNullOrUndefined()) {
    /*
     * Enumerating over null and undefined gives an empty enumerator, so
     * that |for (var p in <null or undefined>) <loop>;| never executes
     * <loop>, per ES5 12.6.4.
     */
    RootedIdVector props(cx);  // Empty
    return CreatePropertyIterator(cx, nullptr, props, 0, 0);
  } else {
    obj = ToObject(cx, vp);
    if (!obj) {
      return nullptr;
    }
  }

  return GetIterator(cx, obj);
}

void js::CloseIterator(JSObject* obj) {
  if (obj->is<PropertyIteratorObject>()) {
    /* Remove enumerators from the active list, which is a stack. */
    NativeIterator* ni = obj->as<PropertyIteratorObject>().getNativeIterator();

    ni->unlink();

    MOZ_ASSERT(ni->isActive());
    ni->markInactive();

    // Reset the enumerator; it may still be in the cached iterators for
    // this thread and can be reused.
    ni->resetPropertyCursorForReuse();
  }
}

bool js::IteratorCloseForException(JSContext* cx, HandleObject obj) {
  MOZ_ASSERT(cx->isExceptionPending());

  bool isClosingGenerator = cx->isClosingGenerator();
  JS::AutoSaveExceptionState savedExc(cx);

  // Implements IteratorClose (ES 7.4.6) for exception unwinding. See
  // also the bytecode generated by BytecodeEmitter::emitIteratorClose.

  // Step 3.
  //
  // Get the "return" method.
  RootedValue returnMethod(cx);
  if (!GetProperty(cx, obj, obj, cx->names().return_, &returnMethod)) {
    return false;
  }

  // Step 4.
  //
  // Do nothing if "return" is null or undefined. Throw a TypeError if the
  // method is not IsCallable.
  if (returnMethod.isNullOrUndefined()) {
    return true;
  }
  if (!IsCallable(returnMethod)) {
    return ReportIsNotFunction(cx, returnMethod);
  }

  // Step 5, 6, 8.
  //
  // Call "return" if it is not null or undefined.
  RootedValue rval(cx);
  bool ok = Call(cx, returnMethod, obj, &rval);
  if (isClosingGenerator) {
    // Closing an iterator is implemented as an exception, but in spec
    // terms it is a Completion value with [[Type]] return. In this case
    // we *do* care if the call threw and if it returned an object.
    if (!ok) {
      return false;
    }
    if (!rval.isObject()) {
      return ThrowCheckIsObject(cx, CheckIsObjectKind::IteratorReturn);
    }
  } else {
    // We don't care if the call threw or that it returned an Object, as
    // Step 6 says if IteratorClose is being called during a throw, the
    // original throw has primacy.
    savedExc.restore();
  }

  return true;
}

void js::UnwindIteratorForUncatchableException(JSObject* obj) {
  if (obj->is<PropertyIteratorObject>()) {
    NativeIterator* ni = obj->as<PropertyIteratorObject>().getNativeIterator();
    ni->unlink();
  }
}

static bool SuppressDeletedProperty(JSContext* cx, NativeIterator* ni,
                                    HandleObject obj,
                                    Handle<JSFlatString*> str) {
  if (ni->objectBeingIterated() != obj) {
    return true;
  }

  // Optimization for the following common case:
  //
  //    for (var p in o) {
  //        delete o[p];
  //    }
  //
  // Note that usually both strings will be atoms so we only check for pointer
  // equality here.
  if (ni->previousPropertyWas(str)) {
    return true;
  }

  while (true) {
    bool restart = false;

    // Check whether id is still to come.
    GCPtrFlatString* const cursor = ni->nextProperty();
    GCPtrFlatString* const end = ni->propertiesEnd();
    for (GCPtrFlatString* idp = cursor; idp < end; ++idp) {
      // Common case: both strings are atoms.
      if ((*idp)->isAtom() && str->isAtom()) {
        if (*idp != str) {
          continue;
        }
      } else {
        if (!EqualStrings(*idp, str)) {
          continue;
        }
      }

      // Check whether another property along the prototype chain became
      // visible as a result of this deletion.
      RootedObject proto(cx);
      if (!GetPrototype(cx, obj, &proto)) {
        return false;
      }
      if (proto) {
        RootedId id(cx);
        RootedValue idv(cx, StringValue(*idp));
        if (!ValueToId<CanGC>(cx, idv, &id)) {
          return false;
        }

        Rooted<PropertyDescriptor> desc(cx);
        if (!GetPropertyDescriptor(cx, proto, id, &desc)) {
          return false;
        }

        if (desc.object() && desc.enumerable()) {
          continue;
        }
      }

      // If GetPropertyDescriptor above removed a property from ni, start
      // over.
      if (end != ni->propertiesEnd() || cursor != ni->nextProperty()) {
        restart = true;
        break;
      }

      // No property along the prototype chain stepped in to take the
      // property's place, so go ahead and delete id from the list.
      // If it is the next property to be enumerated, just skip it.
      if (idp == cursor) {
        ni->incCursor();
      } else {
        for (GCPtrFlatString* p = idp; p + 1 != end; p++) {
          *p = *(p + 1);
        }

        ni->trimLastProperty();
      }

      ni->markHasUnvisitedPropertyDeletion();
      return true;
    }

    if (!restart) {
      return true;
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
 */
static bool SuppressDeletedPropertyHelper(JSContext* cx, HandleObject obj,
                                          Handle<JSFlatString*> str) {
  NativeIterator* enumeratorList = ObjectRealm::get(obj).enumerators;
  NativeIterator* ni = enumeratorList->next();

  while (ni != enumeratorList) {
    if (!SuppressDeletedProperty(cx, ni, obj, str)) {
      return false;
    }
    ni = ni->next();
  }

  return true;
}

bool js::SuppressDeletedProperty(JSContext* cx, HandleObject obj, jsid id) {
  if (MOZ_LIKELY(!ObjectRealm::get(obj).objectMaybeInIteration(obj))) {
    return true;
  }

  if (JSID_IS_SYMBOL(id)) {
    return true;
  }

  Rooted<JSFlatString*> str(cx, IdToString(cx, id));
  if (!str) {
    return false;
  }
  return SuppressDeletedPropertyHelper(cx, obj, str);
}

bool js::SuppressDeletedElement(JSContext* cx, HandleObject obj,
                                uint32_t index) {
  if (MOZ_LIKELY(!ObjectRealm::get(obj).objectMaybeInIteration(obj))) {
    return true;
  }

  RootedId id(cx);
  if (!IndexToId(cx, index, &id)) {
    return false;
  }

  Rooted<JSFlatString*> str(cx, IdToString(cx, id));
  if (!str) {
    return false;
  }
  return SuppressDeletedPropertyHelper(cx, obj, str);
}

static const JSFunctionSpec iterator_proto_methods[] = {
    JS_SELF_HOSTED_SYM_FN(iterator, "IteratorIdentity", 0, 0), JS_FS_END};

/* static */
bool GlobalObject::initIteratorProto(JSContext* cx,
                                     Handle<GlobalObject*> global) {
  if (global->getReservedSlot(ITERATOR_PROTO).isObject()) {
    return true;
  }

  RootedObject proto(
      cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
  if (!proto || !DefinePropertiesAndFunctions(cx, proto, nullptr,
                                              iterator_proto_methods)) {
    return false;
  }

  global->setReservedSlot(ITERATOR_PROTO, ObjectValue(*proto));
  return true;
}

/* static */
bool GlobalObject::initArrayIteratorProto(JSContext* cx,
                                          Handle<GlobalObject*> global) {
  if (global->getReservedSlot(ARRAY_ITERATOR_PROTO).isObject()) {
    return true;
  }

  RootedObject iteratorProto(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!iteratorProto) {
    return false;
  }

  const Class* cls = &ArrayIteratorPrototypeClass;
  RootedObject proto(
      cx, GlobalObject::createBlankPrototypeInheriting(cx, cls, iteratorProto));
  if (!proto ||
      !DefinePropertiesAndFunctions(cx, proto, nullptr,
                                    array_iterator_methods) ||
      !DefineToStringTag(cx, proto, cx->names().ArrayIterator)) {
    return false;
  }

  global->setReservedSlot(ARRAY_ITERATOR_PROTO, ObjectValue(*proto));
  return true;
}

/* static */
bool GlobalObject::initStringIteratorProto(JSContext* cx,
                                           Handle<GlobalObject*> global) {
  if (global->getReservedSlot(STRING_ITERATOR_PROTO).isObject()) {
    return true;
  }

  RootedObject iteratorProto(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!iteratorProto) {
    return false;
  }

  const Class* cls = &StringIteratorPrototypeClass;
  RootedObject proto(
      cx, GlobalObject::createBlankPrototypeInheriting(cx, cls, iteratorProto));
  if (!proto ||
      !DefinePropertiesAndFunctions(cx, proto, nullptr,
                                    string_iterator_methods) ||
      !DefineToStringTag(cx, proto, cx->names().StringIterator)) {
    return false;
  }

  global->setReservedSlot(STRING_ITERATOR_PROTO, ObjectValue(*proto));
  return true;
}

/* static */
bool GlobalObject::initRegExpStringIteratorProto(JSContext* cx,
                                                 Handle<GlobalObject*> global) {
  if (global->getReservedSlot(REGEXP_STRING_ITERATOR_PROTO).isObject()) {
    return true;
  }

  RootedObject iteratorProto(
      cx, GlobalObject::getOrCreateIteratorPrototype(cx, global));
  if (!iteratorProto) {
    return false;
  }

  const Class* cls = &RegExpStringIteratorPrototypeClass;
  RootedObject proto(
      cx, GlobalObject::createBlankPrototypeInheriting(cx, cls, iteratorProto));
  if (!proto ||
      !DefinePropertiesAndFunctions(cx, proto, nullptr,
                                    regexp_string_iterator_methods) ||
      !DefineToStringTag(cx, proto, cx->names().RegExpStringIterator)) {
    return false;
  }

  global->setReservedSlot(REGEXP_STRING_ITERATOR_PROTO, ObjectValue(*proto));
  return true;
}
