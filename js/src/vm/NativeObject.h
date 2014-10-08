/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_NativeObject_h
#define vm_NativeObject_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stdint.h>

#include "jsfriendapi.h"
#include "jsinfer.h"
#include "jsobj.h"
#include "NamespaceImports.h"

#include "gc/Barrier.h"
#include "gc/Heap.h"
#include "gc/Marking.h"
#include "js/Value.h"
#include "vm/NumericConversions.h"
#include "vm/Shape.h"
#include "vm/String.h"

namespace js {

class Nursery;
class Shape;

namespace gc {
class ForkJoinNursery;
}

/*
 * To really poison a set of values, using 'magic' or 'undefined' isn't good
 * enough since often these will just be ignored by buggy code (see bug 629974)
 * in debug builds and crash in release builds. Instead, we use a safe-for-crash
 * pointer.
 */
static MOZ_ALWAYS_INLINE void
Debug_SetValueRangeToCrashOnTouch(Value *beg, Value *end)
{
#ifdef DEBUG
    for (Value *v = beg; v != end; ++v)
        v->setObject(*reinterpret_cast<JSObject *>(0x42));
#endif
}

static MOZ_ALWAYS_INLINE void
Debug_SetValueRangeToCrashOnTouch(Value *vec, size_t len)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch(vec, vec + len);
#endif
}

static MOZ_ALWAYS_INLINE void
Debug_SetValueRangeToCrashOnTouch(HeapValue *vec, size_t len)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) vec, len);
#endif
}

static MOZ_ALWAYS_INLINE void
Debug_SetSlotRangeToCrashOnTouch(HeapSlot *vec, uint32_t len)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) vec, len);
#endif
}

static MOZ_ALWAYS_INLINE void
Debug_SetSlotRangeToCrashOnTouch(HeapSlot *begin, HeapSlot *end)
{
#ifdef DEBUG
    Debug_SetValueRangeToCrashOnTouch((Value *) begin, end - begin);
#endif
}

class ArrayObject;

/*
 * ES6 20130308 draft 8.4.2.4 ArraySetLength.
 *
 * |id| must be "length", |attrs| are the attributes to be used for the newly-
 * changed length property, |value| is the value for the new length, and
 * |setterIsStrict| indicates whether invalid changes will cause a TypeError
 * to be thrown.
 */
template <ExecutionMode mode>
extern bool
ArraySetLength(typename ExecutionModeTraits<mode>::ContextType cx,
               Handle<ArrayObject*> obj, HandleId id,
               unsigned attrs, HandleValue value, bool setterIsStrict);

/*
 * Elements header used for native objects. The elements component of such objects
 * offers an efficient representation for all or some of the indexed properties
 * of the object, using a flat array of Values rather than a shape hierarchy
 * stored in the object's slots. This structure is immediately followed by an
 * array of elements, with the elements member in an object pointing to the
 * beginning of that array (the end of this structure).
 * See below for usage of this structure.
 *
 * The sets of properties represented by an object's elements and slots
 * are disjoint. The elements contain only indexed properties, while the slots
 * can contain both named and indexed properties; any indexes in the slots are
 * distinct from those in the elements. If isIndexed() is false for an object,
 * all indexed properties (if any) are stored in the dense elements.
 *
 * Indexes will be stored in the object's slots instead of its elements in
 * the following case:
 *  - there are more than MIN_SPARSE_INDEX slots total and the load factor
 *    (COUNT / capacity) is less than 0.25
 *  - a property is defined that has non-default property attributes.
 *
 * We track these pieces of metadata for dense elements:
 *  - The length property as a uint32_t, accessible for array objects with
 *    ArrayObject::{length,setLength}().  This is unused for non-arrays.
 *  - The number of element slots (capacity), gettable with
 *    getDenseElementsCapacity().
 *  - The array's initialized length, accessible with
 *    getDenseElementsInitializedLength().
 *
 * Holes in the array are represented by MagicValue(JS_ELEMENTS_HOLE) values.
 * These indicate indexes which are not dense properties of the array. The
 * property may, however, be held by the object's properties.
 *
 * The capacity and length of an object's elements are almost entirely
 * unrelated!  In general the length may be greater than, less than, or equal
 * to the capacity.  The first case occurs with |new Array(100)|.  The length
 * is 100, but the capacity remains 0 (indices below length and above capacity
 * must be treated as holes) until elements between capacity and length are
 * set.  The other two cases are common, depending upon the number of elements
 * in an array and the underlying allocator used for element storage.
 *
 * The only case in which the capacity and length of an object's elements are
 * related is when the object is an array with non-writable length.  In this
 * case the capacity is always less than or equal to the length.  This permits
 * JIT code to optimize away the check for non-writable length when assigning
 * to possibly out-of-range elements: such code already has to check for
 * |index < capacity|, and fallback code checks for non-writable length.
 *
 * The initialized length of an object specifies the number of elements that
 * have been initialized. All elements above the initialized length are
 * holes in the object, and the memory for all elements between the initialized
 * length and capacity is left uninitialized. The initialized length is some
 * value less than or equal to both the object's length and the object's
 * capacity.
 *
 * There is flexibility in exactly the value the initialized length must hold,
 * e.g. if an array has length 5, capacity 10, completely empty, it is valid
 * for the initialized length to be any value between zero and 5, as long as
 * the in memory values below the initialized length have been initialized with
 * a hole value. However, in such cases we want to keep the initialized length
 * as small as possible: if the object is known to have no hole values below
 * its initialized length, then it is "packed" and can be accessed much faster
 * by JIT code.
 *
 * Elements do not track property creation order, so enumerating the elements
 * of an object does not necessarily visit indexes in the order they were
 * created.
 */
class ObjectElements
{
  public:
    enum Flags {
        // Integers written to these elements must be converted to doubles.
        CONVERT_DOUBLE_ELEMENTS     = 0x1,

        // Present only if these elements correspond to an array with
        // non-writable length; never present for non-arrays.
        NONWRITABLE_ARRAY_LENGTH    = 0x2,

        // These elements are shared with another object and must be copied
        // before they can be changed. A pointer to the original owner of the
        // elements, which is immutable, is stored immediately after the
        // elements data. There is one case where elements can be written to
        // before being copied: when setting the CONVERT_DOUBLE_ELEMENTS flag
        // the shared elements may change (from ints to doubles) without
        // making a copy first.
        COPY_ON_WRITE               = 0x4
    };

  private:
    friend class ::JSObject;
    friend class NativeObject;
    friend class ArrayObject;
    friend class Nursery;
    friend class gc::ForkJoinNursery;

    template <ExecutionMode mode>
    friend bool
    ArraySetLength(typename ExecutionModeTraits<mode>::ContextType cx,
                   Handle<ArrayObject*> obj, HandleId id,
                   unsigned attrs, HandleValue value, bool setterIsStrict);

    /* See Flags enum above. */
    uint32_t flags;

    /*
     * Number of initialized elements. This is <= the capacity, and for arrays
     * is <= the length. Memory for elements above the initialized length is
     * uninitialized, but values between the initialized length and the proper
     * length are conceptually holes.
     */
    uint32_t initializedLength;

    /* Number of allocated slots. */
    uint32_t capacity;

    /* 'length' property of array objects, unused for other objects. */
    uint32_t length;

    bool shouldConvertDoubleElements() const {
        return flags & CONVERT_DOUBLE_ELEMENTS;
    }
    void setShouldConvertDoubleElements() {
        // Note: allow isCopyOnWrite() here, see comment above.
        flags |= CONVERT_DOUBLE_ELEMENTS;
    }
    void clearShouldConvertDoubleElements() {
        MOZ_ASSERT(!isCopyOnWrite());
        flags &= ~CONVERT_DOUBLE_ELEMENTS;
    }
    bool hasNonwritableArrayLength() const {
        return flags & NONWRITABLE_ARRAY_LENGTH;
    }
    void setNonwritableArrayLength() {
        MOZ_ASSERT(!isCopyOnWrite());
        flags |= NONWRITABLE_ARRAY_LENGTH;
    }
    bool isCopyOnWrite() const {
        return flags & COPY_ON_WRITE;
    }
    void clearCopyOnWrite() {
        MOZ_ASSERT(isCopyOnWrite());
        flags &= ~COPY_ON_WRITE;
    }

  public:
    MOZ_CONSTEXPR ObjectElements(uint32_t capacity, uint32_t length)
      : flags(0), initializedLength(0), capacity(capacity), length(length)
    {}

    HeapSlot *elements() {
        return reinterpret_cast<HeapSlot*>(uintptr_t(this) + sizeof(ObjectElements));
    }
    const HeapSlot *elements() const {
        return reinterpret_cast<const HeapSlot*>(uintptr_t(this) + sizeof(ObjectElements));
    }
    static ObjectElements * fromElements(HeapSlot *elems) {
        return reinterpret_cast<ObjectElements*>(uintptr_t(elems) - sizeof(ObjectElements));
    }

    HeapPtrNativeObject &ownerObject() const {
        MOZ_ASSERT(isCopyOnWrite());
        return *(HeapPtrNativeObject *)(&elements()[initializedLength]);
    }

    static int offsetOfFlags() {
        return int(offsetof(ObjectElements, flags)) - int(sizeof(ObjectElements));
    }
    static int offsetOfInitializedLength() {
        return int(offsetof(ObjectElements, initializedLength)) - int(sizeof(ObjectElements));
    }
    static int offsetOfCapacity() {
        return int(offsetof(ObjectElements, capacity)) - int(sizeof(ObjectElements));
    }
    static int offsetOfLength() {
        return int(offsetof(ObjectElements, length)) - int(sizeof(ObjectElements));
    }

    static bool ConvertElementsToDoubles(JSContext *cx, uintptr_t elements);
    static bool MakeElementsCopyOnWrite(ExclusiveContext *cx, NativeObject *obj);

    // This is enough slots to store an object of this class. See the static
    // assertion below.
    static const size_t VALUES_PER_HEADER = 2;
};

static_assert(ObjectElements::VALUES_PER_HEADER * sizeof(HeapSlot) == sizeof(ObjectElements),
              "ObjectElements doesn't fit in the given number of slots");

/* Shared singleton for objects with no elements. */
extern HeapSlot *const emptyObjectElements;

struct Class;
class GCMarker;
struct ObjectOps;
class Shape;

class NewObjectCache;
class TaggedProto;

#ifdef DEBUG
static inline bool
IsObjectValueInCompartment(Value v, JSCompartment *comp);
#endif

/*
 * NativeObject specifies the internal implementation of a native object.
 *
 * Native objects extend the base implementation of an object with storage
 * for the object's named properties and indexed elements.
 *
 * These are stored separately from one another. Objects are followed by a
 * variable-sized array of values for inline storage, which may be used by
 * either properties of native objects (fixed slots), by elements (fixed
 * elements), or by other data for certain kinds of objects, such as
 * ArrayBufferObjects and TypedArrayObjects.
 *
 * Two native objects with the same shape are guaranteed to have the same
 * number of fixed slots.
 *
 * Named property storage can be split between fixed slots and a dynamically
 * allocated array (the slots member). For an object with N fixed slots, shapes
 * with slots [0..N-1] are stored in the fixed slots, and the remainder are
 * stored in the dynamic array. If all properties fit in the fixed slots, the
 * 'slots' member is nullptr.
 *
 * Elements are indexed via the 'elements' member. This member can point to
 * either the shared emptyObjectElements singleton, into the inline value array
 * (the address of the third value, to leave room for a ObjectElements header;
 * in this case numFixedSlots() is zero) or to a dynamically allocated array.
 *
 * Slots and elements may both be non-empty. The slots may be either names or
 * indexes; no indexed property will be in both the slots and elements.
 */
class NativeObject : public JSObject
{
  protected:
    friend bool
    ArraySetLength(JSContext *cx, Handle<ArrayObject*> obj, HandleId id, unsigned attrs,
                   HandleValue value, bool setterIsStrict);

    // FIXME Bug 1073842: this is temporary until non-native objects can
    // access non-slot storage.
    friend class ::JSObject;

  private:
    static void staticAsserts() {
        static_assert(sizeof(NativeObject) == sizeof(shadow::Object),
                      "shadow interface must match actual implementation");
        static_assert(sizeof(NativeObject) % sizeof(Value) == 0,
                      "fixed slots after an object must be aligned");

        static_assert(offsetof(NativeObject, shape_) == offsetof(shadow::Object, shape),
                      "shadow shape must match actual shape");
        static_assert(offsetof(NativeObject, type_) == offsetof(shadow::Object, type),
                      "shadow type must match actual type");
        static_assert(offsetof(NativeObject, slots) == offsetof(shadow::Object, slots),
                      "shadow slots must match actual slots");
        static_assert(offsetof(NativeObject, elements) == offsetof(shadow::Object, _1),
                      "shadow placeholder must match actual elements");

        static_assert(js::shadow::Object::MAX_FIXED_SLOTS == MAX_FIXED_SLOTS,
                      "We shouldn't be confused about our actual maximum "
                      "number of fixed slots");
    }

  public:
    HeapSlotArray getDenseElements() {
        return HeapSlotArray(elements, !getElementsHeader()->isCopyOnWrite());
    }
    HeapSlotArray getDenseElementsAllowCopyOnWrite() {
        // Backdoor allowing direct access to copy on write elements.
        return HeapSlotArray(elements, true);
    }
    const Value &getDenseElement(uint32_t idx) {
        MOZ_ASSERT(idx < getDenseInitializedLength());
        return elements[idx];
    }
    bool containsDenseElement(uint32_t idx) {
        return idx < getDenseInitializedLength() && !elements[idx].isMagic(JS_ELEMENTS_HOLE);
    }
    uint32_t getDenseInitializedLength() {
        return getElementsHeader()->initializedLength;
    }
    uint32_t getDenseCapacity() {
        return getElementsHeader()->capacity;
    }

    /*
     * Update the last property, keeping the number of allocated slots in sync
     * with the object's new slot span.
     */
    static bool setLastProperty(ThreadSafeContext *cx,
                                HandleNativeObject obj, HandleShape shape);

    // As for setLastProperty(), but allows the number of fixed slots to
    // change. This can only be used when fixed slots are being erased from the
    // object, and only when the object will not require dynamic slots to cover
    // the new properties.
    void setLastPropertyShrinkFixedSlots(Shape *shape);

  protected:
#ifdef DEBUG
    void checkShapeConsistency();
#else
    void checkShapeConsistency() { }
#endif

    Shape *
    replaceWithNewEquivalentShape(ThreadSafeContext *cx,
                                  Shape *existingShape, Shape *newShape = nullptr);

    /*
     * Remove the last property of an object, provided that it is safe to do so
     * (the shape and previous shape do not carry conflicting information about
     * the object itself).
     */
    inline void removeLastProperty(ExclusiveContext *cx);
    inline bool canRemoveLastProperty();

    /*
     * Update the slot span directly for a dictionary object, and allocate
     * slots to cover the new span if necessary.
     */
    static bool setSlotSpan(ThreadSafeContext *cx, HandleNativeObject obj, uint32_t span);

    bool toDictionaryMode(ThreadSafeContext *cx);

  private:
    friend class Nursery;
    friend class gc::ForkJoinNursery;

    /*
     * Get internal pointers to the range of values starting at start and
     * running for length.
     */
    void getSlotRangeUnchecked(uint32_t start, uint32_t length,
                               HeapSlot **fixedStart, HeapSlot **fixedEnd,
                               HeapSlot **slotsStart, HeapSlot **slotsEnd)
    {
        MOZ_ASSERT(start + length >= start);

        uint32_t fixed = numFixedSlots();
        if (start < fixed) {
            if (start + length < fixed) {
                *fixedStart = &fixedSlots()[start];
                *fixedEnd = &fixedSlots()[start + length];
                *slotsStart = *slotsEnd = nullptr;
            } else {
                uint32_t localCopy = fixed - start;
                *fixedStart = &fixedSlots()[start];
                *fixedEnd = &fixedSlots()[start + localCopy];
                *slotsStart = &slots[0];
                *slotsEnd = &slots[length - localCopy];
            }
        } else {
            *fixedStart = *fixedEnd = nullptr;
            *slotsStart = &slots[start - fixed];
            *slotsEnd = &slots[start - fixed + length];
        }
    }

    void getSlotRange(uint32_t start, uint32_t length,
                      HeapSlot **fixedStart, HeapSlot **fixedEnd,
                      HeapSlot **slotsStart, HeapSlot **slotsEnd)
    {
        MOZ_ASSERT(slotInRange(start + length, SENTINEL_ALLOWED));
        getSlotRangeUnchecked(start, length, fixedStart, fixedEnd, slotsStart, slotsEnd);
    }

  protected:
    friend class GCMarker;
    friend class Shape;
    friend class NewObjectCache;

    void invalidateSlotRange(uint32_t start, uint32_t length) {
#ifdef DEBUG
        HeapSlot *fixedStart, *fixedEnd, *slotsStart, *slotsEnd;
        getSlotRange(start, length, &fixedStart, &fixedEnd, &slotsStart, &slotsEnd);
        Debug_SetSlotRangeToCrashOnTouch(fixedStart, fixedEnd);
        Debug_SetSlotRangeToCrashOnTouch(slotsStart, slotsEnd);
#endif /* DEBUG */
    }

    void initializeSlotRange(uint32_t start, uint32_t count);

    /*
     * Initialize a flat array of slots to this object at a start slot.  The
     * caller must ensure that are enough slots.
     */
    void initSlotRange(uint32_t start, const Value *vector, uint32_t length);

    /*
     * Copy a flat array of slots to this object at a start slot. Caller must
     * ensure there are enough slots in this object.
     */
    void copySlotRange(uint32_t start, const Value *vector, uint32_t length);

#ifdef DEBUG
    enum SentinelAllowed {
        SENTINEL_NOT_ALLOWED,
        SENTINEL_ALLOWED
    };

    /*
     * Check that slot is in range for the object's allocated slots.
     * If sentinelAllowed then slot may equal the slot capacity.
     */
    bool slotInRange(uint32_t slot, SentinelAllowed sentinel = SENTINEL_NOT_ALLOWED) const;
#endif

    /*
     * Minimum size for dynamically allocated slots in normal Objects.
     * ArrayObjects don't use this limit and can have a lower slot capacity,
     * since they normally don't have a lot of slots.
     */
    static const uint32_t SLOT_CAPACITY_MIN = 8;

    HeapSlot *fixedSlots() const {
        return reinterpret_cast<HeapSlot *>(uintptr_t(this) + sizeof(NativeObject));
    }

  public:
    bool generateOwnShape(ThreadSafeContext *cx, Shape *newShape = nullptr) {
        return replaceWithNewEquivalentShape(cx, lastProperty(), newShape);
    }

    bool shadowingShapeChange(ExclusiveContext *cx, const Shape &shape);
    bool clearFlag(ExclusiveContext *cx, BaseShape::Flag flag);

    uint32_t numFixedSlots() const {
        return reinterpret_cast<const shadow::Object *>(this)->numFixedSlots();
    }
    uint32_t numUsedFixedSlots() const {
        uint32_t nslots = lastProperty()->slotSpan(getClass());
        return Min(nslots, numFixedSlots());
    }

    uint32_t slotSpan() const {
        if (inDictionaryMode())
            return lastProperty()->base()->slotSpan();
        return lastProperty()->slotSpan();
    }

    /* Whether a slot is at a fixed offset from this object. */
    bool isFixedSlot(size_t slot) {
        return slot < numFixedSlots();
    }

    /* Index into the dynamic slots array to use for a dynamic slot. */
    size_t dynamicSlotIndex(size_t slot) {
        MOZ_ASSERT(slot >= numFixedSlots());
        return slot - numFixedSlots();
    }

    /*
     * Grow or shrink slots immediately before changing the slot span.
     * The number of allocated slots is not stored explicitly, and changes to
     * the slots must track changes in the slot span.
     */
    static bool growSlots(ThreadSafeContext *cx, HandleNativeObject obj, uint32_t oldCount,
                          uint32_t newCount);
    static void shrinkSlots(ThreadSafeContext *cx, HandleNativeObject obj, uint32_t oldCount,
                            uint32_t newCount);

    bool hasDynamicSlots() const { return !!slots; }

    /* Compute dynamicSlotsCount() for this object. */
    uint32_t numDynamicSlots() const {
        return dynamicSlotsCount(numFixedSlots(), slotSpan(), getClass());
    }

    bool empty() const {
        return lastProperty()->isEmptyShape();
    }

    Shape *lookup(ExclusiveContext *cx, jsid id);
    Shape *lookup(ExclusiveContext *cx, PropertyName *name) {
        return lookup(cx, NameToId(name));
    }

    bool contains(ExclusiveContext *cx, jsid id) {
        return lookup(cx, id) != nullptr;
    }
    bool contains(ExclusiveContext *cx, PropertyName* name) {
        return lookup(cx, name) != nullptr;
    }
    bool contains(ExclusiveContext *cx, Shape* shape) {
        return lookup(cx, shape->propid()) == shape;
    }

    /* Contextless; can be called from parallel code. */
    Shape *lookupPure(jsid id);
    Shape *lookupPure(PropertyName *name) {
        return lookupPure(NameToId(name));
    }

    bool containsPure(jsid id) {
        return lookupPure(id) != nullptr;
    }
    bool containsPure(PropertyName* name) {
        return containsPure(NameToId(name));
    }
    bool containsPure(Shape* shape) {
        return lookupPure(shape->propid()) == shape;
    }

    /*
     * Allocate and free an object slot.
     *
     * FIXME: bug 593129 -- slot allocation should be done by object methods
     * after calling object-parameter-free shape methods, avoiding coupling
     * logic across the object vs. shape module wall.
     */
    static bool allocSlot(ThreadSafeContext *cx, HandleNativeObject obj, uint32_t *slotp);
    void freeSlot(uint32_t slot);

  private:
    static Shape *getChildPropertyOnDictionary(ThreadSafeContext *cx, HandleNativeObject obj,
                                               HandleShape parent, StackShape &child);
    static Shape *getChildProperty(ExclusiveContext *cx, HandleNativeObject obj,
                                   HandleShape parent, StackShape &child);
    template <ExecutionMode mode>
    static inline Shape *
    getOrLookupChildProperty(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                             HandleNativeObject obj, HandleShape parent, StackShape &child)
    {
        if (mode == ParallelExecution)
            return lookupChildProperty(cx, obj, parent, child);
        return getChildProperty(cx->asExclusiveContext(), obj, parent, child);
    }

  public:
    /*
     * XXX: This should be private, but is public because it needs to be a
     * friend of ThreadSafeContext to get to the propertyTree on cx->compartment_.
     */
    static Shape *lookupChildProperty(ThreadSafeContext *cx, HandleNativeObject obj,
                                      HandleShape parent, StackShape &child);

    /* Add a property whose id is not yet in this scope. */
    static Shape *addProperty(ExclusiveContext *cx, HandleNativeObject obj, HandleId id,
                              JSPropertyOp getter, JSStrictPropertyOp setter,
                              uint32_t slot, unsigned attrs, unsigned flags,
                              bool allowDictionary = true);

    /* Add a data property whose id is not yet in this scope. */
    Shape *addDataProperty(ExclusiveContext *cx,
                           jsid id_, uint32_t slot, unsigned attrs);
    Shape *addDataProperty(ExclusiveContext *cx, HandlePropertyName name,
                           uint32_t slot, unsigned attrs);

    /* Add or overwrite a property for id in this scope. */
    template <ExecutionMode mode>
    static Shape *
    putProperty(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                HandleNativeObject obj, HandleId id,
                JSPropertyOp getter, JSStrictPropertyOp setter,
                uint32_t slot, unsigned attrs,
                unsigned flags);
    template <ExecutionMode mode>
    static inline Shape *
    putProperty(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                HandleObject obj, PropertyName *name,
                JSPropertyOp getter, JSStrictPropertyOp setter,
                uint32_t slot, unsigned attrs,
                unsigned flags);

    /* Change the given property into a sibling with the same id in this scope. */
    template <ExecutionMode mode>
    static Shape *
    changeProperty(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                   HandleNativeObject obj, HandleShape shape, unsigned attrs, unsigned mask,
                   JSPropertyOp getter, JSStrictPropertyOp setter);

    static inline bool changePropertyAttributes(JSContext *cx, HandleNativeObject obj,
                                                HandleShape shape, unsigned attrs);

    /* Remove the property named by id from this object. */
    bool removeProperty(ExclusiveContext *cx, jsid id);

    /* Clear the scope, making it empty. */
    static void clear(JSContext *cx, HandleNativeObject obj);

  protected:
    /*
     * Internal helper that adds a shape not yet mapped by this object.
     *
     * Notes:
     * 1. getter and setter must be normalized based on flags (see jsscope.cpp).
     * 2. Checks for non-extensibility must be done by callers.
     */
    template <ExecutionMode mode>
    static Shape *
    addPropertyInternal(typename ExecutionModeTraits<mode>::ExclusiveContextType cx,
                        HandleNativeObject obj, HandleId id,
                        JSPropertyOp getter, JSStrictPropertyOp setter,
                        uint32_t slot, unsigned attrs, unsigned flags, Shape **spp,
                        bool allowDictionary);

  public:
    // Return true if this object has been converted from shared-immutable
    // prototype-rooted shape storage to dictionary-shapes in a doubly-linked
    // list.
    bool inDictionaryMode() const {
        return lastProperty()->inDictionary();
    }

    const Value &getSlot(uint32_t slot) const {
        MOZ_ASSERT(slotInRange(slot));
        uint32_t fixed = numFixedSlots();
        if (slot < fixed)
            return fixedSlots()[slot];
        return slots[slot - fixed];
    }

    const HeapSlot *getSlotAddressUnchecked(uint32_t slot) const {
        uint32_t fixed = numFixedSlots();
        if (slot < fixed)
            return fixedSlots() + slot;
        return slots + (slot - fixed);
    }

    HeapSlot *getSlotAddressUnchecked(uint32_t slot) {
        uint32_t fixed = numFixedSlots();
        if (slot < fixed)
            return fixedSlots() + slot;
        return slots + (slot - fixed);
    }

    HeapSlot *getSlotAddress(uint32_t slot) {
        /*
         * This can be used to get the address of the end of the slots for the
         * object, which may be necessary when fetching zero-length arrays of
         * slots (e.g. for callObjVarArray).
         */
        MOZ_ASSERT(slotInRange(slot, SENTINEL_ALLOWED));
        return getSlotAddressUnchecked(slot);
    }

    const HeapSlot *getSlotAddress(uint32_t slot) const {
        /*
         * This can be used to get the address of the end of the slots for the
         * object, which may be necessary when fetching zero-length arrays of
         * slots (e.g. for callObjVarArray).
         */
        MOZ_ASSERT(slotInRange(slot, SENTINEL_ALLOWED));
        return getSlotAddressUnchecked(slot);
    }

    HeapSlot &getSlotRef(uint32_t slot) {
        MOZ_ASSERT(slotInRange(slot));
        return *getSlotAddress(slot);
    }

    const HeapSlot &getSlotRef(uint32_t slot) const {
        MOZ_ASSERT(slotInRange(slot));
        return *getSlotAddress(slot);
    }

    void setSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slotInRange(slot));
        MOZ_ASSERT(IsObjectValueInCompartment(value, compartment()));
        getSlotRef(slot).set(this, HeapSlot::Slot, slot, value);
    }

    MOZ_ALWAYS_INLINE void setCrossCompartmentSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slotInRange(slot));
        getSlotRef(slot).set(this, HeapSlot::Slot, slot, value);
    }

    void initSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(getSlot(slot).isUndefined());
        MOZ_ASSERT(slotInRange(slot));
        MOZ_ASSERT(IsObjectValueInCompartment(value, compartment()));
        initSlotUnchecked(slot, value);
    }

    void initCrossCompartmentSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(getSlot(slot).isUndefined());
        MOZ_ASSERT(slotInRange(slot));
        initSlotUnchecked(slot, value);
    }

    void initSlotUnchecked(uint32_t slot, const Value &value) {
        getSlotAddressUnchecked(slot)->init(this, HeapSlot::Slot, slot, value);
    }

    // MAX_FIXED_SLOTS is the biggest number of fixed slots our GC
    // size classes will give an object.
    static const uint32_t MAX_FIXED_SLOTS = 16;

  protected:
    static inline bool updateSlotsForSpan(ThreadSafeContext *cx,
                                          HandleNativeObject obj, size_t oldSpan, size_t newSpan);

  public:
    /*
     * Trigger the write barrier on a range of slots that will no longer be
     * reachable.
     */
    void prepareSlotRangeForOverwrite(size_t start, size_t end) {
        for (size_t i = start; i < end; i++)
            getSlotAddressUnchecked(i)->HeapSlot::~HeapSlot();
    }

    void prepareElementRangeForOverwrite(size_t start, size_t end) {
        MOZ_ASSERT(end <= getDenseInitializedLength());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        for (size_t i = start; i < end; i++)
            elements[i].HeapSlot::~HeapSlot();
    }

    static bool rollbackProperties(ExclusiveContext *cx, HandleNativeObject obj,
                                   uint32_t slotSpan);

    inline bool setSlotIfHasType(Shape *shape, const Value &value,
                                 bool overwriting = true);
    inline void setSlotWithType(ExclusiveContext *cx, Shape *shape,
                                const Value &value, bool overwriting = true);

    inline const Value &getReservedSlot(uint32_t index) const {
        MOZ_ASSERT(index < JSSLOT_FREE(getClass()));
        return getSlot(index);
    }

    const HeapSlot &getReservedSlotRef(uint32_t index) const {
        MOZ_ASSERT(index < JSSLOT_FREE(getClass()));
        return getSlotRef(index);
    }

    HeapSlot &getReservedSlotRef(uint32_t index) {
        MOZ_ASSERT(index < JSSLOT_FREE(getClass()));
        return getSlotRef(index);
    }

    void initReservedSlot(uint32_t index, const Value &v) {
        MOZ_ASSERT(index < JSSLOT_FREE(getClass()));
        initSlot(index, v);
    }

    void setReservedSlot(uint32_t index, const Value &v) {
        MOZ_ASSERT(index < JSSLOT_FREE(getClass()));
        setSlot(index, v);
    }

    /* For slots which are known to always be fixed, due to the way they are allocated. */

    HeapSlot &getFixedSlotRef(uint32_t slot) {
        MOZ_ASSERT(slot < numFixedSlots());
        return fixedSlots()[slot];
    }

    const Value &getFixedSlot(uint32_t slot) const {
        MOZ_ASSERT(slot < numFixedSlots());
        return fixedSlots()[slot];
    }

    void setFixedSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slot < numFixedSlots());
        fixedSlots()[slot].set(this, HeapSlot::Slot, slot, value);
    }

    void initFixedSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slot < numFixedSlots());
        fixedSlots()[slot].init(this, HeapSlot::Slot, slot, value);
    }

    /*
     * Get the number of dynamic slots to allocate to cover the properties in
     * an object with the given number of fixed slots and slot span. The slot
     * capacity is not stored explicitly, and the allocated size of the slot
     * array is kept in sync with this count.
     */
    static uint32_t dynamicSlotsCount(uint32_t nfixed, uint32_t span, const Class *clasp);

    /* Elements accessors. */

    /* Upper bound on the number of elements in an object. */
    static const uint32_t NELEMENTS_LIMIT = JS_BIT(28);

    ObjectElements * getElementsHeader() const {
        return ObjectElements::fromElements(elements);
    }

    /* Accessors for elements. */
    bool ensureElements(ThreadSafeContext *cx, uint32_t capacity) {
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        if (capacity > getDenseCapacity())
            return growElements(cx, capacity);
        return true;
    }

    static uint32_t goodAllocated(uint32_t n, uint32_t length);
    bool growElements(ThreadSafeContext *cx, uint32_t newcap);
    void shrinkElements(ThreadSafeContext *cx, uint32_t cap);
    void setDynamicElements(ObjectElements *header) {
        MOZ_ASSERT(!hasDynamicElements());
        elements = header->elements();
        MOZ_ASSERT(hasDynamicElements());
    }

    static bool CopyElementsForWrite(ThreadSafeContext *cx, NativeObject *obj);

    bool maybeCopyElementsForWrite(ThreadSafeContext *cx) {
        if (denseElementsAreCopyOnWrite())
            return CopyElementsForWrite(cx, this);
        return true;
    }

  private:
    inline void ensureDenseInitializedLengthNoPackedCheck(ThreadSafeContext *cx,
                                                          uint32_t index, uint32_t extra);

  public:
    void setDenseInitializedLength(uint32_t length) {
        MOZ_ASSERT(length <= getDenseCapacity());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        prepareElementRangeForOverwrite(length, getElementsHeader()->initializedLength);
        getElementsHeader()->initializedLength = length;
    }

    inline void ensureDenseInitializedLength(ExclusiveContext *cx,
                                             uint32_t index, uint32_t extra);
    inline void ensureDenseInitializedLengthPreservePackedFlag(ThreadSafeContext *cx,
                                                               uint32_t index, uint32_t extra);
    void setDenseElement(uint32_t index, const Value &val) {
        MOZ_ASSERT(index < getDenseInitializedLength());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        elements[index].set(this, HeapSlot::Element, index, val);
    }

    void initDenseElement(uint32_t index, const Value &val) {
        MOZ_ASSERT(index < getDenseInitializedLength());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        elements[index].init(this, HeapSlot::Element, index, val);
    }

    void setDenseElementMaybeConvertDouble(uint32_t index, const Value &val) {
        if (val.isInt32() && shouldConvertDoubleElements())
            setDenseElement(index, DoubleValue(val.toInt32()));
        else
            setDenseElement(index, val);
    }

    inline bool setDenseElementIfHasType(uint32_t index, const Value &val);
    inline void setDenseElementWithType(ExclusiveContext *cx, uint32_t index,
                                        const Value &val);
    inline void initDenseElementWithType(ExclusiveContext *cx, uint32_t index,
                                         const Value &val);
    inline void setDenseElementHole(ExclusiveContext *cx, uint32_t index);
    static inline void removeDenseElementForSparseIndex(ExclusiveContext *cx,
                                                        HandleNativeObject obj, uint32_t index);

    inline Value getDenseOrTypedArrayElement(uint32_t idx);

    void copyDenseElements(uint32_t dstStart, const Value *src, uint32_t count) {
        MOZ_ASSERT(dstStart + count <= getDenseCapacity());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        JSRuntime *rt = runtimeFromMainThread();
        if (JS::IsIncrementalBarrierNeeded(rt)) {
            Zone *zone = this->zone();
            for (uint32_t i = 0; i < count; ++i)
                elements[dstStart + i].set(zone, this, HeapSlot::Element, dstStart + i, src[i]);
        } else {
            memcpy(&elements[dstStart], src, count * sizeof(HeapSlot));
            DenseRangeWriteBarrierPost(rt, this, dstStart, count);
        }
    }

    void initDenseElements(uint32_t dstStart, const Value *src, uint32_t count) {
        MOZ_ASSERT(dstStart + count <= getDenseCapacity());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());
        memcpy(&elements[dstStart], src, count * sizeof(HeapSlot));
        DenseRangeWriteBarrierPost(runtimeFromMainThread(), this, dstStart, count);
    }

    void initDenseElementsUnbarriered(uint32_t dstStart, const Value *src, uint32_t count);

    void moveDenseElements(uint32_t dstStart, uint32_t srcStart, uint32_t count) {
        MOZ_ASSERT(dstStart + count <= getDenseCapacity());
        MOZ_ASSERT(srcStart + count <= getDenseInitializedLength());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());

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
        Zone *zone = this->zone();
        JS::shadow::Zone *shadowZone = JS::shadow::Zone::asShadowZone(zone);
        if (shadowZone->needsIncrementalBarrier()) {
            if (dstStart < srcStart) {
                HeapSlot *dst = elements + dstStart;
                HeapSlot *src = elements + srcStart;
                for (uint32_t i = 0; i < count; i++, dst++, src++)
                    dst->set(zone, this, HeapSlot::Element, dst - elements, *src);
            } else {
                HeapSlot *dst = elements + dstStart + count - 1;
                HeapSlot *src = elements + srcStart + count - 1;
                for (uint32_t i = 0; i < count; i++, dst--, src--)
                    dst->set(zone, this, HeapSlot::Element, dst - elements, *src);
            }
        } else {
            memmove(elements + dstStart, elements + srcStart, count * sizeof(HeapSlot));
            DenseRangeWriteBarrierPost(runtimeFromMainThread(), this, dstStart, count);
        }
    }

    void moveDenseElementsNoPreBarrier(uint32_t dstStart, uint32_t srcStart, uint32_t count) {
        MOZ_ASSERT(!shadowZone()->needsIncrementalBarrier());

        MOZ_ASSERT(dstStart + count <= getDenseCapacity());
        MOZ_ASSERT(srcStart + count <= getDenseCapacity());
        MOZ_ASSERT(!denseElementsAreCopyOnWrite());

        memmove(elements + dstStart, elements + srcStart, count * sizeof(Value));
        DenseRangeWriteBarrierPost(runtimeFromMainThread(), this, dstStart, count);
    }

    bool shouldConvertDoubleElements() {
        return getElementsHeader()->shouldConvertDoubleElements();
    }

    inline void setShouldConvertDoubleElements();
    inline void clearShouldConvertDoubleElements();

    bool denseElementsAreCopyOnWrite() {
        return getElementsHeader()->isCopyOnWrite();
    }

    /* Packed information for this object's elements. */
    inline bool writeToIndexWouldMarkNotPacked(uint32_t index);
    inline void markDenseElementsNotPacked(ExclusiveContext *cx);

    /*
     * ensureDenseElements ensures that the object can hold at least
     * index + extra elements. It returns ED_OK on success, ED_FAILED on
     * failure to grow the array, ED_SPARSE when the object is too sparse to
     * grow (this includes the case of index + extra overflow). In the last
     * two cases the object is kept intact.
     */
    enum EnsureDenseResult { ED_OK, ED_FAILED, ED_SPARSE };

  private:
    inline EnsureDenseResult ensureDenseElementsNoPackedCheck(ThreadSafeContext *cx,
                                                              uint32_t index, uint32_t extra);

  public:
    inline EnsureDenseResult ensureDenseElements(ExclusiveContext *cx,
                                                 uint32_t index, uint32_t extra);
    inline EnsureDenseResult ensureDenseElementsPreservePackedFlag(ThreadSafeContext *cx,
                                                                   uint32_t index, uint32_t extra);

    inline EnsureDenseResult extendDenseElements(ThreadSafeContext *cx,
                                                 uint32_t requiredCapacity, uint32_t extra);

    /* Convert a single dense element to a sparse property. */
    static bool sparsifyDenseElement(ExclusiveContext *cx,
                                     HandleNativeObject obj, uint32_t index);

    /* Convert all dense elements to sparse properties. */
    static bool sparsifyDenseElements(ExclusiveContext *cx, HandleNativeObject obj);

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
    static EnsureDenseResult maybeDensifySparseElements(ExclusiveContext *cx,
                                                        HandleNativeObject obj);

    inline HeapSlot *fixedElements() const {
        static_assert(2 * sizeof(Value) == sizeof(ObjectElements),
                      "when elements are stored inline, the first two "
                      "slots will hold the ObjectElements header");
        return &fixedSlots()[2];
    }

#ifdef DEBUG
    bool canHaveNonEmptyElements();
#endif

    void setFixedElements() {
        MOZ_ASSERT(canHaveNonEmptyElements());
        this->elements = fixedElements();
    }

    inline bool hasDynamicElements() const {
        /*
         * Note: for objects with zero fixed slots this could potentially give
         * a spurious 'true' result, if the end of this object is exactly
         * aligned with the end of its arena and dynamic slots are allocated
         * immediately afterwards. Such cases cannot occur for dense arrays
         * (which have at least two fixed slots) and can only result in a leak.
         */
        return !hasEmptyElements() && elements != fixedElements();
    }

    inline bool hasFixedElements() const {
        return elements == fixedElements();
    }

    inline bool hasEmptyElements() const {
        return elements == emptyObjectElements;
    }

    /*
     * Get a pointer to the unused data in the object's allocation immediately
     * following this object, for use with objects which allocate a larger size
     * class than they need and store non-elements data inline.
     */
    inline uint8_t *fixedData(size_t nslots) const;

    inline void privateWriteBarrierPre(void **oldval);

    void privateWriteBarrierPost(void **pprivate) {
#ifdef JSGC_GENERATIONAL
        gc::Cell **cellp = reinterpret_cast<gc::Cell **>(pprivate);
        MOZ_ASSERT(cellp);
        MOZ_ASSERT(*cellp);
        gc::StoreBuffer *storeBuffer = (*cellp)->storeBuffer();
        if (storeBuffer)
            storeBuffer->putCellFromAnyThread(cellp);
#endif
    }

    /* Private data accessors. */

    inline void *&privateRef(uint32_t nfixed) const { /* XXX should be private, not protected! */
        /*
         * The private pointer of an object can hold any word sized value.
         * Private pointers are stored immediately after the last fixed slot of
         * the object.
         */
        MOZ_ASSERT(nfixed == numFixedSlots());
        MOZ_ASSERT(hasPrivate());
        HeapSlot *end = &fixedSlots()[nfixed];
        return *reinterpret_cast<void**>(end);
    }

    bool hasPrivate() const {
        return getClass()->hasPrivate();
    }
    void *getPrivate() const {
        return privateRef(numFixedSlots());
    }
    void setPrivate(void *data) {
        void **pprivate = &privateRef(numFixedSlots());
        privateWriteBarrierPre(pprivate);
        *pprivate = data;
    }

    void setPrivateGCThing(gc::Cell *cell) {
        void **pprivate = &privateRef(numFixedSlots());
        privateWriteBarrierPre(pprivate);
        *pprivate = reinterpret_cast<void *>(cell);
        privateWriteBarrierPost(pprivate);
    }

    void setPrivateUnbarriered(void *data) {
        void **pprivate = &privateRef(numFixedSlots());
        *pprivate = data;
    }
    void initPrivate(void *data) {
        privateRef(numFixedSlots()) = data;
    }

    /* Access private data for an object with a known number of fixed slots. */
    inline void *getPrivate(uint32_t nfixed) const {
        return privateRef(nfixed);
    }

    void setInitialSlots(HeapSlot *newSlots) { slots = newSlots; }

    static inline NativeObject *
    copy(ExclusiveContext *cx, gc::AllocKind kind, gc::InitialHeap heap,
         HandleNativeObject templateObject);

    /* JIT Accessors */
    static size_t offsetOfElements() { return offsetof(NativeObject, elements); }
    static size_t offsetOfFixedElements() {
        return sizeof(NativeObject) + sizeof(ObjectElements);
    }

    static size_t getFixedSlotOffset(size_t slot) {
        return sizeof(NativeObject) + slot * sizeof(Value);
    }
    static size_t getPrivateDataOffset(size_t nfixed) { return getFixedSlotOffset(nfixed); }
    static size_t offsetOfSlots() { return offsetof(NativeObject, slots); }
};

inline void
NativeObject::privateWriteBarrierPre(void **oldval)
{
#ifdef JSGC_INCREMENTAL
    JS::shadow::Zone *shadowZone = this->shadowZoneFromAnyThread();
    if (shadowZone->needsIncrementalBarrier()) {
        if (*oldval && getClass()->trace)
            getClass()->trace(shadowZone->barrierTracer(), this);
    }
#endif
}

#ifdef DEBUG
static inline bool
IsObjectValueInCompartment(Value v, JSCompartment *comp)
{
    if (!v.isObject())
        return true;
    return v.toObject().compartment() == comp;
}
#endif

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
               typename MaybeRooted<NativeObject*, allowGC>::HandleType obj,
               typename MaybeRooted<jsid, allowGC>::HandleType id,
               typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
               typename MaybeRooted<Shape*, allowGC>::MutableHandleType propp);

extern bool
LookupElement(JSContext *cx, HandleNativeObject obj, uint32_t index,
              MutableHandleObject objp, MutableHandleShape propp);

extern bool
DefineGeneric(ExclusiveContext *cx, HandleNativeObject obj, HandleId id, HandleValue value,
              JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs);

extern bool
DefineElement(ExclusiveContext *cx, HandleNativeObject obj, uint32_t index, HandleValue value,
              JSPropertyOp getter, JSStrictPropertyOp setter, unsigned attrs);

extern bool
GetProperty(JSContext *cx, HandleNativeObject obj, HandleObject receiver, HandleId id, MutableHandleValue vp);

extern bool
GetPropertyNoGC(JSContext *cx, NativeObject *obj, JSObject *receiver, jsid id, Value *vp);

extern bool
GetElement(JSContext *cx, HandleNativeObject obj, HandleObject receiver,
           uint32_t index, MutableHandleValue vp);

inline bool
GetProperty(JSContext *cx, HandleNativeObject obj, HandleId id, MutableHandleValue vp)
{
    return GetProperty(cx, obj, obj, id, vp);
}

inline bool
GetElement(JSContext *cx, HandleNativeObject obj, uint32_t index, MutableHandleValue vp)
{
    return GetElement(cx, obj, obj, index, vp);
}

/*
 * Indicates whether an assignment operation is qualified (`x.y = 0`) or
 * unqualified (`y = 0`). In strict mode, the latter is an error if no such
 * variable already exists.
 *
 * Used as an argument to baseops::SetPropertyHelper.
 */
enum QualifiedBool {
    Unqualified = 0,
    Qualified = 1
};

template <ExecutionMode mode>
extern bool
SetPropertyHelper(typename ExecutionModeTraits<mode>::ContextType cx,
                  HandleNativeObject obj,
                  HandleObject receiver, HandleId id, QualifiedBool qualified,
                  MutableHandleValue vp, bool strict);

extern bool
SetElementHelper(JSContext *cx, HandleNativeObject obj, HandleObject Receiver, uint32_t index,
                 MutableHandleValue vp, bool strict);

extern bool
GetAttributes(JSContext *cx, HandleNativeObject obj, HandleId id, unsigned *attrsp);

extern bool
SetAttributes(JSContext *cx, HandleNativeObject obj, HandleId id, unsigned *attrsp);

extern bool
DeleteGeneric(JSContext *cx, HandleNativeObject obj, HandleId id, bool *succeeded);

} /* namespace js::baseops */

/*
 * Return successfully added or changed shape or nullptr on error.
 */
extern bool
DefineNativeProperty(ExclusiveContext *cx, HandleNativeObject obj, HandleId id, HandleValue value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

extern bool
LookupNativeProperty(ExclusiveContext *cx, HandleNativeObject obj, HandleId id,
                     js::MutableHandleObject objp, js::MutableHandleShape propp);

bool
NativeGet(JSContext *cx, HandleObject obj, HandleNativeObject pobj,
          HandleShape shape, MutableHandle<Value> vp);

template <ExecutionMode mode>
bool
NativeSet(typename ExecutionModeTraits<mode>::ContextType cx,
          HandleNativeObject obj, HandleObject receiver,
          HandleShape shape, bool strict, MutableHandleValue vp);

/*
 * If obj has an already-resolved data property for id, return true and
 * store the property value in *vp.
 */
extern bool
HasDataProperty(JSContext *cx, NativeObject *obj, jsid id, Value *vp);

inline bool
HasDataProperty(JSContext *cx, NativeObject *obj, PropertyName *name, Value *vp)
{
    return HasDataProperty(cx, obj, NameToId(name), vp);
}

} /* namespace js */

inline void *
JSObject::fakeNativeGetPrivate() const
{
    return static_cast<const js::NativeObject*>(this)->getPrivate();
}

inline void *
JSObject::fakeNativeGetPrivate(uint32_t nfixed) const
{
    return static_cast<const js::NativeObject*>(this)->getPrivate(nfixed);
}

inline void
JSObject::fakeNativeSetPrivate(void *data)
{
    static_cast<js::NativeObject*>(this)->setPrivate(data);
}

inline bool
JSObject::fakeNativeHasPrivate() const
{
    return static_cast<const js::NativeObject*>(this)->hasPrivate();
}

inline void
JSObject::fakeNativeInitPrivate(void *data)
{
    static_cast<js::NativeObject*>(this)->initPrivate(data);
}

inline void *&
JSObject::fakeNativePrivateRef(uint32_t nfixed) const
{
    return static_cast<const js::NativeObject*>(this)->privateRef(nfixed);
}

inline uint32_t
JSObject::fakeNativeSlotSpan()
{
    return static_cast<js::NativeObject*>(this)->slotSpan();
}

inline const js::Value &
JSObject::fakeNativeGetSlot(uint32_t slot)
{
    return static_cast<js::NativeObject*>(this)->getSlot(slot);
}

inline void
JSObject::fakeNativeSetSlot(uint32_t slot, const js::Value &value)
{
    static_cast<js::NativeObject*>(this)->setSlot(slot, value);
}

inline js::HeapSlot &
JSObject::fakeNativeGetSlotRef(uint32_t slot)
{
    return static_cast<js::NativeObject*>(this)->getSlotRef(slot);
}

inline const js::Value &
JSObject::fakeNativeGetReservedSlot(uint32_t slot) const
{
    return static_cast<const js::NativeObject*>(this)->getReservedSlot(slot);
}

inline js::HeapSlot &
JSObject::fakeNativeGetReservedSlotRef(uint32_t slot)
{
    return static_cast<js::NativeObject*>(this)->getReservedSlotRef(slot);
}

inline void
JSObject::fakeNativeSetReservedSlot(uint32_t slot, const js::Value &value)
{
    static_cast<js::NativeObject*>(this)->setReservedSlot(slot, value);
}

inline void
JSObject::fakeNativeInitReservedSlot(uint32_t slot, const js::Value &value)
{
    static_cast<js::NativeObject*>(this)->initReservedSlot(slot, value);
}

inline void
JSObject::fakeNativeSetCrossCompartmentSlot(uint32_t slot, const js::Value &value)
{
    static_cast<js::NativeObject*>(this)->setCrossCompartmentSlot(slot, value);
}

inline void
JSObject::fakeNativeInitCrossCompartmentSlot(uint32_t slot, const js::Value &value)
{
    static_cast<js::NativeObject*>(this)->initCrossCompartmentSlot(slot, value);
}

inline void
JSObject::fakeNativeSetInitialSlots(js::HeapSlot *newSlots)
{
    static_cast<js::NativeObject*>(this)->setInitialSlots(newSlots);
}

inline bool
JSObject::fakeNativeHasDynamicSlots() const
{
    return static_cast<const js::NativeObject*>(this)->hasDynamicSlots();
}

inline uint32_t
JSObject::fakeNativeNumFixedSlots() const
{
    return static_cast<const js::NativeObject*>(this)->numFixedSlots();
}

inline uint32_t
JSObject::fakeNativeNumDynamicSlots() const
{
    return static_cast<const js::NativeObject*>(this)->numDynamicSlots();
}

inline js::HeapSlot *&
JSObject::fakeNativeSlots()
{
    return slots;
}

inline void
JSObject::fakeNativeInitSlot(uint32_t slot, const js::Value &value)
{
    static_cast<js::NativeObject*>(this)->initSlot(slot, value);
}

inline void
JSObject::fakeNativeInitSlotRange(uint32_t start, const js::Value *vector, uint32_t length)
{
    static_cast<js::NativeObject*>(this)->initSlotRange(start, vector, length);
}

inline void
JSObject::fakeNativeInitializeSlotRange(uint32_t start, uint32_t count)
{
    static_cast<js::NativeObject*>(this)->initializeSlotRange(start, count);
}

inline bool
JSObject::fakeNativeHasDynamicElements() const
{
    return static_cast<const js::NativeObject*>(this)->hasDynamicElements();
}

inline bool
JSObject::fakeNativeHasEmptyElements() const
{
    return static_cast<const js::NativeObject*>(this)->hasEmptyElements();
}

inline js::HeapSlotArray
JSObject::fakeNativeGetDenseElements()
{
    return static_cast<js::NativeObject*>(this)->getDenseElements();
}

inline bool
JSObject::fakeNativeDenseElementsAreCopyOnWrite()
{
    return static_cast<js::NativeObject*>(this)->denseElementsAreCopyOnWrite();
}

inline js::ObjectElements *
JSObject::fakeNativeGetElementsHeader() const
{
    return static_cast<const js::NativeObject*>(this)->getElementsHeader();
}

inline js::HeapSlot *&
JSObject::fakeNativeElements()
{
    return elements;
}

inline const js::Value &
JSObject::fakeNativeGetDenseElement(uint32_t idx)
{
    return static_cast<js::NativeObject*>(this)->getDenseElement(idx);
}

inline uint32_t
JSObject::fakeNativeGetDenseInitializedLength()
{
    return static_cast<js::NativeObject*>(this)->getDenseInitializedLength();
}

inline const js::HeapSlot *
JSObject::fakeNativeGetSlotAddressUnchecked(uint32_t slot) const
{
    return static_cast<const js::NativeObject*>(this)->getSlotAddressUnchecked(slot);
}

template <>
inline bool
JSObject::is<js::NativeObject>() const { return isNative(); }

/* static */ inline bool
JSObject::lookupElement(JSContext *cx, js::HandleObject obj, uint32_t index,
                        js::MutableHandleObject objp, js::MutableHandleShape propp)
{
    js::LookupElementOp op = obj->getOps()->lookupElement;
    if (op)
        return op(cx, obj, index, objp, propp);
    return js::baseops::LookupElement(cx, obj.as<js::NativeObject>(), index, objp, propp);
}

/* static */ inline bool
JSObject::getGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     js::HandleId id, js::MutableHandleValue vp)
{
    MOZ_ASSERT(!!obj->getOps()->getGeneric == !!obj->getOps()->getProperty);
    js::GenericIdOp op = obj->getOps()->getGeneric;
    if (op) {
        if (!op(cx, obj, receiver, id, vp))
            return false;
    } else {
        if (!js::baseops::GetProperty(cx, obj.as<js::NativeObject>(), receiver, id, vp))
            return false;
    }
    return true;
}

/* static */ inline bool
JSObject::getGenericNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                         jsid id, js::Value *vp)
{
    js::GenericIdOp op = obj->getOps()->getGeneric;
    if (op)
        return false;
    return js::baseops::GetPropertyNoGC(cx, &obj->as<js::NativeObject>(), receiver, id, vp);
}

/* static */ inline bool
JSObject::setGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     js::HandleId id, js::MutableHandleValue vp, bool strict)
{
    if (obj->getOps()->setGeneric)
        return nonNativeSetProperty(cx, obj, id, vp, strict);
    return js::baseops::SetPropertyHelper<js::SequentialExecution>(
        cx, obj.as<js::NativeObject>(), receiver, id, js::baseops::Qualified, vp, strict);
}

/* static */ inline bool
JSObject::setElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                     uint32_t index, js::MutableHandleValue vp, bool strict)
{
    if (obj->getOps()->setElement)
        return nonNativeSetElement(cx, obj, index, vp, strict);
    return js::baseops::SetElementHelper(cx, obj.as<js::NativeObject>(),
                                         receiver, index, vp, strict);
}

/* static */ inline bool
JSObject::getGenericAttributes(JSContext *cx, js::HandleObject obj,
                               js::HandleId id, unsigned *attrsp)
{
    js::GenericAttributesOp op = obj->getOps()->getGenericAttributes;
    if (op)
        return op(cx, obj, id, attrsp);
    return js::baseops::GetAttributes(cx, obj.as<js::NativeObject>(), id, attrsp);
}

namespace js {

// Alternate to JSObject::as<NativeObject>() that tolerates null pointers.
inline NativeObject *
MaybeNativeObject(JSObject *obj)
{
    return obj ? &obj->as<NativeObject>() : nullptr;
}

} // namespace js

#endif /* vm_NativeObject_h */
