/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef Iterator_inl_h_
#define Iterator_inl_h_

#include "jsiter.h"
#include "jsobjinlines.h"

inline bool
JSObject::isPropertyIterator() const
{
    return hasClass(&js::PropertyIteratorObject::class_);
}

inline js::PropertyIteratorObject &
JSObject::asPropertyIterator()
{
    JS_ASSERT(isPropertyIterator());
    return *static_cast<js::PropertyIteratorObject *>(this);
}

js::NativeIterator *
js::PropertyIteratorObject::getNativeIterator() const
{
    JS_ASSERT(isPropertyIterator());
    return static_cast<js::NativeIterator *>(getPrivate());
}

inline void
js::PropertyIteratorObject::setNativeIterator(js::NativeIterator *ni)
{
    setPrivate(ni);
}

#endif  // Iterator_inl_h_
