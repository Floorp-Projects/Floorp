/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StringObject_inl_h___
#define StringObject_inl_h___

#include "StringObject.h"

inline js::StringObject &
JSObject::asString()
{
    JS_ASSERT(isString());
    return *static_cast<js::StringObject *>(this);
}

namespace js {

inline bool
StringObject::init(JSContext *cx, HandleString str)
{
    JS_ASSERT(gc::GetGCKindSlots(getAllocKind()) == 2);

    RootedVar<StringObject *> self(cx, this);

    if (nativeEmpty()) {
        if (isDelegate()) {
            if (!assignInitialShape(cx))
                return false;
        } else {
            Shape *shape = assignInitialShape(cx);
            if (!shape)
                return false;
            EmptyShape::insertInitialShape(cx, shape, self->getProto());
        }
    }

    JS_ASSERT(self->nativeLookupNoAllocation(cx, NameToId(cx->runtime->atomState.lengthAtom))->slot()
              == LENGTH_SLOT);

    self->setStringThis(str);

    return true;
}

inline StringObject *
StringObject::create(JSContext *cx, HandleString str)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &StringClass);
    if (!obj)
        return NULL;
    RootedVar<StringObject*> strobj(cx, &obj->asString());
    if (!strobj->init(cx, str))
        return NULL;
    return strobj;
}

inline StringObject *
StringObject::createWithProto(JSContext *cx, HandleString str, JSObject &proto)
{
    JSObject *obj = NewObjectWithClassProto(cx, &StringClass, &proto, NULL);
    if (!obj)
        return NULL;
    RootedVar<StringObject*> strobj(cx, &obj->asString());
    if (!strobj->init(cx, str))
        return NULL;
    return strobj;
}

} // namespace js

#endif /* StringObject_inl_h__ */
