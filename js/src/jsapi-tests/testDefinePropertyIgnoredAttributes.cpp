/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static bool
Getter(JSContext* cx, unsigned argc, JS::Value* vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(true);
    return true;
}

enum PropertyDescriptorKind {
    DataDescriptor, AccessorDescriptor
};

static bool
CheckDescriptor(JS::Handle<JS::PropertyDescriptor> desc, PropertyDescriptorKind kind,
                bool enumerable, bool writable, bool configurable)
{
    if (!desc.object())
        return false;
    if (!(kind == DataDescriptor ? desc.isDataDescriptor() : desc.isAccessorDescriptor()))
        return false;
    if (desc.enumerable() != enumerable)
        return false;
    if (kind == DataDescriptor && desc.writable() != writable)
        return false;
    if (desc.configurable() != configurable)
        return false;
    return true;
}

BEGIN_TEST(testDefinePropertyIgnoredAttributes)
{
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    JS::Rooted<JS::PropertyDescriptor> desc(cx);
    JS::RootedValue defineValue(cx);

    // Try a getter. Allow it to fill in the defaults. Because we're passing a
    // JSNative, JS_DefineProperty will infer JSPROP_GETTER even though we
    // aren't passing it.
    CHECK(JS_DefineProperty(cx, obj, "foo", defineValue,
                            JSPROP_IGNORE_ENUMERATE | JSPROP_IGNORE_PERMANENT | JSPROP_SHARED,
                            Getter));

    CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "foo", &desc));

    // Note that JSPROP_READONLY is meaningless for accessor properties.
    CHECK(CheckDescriptor(desc, AccessorDescriptor, false, true, false));

    // Install another configurable property, so we can futz with it.
    CHECK(JS_DefineProperty(cx, obj, "bar", defineValue,
                            JSPROP_IGNORE_ENUMERATE | JSPROP_SHARED,
                            Getter));
    CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "bar", &desc));
    CHECK(CheckDescriptor(desc, AccessorDescriptor, false, true, true));

    // Rewrite the descriptor to now be enumerable, leaving the configurability
    // unchanged.
    CHECK(JS_DefineProperty(cx, obj, "bar", defineValue,
                            JSPROP_IGNORE_PERMANENT | JSPROP_ENUMERATE | JSPROP_SHARED,
                            Getter));
    CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "bar", &desc));
    CHECK(CheckDescriptor(desc, AccessorDescriptor, true, true, true));

    // Now try the same game with a value property
    defineValue.setObject(*obj);
    CHECK(JS_DefineProperty(cx, obj, "baz", defineValue,
                            JSPROP_IGNORE_ENUMERATE |
                            JSPROP_IGNORE_READONLY |
                            JSPROP_IGNORE_PERMANENT));
    CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "baz", &desc));
    CHECK(CheckDescriptor(desc, DataDescriptor, false, false, false));

    // Now again with a configurable property
    CHECK(JS_DefineProperty(cx, obj, "quux", defineValue,
                            JSPROP_IGNORE_ENUMERATE | JSPROP_IGNORE_READONLY));
    CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "quux", &desc));
    CHECK(CheckDescriptor(desc, DataDescriptor, false, false, true));

    // Just make it writable. Leave the old value and everything else alone.
    defineValue.setUndefined();
    CHECK(JS_DefineProperty(cx, obj, "quux", defineValue,
                            JSPROP_IGNORE_ENUMERATE |
                            JSPROP_IGNORE_PERMANENT |
                            JSPROP_IGNORE_VALUE));

    CHECK(JS_GetOwnPropertyDescriptor(cx, obj, "quux", &desc));
    CHECK(CheckDescriptor(desc, DataDescriptor, false, true, true));
    CHECK_SAME(JS::ObjectValue(*obj), desc.value());

    return true;
}
END_TEST(testDefinePropertyIgnoredAttributes)
