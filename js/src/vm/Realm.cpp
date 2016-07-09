/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/Realm.h"

#include "jscntxt.h"
#include "jscompartment.h" // For ExclusiveContext::global

#include "vm/GlobalObject.h"

using namespace js;

JS_PUBLIC_API(JSObject*)
JS::GetRealmObjectPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateObjectPrototype(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmFunctionPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateFunctionPrototype(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmArrayPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateArrayPrototype(cx, cx->global());
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmErrorPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateCustomErrorPrototype(cx, cx->global(), JSEXN_ERR);
}

JS_PUBLIC_API(JSObject*)
JS::GetRealmIteratorPrototype(JSContext* cx)
{
    CHECK_REQUEST(cx);
    return GlobalObject::getOrCreateIteratorPrototype(cx, cx->global());
}
