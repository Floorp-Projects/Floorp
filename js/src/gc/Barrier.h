/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef gc_Barrier_h
#define gc_Barrier_h

#include "NamespaceImports.h"

#include "gc/Heap.h"
#include "gc/StoreBuffer.h"
#include "js/HeapAPI.h"
#include "js/Id.h"
#include "js/RootingAPI.h"
#include "js/Value.h"

/*
 * A write barrier is a mechanism used by incremental or generation GCs to
 * ensure that every value that needs to be marked is marked. In general, the
 * write barrier should be invoked whenever a write can cause the set of things
 * traced through by the GC to change. This includes:
 *   - writes to object properties
 *   - writes to array slots
 *   - writes to fields like JSObject::shape_ that we trace through
 *   - writes to fields in private data
 *   - writes to non-markable fields like JSObject::private that point to
 *     markable data
 * The last category is the trickiest. Even though the private pointers does not
 * point to a GC thing, changing the private pointer may change the set of
 * objects that are traced by the GC. Therefore it needs a write barrier.
 *
 * Every barriered write should have the following form:
 *   <pre-barrier>
 *   obj->field = value; // do the actual write
 *   <post-barrier>
 * The pre-barrier is used for incremental GC and the post-barrier is for
 * generational GC.
 *
 *                               PRE-BARRIER
 *
 * To understand the pre-barrier, let's consider how incremental GC works. The
 * GC itself is divided into "slices". Between each slice, JS code is allowed to
 * run. Each slice should be short so that the user doesn't notice the
 * interruptions. In our GC, the structure of the slices is as follows:
 *
 * 1. ... JS work, which leads to a request to do GC ...
 * 2. [first GC slice, which performs all root marking and possibly more marking]
 * 3. ... more JS work is allowed to run ...
 * 4. [GC mark slice, which runs entirely in drainMarkStack]
 * 5. ... more JS work ...
 * 6. [GC mark slice, which runs entirely in drainMarkStack]
 * 7. ... more JS work ...
 * 8. [GC marking finishes; sweeping done non-incrementally; GC is done]
 * 9. ... JS continues uninterrupted now that GC is finishes ...
 *
 * Of course, there may be a different number of slices depending on how much
 * marking is to be done.
 *
 * The danger inherent in this scheme is that the JS code in steps 3, 5, and 7
 * might change the heap in a way that causes the GC to collect an object that
 * is actually reachable. The write barrier prevents this from happening. We use
 * a variant of incremental GC called "snapshot at the beginning." This approach
 * guarantees the invariant that if an object is reachable in step 2, then we
 * will mark it eventually. The name comes from the idea that we take a
 * theoretical "snapshot" of all reachable objects in step 2; all objects in
 * that snapshot should eventually be marked. (Note that the write barrier
 * verifier code takes an actual snapshot.)
 *
 * The basic correctness invariant of a snapshot-at-the-beginning collector is
 * that any object reachable at the end of the GC (step 9) must either:
 *   (1) have been reachable at the beginning (step 2) and thus in the snapshot
 *   (2) or must have been newly allocated, in steps 3, 5, or 7.
 * To deal with case (2), any objects allocated during an incremental GC are
 * automatically marked black.
 *
 * This strategy is actually somewhat conservative: if an object becomes
 * unreachable between steps 2 and 8, it would be safe to collect it. We won't,
 * mainly for simplicity. (Also, note that the snapshot is entirely
 * theoretical. We don't actually do anything special in step 2 that we wouldn't
 * do in a non-incremental GC.
 *
 * It's the pre-barrier's job to maintain the snapshot invariant. Consider the
 * write "obj->field = value". Let the prior value of obj->field be
 * value0. Since it's possible that value0 may have been what obj->field
 * contained in step 2, when the snapshot was taken, the barrier marks
 * value0. Note that it only does this if we're in the middle of an incremental
 * GC. Since this is rare, the cost of the write barrier is usually just an
 * extra branch.
 *
 * In practice, we implement the pre-barrier differently based on the type of
 * value0. E.g., see JSObject::writeBarrierPre, which is used if obj->field is
 * a JSObject*. It takes value0 as a parameter.
 *
 *                                POST-BARRIER
 *
 * For generational GC, we want to be able to quickly collect the nursery in a
 * minor collection.  Part of the way this is achieved is to only mark the
 * nursery itself; tenured things, which may form the majority of the heap, are
 * not traced through or marked.  This leads to the problem of what to do about
 * tenured objects that have pointers into the nursery: if such things are not
 * marked, they may be discarded while there are still live objects which
 * reference them. The solution is to maintain information about these pointers,
 * and mark their targets when we start a minor collection.
 *
 * The pointers can be thought of as edges in object graph, and the set of edges
 * from the tenured generation into the nursery is know as the remembered set.
 * Post barriers are used to track this remembered set.
 *
 * Whenever a slot which could contain such a pointer is written, we use a write
 * barrier to check if the edge created is in the remembered set, and if so we
 * insert it into the store buffer, which is the collector's representation of
 * the remembered set.  This means than when we come to do a minor collection we
 * can examine the contents of the store buffer and mark any edge targets that
 * are in the nursery.
 *
 *                            IMPLEMENTATION DETAILS
 *
 * Since it would be awkward to change every write to memory into a function
 * call, this file contains a bunch of C++ classes and templates that use
 * operator overloading to take care of barriers automatically. In many cases,
 * all that's necessary to make some field be barriered is to replace
 *     Type* field;
 * with
 *     HeapPtr<Type> field;
 * There are also special classes HeapValue and HeapId, which barrier js::Value
 * and jsid, respectively.
 *
 * One additional note: not all object writes need to be pre-barriered. Writes
 * to newly allocated objects do not need a pre-barrier. In these cases, we use
 * the "obj->field.init(value)" method instead of "obj->field = value". We use
 * the init naming idiom in many places to signify that a field is being
 * assigned for the first time.
 *
 * This file implements four classes, illustrated here:
 *
 * BarrieredBase          abstract base class which provides common operations
 *  |  |  |
 *  |  | PreBarriered     provides pre-barriers only
 *  |  |
 *  | HeapPtr             provides pre- and post-barriers
 *  |
 * RelocatablePtr         provides pre- and post-barriers and is relocatable
 *
 * The implementation of the barrier logic is implemented on T::writeBarrier.*,
 * via:
 *
 * BarrieredBase<T>::pre
 *  -> InternalGCMethods<T*>::preBarrier
 *      -> T::writeBarrierPre
 *  -> InternalGCMethods<Value>::preBarrier
 *  -> InternalGCMethods<jsid>::preBarrier
 *      -> InternalGCMethods<T*>::preBarrier
 *          -> T::writeBarrierPre
 *
 * HeapPtr<T>::post and RelocatablePtr<T>::post
 *  -> InternalGCMethods<T*>::postBarrier
 *      -> T::writeBarrierPost
 *  -> InternalGCMethods<Value>::postBarrier
 *      -> StoreBuffer::put
 *
 * These classes are designed to be used by the internals of the JS engine.
 * Barriers designed to be used externally are provided in js/RootingAPI.h.
 * These external barriers call into the same post-barrier implementations at
 * InternalGCMethods<T>::post via an indirect call to Heap(.+)Barrier.
 */

class JSAtom;
struct JSCompartment;
class JSFlatString;
class JSLinearString;

namespace JS {
class Symbol;
} // namespace JS

namespace js {

class AccessorShape;
class ArrayObject;
class ArgumentsObject;
class ArrayBufferObjectMaybeShared;
class ArrayBufferObject;
class ArrayBufferViewObject;
class SharedArrayBufferObject;
class SharedTypedArrayObject;
class BaseShape;
class DebugScopeObject;
class GlobalObject;
class LazyScript;
class NativeObject;
class NestedScopeObject;
class PlainObject;
class PropertyName;
class SavedFrame;
class ScopeObject;
class ScriptSourceObject;
class Shape;
class UnownedBaseShape;
class ObjectGroup;

namespace jit {
class JitCode;
} // namespace jit

#ifdef DEBUG
// Barriers can't be triggered during backend Ion compilation, which may run on
// a helper thread.
bool
CurrentThreadIsIonCompiling();

bool
CurrentThreadIsIonCompilingSafeForMinorGC();

bool
CurrentThreadIsGCSweeping();

bool
CurrentThreadIsHandlingInitFailure();
#endif

namespace gc {

// Marking.h depends on these barrier definitions, so we need a separate
// entry point for marking to implement the pre-barrier.
void MarkValueForBarrier(JSTracer* trc, Value* v, const char* name);
void MarkIdForBarrier(JSTracer* trc, jsid* idp, const char* name);

} // namespace gc

template <typename T>
struct InternalGCMethods {};

template <typename T>
struct InternalGCMethods<T*>
{
    static bool isMarkable(T* v) { return v != nullptr; }

    static bool isMarkableTaggedPointer(T* v) { return !IsNullTaggedPointer(v); }

    static void preBarrier(T* v) { T::writeBarrierPre(v); }

    static void postBarrier(T** vp, T* prev, T* next) { T::writeBarrierPost(vp, prev, next); }

    static void readBarrier(T* v) { T::readBarrier(v); }
};

template <typename S> struct PreBarrierFunctor : VoidDefaultAdaptor<S> {
    template <typename T> void operator()(T* t);
};

template <typename S> struct ReadBarrierFunctor : public VoidDefaultAdaptor<S> {
    template <typename T> void operator()(T* t);
};

template <>
struct InternalGCMethods<Value>
{
    static bool isMarkable(Value v) { return v.isMarkable(); }
    static bool isMarkableTaggedPointer(Value v) { return isMarkable(v); }

    static void preBarrier(Value v) {
        DispatchValueTyped(PreBarrierFunctor<Value>(), v);
    }

    static void postBarrier(Value* vp, const Value& prev, const Value& next) {
        MOZ_ASSERT(!CurrentThreadIsIonCompiling());
        MOZ_ASSERT(vp);

        // If the target needs an entry, add it.
        js::gc::StoreBuffer* sb;
        if (next.isObject() && (sb = reinterpret_cast<gc::Cell*>(&next.toObject())->storeBuffer())) {
            // If we know that the prev has already inserted an entry, we can skip
            // doing the lookup to add the new entry.
            if (prev.isObject() && reinterpret_cast<gc::Cell*>(&prev.toObject())->storeBuffer()) {
                sb->assertHasValueEdge(vp);
                return;
            }
            sb->putValueFromAnyThread(vp);
            return;
        }
        // Remove the prev entry if the new value does not need it.
        if (prev.isObject() && (sb = reinterpret_cast<gc::Cell*>(&prev.toObject())->storeBuffer()))
            sb->unputValueFromAnyThread(vp);
    }

    static void readBarrier(const Value& v) {
        DispatchValueTyped(ReadBarrierFunctor<Value>(), v);
    }
};

template <>
struct InternalGCMethods<jsid>
{
    static bool isMarkable(jsid id) { return JSID_IS_STRING(id) || JSID_IS_SYMBOL(id); }
    static bool isMarkableTaggedPointer(jsid id) { return isMarkable(id); }

    static void preBarrier(jsid id) { DispatchIdTyped(PreBarrierFunctor<jsid>(), id); }
    static void postBarrier(jsid* idp, jsid prev, jsid next) {}
};

template <typename T>
class BarrieredBaseMixins {};

/*
 * Base class for barriered pointer types.
 */
template <class T>
class BarrieredBase : public BarrieredBaseMixins<T>
{
  protected:
    T value;

    explicit BarrieredBase(T v) : value(v) {}

  public:
    void init(T v) {
        this->value = v;
    }

    DECLARE_POINTER_COMPARISON_OPS(T);
    DECLARE_POINTER_CONSTREF_OPS(T);

    /* Use this if the automatic coercion to T isn't working. */
    const T& get() const { return value; }

    /*
     * Use these if you want to change the value without invoking the barrier.
     * Obviously this is dangerous unless you know the barrier is not needed.
     */
    T* unsafeGet() { return &value; }
    const T* unsafeGet() const { return &value; }
    void unsafeSet(T v) { value = v; }

    /* For users who need to manually barrier the raw types. */
    static void writeBarrierPre(const T& v) { InternalGCMethods<T>::preBarrier(v); }

  protected:
    void pre() { InternalGCMethods<T>::preBarrier(value); }
};

template <>
class BarrieredBaseMixins<JS::Value> : public ValueOperations<BarrieredBase<JS::Value> >
{};

/*
 * PreBarriered only automatically handles pre-barriers. Post-barriers must
 * be manually implemented when using this class. HeapPtr and RelocatablePtr
 * should be used in all cases that do not require explicit low-level control
 * of moving behavior, e.g. for HashMap keys.
 */
template <class T>
class PreBarriered : public BarrieredBase<T>
{
  public:
    PreBarriered() : BarrieredBase<T>(GCMethods<T>::initial()) {}
    /*
     * Allow implicit construction for use in generic contexts, such as DebuggerWeakMap::markKeys.
     */
    MOZ_IMPLICIT PreBarriered(T v) : BarrieredBase<T>(v) {}
    explicit PreBarriered(const PreBarriered<T>& v)
      : BarrieredBase<T>(v.value) {}
    ~PreBarriered() { this->pre(); }

    /* Use to set the pointer to nullptr. */
    void clear() {
        this->pre();
        this->value = nullptr;
    }

    DECLARE_POINTER_ASSIGN_OPS(PreBarriered, T);

  private:
    void set(const T& v) {
        this->pre();
        this->value = v;
    }
};

/*
 * A pre- and post-barriered heap pointer, for use inside the JS engine.
 *
 * It must only be stored in memory that has GC lifetime. HeapPtr must not be
 * used in contexts where it may be implicitly moved or deleted, e.g. most
 * containers.
 *
 * Not to be confused with JS::Heap<T>. This is a different class from the
 * external interface and implements substantially different semantics.
 *
 * The post-barriers implemented by this class are faster than those
 * implemented by RelocatablePtr<T> or JS::Heap<T> at the cost of not
 * automatically handling deletion or movement.
 */
template <class T>
class HeapPtr : public BarrieredBase<T>
{
  public:
    HeapPtr() : BarrieredBase<T>(GCMethods<T>::initial()) {}
    explicit HeapPtr(T v) : BarrieredBase<T>(v) { post(GCMethods<T>::initial(), v); }
    explicit HeapPtr(const HeapPtr<T>& v) : BarrieredBase<T>(v) { post(GCMethods<T>::initial(), v); }
#ifdef DEBUG
    ~HeapPtr() {
        // No prebarrier necessary as this only happens when we are sweeping or
        // before the containing object becomes part of the GC graph.
        MOZ_ASSERT(CurrentThreadIsGCSweeping() || CurrentThreadIsHandlingInitFailure());
    }
#endif

    void init(T v) {
        this->value = v;
        post(GCMethods<T>::initial(), v);
    }

    DECLARE_POINTER_ASSIGN_OPS(HeapPtr, T);

  protected:
    void post(T prev, T next) { InternalGCMethods<T>::postBarrier(&this->value, prev, next); }

  private:
    void set(const T& v) {
        this->pre();
        T tmp = this->value;
        this->value = v;
        post(tmp, this->value);
    }

    /*
     * Unlike RelocatablePtr<T>, HeapPtr<T> must be managed with GC lifetimes.
     * Specifically, the memory used by the pointer itself must be live until
     * at least the next minor GC. For that reason, move semantics are invalid
     * and are deleted here. Please note that not all containers support move
     * semantics, so this does not completely prevent invalid uses.
     */
    HeapPtr(HeapPtr<T>&&) = delete;
    HeapPtr<T>& operator=(HeapPtr<T>&&) = delete;
};

/*
 * ImmutableTenuredPtr is designed for one very narrow case: replacing
 * immutable raw pointers to GC-managed things, implicitly converting to a
 * handle type for ease of use. Pointers encapsulated by this type must:
 *
 *   be immutable (no incremental write barriers),
 *   never point into the nursery (no generational write barriers), and
 *   be traced via MarkRuntime (we use fromMarkedLocation).
 *
 * In short: you *really* need to know what you're doing before you use this
 * class!
 */
template <typename T>
class ImmutableTenuredPtr
{
    T value;

  public:
    operator T() const { return value; }
    T operator->() const { return value; }

    operator Handle<T>() const {
        return Handle<T>::fromMarkedLocation(&value);
    }

    void init(T ptr) {
        MOZ_ASSERT(ptr->isTenured());
        value = ptr;
    }

    T get() const { return value; }
    const T* address() { return &value; }
};

/*
 * A pre- and post-barriered heap pointer, for use inside the JS engine.
 *
 * Unlike HeapPtr<T>, it can be used in memory that is not managed by the GC,
 * i.e. in C++ containers.  It is, however, somewhat slower, so should only be
 * used in contexts where this ability is necessary.
 */
template <class T>
class RelocatablePtr : public BarrieredBase<T>
{
  public:
    RelocatablePtr() : BarrieredBase<T>(GCMethods<T>::initial()) {}
    explicit RelocatablePtr(T v) : BarrieredBase<T>(v) {
        post(GCMethods<T>::initial(), this->value);
    }

    /*
     * For RelocatablePtr, move semantics are equivalent to copy semantics. In
     * C++, a copy constructor taking const-ref is the way to get a single
     * function that will be used for both lvalue and rvalue copies, so we can
     * simply omit the rvalue variant.
     */
    RelocatablePtr(const RelocatablePtr<T>& v) : BarrieredBase<T>(v) {
        post(GCMethods<T>::initial(), this->value);
    }

    ~RelocatablePtr() {
        this->pre();
        post(this->value, GCMethods<T>::initial());
    }

    DECLARE_POINTER_ASSIGN_OPS(RelocatablePtr, T);

    /* Make this friend so it can access pre() and post(). */
    template <class T1, class T2>
    friend inline void
    BarrieredSetPair(Zone* zone,
                     RelocatablePtr<T1*>& v1, T1* val1,
                     RelocatablePtr<T2*>& v2, T2* val2);

  protected:
    void set(const T& v) {
        this->pre();
        postBarrieredSet(v);
    }

    void postBarrieredSet(const T& v) {
        T tmp = this->value;
        this->value = v;
        post(tmp, this->value);
    }

    void post(T prev, T next) {
        InternalGCMethods<T>::postBarrier(&this->value, prev, next);
    }
};

/*
 * This is a hack for RegExpStatics::updateFromMatch. It allows us to do two
 * barriers with only one branch to check if we're in an incremental GC.
 */
template <class T1, class T2>
static inline void
BarrieredSetPair(Zone* zone,
                 RelocatablePtr<T1*>& v1, T1* val1,
                 RelocatablePtr<T2*>& v2, T2* val2)
{
    if (T1::needWriteBarrierPre(zone)) {
        v1.pre();
        v2.pre();
    }
    v1.postBarrieredSet(val1);
    v2.postBarrieredSet(val2);
}

// Provide hash codes for Cell kinds that may be relocated and, thus, not have
// a stable address to use as the base for a hash code. Instead of the address,
// this hasher uses Cell::getUniqueId to provide exact matches and as a base
// for generating hash codes.
//
// Note: this hasher, like PointerHasher can "hash" a nullptr. While a nullptr
// would not likely be a useful key, there are some cases where being able to
// hash a nullptr is useful, either on purpose or because of bugs:
// (1) existence checks where the key may happen to be null and (2) some
// aggregate Lookup kinds embed a JSObject* that is frequently null and do not
// null test before dispatching to the hasher.
template <typename T>
struct MovableCellHasher
{
    static_assert(mozilla::IsBaseOf<JSObject, typename mozilla::RemovePointer<T>::Type>::value,
                  "MovableCellHasher's T must be a Cell type that may move");

    using Key = T;
    using Lookup = T;

    static HashNumber hash(const Lookup& l);
    static bool match(const Key& k, const Lookup& l);
    static void rekey(Key& k, const Key& newKey) { k = newKey; }
};

/* Useful for hashtables with a HeapPtr as key. */
template <class T>
struct HeapPtrHasher
{
    typedef HeapPtr<T> Key;
    typedef T Lookup;

    static HashNumber hash(Lookup obj) { return DefaultHasher<T>::hash(obj); }
    static bool match(const Key& k, Lookup l) { return k.get() == l; }
    static void rekey(Key& k, const Key& newKey) { k.unsafeSet(newKey); }
};

/* Specialized hashing policy for HeapPtrs. */
template <class T>
struct DefaultHasher<HeapPtr<T>> : HeapPtrHasher<T> { };

template <class T>
struct PreBarrieredHasher
{
    typedef PreBarriered<T> Key;
    typedef T Lookup;

    static HashNumber hash(Lookup obj) { return DefaultHasher<T>::hash(obj); }
    static bool match(const Key& k, Lookup l) { return k.get() == l; }
    static void rekey(Key& k, const Key& newKey) { k.unsafeSet(newKey); }
};

template <class T>
struct DefaultHasher<PreBarriered<T>> : PreBarrieredHasher<T> { };

/*
 * Incremental GC requires that weak pointers have read barriers. This is mostly
 * an issue for empty shapes stored in JSCompartment. The problem happens when,
 * during an incremental GC, some JS code stores one of the compartment's empty
 * shapes into an object already marked black. Normally, this would not be a
 * problem, because the empty shape would have been part of the initial snapshot
 * when the GC started. However, since this is a weak pointer, it isn't. So we
 * may collect the empty shape even though a live object points to it. To fix
 * this, we mark these empty shapes black whenever they get read out.
 */
template <class T>
class ReadBarriered
{
    T value;

  public:
    ReadBarriered() : value(nullptr) {}
    explicit ReadBarriered(T value) : value(value) {}
    explicit ReadBarriered(const Rooted<T>& rooted) : value(rooted) {}

    T get() const {
        if (!InternalGCMethods<T>::isMarkable(value))
            return GCMethods<T>::initial();
        InternalGCMethods<T>::readBarrier(value);
        return value;
    }

    T unbarrieredGet() const {
        return value;
    }

    operator T() const { return get(); }

    T& operator*() const { return *get(); }
    T operator->() const { return get(); }

    T* unsafeGet() { return &value; }
    T const * unsafeGet() const { return &value; }

    void set(T v) { value = v; }
};

/* Useful for hashtables with a ReadBarriered as key. */
template <class T>
struct ReadBarrieredHasher
{
    typedef ReadBarriered<T> Key;
    typedef T Lookup;

    static HashNumber hash(Lookup obj) { return DefaultHasher<T>::hash(obj); }
    static bool match(const Key& k, Lookup l) { return k.get() == l; }
    static void rekey(Key& k, const Key& newKey) { k.set(newKey); }
};

/* Specialized hashing policy for ReadBarriereds. */
template <class T>
struct DefaultHasher<ReadBarriered<T>> : ReadBarrieredHasher<T> { };

class ArrayObject;
class ArrayBufferObject;
class NestedScopeObject;
class DebugScopeObject;
class GlobalObject;
class ScriptSourceObject;
class Shape;
class BaseShape;
class UnownedBaseShape;
namespace jit {
class JitCode;
} // namespace jit

typedef PreBarriered<JSObject*> PreBarrieredObject;
typedef PreBarriered<JSScript*> PreBarrieredScript;
typedef PreBarriered<jit::JitCode*> PreBarrieredJitCode;
typedef PreBarriered<JSString*> PreBarrieredString;
typedef PreBarriered<JSAtom*> PreBarrieredAtom;

typedef RelocatablePtr<JSObject*> RelocatablePtrObject;
typedef RelocatablePtr<JSFunction*> RelocatablePtrFunction;
typedef RelocatablePtr<PlainObject*> RelocatablePtrPlainObject;
typedef RelocatablePtr<JSScript*> RelocatablePtrScript;
typedef RelocatablePtr<NativeObject*> RelocatablePtrNativeObject;
typedef RelocatablePtr<NestedScopeObject*> RelocatablePtrNestedScopeObject;
typedef RelocatablePtr<Shape*> RelocatablePtrShape;
typedef RelocatablePtr<ObjectGroup*> RelocatablePtrObjectGroup;
typedef RelocatablePtr<jit::JitCode*> RelocatablePtrJitCode;
typedef RelocatablePtr<JSLinearString*> RelocatablePtrLinearString;
typedef RelocatablePtr<JSString*> RelocatablePtrString;
typedef RelocatablePtr<JSAtom*> RelocatablePtrAtom;
typedef RelocatablePtr<ArrayBufferObjectMaybeShared*> RelocatablePtrArrayBufferObjectMaybeShared;

typedef HeapPtr<NativeObject*> HeapPtrNativeObject;
typedef HeapPtr<ArrayObject*> HeapPtrArrayObject;
typedef HeapPtr<ArrayBufferObjectMaybeShared*> HeapPtrArrayBufferObjectMaybeShared;
typedef HeapPtr<ArrayBufferObject*> HeapPtrArrayBufferObject;
typedef HeapPtr<BaseShape*> HeapPtrBaseShape;
typedef HeapPtr<JSAtom*> HeapPtrAtom;
typedef HeapPtr<JSFlatString*> HeapPtrFlatString;
typedef HeapPtr<JSFunction*> HeapPtrFunction;
typedef HeapPtr<JSLinearString*> HeapPtrLinearString;
typedef HeapPtr<JSObject*> HeapPtrObject;
typedef HeapPtr<JSScript*> HeapPtrScript;
typedef HeapPtr<JSString*> HeapPtrString;
typedef HeapPtr<PlainObject*> HeapPtrPlainObject;
typedef HeapPtr<PropertyName*> HeapPtrPropertyName;
typedef HeapPtr<Shape*> HeapPtrShape;
typedef HeapPtr<UnownedBaseShape*> HeapPtrUnownedBaseShape;
typedef HeapPtr<jit::JitCode*> HeapPtrJitCode;
typedef HeapPtr<ObjectGroup*> HeapPtrObjectGroup;

typedef PreBarriered<Value> PreBarrieredValue;
typedef RelocatablePtr<Value> RelocatableValue;
typedef HeapPtr<Value> HeapValue;

typedef PreBarriered<jsid> PreBarrieredId;
typedef RelocatablePtr<jsid> RelocatableId;
typedef HeapPtr<jsid> HeapId;

typedef ImmutableTenuredPtr<PropertyName*> ImmutablePropertyNamePtr;
typedef ImmutableTenuredPtr<JS::Symbol*> ImmutableSymbolPtr;

typedef ReadBarriered<DebugScopeObject*> ReadBarrieredDebugScopeObject;
typedef ReadBarriered<GlobalObject*> ReadBarrieredGlobalObject;
typedef ReadBarriered<JSFunction*> ReadBarrieredFunction;
typedef ReadBarriered<JSObject*> ReadBarrieredObject;
typedef ReadBarriered<JSScript*> ReadBarrieredScript;
typedef ReadBarriered<ScriptSourceObject*> ReadBarrieredScriptSourceObject;
typedef ReadBarriered<Shape*> ReadBarrieredShape;
typedef ReadBarriered<UnownedBaseShape*> ReadBarrieredUnownedBaseShape;
typedef ReadBarriered<jit::JitCode*> ReadBarrieredJitCode;
typedef ReadBarriered<ObjectGroup*> ReadBarrieredObjectGroup;
typedef ReadBarriered<JSAtom*> ReadBarrieredAtom;
typedef ReadBarriered<JS::Symbol*> ReadBarrieredSymbol;

typedef ReadBarriered<Value> ReadBarrieredValue;

// A pre- and post-barriered Value that is specialized to be aware that it
// resides in a slots or elements vector. This allows it to be relocated in
// memory, but with substantially less overhead than a RelocatablePtr.
class HeapSlot : public BarrieredBase<Value>
{
  public:
    enum Kind {
        Slot = 0,
        Element = 1
    };

    explicit HeapSlot() = delete;

    explicit HeapSlot(NativeObject* obj, Kind kind, uint32_t slot, const Value& v)
      : BarrieredBase<Value>(v)
    {
        post(obj, kind, slot, v);
    }

    explicit HeapSlot(NativeObject* obj, Kind kind, uint32_t slot, const HeapSlot& s)
      : BarrieredBase<Value>(s.value)
    {
        post(obj, kind, slot, s);
    }

    ~HeapSlot() {
        pre();
    }

    void init(NativeObject* owner, Kind kind, uint32_t slot, const Value& v) {
        value = v;
        post(owner, kind, slot, v);
    }

#ifdef DEBUG
    bool preconditionForSet(NativeObject* owner, Kind kind, uint32_t slot);
    bool preconditionForWriteBarrierPost(NativeObject* obj, Kind kind, uint32_t slot, Value target) const;
#endif

    void set(NativeObject* owner, Kind kind, uint32_t slot, const Value& v) {
        MOZ_ASSERT(preconditionForSet(owner, kind, slot));
        pre();
        value = v;
        post(owner, kind, slot, v);
    }

    /* For users who need to manually barrier the raw types. */
    static void writeBarrierPost(NativeObject* owner, Kind kind, uint32_t slot, const Value& target) {
        reinterpret_cast<HeapSlot*>(const_cast<Value*>(&target))->post(owner, kind, slot, target);
    }

    Value* unsafeGet() { return &value; }

  private:
    void post(NativeObject* owner, Kind kind, uint32_t slot, const Value& target) {
        MOZ_ASSERT(preconditionForWriteBarrierPost(owner, kind, slot, target));
        if (this->value.isObject()) {
            gc::Cell* cell = reinterpret_cast<gc::Cell*>(&this->value.toObject());
            if (cell->storeBuffer())
                cell->storeBuffer()->putSlotFromAnyThread(owner, kind, slot, 1);
        }
    }
};

class HeapSlotArray
{
    HeapSlot* array;

    // Whether writes may be performed to the slots in this array. This helps
    // to control how object elements which may be copy on write are used.
#ifdef DEBUG
    bool allowWrite_;
#endif

  public:
    explicit HeapSlotArray(HeapSlot* array, bool allowWrite)
      : array(array)
#ifdef DEBUG
      , allowWrite_(allowWrite)
#endif
    {}

    operator const Value*() const {
        JS_STATIC_ASSERT(sizeof(HeapValue) == sizeof(Value));
        JS_STATIC_ASSERT(sizeof(HeapSlot) == sizeof(Value));
        return reinterpret_cast<const Value*>(array);
    }
    operator HeapSlot*() const { MOZ_ASSERT(allowWrite()); return array; }

    HeapSlotArray operator +(int offset) const { return HeapSlotArray(array + offset, allowWrite()); }
    HeapSlotArray operator +(uint32_t offset) const { return HeapSlotArray(array + offset, allowWrite()); }

  private:
    bool allowWrite() const {
#ifdef DEBUG
        return allowWrite_;
#else
        return true;
#endif
    }
};

} /* namespace js */

#endif /* gc_Barrier_h */
