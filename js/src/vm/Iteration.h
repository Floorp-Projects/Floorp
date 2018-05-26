/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_Iteration_h
#define vm_Iteration_h

/*
 * JavaScript iterators.
 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/MemoryReporting.h"

#include "gc/Barrier.h"
#include "vm/JSContext.h"
#include "vm/ReceiverGuard.h"
#include "vm/Stack.h"

namespace js {

class PropertyIteratorObject;

struct NativeIterator
{
  private:
    // Object being iterated.  Non-null except in NativeIterator sentinels and
    // empty property iterators created when |null| or |undefined| is iterated.
    GCPtrObject objectBeingIterated_ = {};

    // Internal iterator object.
    JSObject* iterObj_ = nullptr;

    // The end of HeapReceiverGuards that appear directly after |this|, as part
    // of an overall allocation that stores |*this|, receiver guards, and
    // iterated strings.  Once this has been fully initialized, it also equals
    // the start of iterated strings.
    HeapReceiverGuard* guardsEnd_; // initialized by constructor

    // The next property, pointing into an array of strings directly after any
    // HeapReceiverGuards that appear directly after |*this|, as part of an
    // overall allocation that stores |*this|, receiver guards, and iterated
    // strings.
    GCPtrFlatString* propertyCursor_; // initialized by constructor

    // The limit/end of properties to iterate (and, assuming no error occurred
    // while constructing this NativeIterator, the end of the full allocation
    // storing |*this|, receiver guards, and strings).  Beware!  This value may
    // change as properties are deleted from the observed object.
    GCPtrFlatString* propertiesEnd_; // initialized by constructor

    uint32_t guardKey_; // initialized by constructor

  public:
    // For cacheable native iterators, whether the iterator is currently
    // active.  Not serialized by XDR.
    struct Flags
    {
        static constexpr uint32_t Initialized = 0x1;
        static constexpr uint32_t Active = 0x2;
        static constexpr uint32_t HasUnvisitedPropertyDeletion = 0x4;
        static constexpr uint32_t NotReusable = Active | HasUnvisitedPropertyDeletion;
    };

  private:
    uint32_t flags_ = 0; // consists of Flags bits

    /* While in compartment->enumerators, these form a doubly linked list. */
    NativeIterator* next_ = nullptr;
    NativeIterator* prev_ = nullptr;

    // END OF PROPERTIES

    // No further fields appear after here *in NativeIterator*, but this class
    // is always allocated with space tacked on immediately after |this| to
    // store iterated property names up to |props_end| and |guard_length|
    // HeapReceiverGuards after that.

  public:
    /**
     * Initialize a NativeIterator properly allocated for |props.length()|
     * properties and |numGuards| guards.
     *
     * Despite being a constructor, THIS FUNCTION CAN REPORT ERRORS.  Users
     * MUST set |*hadError = false| on entry and consider |*hadError| on return
     * to mean this function failed.
     */
    NativeIterator(JSContext* cx, Handle<PropertyIteratorObject*> propIter,
                   Handle<JSObject*> objBeingIterated, const AutoIdVector& props,
                   uint32_t numGuards, uint32_t guardKey, bool* hadError);

    /** Initialize a |JSCompartment::enumerators| sentinel. */
    NativeIterator();

    JSObject* objectBeingIterated() const {
        return objectBeingIterated_;
    }

    void changeObjectBeingIterated(JSObject& obj) {
        objectBeingIterated_ = &obj;
    }

    HeapReceiverGuard* guardsBegin() const {
        static_assert(alignof(HeapReceiverGuard) <= alignof(NativeIterator),
                      "NativeIterator must be aligned to begin storing "
                      "HeapReceiverGuards immediately after it with no "
                      "required padding");
        const NativeIterator* immediatelyAfter = this + 1;
        auto* afterNonConst = const_cast<NativeIterator*>(immediatelyAfter);
        return reinterpret_cast<HeapReceiverGuard*>(afterNonConst);
    }

    HeapReceiverGuard* guardsEnd() const {
        return guardsEnd_;
    }

    uint32_t guardCount() const {
        return mozilla::PointerRangeSize(guardsBegin(), guardsEnd());
    }

    GCPtrFlatString* propertiesBegin() const {
        static_assert(alignof(HeapReceiverGuard) >= alignof(GCPtrFlatString),
                      "GCPtrFlatStrings for properties must be able to appear "
                      "directly after any HeapReceiverGuards after this "
                      "NativeIterator, with no padding space required for "
                      "correct alignment");
        static_assert(alignof(NativeIterator) >= alignof(GCPtrFlatString),
                      "GCPtrFlatStrings for properties must be able to appear "
                      "directly after this NativeIterator when no "
                      "HeapReceiverGuards are present, with no padding space "
                      "required for correct alignment");

        // We *could* just check the assertion below if we wanted, but the
        // incompletely-initialized NativeIterator case matters for so little
        // code that we prefer not imposing the condition-check on every single
        // user.
        MOZ_ASSERT(isInitialized(),
                   "NativeIterator must be initialized, or else |guardsEnd_| "
                   "isn't necessarily the start of properties and instead "
                   "|propertyCursor_| instead is");

        return reinterpret_cast<GCPtrFlatString*>(guardsEnd_);
    }

    GCPtrFlatString* propertiesEnd() const {
        return propertiesEnd_;
    }

    GCPtrFlatString* nextProperty() const {
        return propertyCursor_;
    }

    MOZ_ALWAYS_INLINE JS::Value nextIteratedValueAndAdvance() {
        if (propertyCursor_ >= propertiesEnd_) {
            MOZ_ASSERT(propertyCursor_ == propertiesEnd_);
            return JS::MagicValue(JS_NO_ITER_VALUE);
        }

        JSFlatString* str = *propertyCursor_;
        incCursor();
        return JS::StringValue(str);
    }

    void resetPropertyCursorForReuse() {
        MOZ_ASSERT(isInitialized());

        // This function is called unconditionally on IteratorClose, so
        // unvisited properties might have been deleted, so we can't assert
        // this NativeIterator is reusable.  (Should we not bother resetting
        // the cursor in that case?)

        // Note: JIT code inlines |propertyCursor_| resetting when an iterator
        //       ends: see |CodeGenerator::visitIteratorEnd|.
        propertyCursor_ = propertiesBegin();
    }

    bool previousPropertyWas(JS::Handle<JSFlatString*> str) {
        MOZ_ASSERT(isInitialized());
        return propertyCursor_ > propertiesBegin() && propertyCursor_[-1] == str;
    }

    size_t numKeys() const {
        return mozilla::PointerRangeSize(propertiesBegin(), propertiesEnd());
    }

    void trimLastProperty() {
        MOZ_ASSERT(isInitialized());

        propertiesEnd_--;

        // This invokes the pre barrier on this property, since it's no longer
        // going to be marked, and it ensures that any existing remembered set
        // entry will be dropped.
        *propertiesEnd_ = nullptr;
    }

    JSObject* iterObj() const {
        return iterObj_;
    }
    GCPtrFlatString* currentProperty() const {
        MOZ_ASSERT(propertyCursor_ < propertiesEnd());
        return propertyCursor_;
    }

    NativeIterator* next() {
        return next_;
    }

    void incCursor() {
        MOZ_ASSERT(isInitialized());
        propertyCursor_++;
    }

    uint32_t guardKey() const {
        return guardKey_;
    }

    bool isInitialized() const {
        return flags_ & Flags::Initialized;
    }

  private:
    void markInitialized() {
        MOZ_ASSERT(flags_ == 0);
        flags_ = Flags::Initialized;
    }

  public:
    bool isActive() const {
        MOZ_ASSERT(isInitialized());

        return flags_ & Flags::Active;
    }

    void markActive() {
        MOZ_ASSERT(isInitialized());

        flags_ |= Flags::Active;
    }

    void markInactive() {
        MOZ_ASSERT(isInitialized());

        flags_ &= ~Flags::Active;
    }

    bool isReusable() const {
        MOZ_ASSERT(isInitialized());
        return flags_ == Flags::Initialized;
    }

    void markHasUnvisitedPropertyDeletion() {
        MOZ_ASSERT(isInitialized());

        flags_ |= Flags::HasUnvisitedPropertyDeletion;
    }

    void link(NativeIterator* other) {
        // The NativeIterator sentinel doesn't have to be linked, because it's
        // the start of the list.  Anything else added should have been
        // initialized.
        MOZ_ASSERT(isInitialized());

        /* A NativeIterator cannot appear in the enumerator list twice. */
        MOZ_ASSERT(!next_ && !prev_);

        this->next_ = other;
        this->prev_ = other->prev_;
        other->prev_->next_ = this;
        other->prev_ = this;
    }
    void unlink() {
        MOZ_ASSERT(isInitialized());

        next_->prev_ = prev_;
        prev_->next_ = next_;
        next_ = nullptr;
        prev_ = nullptr;
    }

    static NativeIterator* allocateSentinel(JSContext* maybecx);

    void trace(JSTracer* trc);

    static constexpr size_t offsetOfObjectBeingIterated() {
        return offsetof(NativeIterator, objectBeingIterated_);
    }

    static constexpr size_t offsetOfGuardsEnd() {
        return offsetof(NativeIterator, guardsEnd_);
    }

    static constexpr size_t offsetOfPropertyCursor() {
        return offsetof(NativeIterator, propertyCursor_);
    }

    static constexpr size_t offsetOfPropertiesEnd() {
        return offsetof(NativeIterator, propertiesEnd_);
    }

    static constexpr size_t offsetOfFlags() {
        return offsetof(NativeIterator, flags_);
    }

    static constexpr size_t offsetOfNext() {
        return offsetof(NativeIterator, next_);
    }

    static constexpr size_t offsetOfPrev() {
        return offsetof(NativeIterator, prev_);
    }
};

class PropertyIteratorObject : public NativeObject
{
    static const ClassOps classOps_;

  public:
    static const Class class_;

    NativeIterator* getNativeIterator() const {
        return static_cast<js::NativeIterator*>(getPrivate());
    }
    void setNativeIterator(js::NativeIterator* ni) {
        setPrivate(ni);
    }

    size_t sizeOfMisc(mozilla::MallocSizeOf mallocSizeOf) const;

  private:
    static void trace(JSTracer* trc, JSObject* obj);
    static void finalize(FreeOp* fop, JSObject* obj);
};

class ArrayIteratorObject : public NativeObject
{
  public:
    static const Class class_;
};

ArrayIteratorObject*
NewArrayIteratorObject(JSContext* cx, NewObjectKind newKind = GenericObject);

class StringIteratorObject : public NativeObject
{
  public:
    static const Class class_;
};

StringIteratorObject*
NewStringIteratorObject(JSContext* cx, NewObjectKind newKind = GenericObject);

JSObject*
GetIterator(JSContext* cx, HandleObject obj);

PropertyIteratorObject*
LookupInIteratorCache(JSContext* cx, HandleObject obj);

JSObject*
EnumeratedIdVectorToIterator(JSContext* cx, HandleObject obj, AutoIdVector& props);

JSObject*
NewEmptyPropertyIterator(JSContext* cx);

JSObject*
ValueToIterator(JSContext* cx, HandleValue vp);

void
CloseIterator(JSObject* obj);

bool
IteratorCloseForException(JSContext* cx, HandleObject obj);

void
UnwindIteratorForUncatchableException(JSObject* obj);

extern bool
SuppressDeletedProperty(JSContext* cx, HandleObject obj, jsid id);

extern bool
SuppressDeletedElement(JSContext* cx, HandleObject obj, uint32_t index);

/*
 * IteratorMore() returns the next iteration value. If no value is available,
 * MagicValue(JS_NO_ITER_VALUE) is returned.
 */
extern bool
IteratorMore(JSContext* cx, HandleObject iterobj, MutableHandleValue rval);

/*
 * Create an object of the form { value: VALUE, done: DONE }.
 * ES 2017 draft 7.4.7.
 */
extern JSObject*
CreateIterResultObject(JSContext* cx, HandleValue value, bool done);

bool
IsPropertyIterator(HandleValue v);

enum class IteratorKind { Sync, Async };

} /* namespace js */

#endif /* vm_Iteration_h */
