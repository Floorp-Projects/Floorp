/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_StringObject_inl_h
#define vm_StringObject_inl_h

#include "vm/StringObject.h"

#include "jsobjinlines.h"

namespace js {

inline bool
StringObject::init(JSContext *cx, HandleString str)
{
    JS_ASSERT(numFixedSlots() == 2);

    Rooted<StringObject *> self(cx, this);

    if (nativeEmpty()) {
        if (isDelegate()) {
            if (!assignInitialShape(cx))
                return false;
        } else {
            RootedShape shape(cx, assignInitialShape(cx));
            if (!shape)
                return false;
            RootedObject proto(cx, self->getProto());
            EmptyShape::insertInitialShape(cx, shape, proto);
        }
    }

    JS_ASSERT(self->nativeLookup(cx, NameToId(cx->names().length))->slot() == LENGTH_SLOT);

    self->setStringThis(str);

    return true;
}

inline StringObject *
StringObject::create(JSContext *cx, HandleString str, NewObjectKind newKind)
{
    JSObject *obj = NewBuiltinClassInstance(cx, &class_, newKind);
    if (!obj)
        return nullptr;
    Rooted<StringObject*> strobj(cx, &obj->as<StringObject>());
    if (!strobj->init(cx, str))
        return nullptr;
    return strobj;
}

} // namespace js

#endif /* vm_StringObject_inl_h */
