/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayObject_h
#define vm_ArrayObject_h

#include "jsobj.h"

namespace js {

class ArrayObject : public JSObject
{
  public:
    // Array(x) eagerly allocates dense elements if x <= this value. Without
    // the subtraction the max would roll over to the next power-of-two (4096)
    // due to the way that growElements() and goodAllocated() work.
    static const uint32_t EagerAllocationMaxLength = 2048 - ObjectElements::VALUES_PER_HEADER;

    static const Class class_;

    bool lengthIsWritable() const {
        return !getElementsHeader()->hasNonwritableArrayLength();
    }

    uint32_t length() const {
        return getElementsHeader()->length;
    }

    inline void setLength(ExclusiveContext *cx, uint32_t length);

    // Variant of setLength for use on arrays where the length cannot overflow int32_t.
    void setLengthInt32(uint32_t length) {
        MOZ_ASSERT(lengthIsWritable());
        MOZ_ASSERT(length <= INT32_MAX);
        getElementsHeader()->length = length;
    }
};

} // namespace js

#endif // vm_ArrayObject_h

