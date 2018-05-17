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

#include "mozilla/MemoryReporting.h"

#include "gc/Barrier.h"
#include "vm/JSContext.h"
#include "vm/ReceiverGuard.h"
#include "vm/Stack.h"

/*
 * For cacheable native iterators, whether the iterator is currently active.
 * Not serialized by XDR.
 */
#define JSITER_ACTIVE       0x1000
#define JSITER_UNREUSABLE   0x2000

namespace js {

class PropertyIteratorObject;

struct NativeIterator
{
    // Object being iterated.
    GCPtrObject obj = {};

    // Internal iterator object.
    JSObject* iterObj_ = nullptr;

    // The next property, pointing into an array of strings directly after this
    // NativeIterator as part of the overall allocation containing |*this|.
    GCPtrFlatString* props_cursor; // initialized by constructor

    // The limit/end of properties to iterate.  (This is also, after casting,
    // the start of an array of HeapReceiverGuards included in the overall
    // allocation that stores |*this| and the iterated strings.)
    GCPtrFlatString* props_end; // initialized by constructor

    uint32_t guard_length = 0;
    uint32_t guard_key = 0;
    uint32_t flags = 0;

  private:
    /* While in compartment->enumerators, these form a doubly linked list. */
    NativeIterator* next_ = nullptr;
    NativeIterator* prev_ = nullptr;

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

    GCPtrFlatString* begin() const {
        static_assert(alignof(NativeIterator) >= alignof(GCPtrFlatString),
                      "GCPtrFlatStrings for properties must be able to appear "
                      "directly after NativeIterator, with no padding space "
                      "required for correct alignment");

        // Note that JIT code inlines this computation to reset |props_cursor|
        // when an iterator ends: see |CodeGenerator::visitIteratorEnd|.
        const NativeIterator* immediatelyAfter = this + 1;
        auto* afterNonConst = const_cast<NativeIterator*>(immediatelyAfter);
        return reinterpret_cast<GCPtrFlatString*>(afterNonConst);
    }

    GCPtrFlatString* end() const {
        return props_end;
    }

    HeapReceiverGuard* guardArray() const {
        static_assert(alignof(ReceiverGuard) == alignof(GCPtrFlatString),
                      "the end of all properties must be exactly aligned "
                      "adequate to begin storing ReceiverGuards, else the "
                      "full tacked-on memory won't be enough to store all "
                      "properties/guards");
        return reinterpret_cast<HeapReceiverGuard*>(props_end);
    }

    size_t numKeys() const {
        return end() - begin();
    }

    JSObject* iterObj() const {
        return iterObj_;
    }
    GCPtrFlatString* current() const {
        MOZ_ASSERT(props_cursor < props_end);
        return props_cursor;
    }

    NativeIterator* next() {
        return next_;
    }

    static inline size_t offsetOfNext() {
        return offsetof(NativeIterator, next_);
    }
    static inline size_t offsetOfPrev() {
        return offsetof(NativeIterator, prev_);
    }

    void incCursor() {
        props_cursor = props_cursor + 1;
    }
    void link(NativeIterator* other) {
        /* A NativeIterator cannot appear in the enumerator list twice. */
        MOZ_ASSERT(!next_ && !prev_);

        this->next_ = other;
        this->prev_ = other->prev_;
        other->prev_->next_ = this;
        other->prev_ = this;
    }
    void unlink() {
        next_->prev_ = prev_;
        prev_->next_ = next_;
        next_ = nullptr;
        prev_ = nullptr;
    }

    static NativeIterator* allocateSentinel(JSContext* maybecx);

    void trace(JSTracer* trc);
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
