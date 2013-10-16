/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsobj_h
#define jsobj_h

/*
 * JS object definitions.
 *
 * A JS object consists of a possibly-shared object descriptor containing
 * ordered property names, called the map; and a dense vector of property
 * values, called slots.  The map/slot pointer pair is GC'ed, while the map
 * is reference counted and the slot vector is malloc'ed.
 */

#include "mozilla/MemoryReporting.h"

#include "gc/Marking.h"
#include "js/GCAPI.h"
#include "vm/ObjectImpl.h"
#include "vm/Shape.h"

namespace JS {
struct ObjectsExtraSizes;
}

namespace js {

class AutoPropDescArrayRooter;
struct GCMarker;
struct NativeIterator;
class Nursery;
struct StackShape;

inline JSObject *
CastAsObject(PropertyOp op)
{
    return JS_FUNC_TO_DATA_PTR(JSObject *, op);
}

inline JSObject *
CastAsObject(StrictPropertyOp op)
{
    return JS_FUNC_TO_DATA_PTR(JSObject *, op);
}

inline Value
CastAsObjectJsval(PropertyOp op)
{
    return ObjectOrNullValue(CastAsObject(op));
}

inline Value
CastAsObjectJsval(StrictPropertyOp op)
{
    return ObjectOrNullValue(CastAsObject(op));
}

/******************************************************************************/

typedef Vector<PropDesc, 1> PropDescArray;

/*
 * The baseops namespace encapsulates the default behavior when performing
 * various operations on an object, irrespective of hooks installed in the
 * object's class. In general, instance methods on the object itself should be
 * called instead of calling these methods directly.
 */
namespace baseops {

/*
 * On success, and if id was found, return true with *objp non-null and with a
 * property of *objp stored in *propp. If successful but id was not found,
 * return true with both *objp and *propp null.
 */
template <AllowGC allowGC>
extern bool
LookupProperty(ExclusiveContext *cx,
               typename MaybeRooted<JSObject*, allowGC>::HandleType obj,
               typename MaybeRooted<jsid, allowGC>::HandleType id,
               typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
               typename MaybeRooted<Shape*, allowGC>::MutableHandleType propp);

extern bool
LookupElement(JSContext *cx, HandleObject obj, uint32_t index,
              MutableHandleObject objp, MutableHandleShape propp);

extern bool
DefineGeneric(ExclusiveContext *cx, HandleObject obj, HandleId id, HandleValue value,
               JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs);

extern bool
DefineElement(ExclusiveContext *cx, HandleObject obj, uint32_t index, HandleValue value,
              JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs);

extern bool
GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id, MutableHandleValue vp);

extern bool
GetPropertyNoGC(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp);

extern bool
GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, MutableHandleValue vp);

inline bool
GetProperty(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp)
{
    return GetProperty(cx, obj, obj, id, vp);
}

inline bool
GetElement(JSContext *cx, HandleObject obj, uint32_t index, MutableHandleValue vp)
{
    return GetElement(cx, obj, obj, index, vp);
}

template <ExecutionMode mode>
extern bool
SetPropertyHelper(typename ExecutionModeTraits<mode>::ContextType cx, HandleObject obj,
                  HandleObject receiver, HandleId id, unsigned defineHow,
                  MutableHandleValue vp, bool strict);

template <ExecutionMode mode>
inline bool
SetPropertyHelper(typename ExecutionModeTraits<mode>::ContextType cx, HandleObject obj,
                  HandleObject receiver, PropertyName *name, unsigned defineHow,
                  MutableHandleValue vp, bool strict)
{
    Rooted<jsid> id(cx, NameToId(name));
    return SetPropertyHelper<mode>(cx, obj, receiver, id, defineHow, vp, strict);
}

extern bool
SetElementHelper(JSContext *cx, HandleObject obj, HandleObject Receiver, uint32_t index,
                 unsigned defineHow, MutableHandleValue vp, bool strict);

extern bool
GetAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp);

extern bool
SetAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp);

extern bool
DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, bool *succeeded);

extern bool
DeleteElement(JSContext *cx, HandleObject obj, uint32_t index, bool *succeeded);

extern bool
DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, bool *succeeded);

extern bool
DeleteGeneric(JSContext *cx, HandleObject obj, HandleId id, bool *succeeded);

} /* namespace js::baseops */

extern const Class IntlClass;
extern const Class JSONClass;
extern const Class MathClass;

class ArrayBufferObject;
class GlobalObject;
class MapObject;
class NewObjectCache;
class NormalArgumentsObject;
class SetObject;
class StrictArgumentsObject;

#ifdef JSGC_GENERATIONAL
class DenseRangeRef : public gc::BufferableRef
{
    JSObject *owner;
    uint32_t start;
    uint32_t end;

  public:
    DenseRangeRef(JSObject *obj, uint32_t start, uint32_t end)
      : owner(obj), start(start), end(end)
    {
        JS_ASSERT(start < end);
    }

    inline void mark(JSTracer *trc);
};
#endif

/*
 * NOTE: This is a placeholder for bug 619558.
 *
 * Run a post write barrier that encompasses multiple contiguous slots in a
 * single step.
 */
inline void
DenseRangeWriteBarrierPost(JSRuntime *rt, JSObject *obj, uint32_t start, uint32_t count)
{
#ifdef JSGC_GENERATIONAL
    if (count > 0) {
        JS::shadow::Runtime *shadowRuntime = JS::shadow::Runtime::asShadowRuntime(rt);
        shadowRuntime->gcStoreBufferPtr()->putGeneric(DenseRangeRef(obj, start, start + count));
    }
#endif
}

}  /* namespace js */

/*
 * The public interface for an object.
 *
 * Implementation of the underlying structure occurs in ObjectImpl, from which
 * this struct inherits.  This inheritance is currently public, but it will
 * eventually be made protected.  For full details, see vm/ObjectImpl.{h,cpp}.
 *
 * The JSFunction struct is an extension of this struct allocated from a larger
 * GC size-class.
 */
class JSObject : public js::ObjectImpl
{
  private:
    friend class js::Shape;
    friend struct js::GCMarker;
    friend class  js::NewObjectCache;
    friend class js::Nursery;

    /* Make the type object to use for LAZY_TYPE objects. */
    static js::types::TypeObject *makeLazyType(JSContext *cx, js::HandleObject obj);

  public:
    static const js::Class class_;

    /*
     * Update the last property, keeping the number of allocated slots in sync
     * with the object's new slot span.
     */
    static bool setLastProperty(js::ThreadSafeContext *cx,
                                JS::HandleObject obj, js::HandleShape shape);

    /* As above, but does not change the slot span. */
    inline void setLastPropertyInfallible(js::Shape *shape);

    /*
     * Make a non-array object with the specified initial state. This method
     * takes ownership of any extantSlots it is passed.
     */
    static inline JSObject *create(js::ExclusiveContext *cx,
                                   js::gc::AllocKind kind,
                                   js::gc::InitialHeap heap,
                                   js::HandleShape shape,
                                   js::HandleTypeObject type,
                                   js::HeapSlot *extantSlots = nullptr);

    /* Make an array object with the specified initial state. */
    static inline js::ArrayObject *createArray(js::ExclusiveContext *cx,
                                               js::gc::AllocKind kind,
                                               js::gc::InitialHeap heap,
                                               js::HandleShape shape,
                                               js::HandleTypeObject type,
                                               uint32_t length);

    /*
     * Remove the last property of an object, provided that it is safe to do so
     * (the shape and previous shape do not carry conflicting information about
     * the object itself).
     */
    inline void removeLastProperty(js::ExclusiveContext *cx);
    inline bool canRemoveLastProperty();

    /*
     * Update the slot span directly for a dictionary object, and allocate
     * slots to cover the new span if necessary.
     */
    static bool setSlotSpan(js::ThreadSafeContext *cx, JS::HandleObject obj, uint32_t span);

    /* Upper bound on the number of elements in an object. */
    static const uint32_t NELEMENTS_LIMIT = JS_BIT(28);

  public:
    bool setDelegate(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::DELEGATE, GENERATE_SHAPE);
    }

    bool isBoundFunction() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::BOUND_FUNCTION);
    }

    inline bool hasSpecialEquality() const;

    bool watched() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::WATCHED);
    }
    bool setWatched(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::WATCHED, GENERATE_SHAPE);
    }

    /* See StackFrame::varObj. */
    inline bool isVarObj();
    bool setVarObj(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::VAROBJ);
    }

    /*
     * Objects with an uncacheable proto can have their prototype mutated
     * without inducing a shape change on the object. Property cache entries
     * and JIT inline caches should not be filled for lookups across prototype
     * lookups on the object.
     */
    bool hasUncacheableProto() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::UNCACHEABLE_PROTO);
    }
    bool setUncacheableProto(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::UNCACHEABLE_PROTO, GENERATE_SHAPE);
    }

    /*
     * Whether SETLELEM was used to access this object. See also the comment near
     * PropertyTree::MAX_HEIGHT.
     */
    bool hadElementsAccess() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::HAD_ELEMENTS_ACCESS);
    }
    bool setHadElementsAccess(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::HAD_ELEMENTS_ACCESS);
    }

  public:
    bool nativeEmpty() const {
        return lastProperty()->isEmptyShape();
    }

    bool shadowingShapeChange(js::ExclusiveContext *cx, const js::Shape &shape);

    /*
     * Whether there may be indexed properties on this object, excluding any in
     * the object's elements.
     */
    bool isIndexed() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::INDEXED);
    }

    uint32_t propertyCount() const {
        return lastProperty()->entryCount();
    }

    bool hasShapeTable() const {
        return lastProperty()->hasTable();
    }

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::ObjectsExtraSizes *sizes);

    bool hasIdempotentProtoChain() const;

    static const uint32_t MAX_FIXED_SLOTS = 16;

  public:

    /* Accessors for properties. */

    /* Whether a slot is at a fixed offset from this object. */
    bool isFixedSlot(size_t slot) {
        return slot < numFixedSlots();
    }

    /* Index into the dynamic slots array to use for a dynamic slot. */
    size_t dynamicSlotIndex(size_t slot) {
        JS_ASSERT(slot >= numFixedSlots());
        return slot - numFixedSlots();
    }

    /*
     * Grow or shrink slots immediately before changing the slot span.
     * The number of allocated slots is not stored explicitly, and changes to
     * the slots must track changes in the slot span.
     */
    static bool growSlots(js::ThreadSafeContext *cx, js::HandleObject obj, uint32_t oldCount,
                          uint32_t newCount);
    static void shrinkSlots(js::ThreadSafeContext *cx, js::HandleObject obj, uint32_t oldCount,
                            uint32_t newCount);

    bool hasDynamicSlots() const { return slots != nullptr; }

  protected:
    static inline bool updateSlotsForSpan(js::ThreadSafeContext *cx,
                                          js::HandleObject obj, size_t oldSpan, size_t newSpan);

  public:
    /*
     * Trigger the write barrier on a range of slots that will no longer be
     * reachable.
     */
    void prepareSlotRangeForOverwrite(size_t start, size_t end) {
        for (size_t i = start; i < end; i++)
            getSlotAddressUnchecked(i)->js::HeapSlot::~HeapSlot();
    }

    void prepareElementRangeForOverwrite(size_t start, size_t end) {
        JS_ASSERT(end <= getDenseInitializedLength());
        for (size_t i = start; i < end; i++)
            elements[i].js::HeapSlot::~HeapSlot();
    }

    void rollbackProperties(js::ExclusiveContext *cx, uint32_t slotSpan);

    void nativeSetSlot(uint32_t slot, const js::Value &value) {
        JS_ASSERT(isNative());
        JS_ASSERT(slot < slotSpan());
        return setSlot(slot, value);
    }

    inline bool nativeSetSlotIfHasType(js::Shape *shape, const js::Value &value);

    static inline void nativeSetSlotWithType(js::ExclusiveContext *cx,
                                             js::HandleObject, js::Shape *shape,
                                             const js::Value &value);

    inline const js::Value &getReservedSlot(uint32_t index) const {
        JS_ASSERT(index < JSSLOT_FREE(getClass()));
        return getSlot(index);
    }

    inline js::HeapSlot &getReservedSlotRef(uint32_t index) {
        JS_ASSERT(index < JSSLOT_FREE(getClass()));
        return getSlotRef(index);
    }

    void initReservedSlot(uint32_t index, const js::Value &v) {
        JS_ASSERT(index < JSSLOT_FREE(getClass()));
        initSlot(index, v);
    }

    void setReservedSlot(uint32_t index, const js::Value &v) {
        JS_ASSERT(index < JSSLOT_FREE(getClass()));
        setSlot(index, v);
    }

    /*
     * Marks this object as having a singleton type, and leave the type lazy.
     * Constructs a new, unique shape for the object.
     */
    static inline bool setSingletonType(js::ExclusiveContext *cx, js::HandleObject obj);

    // uninlinedGetType() is the same as getType(), but not inlined.
    inline js::types::TypeObject* getType(JSContext *cx);
    js::types::TypeObject* uninlinedGetType(JSContext *cx);

    const js::HeapPtr<js::types::TypeObject> &typeFromGC() const {
        /* Direct field access for use by GC. */
        return type_;
    }

    /*
     * We allow the prototype of an object to be lazily computed if the object
     * is a proxy. In the lazy case, we store (JSObject *)0x1 in the proto field
     * of the object's TypeObject. We offer three ways of getting the prototype:
     *
     * 1. obj->getProto() returns the prototype, but asserts if obj is a proxy.
     * 2. obj->getTaggedProto() returns a TaggedProto, which can be tested to
     *    check if the proto is an object, nullptr, or lazily computed.
     * 3. JSObject::getProto(cx, obj, &proto) computes the proto of an object.
     *    If obj is a proxy and the proto is lazy, this code may allocate or
     *    GC in order to compute the proto. Currently, it will not run JS code.
     */
    bool uninlinedIsProxy() const;
    JSObject *getProto() const {
        JS_ASSERT(!uninlinedIsProxy());
        return js::ObjectImpl::getProto();
    }
    static inline bool getProto(JSContext *cx, js::HandleObject obj,
                                js::MutableHandleObject protop);

    // uninlinedSetType() is the same as setType(), but not inlined.
    inline void setType(js::types::TypeObject *newType);
    void uninlinedSetType(js::types::TypeObject *newType);

#ifdef DEBUG
    bool hasNewType(const js::Class *clasp, js::types::TypeObject *newType);
#endif

    /*
     * Mark an object that has been iterated over and is a singleton. We need
     * to recover this information in the object's type information after it
     * is purged on GC.
     */
    bool setIteratedSingleton(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::ITERATED_SINGLETON);
    }

    /*
     * Mark an object as requiring its default 'new' type to have unknown
     * properties.
     */
    static bool setNewTypeUnknown(JSContext *cx, const js::Class *clasp, JS::HandleObject obj);

    /* Set a new prototype for an object with a singleton type. */
    bool splicePrototype(JSContext *cx, const js::Class *clasp, js::Handle<js::TaggedProto> proto);

    /*
     * For bootstrapping, whether to splice a prototype for Function.prototype
     * or the global object.
     */
    bool shouldSplicePrototype(JSContext *cx);

    /*
     * Parents and scope chains.
     *
     * All script-accessible objects with a nullptr parent are global objects,
     * and all global objects have a nullptr parent. Some builtin objects
     * which are not script-accessible also have a nullptr parent, such as
     * parser created functions for non-compileAndGo scripts.
     *
     * Except for the non-script-accessible builtins, the global with which an
     * object is associated can be reached by following parent links to that
     * global (see global()).
     *
     * The scope chain of an object is the link in the search path when a
     * script does a name lookup on a scope object. For JS internal scope
     * objects --- Call, DeclEnv and Block --- the chain is stored in
     * the first fixed slot of the object, and the object's parent is the
     * associated global. For other scope objects, the chain is stored in the
     * object's parent.
     *
     * In compileAndGo code, scope chains can contain only internal scope
     * objects with a global object at the root as the scope of the outermost
     * non-function script. In non-compileAndGo code, the scope of the
     * outermost non-function script might not be a global object, and can have
     * a mix of other objects above it before the global object is reached.
     */

    /* Access the parent link of an object. */
    JSObject *getParent() const {
        return lastProperty()->getObjectParent();
    }
    static bool setParent(JSContext *cx, js::HandleObject obj, js::HandleObject newParent);

    /*
     * Get the enclosing scope of an object. When called on non-scope object,
     * this will just be the global (the name "enclosing scope" still applies
     * in this situation because non-scope objects can be on the scope chain).
     */
    inline JSObject *enclosingScope();

    /* Access the metadata on an object. */
    inline JSObject *getMetadata() const {
        return lastProperty()->getObjectMetadata();
    }
    static bool setMetadata(JSContext *cx, js::HandleObject obj, js::HandleObject newMetadata);

    inline js::GlobalObject &global() const;

    /* Remove the type (and prototype) or parent from a new object. */
    static inline bool clearType(JSContext *cx, js::HandleObject obj);
    static bool clearParent(JSContext *cx, js::HandleObject obj);

    /*
     * ES5 meta-object properties and operations.
     */

  private:
    enum ImmutabilityType { SEAL, FREEZE };

    /*
     * The guts of Object.seal (ES5 15.2.3.8) and Object.freeze (ES5 15.2.3.9): mark the
     * object as non-extensible, and adjust each property's attributes appropriately: each
     * property becomes non-configurable, and if |freeze|, data properties become
     * read-only as well.
     */
    static bool sealOrFreeze(JSContext *cx, js::HandleObject obj, ImmutabilityType it);

    static bool isSealedOrFrozen(JSContext *cx, js::HandleObject obj, ImmutabilityType it, bool *resultp);

    static inline unsigned getSealedOrFrozenAttributes(unsigned attrs, ImmutabilityType it);

  public:
    /* ES5 15.2.3.8: non-extensible, all props non-configurable */
    static inline bool seal(JSContext *cx, js::HandleObject obj) { return sealOrFreeze(cx, obj, SEAL); }
    /* ES5 15.2.3.9: non-extensible, all properties non-configurable, all data props read-only */
    static inline bool freeze(JSContext *cx, js::HandleObject obj) { return sealOrFreeze(cx, obj, FREEZE); }

    static inline bool isSealed(JSContext *cx, js::HandleObject obj, bool *resultp) {
        return isSealedOrFrozen(cx, obj, SEAL, resultp);
    }
    static inline bool isFrozen(JSContext *cx, js::HandleObject obj, bool *resultp) {
        return isSealedOrFrozen(cx, obj, FREEZE, resultp);
    }

    /* toString support. */
    static const char *className(JSContext *cx, js::HandleObject obj);

    /* Accessors for elements. */
    bool ensureElements(js::ThreadSafeContext *cx, uint32_t capacity) {
        if (capacity > getDenseCapacity())
            return growElements(cx, capacity);
        return true;
    }

    bool growElements(js::ThreadSafeContext *cx, uint32_t newcap);
    void shrinkElements(js::ThreadSafeContext *cx, uint32_t cap);
    void setDynamicElements(js::ObjectElements *header) {
        JS_ASSERT(!hasDynamicElements());
        elements = header->elements();
        JS_ASSERT(hasDynamicElements());
    }

    uint32_t getDenseCapacity() {
        JS_ASSERT(isNative());
        JS_ASSERT(getElementsHeader()->capacity >= getElementsHeader()->initializedLength);
        return getElementsHeader()->capacity;
    }

  private:
    inline void ensureDenseInitializedLengthNoPackedCheck(js::ThreadSafeContext *cx,
                                                          uint32_t index, uint32_t extra);

  public:
    void setDenseInitializedLength(uint32_t length) {
        JS_ASSERT(isNative());
        JS_ASSERT(length <= getDenseCapacity());
        prepareElementRangeForOverwrite(length, getElementsHeader()->initializedLength);
        getElementsHeader()->initializedLength = length;
    }

    inline void ensureDenseInitializedLength(js::ExclusiveContext *cx,
                                             uint32_t index, uint32_t extra);
    inline void ensureDenseInitializedLengthPreservePackedFlag(js::ThreadSafeContext *cx,
                                                               uint32_t index, uint32_t extra);
    void setDenseElement(uint32_t index, const js::Value &val) {
        JS_ASSERT(isNative() && index < getDenseInitializedLength());
        elements[index].set(this, js::HeapSlot::Element, index, val);
    }

    void initDenseElement(uint32_t index, const js::Value &val) {
        JS_ASSERT(isNative() && index < getDenseInitializedLength());
        elements[index].init(this, js::HeapSlot::Element, index, val);
    }

    void setDenseElementMaybeConvertDouble(uint32_t index, const js::Value &val) {
        if (val.isInt32() && shouldConvertDoubleElements())
            setDenseElement(index, js::DoubleValue(val.toInt32()));
        else
            setDenseElement(index, val);
    }

    inline bool setDenseElementIfHasType(uint32_t index, const js::Value &val);
    static inline void setDenseElementWithType(js::ExclusiveContext *cx, js::HandleObject obj,
                                               uint32_t index, const js::Value &val);
    static inline void initDenseElementWithType(js::ExclusiveContext *cx, js::HandleObject obj,
                                                uint32_t index, const js::Value &val);
    static inline void setDenseElementHole(js::ExclusiveContext *cx,
                                           js::HandleObject obj, uint32_t index);
    static inline void removeDenseElementForSparseIndex(js::ExclusiveContext *cx,
                                                        js::HandleObject obj, uint32_t index);

    void copyDenseElements(uint32_t dstStart, const js::Value *src, uint32_t count) {
        JS_ASSERT(dstStart + count <= getDenseCapacity());
        JSRuntime *rt = runtimeFromMainThread();
        if (JS::IsIncrementalBarrierNeeded(rt)) {
            JS::Zone *zone = this->zone();
            for (uint32_t i = 0; i < count; ++i)
                elements[dstStart + i].set(zone, this, js::HeapSlot::Element, dstStart + i, src[i]);
        } else {
            memcpy(&elements[dstStart], src, count * sizeof(js::HeapSlot));
            DenseRangeWriteBarrierPost(rt, this, dstStart, count);
        }
    }

    void initDenseElements(uint32_t dstStart, const js::Value *src, uint32_t count) {
        JS_ASSERT(dstStart + count <= getDenseCapacity());
        memcpy(&elements[dstStart], src, count * sizeof(js::HeapSlot));
        DenseRangeWriteBarrierPost(runtimeFromMainThread(), this, dstStart, count);
    }

    void initDenseElementsUnbarriered(uint32_t dstStart, const js::Value *src, uint32_t count) {
        /*
         * For use by parallel threads, which since they cannot see nursery
         * things do not require a barrier.
         */
        JS_ASSERT(dstStart + count <= getDenseCapacity());
#if defined(DEBUG) && defined(JSGC_GENERATIONAL)
        JS::shadow::Runtime *rt = JS::shadow::Runtime::asShadowRuntime(runtimeFromAnyThread());
        for (uint32_t index = 0; index < count; ++index) {
            const JS::Value& value = src[index];
            if (value.isMarkable())
                JS_ASSERT(js::gc::IsInsideNursery(rt, value.toGCThing()));
        }
#endif
        memcpy(&elements[dstStart], src, count * sizeof(js::HeapSlot));
    }

    void moveDenseElements(uint32_t dstStart, uint32_t srcStart, uint32_t count) {
        JS_ASSERT(dstStart + count <= getDenseCapacity());
        JS_ASSERT(srcStart + count <= getDenseInitializedLength());

        /*
         * Using memmove here would skip write barriers. Also, we need to consider
         * an array containing [A, B, C], in the following situation:
         *
         * 1. Incremental GC marks slot 0 of array (i.e., A), then returns to JS code.
         * 2. JS code moves slots 1..2 into slots 0..1, so it contains [B, C, C].
         * 3. Incremental GC finishes by marking slots 1 and 2 (i.e., C).
         *
         * Since normal marking never happens on B, it is very important that the
         * write barrier is invoked here on B, despite the fact that it exists in
         * the array before and after the move.
        */
        JS::Zone *zone = this->zone();
        JS::shadow::Zone *shadowZone = JS::shadow::Zone::asShadowZone(zone);
        if (shadowZone->needsBarrier()) {
            if (dstStart < srcStart) {
                js::HeapSlot *dst = elements + dstStart;
                js::HeapSlot *src = elements + srcStart;
                for (uint32_t i = 0; i < count; i++, dst++, src++)
                    dst->set(zone, this, js::HeapSlot::Element, dst - elements, *src);
            } else {
                js::HeapSlot *dst = elements + dstStart + count - 1;
                js::HeapSlot *src = elements + srcStart + count - 1;
                for (uint32_t i = 0; i < count; i++, dst--, src--)
                    dst->set(zone, this, js::HeapSlot::Element, dst - elements, *src);
            }
        } else {
            memmove(elements + dstStart, elements + srcStart, count * sizeof(js::HeapSlot));
            DenseRangeWriteBarrierPost(runtimeFromMainThread(), this, dstStart, count);
        }
    }

    void moveDenseElementsUnbarriered(uint32_t dstStart, uint32_t srcStart, uint32_t count) {
        JS_ASSERT(!shadowZone()->needsBarrier());

        JS_ASSERT(dstStart + count <= getDenseCapacity());
        JS_ASSERT(srcStart + count <= getDenseCapacity());

        memmove(elements + dstStart, elements + srcStart, count * sizeof(js::Value));
    }

    bool shouldConvertDoubleElements() {
        JS_ASSERT(isNative());
        return getElementsHeader()->shouldConvertDoubleElements();
    }

    inline void setShouldConvertDoubleElements();

    /* Packed information for this object's elements. */
    inline bool writeToIndexWouldMarkNotPacked(uint32_t index);
    inline void markDenseElementsNotPacked(js::ExclusiveContext *cx);

    /*
     * ensureDenseElements ensures that the object can hold at least
     * index + extra elements. It returns ED_OK on success, ED_FAILED on
     * failure to grow the array, ED_SPARSE when the object is too sparse to
     * grow (this includes the case of index + extra overflow). In the last
     * two cases the object is kept intact.
     */
    enum EnsureDenseResult { ED_OK, ED_FAILED, ED_SPARSE };

  private:
    inline EnsureDenseResult ensureDenseElementsNoPackedCheck(js::ThreadSafeContext *cx,
                                                              uint32_t index, uint32_t extra);

  public:
    inline EnsureDenseResult ensureDenseElements(js::ExclusiveContext *cx,
                                                 uint32_t index, uint32_t extra);
    inline EnsureDenseResult ensureDenseElementsPreservePackedFlag(js::ThreadSafeContext *cx,
                                                                   uint32_t index, uint32_t extra);

    inline EnsureDenseResult extendDenseElements(js::ThreadSafeContext *cx,
                                                 uint32_t requiredCapacity, uint32_t extra);

    /* Convert a single dense element to a sparse property. */
    static bool sparsifyDenseElement(js::ExclusiveContext *cx,
                                     js::HandleObject obj, uint32_t index);

    /* Convert all dense elements to sparse properties. */
    static bool sparsifyDenseElements(js::ExclusiveContext *cx, js::HandleObject obj);

    /* Small objects are dense, no matter what. */
    static const uint32_t MIN_SPARSE_INDEX = 1000;

    /*
     * Element storage for an object will be sparse if fewer than 1/8 indexes
     * are filled in.
     */
    static const unsigned SPARSE_DENSITY_RATIO = 8;

    /*
     * Check if after growing the object's elements will be too sparse.
     * newElementsHint is an estimated number of elements to be added.
     */
    bool willBeSparseElements(uint32_t requiredCapacity, uint32_t newElementsHint);

    /*
     * After adding a sparse index to obj, see if it should be converted to use
     * dense elements.
     */
    static EnsureDenseResult maybeDensifySparseElements(js::ExclusiveContext *cx, js::HandleObject obj);

  public:
    /*
     * Iterator-specific getters and setters.
     */

    static const uint32_t ITER_CLASS_NFIXED_SLOTS = 1;

    /*
     * Back to generic stuff.
     */
    bool isCallable() {
        return getClass()->isCallable();
    }

    inline void finish(js::FreeOp *fop);
    JS_ALWAYS_INLINE void finalize(js::FreeOp *fop);

    static inline bool hasProperty(JSContext *cx, js::HandleObject obj,
                                   js::HandleId id, bool *foundp, unsigned flags = 0);

    /*
     * Allocate and free an object slot.
     *
     * FIXME: bug 593129 -- slot allocation should be done by object methods
     * after calling object-parameter-free shape methods, avoiding coupling
     * logic across the object vs. shape module wall.
     */
    static bool allocSlot(js::ThreadSafeContext *cx, JS::HandleObject obj, uint32_t *slotp);
    void freeSlot(uint32_t slot);

  public:
    static bool reportReadOnly(js::ThreadSafeContext *cx, jsid id, unsigned report = JSREPORT_ERROR);
    bool reportNotConfigurable(js::ThreadSafeContext *cx, jsid id, unsigned report = JSREPORT_ERROR);
    bool reportNotExtensible(js::ThreadSafeContext *cx, unsigned report = JSREPORT_ERROR);

    /*
     * Get the property with the given id, then call it as a function with the
     * given arguments, providing this object as |this|. If the property isn't
     * callable a TypeError will be thrown. On success the value returned by
     * the call is stored in *vp.
     */
    bool callMethod(JSContext *cx, js::HandleId id, unsigned argc, js::Value *argv,
                    js::MutableHandleValue vp);

  private:
    static js::Shape *getChildPropertyOnDictionary(js::ThreadSafeContext *cx, JS::HandleObject obj,
                                                   js::HandleShape parent, js::StackShape &child);
    static js::Shape *getChildProperty(js::ExclusiveContext *cx, JS::HandleObject obj,
                                       js::HandleShape parent, js::StackShape &child);
    template <js::ExecutionMode mode>
    static inline js::Shape *
    getOrLookupChildProperty(typename js::ExecutionModeTraits<mode>::ExclusiveContextType cx,
                             JS::HandleObject obj, js::HandleShape parent, js::StackShape &child)
    {
        if (mode == js::ParallelExecution)
            return lookupChildProperty(cx, obj, parent, child);
        return getChildProperty(cx->asExclusiveContext(), obj, parent, child);
    }

  public:
    /*
     * XXX: This should be private, but is public because it needs to be a
     * friend of ThreadSafeContext to get to the propertyTree on cx->compartment_.
     */
    static js::Shape *lookupChildProperty(js::ThreadSafeContext *cx, JS::HandleObject obj,
                                          js::HandleShape parent, js::StackShape &child);


  protected:
    /*
     * Internal helper that adds a shape not yet mapped by this object.
     *
     * Notes:
     * 1. getter and setter must be normalized based on flags (see jsscope.cpp).
     * 2. Checks for non-extensibility must be done by callers.
     */
    template <js::ExecutionMode mode>
    static js::Shape *
    addPropertyInternal(typename js::ExecutionModeTraits<mode>::ExclusiveContextType cx,
                        JS::HandleObject obj, JS::HandleId id,
                        JSPropertyOp getter, JSStrictPropertyOp setter,
                        uint32_t slot, unsigned attrs,
                        unsigned flags, int shortid, js::Shape **spp,
                        bool allowDictionary);

  private:
    struct TradeGutsReserved;
    static bool ReserveForTradeGuts(JSContext *cx, JSObject *a, JSObject *b,
                                    TradeGutsReserved &reserved);

    static void TradeGuts(JSContext *cx, JSObject *a, JSObject *b,
                          TradeGutsReserved &reserved);

  public:
    /* Add a property whose id is not yet in this scope. */
    static js::Shape *addProperty(js::ExclusiveContext *cx, JS::HandleObject, JS::HandleId id,
                                  JSPropertyOp getter, JSStrictPropertyOp setter,
                                  uint32_t slot, unsigned attrs, unsigned flags,
                                  int shortid, bool allowDictionary = true);

    /* Add a data property whose id is not yet in this scope. */
    js::Shape *addDataProperty(js::ExclusiveContext *cx,
                               jsid id_, uint32_t slot, unsigned attrs);
    js::Shape *addDataProperty(js::ExclusiveContext *cx, js::HandlePropertyName name,
                               uint32_t slot, unsigned attrs);

    /* Add or overwrite a property for id in this scope. */
    template <js::ExecutionMode mode>
    static js::Shape *
    putProperty(typename js::ExecutionModeTraits<mode>::ExclusiveContextType cx,
                JS::HandleObject obj, JS::HandleId id,
                JSPropertyOp getter, JSStrictPropertyOp setter,
                uint32_t slot, unsigned attrs,
                unsigned flags, int shortid);
    template <js::ExecutionMode mode>
    static inline js::Shape *
    putProperty(typename js::ExecutionModeTraits<mode>::ExclusiveContextType cx,
                JS::HandleObject obj, js::PropertyName *name,
                JSPropertyOp getter, JSStrictPropertyOp setter,
                uint32_t slot, unsigned attrs,
                unsigned flags, int shortid);

    /* Change the given property into a sibling with the same id in this scope. */
    template <js::ExecutionMode mode>
    static js::Shape *
    changeProperty(typename js::ExecutionModeTraits<mode>::ExclusiveContextType cx,
                   js::HandleObject obj, js::HandleShape shape, unsigned attrs, unsigned mask,
                   JSPropertyOp getter, JSStrictPropertyOp setter);

    static inline bool changePropertyAttributes(JSContext *cx, js::HandleObject obj,
                                                js::HandleShape shape, unsigned attrs);

    /* Remove the property named by id from this object. */
    bool removeProperty(js::ExclusiveContext *cx, jsid id);

    /* Clear the scope, making it empty. */
    static void clear(JSContext *cx, js::HandleObject obj);

    static bool lookupGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                              js::MutableHandleObject objp, js::MutableHandleShape propp);

    static bool lookupProperty(JSContext *cx, js::HandleObject obj, js::PropertyName *name,
                               js::MutableHandleObject objp, js::MutableHandleShape propp)
    {
        JS::RootedId id(cx, js::NameToId(name));
        return lookupGeneric(cx, obj, id, objp, propp);
    }

    static bool lookupElement(JSContext *cx, js::HandleObject obj, uint32_t index,
                              js::MutableHandleObject objp, js::MutableHandleShape propp)
    {
        js::LookupElementOp op = obj->getOps()->lookupElement;
        return (op ? op : js::baseops::LookupElement)(cx, obj, index, objp, propp);
    }

    static bool lookupSpecial(JSContext *cx, js::HandleObject obj, js::SpecialId sid,
                              js::MutableHandleObject objp, js::MutableHandleShape propp)
    {
        JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
        return lookupGeneric(cx, obj, id, objp, propp);
    }

    static bool defineGeneric(js::ExclusiveContext *cx, js::HandleObject obj,
                              js::HandleId id, js::HandleValue value,
                              JSPropertyOp getter = JS_PropertyStub,
                              JSStrictPropertyOp setter = JS_StrictPropertyStub,
                              unsigned attrs = JSPROP_ENUMERATE);

    static bool defineProperty(js::ExclusiveContext *cx, js::HandleObject obj,
                               js::PropertyName *name, js::HandleValue value,
                               JSPropertyOp getter = JS_PropertyStub,
                               JSStrictPropertyOp setter = JS_StrictPropertyStub,
                               unsigned attrs = JSPROP_ENUMERATE);

    static bool defineElement(js::ExclusiveContext *cx, js::HandleObject obj,
                              uint32_t index, js::HandleValue value,
                              JSPropertyOp getter = JS_PropertyStub,
                              JSStrictPropertyOp setter = JS_StrictPropertyStub,
                              unsigned attrs = JSPROP_ENUMERATE);

    static bool defineSpecial(js::ExclusiveContext *cx, js::HandleObject obj,
                              js::SpecialId sid, js::HandleValue value,
                              JSPropertyOp getter = JS_PropertyStub,
                              JSStrictPropertyOp setter = JS_StrictPropertyStub,
                              unsigned attrs = JSPROP_ENUMERATE);

    static bool getGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                           js::HandleId id, js::MutableHandleValue vp)
    {
        JS_ASSERT(!!obj->getOps()->getGeneric == !!obj->getOps()->getProperty);
        js::GenericIdOp op = obj->getOps()->getGeneric;
        if (op) {
            if (!op(cx, obj, receiver, id, vp))
                return false;
        } else {
            if (!js::baseops::GetProperty(cx, obj, receiver, id, vp))
                return false;
        }
        return true;
    }

    static bool getGenericNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                               jsid id, js::Value *vp)
    {
        js::GenericIdOp op = obj->getOps()->getGeneric;
        if (op)
            return false;
        return js::baseops::GetPropertyNoGC(cx, obj, receiver, id, vp);
    }

    static bool getProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                            js::PropertyName *name, js::MutableHandleValue vp)
    {
        JS::RootedId id(cx, js::NameToId(name));
        return getGeneric(cx, obj, receiver, id, vp);
    }

    static bool getPropertyNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                                js::PropertyName *name, js::Value *vp)
    {
        return getGenericNoGC(cx, obj, receiver, js::NameToId(name), vp);
    }

    static inline bool getElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                                  uint32_t index, js::MutableHandleValue vp);
    static inline bool getElementNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                                      uint32_t index, js::Value *vp);

    /* If element is not present (e.g. array hole) *present is set to
       false and the contents of *vp are unusable garbage. */
    static inline bool getElementIfPresent(JSContext *cx, js::HandleObject obj,
                                           js::HandleObject receiver, uint32_t index,
                                           js::MutableHandleValue vp, bool *present);

    static bool getSpecial(JSContext *cx, js::HandleObject obj,
                           js::HandleObject receiver, js::SpecialId sid,
                           js::MutableHandleValue vp)
    {
        JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
        return getGeneric(cx, obj, receiver, id, vp);
    }

    static bool setGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                           js::HandleId id, js::MutableHandleValue vp, bool strict)
    {
        if (obj->getOps()->setGeneric)
            return nonNativeSetProperty(cx, obj, id, vp, strict);
        return js::baseops::SetPropertyHelper<js::SequentialExecution>(cx, obj, receiver, id, 0,
                                                                       vp, strict);
    }

    static bool setProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                            js::PropertyName *name,
                            js::MutableHandleValue vp, bool strict)
    {
        JS::RootedId id(cx, js::NameToId(name));
        return setGeneric(cx, obj, receiver, id, vp, strict);
    }

    static bool setElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                           uint32_t index, js::MutableHandleValue vp, bool strict)
    {
        if (obj->getOps()->setElement)
            return nonNativeSetElement(cx, obj, index, vp, strict);
        return js::baseops::SetElementHelper(cx, obj, receiver, index, 0, vp, strict);
    }

    static bool setSpecial(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                           js::SpecialId sid, js::MutableHandleValue vp, bool strict)
    {
        JS::RootedId id(cx, SPECIALID_TO_JSID(sid));
        return setGeneric(cx, obj, receiver, id, vp, strict);
    }


    static bool nonNativeSetProperty(JSContext *cx, js::HandleObject obj,
                                     js::HandleId id, js::MutableHandleValue vp, bool strict);
    static bool nonNativeSetElement(JSContext *cx, js::HandleObject obj,
                                    uint32_t index, js::MutableHandleValue vp, bool strict);

    static bool getGenericAttributes(JSContext *cx, js::HandleObject obj,
                                     js::HandleId id, unsigned *attrsp)
    {
        js::GenericAttributesOp op = obj->getOps()->getGenericAttributes;
        return (op ? op : js::baseops::GetAttributes)(cx, obj, id, attrsp);
    }

    static inline bool setGenericAttributes(JSContext *cx, js::HandleObject obj,
                                            js::HandleId id, unsigned *attrsp);

    static inline bool deleteProperty(JSContext *cx, js::HandleObject obj,
                                      js::HandlePropertyName name,
                                      bool *succeeded);
    static inline bool deleteElement(JSContext *cx, js::HandleObject obj,
                                     uint32_t index, bool *succeeded);
    static inline bool deleteSpecial(JSContext *cx, js::HandleObject obj,
                                     js::HandleSpecialId sid, bool *succeeded);
    static bool deleteByValue(JSContext *cx, js::HandleObject obj,
                              const js::Value &property, bool *succeeded);

    static bool enumerate(JSContext *cx, JS::HandleObject obj, JSIterateOp iterop,
                          JS::MutableHandleValue statep, JS::MutableHandleId idp)
    {
        JSNewEnumerateOp op = obj->getOps()->enumerate;
        return (op ? op : JS_EnumerateState)(cx, obj, iterop, statep, idp);
    }

    static bool defaultValue(JSContext *cx, js::HandleObject obj, JSType hint,
                             js::MutableHandleValue vp)
    {
        JSConvertOp op = obj->getClass()->convert;
        bool ok;
        if (op == JS_ConvertStub)
            ok = js::DefaultValue(cx, obj, hint, vp);
        else
            ok = op(cx, obj, hint, vp);
        JS_ASSERT_IF(ok, vp.isPrimitive());
        return ok;
    }

    static JSObject *thisObject(JSContext *cx, js::HandleObject obj)
    {
        JSObjectOp op = obj->getOps()->thisObject;
        return op ? op(cx, obj) : obj;
    }

    static bool thisObject(JSContext *cx, const js::Value &v, js::Value *vp);

    static bool swap(JSContext *cx, JS::HandleObject a, JS::HandleObject b);

    inline void initArrayClass();

    /*
     * In addition to the generic object interface provided by JSObject,
     * specific types of objects may provide additional operations. To access,
     * these addition operations, callers should use the pattern:
     *
     *   if (obj.is<XObject>()) {
     *     XObject &x = obj.as<XObject>();
     *     x.foo();
     *   }
     *
     * These XObject classes form a hierarchy. For example, for a cloned block
     * object, the following predicates are true: is<ClonedBlockObject>,
     * is<BlockObject>, is<NestedScopeObject> and is<ScopeObject>. Each of
     * these has a respective class that derives and adds operations.
     *
     * A class XObject is defined in a vm/XObject{.h, .cpp, -inl.h} file
     * triplet (along with any class YObject that derives XObject).
     *
     * Note that X represents a low-level representation and does not query the
     * [[Class]] property of object defined by the spec (for this, see
     * js::ObjectClassIs).
     */

    template <class T>
    inline bool is() const { return getClass() == &T::class_; }

    template <class T>
    T &as() {
        JS_ASSERT(is<T>());
        return *static_cast<T *>(this);
    }

    template <class T>
    const T &as() const {
        JS_ASSERT(is<T>());
        return *static_cast<const T *>(this);
    }

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_OBJECT; }

#ifdef DEBUG
    void dump();
#endif

  private:
    static void staticAsserts() {
        static_assert(sizeof(JSObject) == sizeof(js::shadow::Object),
                      "shadow interface must match actual interface");
        static_assert(sizeof(JSObject) == sizeof(js::ObjectImpl),
                      "JSObject itself must not have any fields");
        static_assert(sizeof(JSObject) % sizeof(js::Value) == 0,
                      "fixed slots after an object must be aligned");
    }

    JSObject() MOZ_DELETE;
    JSObject(const JSObject &other) MOZ_DELETE;
    void operator=(const JSObject &other) MOZ_DELETE;
};

/*
 * The only sensible way to compare JSObject with == is by identity. We use
 * const& instead of * as a syntactic way to assert non-null. This leads to an
 * abundance of address-of operators to identity. Hence this overload.
 */
static JS_ALWAYS_INLINE bool
operator==(const JSObject &lhs, const JSObject &rhs)
{
    return &lhs == &rhs;
}

static JS_ALWAYS_INLINE bool
operator!=(const JSObject &lhs, const JSObject &rhs)
{
    return &lhs != &rhs;
}

struct JSObject_Slots2 : JSObject { js::Value fslots[2]; };
struct JSObject_Slots4 : JSObject { js::Value fslots[4]; };
struct JSObject_Slots8 : JSObject { js::Value fslots[8]; };
struct JSObject_Slots12 : JSObject { js::Value fslots[12]; };
struct JSObject_Slots16 : JSObject { js::Value fslots[16]; };

static inline bool
js_IsCallable(const js::Value &v)
{
    return v.isObject() && v.toObject().isCallable();
}

inline JSObject *
GetInnerObject(JSContext *cx, js::HandleObject obj)
{
    if (JSObjectOp op = obj->getClass()->ext.innerObject)
        return op(cx, obj);
    return obj;
}

inline JSObject *
GetOuterObject(JSContext *cx, js::HandleObject obj)
{
    if (JSObjectOp op = obj->getClass()->ext.outerObject)
        return op(cx, obj);
    return obj;
}

class JSValueArray {
  public:
    jsval *array;
    size_t length;

    JSValueArray(jsval *v, size_t c) : array(v), length(c) {}
};

class ValueArray {
  public:
    js::Value *array;
    size_t length;

    ValueArray(js::Value *v, size_t c) : array(v), length(c) {}
};

namespace js {

#ifdef JSGC_GENERATIONAL
inline void
DenseRangeRef::mark(JSTracer *trc)
{
    /* Apply forwarding, if we have already visited owner. */
    js::gc::IsObjectMarked(&owner);
    uint32_t initLen = owner->getDenseInitializedLength();
    uint32_t clampedStart = Min(start, initLen);
    gc::MarkArraySlots(trc, Min(end, initLen) - clampedStart,
                       owner->getDenseElements() + clampedStart, "element");
}
#endif

template <AllowGC allowGC>
extern bool
HasOwnProperty(JSContext *cx, LookupGenericOp lookup,
               typename MaybeRooted<JSObject*, allowGC>::HandleType obj,
               typename MaybeRooted<jsid, allowGC>::HandleType id,
               typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
               typename MaybeRooted<Shape*, allowGC>::MutableHandleType propp);

typedef JSObject *(*ClassInitializerOp)(JSContext *cx, JS::HandleObject obj);

} /* namespace js */

/*
 * Select Object.prototype method names shared between jsapi.cpp and jsobj.cpp.
 */
extern const char js_watch_str[];
extern const char js_unwatch_str[];
extern const char js_hasOwnProperty_str[];
extern const char js_isPrototypeOf_str[];
extern const char js_propertyIsEnumerable_str[];

#ifdef JS_OLD_GETTER_SETTER_METHODS
extern const char js_defineGetter_str[];
extern const char js_defineSetter_str[];
extern const char js_lookupGetter_str[];
extern const char js_lookupSetter_str[];
#endif

extern bool
js_PopulateObject(JSContext *cx, js::HandleObject newborn, js::HandleObject props);

/*
 * Fast access to immutable standard objects (constructors and prototypes).
 */
extern bool
js_GetClassObject(js::ExclusiveContext *cx, JSObject *obj, JSProtoKey key,
                  js::MutableHandleObject objp);

/*
 * Determine if the given object is a prototype for a standard class. If so,
 * return the associated JSProtoKey. If not, return JSProto_Null.
 */
extern JSProtoKey
js_IdentifyClassPrototype(JSObject *obj);

/*
 * If protoKey is not JSProto_Null, then clasp is ignored. If protoKey is
 * JSProto_Null, clasp must non-null.
 */
bool
js_FindClassObject(js::ExclusiveContext *cx, JSProtoKey protoKey, js::MutableHandleValue vp,
                   const js::Class *clasp = nullptr);

/*
 * Find or create a property named by id in obj's scope, with the given getter
 * and setter, slot, attributes, and other members.
 */
extern js::Shape *
js_AddNativeProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                     JSPropertyOp getter, JSStrictPropertyOp setter, uint32_t slot,
                     unsigned attrs, unsigned flags, int shortid);

namespace js {

extern bool
DefineOwnProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                  JS::HandleValue descriptor, bool *bp);

extern bool
DefineOwnProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                  JS::Handle<js::PropertyDescriptor> descriptor, bool *bp);

/*
 * The NewObjectKind allows an allocation site to specify the type properties
 * and lifetime requirements that must be fixed at allocation time.
 */
enum NewObjectKind {
    /* This is the default. Most objects are generic. */
    GenericObject,

    /*
     * Singleton objects are treated specially by the type system. This flag
     * ensures that the new object is automatically set up correctly as a
     * singleton and is allocated in the correct heap.
     */
    SingletonObject,

    /*
     * Objects which may be marked as a singleton after allocation must still
     * be allocated on the correct heap, but are not automatically setup as a
     * singleton after allocation.
     */
    MaybeSingletonObject,

    /*
     * Objects which will not benefit from being allocated in the nursery
     * (e.g. because they are known to have a long lifetime) may be allocated
     * with this kind to place them immediately into the tenured generation.
     */
    TenuredObject
};

inline gc::InitialHeap
GetInitialHeap(NewObjectKind newKind, const Class *clasp)
{
    if (clasp->finalize || newKind != GenericObject)
        return gc::TenuredHeap;
    return gc::DefaultHeap;
}

// Specialized call for constructing |this| with a known function callee,
// and a known prototype.
extern JSObject *
CreateThisForFunctionWithProto(JSContext *cx, js::HandleObject callee, JSObject *proto,
                               NewObjectKind newKind = GenericObject);

// Specialized call for constructing |this| with a known function callee.
extern JSObject *
CreateThisForFunction(JSContext *cx, js::HandleObject callee, bool newType);

// Generic call for constructing |this|.
extern JSObject *
CreateThis(JSContext *cx, const js::Class *clasp, js::HandleObject callee);

extern JSObject *
CloneObject(JSContext *cx, HandleObject obj, Handle<js::TaggedProto> proto, HandleObject parent);

/*
 * Flags for the defineHow parameter of js_DefineNativeProperty.
 */
const unsigned DNP_DONT_PURGE   = 1;   /* suppress js_PurgeScopeChain */
const unsigned DNP_UNQUALIFIED  = 2;   /* Unqualified property set.  Only used in
                                       the defineHow argument of
                                       js_SetPropertyHelper. */
const unsigned DNP_SKIP_TYPE    = 4;   /* Don't update type information */

/*
 * Return successfully added or changed shape or nullptr on error.
 */
extern bool
DefineNativeProperty(ExclusiveContext *cx, HandleObject obj, HandleId id, HandleValue value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs,
                     unsigned flags, int shortid, unsigned defineHow = 0);

/*
 * Specialized subroutine that allows caller to preset JSRESOLVE_* flags.
 */
extern bool
LookupPropertyWithFlags(ExclusiveContext *cx, HandleObject obj, HandleId id, unsigned flags,
                        js::MutableHandleObject objp, js::MutableHandleShape propp);

/*
 * Call the [[DefineOwnProperty]] internal method of obj.
 *
 * If obj is an array, this follows ES5 15.4.5.1.
 * If obj is any other native object, this follows ES5 8.12.9.
 * If obj is a proxy, this calls the proxy handler's defineProperty method.
 * Otherwise, this reports an error and returns false.
 */
extern bool
DefineProperty(JSContext *cx, js::HandleObject obj,
               js::HandleId id, const PropDesc &desc, bool throwError,
               bool *rval);

bool
DefineProperties(JSContext *cx, HandleObject obj, HandleObject props);

/*
 * Read property descriptors from props, as for Object.defineProperties. See
 * ES5 15.2.3.7 steps 3-5.
 */
extern bool
ReadPropertyDescriptors(JSContext *cx, HandleObject props, bool checkAccessors,
                        AutoIdVector *ids, AutoPropDescArrayRooter *descs);

/*
 * Constant to pass to js_LookupPropertyWithFlags to infer bits from current
 * bytecode.
 */
static const unsigned RESOLVE_INFER = 0xffff;

/* Read the name using a dynamic lookup on the scopeChain. */
extern bool
LookupName(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
           MutableHandleObject objp, MutableHandleObject pobjp, MutableHandleShape propp);

extern bool
LookupNameNoGC(JSContext *cx, PropertyName *name, JSObject *scopeChain,
               JSObject **objp, JSObject **pobjp, Shape **propp);

/*
 * Like LookupName except returns the global object if 'name' is not found in
 * any preceding non-global scope.
 *
 * Additionally, pobjp and propp are not needed by callers so they are not
 * returned.
 */
extern bool
LookupNameWithGlobalDefault(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
                            MutableHandleObject objp);

}

extern JSObject *
js_FindVariableScope(JSContext *cx, JSFunction **funp);


namespace js {

bool
NativeGet(JSContext *cx, js::Handle<JSObject*> obj, js::Handle<JSObject*> pobj,
          js::Handle<js::Shape*> shape, js::MutableHandle<js::Value> vp);

template <js::ExecutionMode mode>
bool
NativeSet(typename js::ExecutionModeTraits<mode>::ContextType cx,
          js::Handle<JSObject*> obj, js::Handle<JSObject*> receiver,
          js::Handle<js::Shape*> shape, bool strict, js::MutableHandleValue vp);

bool
LookupPropertyPure(JSObject *obj, jsid id, JSObject **objp, Shape **propp);

bool
GetPropertyPure(ThreadSafeContext *cx, JSObject *obj, jsid id, Value *vp);

inline bool
GetPropertyPure(ThreadSafeContext *cx, JSObject *obj, PropertyName *name, Value *vp)
{
    return GetPropertyPure(cx, obj, NameToId(name), vp);
}

bool
GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id,
                         MutableHandle<PropertyDescriptor> desc);

bool
GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp);

bool
NewPropertyDescriptorObject(JSContext *cx, Handle<PropertyDescriptor> desc, MutableHandleValue vp);

/*
 * If obj has an already-resolved data property for id, return true and
 * store the property value in *vp.
 */
extern bool
HasDataProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp);

inline bool
HasDataProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp)
{
    return HasDataProperty(cx, obj, NameToId(name), vp);
}

extern bool
CheckAccess(JSContext *cx, JSObject *obj, HandleId id, JSAccessMode mode,
            MutableHandleValue v, unsigned *attrsp);

extern bool
IsDelegate(JSContext *cx, HandleObject obj, const Value &v, bool *result);

bool
GetObjectElementOperationPure(ThreadSafeContext *cx, JSObject *obj, const Value &prop, Value *vp);

/* Wrap boolean, number or string as Boolean, Number or String object. */
extern JSObject *
PrimitiveToObject(JSContext *cx, const Value &v);

} /* namespace js */

namespace js {

/*
 * Invokes the ES5 ToObject algorithm on vp, returning the result. If vp might
 * already be an object, use ToObject. reportCantConvert controls how null and
 * undefined errors are reported.
 */
extern JSObject *
ToObjectSlow(JSContext *cx, HandleValue vp, bool reportScanStack);

/* For object conversion in e.g. native functions. */
JS_ALWAYS_INLINE JSObject *
ToObject(JSContext *cx, HandleValue vp)
{
    if (vp.isObject())
        return &vp.toObject();
    return ToObjectSlow(cx, vp, false);
}

/* For converting stack values to objects. */
JS_ALWAYS_INLINE JSObject *
ToObjectFromStack(JSContext *cx, HandleValue vp)
{
    if (vp.isObject())
        return &vp.toObject();
    return ToObjectSlow(cx, vp, true);
}

extern JSObject *
CloneObjectLiteral(JSContext *cx, HandleObject parent, HandleObject srcObj);

} /* namespace js */

extern void
js_GetObjectSlotName(JSTracer *trc, char *buf, size_t bufsize);

extern bool
js_ReportGetterOnlyAssignment(JSContext *cx, bool strict);

extern unsigned
js_InferFlags(JSContext *cx, unsigned defaultFlags);


/*
 * If protoKey is not JSProto_Null, then clasp is ignored. If protoKey is
 * JSProto_Null, clasp must non-null.
 *
 * If protoKey is constant and scope is non-null, use GlobalObject's prototype
 * methods instead.
 */
extern bool
js_GetClassPrototype(js::ExclusiveContext *cx, JSProtoKey protoKey, js::MutableHandleObject protop,
                     const js::Class *clasp = nullptr);

namespace js {

JSObject *
GetClassPrototypePure(GlobalObject *global, JSProtoKey protoKey);

extern bool
SetClassAndProto(JSContext *cx, HandleObject obj,
                 const Class *clasp, Handle<TaggedProto> proto, bool checkForCycles);

extern JSObject *
NonNullObject(JSContext *cx, const Value &v);

extern const char *
InformalValueTypeName(const Value &v);

extern bool
GetFirstArgumentAsObject(JSContext *cx, const CallArgs &args, const char *method,
                         MutableHandleObject objp);

/* Helpers for throwing. These always return false. */
extern bool
Throw(JSContext *cx, jsid id, unsigned errorNumber);

extern bool
Throw(JSContext *cx, JSObject *obj, unsigned errorNumber);

}  /* namespace js */

#endif /* jsobj_h */
