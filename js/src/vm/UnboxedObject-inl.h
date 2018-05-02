/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_UnboxedObject_inl_h
#define vm_UnboxedObject_inl_h

#include "vm/UnboxedObject.h"

#include "gc/StoreBuffer-inl.h"
#include "vm/ArrayObject-inl.h"
#include "vm/NativeObject-inl.h"

namespace js {

/////////////////////////////////////////////////////////////////////
// UnboxedPlainObject
/////////////////////////////////////////////////////////////////////

inline const UnboxedLayout&
UnboxedPlainObject::layout() const
{
    AutoSweepObjectGroup sweep(group());
    return group()->unboxedLayout(sweep);
}

/////////////////////////////////////////////////////////////////////
// UnboxedLayout
/////////////////////////////////////////////////////////////////////

gc::AllocKind
js::UnboxedLayout::getAllocKind() const
{
    MOZ_ASSERT(size());
    return gc::GetGCObjectKindForBytes(UnboxedPlainObject::offsetOfData() + size());
}

} // namespace js

#endif // vm_UnboxedObject_inl_h
