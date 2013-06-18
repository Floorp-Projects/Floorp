/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Iterator_inl_h_
#define Iterator_inl_h_

#include "jsiter.h"
#include "vm/ObjectImpl-inl.h"

js::NativeIterator *
js::PropertyIteratorObject::getNativeIterator() const
{
    JS_ASSERT(is<PropertyIteratorObject>());
    return static_cast<js::NativeIterator *>(getPrivate());
}

inline void
js::PropertyIteratorObject::setNativeIterator(js::NativeIterator *ni)
{
    setPrivate(ni);
}

#endif  // Iterator_inl_h_
