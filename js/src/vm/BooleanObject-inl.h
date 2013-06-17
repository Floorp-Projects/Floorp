/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BooleanObject_inl_h___
#define BooleanObject_inl_h___

#include "vm/BooleanObject.h"

#include "jsobjinlines.h"

inline js::BooleanObject &
JSObject::asBoolean()
{
    JS_ASSERT(isBoolean());
    return *static_cast<js::BooleanObject *>(this);
}

namespace js {

inline BooleanObject *
BooleanObject::create(JSContext *cx, bool b)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &BooleanClass);
    if (!obj)
        return NULL;
    BooleanObject &boolobj = obj->asBoolean();
    boolobj.setPrimitiveValue(b);
    return &boolobj;
}

} // namespace js

#endif /* BooleanObject_inl_h__ */
