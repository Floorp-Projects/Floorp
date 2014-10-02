/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ObjectImpl_h
#define vm_ObjectImpl_h

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

#include <stdint.h>

#include "jsfriendapi.h"
#include "jsinfer.h"
#include "NamespaceImports.h"

#include "gc/Barrier.h"
#include "gc/Heap.h"
#include "gc/Marking.h"
#include "js/Value.h"
#include "vm/NumericConversions.h"
#include "vm/Shape.h"
#include "vm/String.h"

namespace js {

class ObjectImpl;
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
 * Elements header used for all objects. The elements component of such objects
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
    friend class ObjectImpl;
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

    HeapPtrObject &ownerObject() const {
        MOZ_ASSERT(isCopyOnWrite());
        return *(HeapPtrObject *)(&elements()[initializedLength]);
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
    static bool MakeElementsCopyOnWrite(ExclusiveContext *cx, JSObject *obj);

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

inline Value
ObjectValue(ObjectImpl &obj);

#ifdef DEBUG
static inline bool
IsObjectValueInCompartment(js::Value v, JSCompartment *comp);
#endif

/*
 * ObjectImpl specifies the internal implementation of an object.  (In contrast
 * JSObject specifies an "external" interface, at the conceptual level of that
 * exposed in ECMAScript.)
 *
 * The |shape_| member stores the shape of the object, which includes the
 * object's class and the layout of all its properties.
 *
 * The |type_| member stores the type of the object, which contains its
 * prototype object and the possible types of its properties.
 *
 * The rest of the object stores its named properties and indexed elements.
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
 * Only certain combinations of slots and elements storage are possible.
 *
 * - For native objects, slots and elements may both be non-empty. The
 *   slots may be either names or indexes; no indexed property will be in both
 *   the slots and elements.
 *
 * - For non-native objects, slots and elements are both empty.
 *
 * The members of this class are currently protected; in the long run this will
 * will change so that some members are private, and only certain methods that
 * act upon them will be protected.
 */
class ObjectImpl : public gc::Cell
{
  protected:
    /*
     * Shape of the object, encodes the layout of the object's properties and
     * all other information about its structure. See vm/Shape.h.
     */
    HeapPtrShape shape_;

    /*
     * The object's type and prototype. For objects with the LAZY_TYPE flag
     * set, this is the prototype's default 'new' type and can only be used
     * to get that prototype.
     */
    HeapPtrTypeObject type_;

    HeapSlot *slots;     /* Slots for object properties. */
    HeapSlot *elements;  /* Slots for object elements. */

    friend bool
    ArraySetLength(JSContext *cx, Handle<ArrayObject*> obj, HandleId id, unsigned attrs,
                   HandleValue value, bool setterIsStrict);

  private:
    static void staticAsserts() {
        static_assert(sizeof(ObjectImpl) == sizeof(shadow::Object),
                      "shadow interface must match actual implementation");
        static_assert(sizeof(ObjectImpl) % sizeof(Value) == 0,
                      "fixed slots after an object must be aligned");

        static_assert(offsetof(ObjectImpl, shape_) == offsetof(shadow::Object, shape),
                      "shadow shape must match actual shape");
        static_assert(offsetof(ObjectImpl, type_) == offsetof(shadow::Object, type),
                      "shadow type must match actual type");
        static_assert(offsetof(ObjectImpl, slots) == offsetof(shadow::Object, slots),
                      "shadow slots must match actual slots");
        static_assert(offsetof(ObjectImpl, elements) == offsetof(shadow::Object, _1),
                      "shadow placeholder must match actual elements");
    }

    JSObject * asObjectPtr() { return reinterpret_cast<JSObject *>(this); }
    const JSObject * asObjectPtr() const { return reinterpret_cast<const JSObject *>(this); }

    friend inline Value ObjectValue(ObjectImpl &obj);

    /* These functions are public, and they should remain public. */

  public:
    TaggedProto getTaggedProto() const {
        return type_->proto();
    }

    bool hasTenuredProto() const;

    const Class *getClass() const {
        return type_->clasp();
    }

    static inline bool
    isExtensible(ExclusiveContext *cx, Handle<ObjectImpl*> obj, bool *extensible);

    // Indicates whether a non-proxy is extensible.  Don't call on proxies!
    // This method really shouldn't exist -- but there are a few internal
    // places that want it (JITs and the like), and it'd be a pain to mark them
    // all as friends.
    bool nonProxyIsExtensible() const {
        MOZ_ASSERT(!isProxy());

        // [[Extensible]] for ordinary non-proxy objects is an object flag.
        return !lastProperty()->hasObjectFlag(BaseShape::NOT_EXTENSIBLE);
    }

#ifdef DEBUG
    bool isProxy() const;
#endif

    // Attempt to change the [[Extensible]] bit on |obj| to false.  Callers
    // must ensure that |obj| is currently extensible before calling this!
    static bool
    preventExtensions(JSContext *cx, Handle<ObjectImpl*> obj);

    HeapSlotArray getDenseElements() {
        MOZ_ASSERT(isNative());
        return HeapSlotArray(elements, !getElementsHeader()->isCopyOnWrite());
    }
    HeapSlotArray getDenseElementsAllowCopyOnWrite() {
        // Backdoor allowing direct access to copy on write elements.
        MOZ_ASSERT(isNative());
        return HeapSlotArray(elements, true);
    }
    const Value &getDenseElement(uint32_t idx) {
        MOZ_ASSERT(isNative());
        MOZ_ASSERT(idx < getDenseInitializedLength());
        return elements[idx];
    }
    bool containsDenseElement(uint32_t idx) {
        MOZ_ASSERT(isNative());
        return idx < getDenseInitializedLength() && !elements[idx].isMagic(JS_ELEMENTS_HOLE);
    }
    uint32_t getDenseInitializedLength() {
        MOZ_ASSERT(getClass()->isNative());
        return getElementsHeader()->initializedLength;
    }
    uint32_t getDenseCapacity() {
        MOZ_ASSERT(getClass()->isNative());
        return getElementsHeader()->capacity;
    }

  protected:
#ifdef DEBUG
    void checkShapeConsistency();
#else
    void checkShapeConsistency() { }
#endif

    Shape *
    replaceWithNewEquivalentShape(ThreadSafeContext *cx,
                                  Shape *existingShape, Shape *newShape = nullptr);

    enum GenerateShape {
        GENERATE_NONE,
        GENERATE_SHAPE
    };

    bool setFlag(ExclusiveContext *cx, /*BaseShape::Flag*/ uint32_t flag,
                 GenerateShape generateShape = GENERATE_NONE);
    bool clearFlag(ExclusiveContext *cx, /*BaseShape::Flag*/ uint32_t flag);

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
        return reinterpret_cast<HeapSlot *>(uintptr_t(this) + sizeof(ObjectImpl));
    }

    /*
     * These functions are currently public for simplicity; in the long run
     * it may make sense to make at least some of them private.
     */

  public:
    Shape * lastProperty() const {
        MOZ_ASSERT(shape_);
        return shape_;
    }

    bool generateOwnShape(ThreadSafeContext *cx, js::Shape *newShape = nullptr) {
        return replaceWithNewEquivalentShape(cx, lastProperty(), newShape);
    }

    JSCompartment *compartment() const {
        return lastProperty()->base()->compartment();
    }

    bool isNative() const {
        return lastProperty()->isNative();
    }

    types::TypeObject *type() const {
        MOZ_ASSERT(!hasLazyType());
        return typeRaw();
    }

    types::TypeObject *typeRaw() const {
        return type_;
    }

    uint32_t numFixedSlots() const {
        return reinterpret_cast<const shadow::Object *>(this)->numFixedSlots();
    }
    uint32_t numUsedFixedSlots() const {
        uint32_t nslots = lastProperty()->slotSpan(getClass());
        return Min(nslots, numFixedSlots());
    }

    /*
     * Whether this is the only object which has its specified type. This
     * object will have its type constructed lazily as needed by analysis.
     */
    bool hasSingletonType() const {
        return !!type_->singleton();
    }

    /*
     * Whether the object's type has not been constructed yet. If an object
     * might have a lazy type, use getType() below, otherwise type().
     */
    bool hasLazyType() const {
        return type_->lazy();
    }

    uint32_t slotSpan() const {
        if (inDictionaryMode())
            return lastProperty()->base()->slotSpan();
        return lastProperty()->slotSpan();
    }

    /* Compute dynamicSlotsCount() for this object. */
    uint32_t numDynamicSlots() const {
        return dynamicSlotsCount(numFixedSlots(), slotSpan(), getClass());
    }


    Shape *nativeLookup(ExclusiveContext *cx, jsid id);
    Shape *nativeLookup(ExclusiveContext *cx, PropertyName *name) {
        return nativeLookup(cx, NameToId(name));
    }

    bool nativeContains(ExclusiveContext *cx, jsid id) {
        return nativeLookup(cx, id) != nullptr;
    }
    bool nativeContains(ExclusiveContext *cx, PropertyName* name) {
        return nativeLookup(cx, name) != nullptr;
    }
    bool nativeContains(ExclusiveContext *cx, Shape* shape) {
        return nativeLookup(cx, shape->propid()) == shape;
    }

    /* Contextless; can be called from parallel code. */
    Shape *nativeLookupPure(jsid id);
    Shape *nativeLookupPure(PropertyName *name) {
        return nativeLookupPure(NameToId(name));
    }

    bool nativeContainsPure(jsid id) {
        return nativeLookupPure(id) != nullptr;
    }
    bool nativeContainsPure(PropertyName* name) {
        return nativeContainsPure(NameToId(name));
    }
    bool nativeContainsPure(Shape* shape) {
        return nativeLookupPure(shape->propid()) == shape;
    }

    const JSClass *getJSClass() const {
        return Jsvalify(getClass());
    }
    bool hasClass(const Class *c) const {
        return getClass() == c;
    }
    const ObjectOps *getOps() const {
        return &getClass()->ops;
    }

    /*
     * An object is a delegate if it is on another object's prototype or scope
     * chain, and therefore the delegate might be asked implicitly to get or
     * set a property on behalf of another object. Delegates may be accessed
     * directly too, as may any object, but only those objects linked after the
     * head of any prototype or scope chain are flagged as delegates. This
     * definition helps to optimize shape-based property cache invalidation
     * (see Purge{Scope,Proto}Chain in jsobj.cpp).
     */
    bool isDelegate() const {
        return lastProperty()->hasObjectFlag(BaseShape::DELEGATE);
    }

    /*
     * Return true if this object is a native one that has been converted from
     * shared-immutable prototype-rooted shape storage to dictionary-shapes in
     * a doubly-linked list.
     */
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
        const ObjectImpl *obj = static_cast<const ObjectImpl*>(this);
        return const_cast<HeapSlot*>(obj->getSlotAddressUnchecked(slot));
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

    HeapSlot &nativeGetSlotRef(uint32_t slot) {
        MOZ_ASSERT(isNative() && slot < slotSpan());
        return getSlotRef(slot);
    }
    const Value &nativeGetSlot(uint32_t slot) const {
        MOZ_ASSERT(isNative() && slot < slotSpan());
        return getSlot(slot);
    }

    void setSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slotInRange(slot));
        MOZ_ASSERT(IsObjectValueInCompartment(value, compartment()));
        getSlotRef(slot).set(this->asObjectPtr(), HeapSlot::Slot, slot, value);
    }

    MOZ_ALWAYS_INLINE void setCrossCompartmentSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slotInRange(slot));
        getSlotRef(slot).set(this->asObjectPtr(), HeapSlot::Slot, slot, value);
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
        getSlotAddressUnchecked(slot)->init(this->asObjectPtr(), HeapSlot::Slot, slot, value);
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
        fixedSlots()[slot].set(this->asObjectPtr(), HeapSlot::Slot, slot, value);
    }

    void initFixedSlot(uint32_t slot, const Value &value) {
        MOZ_ASSERT(slot < numFixedSlots());
        fixedSlots()[slot].init(this->asObjectPtr(), HeapSlot::Slot, slot, value);
    }

    /*
     * Get the number of dynamic slots to allocate to cover the properties in
     * an object with the given number of fixed slots and slot span. The slot
     * capacity is not stored explicitly, and the allocated size of the slot
     * array is kept in sync with this count.
     */
    static uint32_t dynamicSlotsCount(uint32_t nfixed, uint32_t span, const Class *clasp);

    /* Memory usage functions. */
    size_t tenuredSizeOfThis() const {
        MOZ_ASSERT(isTenured());
        return js::gc::Arena::thingSize(asTenured().getAllocKind());
    }

    /* Elements accessors. */

    ObjectElements * getElementsHeader() const {
        return ObjectElements::fromElements(elements);
    }

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

    /* GC support. */
    static ThingRootKind rootKind() { return THING_ROOT_OBJECT; }

    inline void privateWriteBarrierPre(void **oldval);

    void privateWriteBarrierPost(void **pprivate) {
#ifdef JSGC_GENERATIONAL
        js::gc::Cell **cellp = reinterpret_cast<js::gc::Cell **>(pprivate);
        MOZ_ASSERT(cellp);
        MOZ_ASSERT(*cellp);
        js::gc::StoreBuffer *storeBuffer = (*cellp)->storeBuffer();
        if (storeBuffer)
            storeBuffer->putCellFromAnyThread(cellp);
#endif
    }

    void markChildren(JSTracer *trc);

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

    /* GC Accessors */
    static const size_t MaxTagBits = 3;
    void setInitialSlots(HeapSlot *newSlots) { slots = newSlots; }
    static bool isNullLike(const ObjectImpl *obj) { return uintptr_t(obj) < (1 << MaxTagBits); }
    MOZ_ALWAYS_INLINE JS::Zone *zone() const {
        return shape_->zone();
    }
    MOZ_ALWAYS_INLINE JS::shadow::Zone *shadowZone() const {
        return JS::shadow::Zone::asShadowZone(zone());
    }
    MOZ_ALWAYS_INLINE JS::Zone *zoneFromAnyThread() const {
        return shape_->zoneFromAnyThread();
    }
    MOZ_ALWAYS_INLINE JS::shadow::Zone *shadowZoneFromAnyThread() const {
        return JS::shadow::Zone::asShadowZone(zoneFromAnyThread());
    }
    static MOZ_ALWAYS_INLINE void readBarrier(ObjectImpl *obj);
    static MOZ_ALWAYS_INLINE void writeBarrierPre(ObjectImpl *obj);
    static MOZ_ALWAYS_INLINE void writeBarrierPost(ObjectImpl *obj, void *cellp);
    static MOZ_ALWAYS_INLINE void writeBarrierPostRelocate(ObjectImpl *obj, void *cellp);
    static MOZ_ALWAYS_INLINE void writeBarrierPostRemove(ObjectImpl *obj, void *cellp);

    /* JIT Accessors */
    static size_t offsetOfShape() { return offsetof(ObjectImpl, shape_); }
    HeapPtrShape *addressOfShape() { return &shape_; }

    static size_t offsetOfType() { return offsetof(ObjectImpl, type_); }
    HeapPtrTypeObject *addressOfType() { return &type_; }

    static size_t offsetOfElements() { return offsetof(ObjectImpl, elements); }
    static size_t offsetOfFixedElements() {
        return sizeof(ObjectImpl) + sizeof(ObjectElements);
    }

    static size_t getFixedSlotOffset(size_t slot) {
        return sizeof(ObjectImpl) + slot * sizeof(Value);
    }
    static size_t getPrivateDataOffset(size_t nfixed) { return getFixedSlotOffset(nfixed); }
    static size_t offsetOfSlots() { return offsetof(ObjectImpl, slots); }
};

/* static */ MOZ_ALWAYS_INLINE void
ObjectImpl::readBarrier(ObjectImpl *obj)
{
    if (!isNullLike(obj) && obj->isTenured())
        obj->asTenured().readBarrier(&obj->asTenured());
}

/* static */ MOZ_ALWAYS_INLINE void
ObjectImpl::writeBarrierPre(ObjectImpl *obj)
{
    if (!isNullLike(obj) && obj->isTenured())
        obj->asTenured().writeBarrierPre(&obj->asTenured());
}

/* static */ MOZ_ALWAYS_INLINE void
ObjectImpl::writeBarrierPost(ObjectImpl *obj, void *cellp)
{
    MOZ_ASSERT(cellp);
#ifdef JSGC_GENERATIONAL
    if (IsNullTaggedPointer(obj))
        return;
    MOZ_ASSERT(obj == *static_cast<ObjectImpl **>(cellp));
    gc::StoreBuffer *storeBuffer = obj->storeBuffer();
    if (storeBuffer)
        storeBuffer->putCellFromAnyThread(static_cast<gc::Cell **>(cellp));
#endif
}

/* static */ MOZ_ALWAYS_INLINE void
ObjectImpl::writeBarrierPostRelocate(ObjectImpl *obj, void *cellp)
{
    MOZ_ASSERT(cellp);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(obj == *static_cast<ObjectImpl **>(cellp));
#ifdef JSGC_GENERATIONAL
    gc::StoreBuffer *storeBuffer = obj->storeBuffer();
    if (storeBuffer)
        storeBuffer->putRelocatableCellFromAnyThread(static_cast<gc::Cell **>(cellp));
#endif
}

/* static */ MOZ_ALWAYS_INLINE void
ObjectImpl::writeBarrierPostRemove(ObjectImpl *obj, void *cellp)
{
    MOZ_ASSERT(cellp);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(obj == *static_cast<ObjectImpl **>(cellp));
#ifdef JSGC_GENERATIONAL
    obj->shadowRuntimeFromAnyThread()->gcStoreBufferPtr()->removeRelocatableCellFromAnyThread(
        static_cast<gc::Cell **>(cellp));
#endif
}

inline void
ObjectImpl::privateWriteBarrierPre(void **oldval)
{
#ifdef JSGC_INCREMENTAL
    JS::shadow::Zone *shadowZone = this->shadowZoneFromAnyThread();
    if (shadowZone->needsIncrementalBarrier()) {
        if (*oldval && getClass()->trace)
            getClass()->trace(shadowZone->barrierTracer(), this->asObjectPtr());
    }
#endif
}

inline Value
ObjectValue(ObjectImpl &obj)
{
    Value v;
    v.setObject(*obj.asObjectPtr());
    return v;
}

inline Handle<JSObject*>
Downcast(Handle<ObjectImpl*> obj)
{
    return Handle<JSObject*>::fromMarkedLocation(reinterpret_cast<JSObject* const*>(obj.address()));
}

#ifdef DEBUG
static inline bool
IsObjectValueInCompartment(js::Value v, JSCompartment *comp)
{
    if (!v.isObject())
        return true;
    return reinterpret_cast<ObjectImpl*>(&v.toObject())->compartment() == comp;
}
#endif

} /* namespace js */

#endif /* vm_ObjectImpl_h */
