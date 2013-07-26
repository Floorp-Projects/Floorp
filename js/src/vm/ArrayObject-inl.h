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

/* static */ inline void
ArrayObject::setLength(ExclusiveContext *cx, Handle<ArrayObject*> arr, uint32_t length)
{
    JS_ASSERT(arr->lengthIsWritable());

    if (length > INT32_MAX) {
        /* Track objects with overflowing lengths in type information. */
        types::MarkTypeObjectFlags(cx, arr, types::OBJECT_FLAG_LENGTH_OVERFLOW);
        jsid lengthId = NameToId(cx->names().length);
        types::AddTypePropertyId(cx, arr, lengthId, types::Type::DoubleType());
    }

    arr->getElementsHeader()->length = length;
}

} // namespace js

#endif // vm_ArrayObject_inl_h

