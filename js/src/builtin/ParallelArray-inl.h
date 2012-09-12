/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ParallelArray_inl_h__
#define ParallelArray_inl_h__

#include "builtin/ParallelArray.h"

#include "jsobjinlines.h"

namespace js {

inline bool
ParallelArrayObject::IndexInfo::inBounds() const
{
    JS_ASSERT(isInitialized());
    JS_ASSERT(indices.length() <= dimensions.length());

    for (uint32_t d = 0; d < indices.length(); d++) {
        if (indices[d] >= dimensions[d])
            return false;
    }

    return true;
}

inline uint32_t
ParallelArrayObject::IndexInfo::scalarLengthOfDimensions()
{
    JS_ASSERT(isInitialized());
    return dimensions[0] * partialProducts[0];
}

inline uint32_t
ParallelArrayObject::IndexInfo::toScalar()
{
    JS_ASSERT(isInitialized());
    JS_ASSERT(indices.length() <= partialProducts.length());

    if (indices.length() == 0)
        return 0;
    if (dimensions.length() == 1)
        return indices[0];

    uint32_t index = indices[0] * partialProducts[0];
    for (uint32_t i = 1; i < indices.length(); i++)
        index += indices[i] * partialProducts[i];
    return index;
}

inline bool
ParallelArrayObject::IndexInfo::fromScalar(uint32_t index)
{
    JS_ASSERT(isInitialized());
    if (!indices.resize(partialProducts.length()))
        return false;

    if (dimensions.length() == 1) {
        indices[0] = index;
        return true;
    }

    uint32_t prev = index;
    uint32_t d;
    for (d = 0; d < partialProducts.length() - 1; d++) {
        indices[d] = prev / partialProducts[d];
        prev = prev % partialProducts[d];
    }
    indices[d] = prev;

    return true;
}

inline bool
ParallelArrayObject::IndexInfo::initialize(uint32_t space)
{
    // Initialize using a manually set dimension vector.
    JS_ASSERT(dimensions.length() > 0);
    JS_ASSERT(space <= dimensions.length());

    // Compute the partial products of the dimensions.
    //
    // NB: partialProducts[i] is the scalar length of dimension i. The scalar
    // length of the entire space is thus dimensions[0] * partialProducts[0].
    uint32_t ndims = dimensions.length();
    if (!partialProducts.resize(ndims))
        return false;
    partialProducts[ndims - 1] = 1;
    for (uint32_t i = ndims - 1; i > 0; i--)
        partialProducts[i - 1] = dimensions[i] * partialProducts[i];

    // Reserve indices.
    return indices.reserve(ndims) && indices.resize(space);
}

inline bool
ParallelArrayObject::IndexInfo::initialize(JSContext *cx, HandleParallelArrayObject source,
                                           uint32_t space)
{
    // Initialize using a dimension vector gotten from a parallel array
    // source.
    if (!source->getDimensions(cx, dimensions))
        return false;

    return initialize(space);
}

inline bool
ParallelArrayObject::DenseArrayToIndexVector(JSContext *cx, HandleObject obj,
                                             IndexVector &indices)
{
    uint32_t length = obj->getDenseArrayInitializedLength();
    if (!indices.resize(length))
        return false;

    // Read the index vector out of the dense array into an actual Vector for
    // ease of access. We're guaranteed that the elements of the dense array
    // are uint32s, so just cast.
    const Value *src = obj->getDenseArrayElements();
    const Value *end = src + length;
    for (uint32_t *dst = indices.begin(); src < end; dst++, src++)
        *dst = static_cast<uint32_t>(src->toInt32());

    return true;
}

inline bool
ParallelArrayObject::is(const Value &v)
{
    return v.isObject() && is(&v.toObject());
}

inline bool
ParallelArrayObject::is(JSObject *obj)
{
    return obj->hasClass(&class_);
}

inline ParallelArrayObject *
ParallelArrayObject::as(JSObject *obj)
{
    JS_ASSERT(is(obj));
    return static_cast<ParallelArrayObject *>(obj);
}

inline JSObject *
ParallelArrayObject::dimensionArray()
{
    JSObject &dimObj = getSlot(SLOT_DIMENSIONS).toObject();
    JS_ASSERT(dimObj.isDenseArray());
    return &dimObj;
}

inline JSObject *
ParallelArrayObject::buffer()
{
    JSObject &buf = getSlot(SLOT_BUFFER).toObject();
    JS_ASSERT(buf.isDenseArray());
    return &buf;
}

inline uint32_t
ParallelArrayObject::bufferOffset()
{
    return static_cast<uint32_t>(getSlot(SLOT_BUFFER_OFFSET).toInt32());
}

inline uint32_t
ParallelArrayObject::outermostDimension()
{
    return static_cast<uint32_t>(dimensionArray()->getDenseArrayElement(0).toInt32());
}

inline bool
ParallelArrayObject::isOneDimensional()
{
    return dimensionArray()->getDenseArrayInitializedLength() == 1;
}

inline bool
ParallelArrayObject::getDimensions(JSContext *cx, IndexVector &dims)
{
    RootedObject obj(cx, dimensionArray());
    if (!obj)
        return false;
    return DenseArrayToIndexVector(cx, obj, dims);
}

} // namespace js

#endif // ParallelArray_inl_h__
