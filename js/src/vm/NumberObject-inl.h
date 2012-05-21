/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NumberObject_inl_h___
#define NumberObject_inl_h___

#include "NumberObject.h"

inline js::NumberObject &
JSObject::asNumber()
{
    JS_ASSERT(isNumber());
    return *static_cast<js::NumberObject *>(this);
}

namespace js {

inline NumberObject *
NumberObject::create(JSContext *cx, double d)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &NumberClass);
    if (!obj)
        return NULL;
    NumberObject &numobj = obj->asNumber();
    numobj.setPrimitiveValue(d);
    return &numobj;
}

inline NumberObject *
NumberObject::createWithProto(JSContext *cx, double d, JSObject &proto)
{
    JSObject *obj = NewObjectWithClassProto(cx, &NumberClass, &proto, NULL,
                                            gc::GetGCObjectKind(RESERVED_SLOTS));
    if (!obj)
        return NULL;
    NumberObject &numobj = obj->asNumber();
    numobj.setPrimitiveValue(d);
    return &numobj;
}

} // namespace js

#endif /* NumberObject_inl_h__ */
