/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_ArrayObject_inl_h
#define vm_ArrayObject_inl_h

#include "vm/ArrayObject.h"

#include "vm/String.h"

#include "jsinferinlines.h"

namespace js {

inline void
ArrayObject::setLength(ExclusiveContext *cx, uint32_t length)
{
    MOZ_ASSERT(lengthIsWritable());

    if (length > INT32_MAX) {
        /* Track objects with overflowing lengths in type information. */
        types::MarkTypeObjectFlags(cx, this, types::OBJECT_FLAG_LENGTH_OVERFLOW);
    }

    getElementsHeader()->length = length;
}

} // namespace js

#endif // vm_ArrayObject_inl_h

