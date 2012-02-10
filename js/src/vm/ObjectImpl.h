/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ObjectImpl_h___
#define ObjectImpl_h___

#include "mozilla/Assertions.h"
#include "mozilla/StdInt.h"

#include "gc/Barrier.h"

namespace js {

/*
 * Header structure for object element arrays. This structure is immediately
 * followed by an array of elements, with the elements member in an object
 * pointing to the beginning of that array (the end of this structure).
 * See below for usage of this structure.
 */
class ObjectElements
{
    friend struct ::JSObject;

    /* Number of allocated slots. */
    uint32_t capacity;

    /*
     * Number of initialized elements. This is <= the capacity, and for arrays
     * is <= the length. Memory for elements above the initialized length is
     * uninitialized, but values between the initialized length and the proper
     * length are conceptually holes.
     */
    uint32_t initializedLength;

    /* 'length' property of array objects, unused for other objects. */
    uint32_t length;

    /* :XXX: bug 586842 store state about sparse slots. */
    uint32_t unused;

    void staticAsserts() {
        MOZ_STATIC_ASSERT(sizeof(ObjectElements) == VALUES_PER_HEADER * sizeof(Value),
                          "Elements size and values-per-Elements mismatch");
    }

  public:

    ObjectElements(uint32_t capacity, uint32_t length)
      : capacity(capacity), initializedLength(0), length(length)
    {}

    HeapValue * elements() { return (HeapValue *)(uintptr_t(this) + sizeof(ObjectElements)); }
    static ObjectElements * fromElements(HeapValue *elems) {
        return (ObjectElements *)(uintptr_t(elems) - sizeof(ObjectElements));
    }

    static int offsetOfCapacity() {
        return (int)offsetof(ObjectElements, capacity) - (int)sizeof(ObjectElements);
    }
    static int offsetOfInitializedLength() {
        return (int)offsetof(ObjectElements, initializedLength) - (int)sizeof(ObjectElements);
    }
    static int offsetOfLength() {
        return (int)offsetof(ObjectElements, length) - (int)sizeof(ObjectElements);
    }

    static const size_t VALUES_PER_HEADER = 2;
};

/* Shared singleton for objects with no elements. */
extern HeapValue *emptyObjectElements;

struct Shape;
struct GCMarker;
class NewObjectCache;

/*
 * ObjectImpl specifies the internal implementation of an object.  (In contrast
 * JSObject specifies an "external" interface, at the conceptual level of that
 * exposed in ECMAScript.)
 *
 * The |shape_| member stores the shape of the object, which includes the
 * object's class and the layout of all its properties.
 *
 * The type member stores the type of the object, which contains its prototype
 * object and the possible types of its properties.
 *
 * The rest of the object stores its named properties and indexed elements.
 * These are stored separately from one another. Objects are followed by an
 * variable-sized array of values for inline storage, which may be used by
 * either properties of native objects (fixed slots) or by elements.
 *
 * Two native objects with the same shape are guaranteed to have the same
 * number of fixed slots.
 *
 * Named property storage can be split between fixed slots and a dynamically
 * allocated array (the slots member). For an object with N fixed slots, shapes
 * with slots [0..N-1] are stored in the fixed slots, and the remainder are
 * stored in the dynamic array. If all properties fit in the fixed slots, the
 * 'slots' member is NULL.
 *
 * Elements are indexed via the 'elements' member. This member can point to
 * either the shared emptyObjectElements singleton, into the inline value array
 * (the address of the third value, to leave room for a ObjectElements header;
 * in this case numFixedSlots() is zero) or to a dynamically allocated array.
 *
 * Only certain combinations of properties and elements storage are currently
 * possible. This will be changing soon :XXX: bug 586842.
 *
 * - For objects other than arrays and typed arrays, the elements are empty.
 *
 * - For 'slow' arrays, both elements and properties are used, but the
 *   elements have zero capacity --- only the length member is used.
 *
 * - For dense arrays, elements are used and properties are not used.
 *
 * - For typed array buffers, elements are used and properties are not used.
 *   The data indexed by the elements do not represent Values, but primitive
 *   unboxed integers or floating point values.
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
     * all other information about its structure. See jsscope.h.
     */
    HeapPtrShape shape_;

    /*
     * The object's type and prototype. For objects with the LAZY_TYPE flag
     * set, this is the prototype's default 'new' type and can only be used
     * to get that prototype.
     */
    HeapPtrTypeObject type_;

    HeapValue *slots;     /* Slots for object properties. */
    HeapValue *elements;  /* Slots for object elements. */

  protected:
    friend struct Shape;
    friend struct GCMarker;
    friend class  NewObjectCache;

};

} /* namespace js */

#endif /* ObjectImpl_h__ */
