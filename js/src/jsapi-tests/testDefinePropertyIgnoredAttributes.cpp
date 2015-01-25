/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

static const unsigned IgnoreWithValue = JSPROP_IGNORE_ENUMERATE | JSPROP_IGNORE_READONLY |
                               JSPROP_IGNORE_PERMANENT;
static const unsigned IgnoreAll = IgnoreWithValue | JSPROP_IGNORE_VALUE;

static const unsigned AllowConfigure = IgnoreAll & ~JSPROP_IGNORE_PERMANENT;
static const unsigned AllowEnumerate = IgnoreAll & ~JSPROP_IGNORE_ENUMERATE;
static const unsigned AllowWritable  = IgnoreAll & ~JSPROP_IGNORE_READONLY;
static const unsigned ValueWithConfigurable = IgnoreWithValue & ~JSPROP_IGNORE_PERMANENT;

static bool
Getter(JSContext *cx, unsigned argc, JS::Value *vp)
{
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(true);
    return true;
}

static bool
CheckDescriptor(JS::Handle<JSPropertyDescriptor> desc, bool enumerable,
                bool writable, bool configurable)
{
    if (!desc.object())
        return false;
    if (desc.isEnumerable() != enumerable)
        return false;
    if (desc.isReadonly() == writable)
        return false;
    if (desc.isPermanent() == configurable)
        return false;
    return true;
}

BEGIN_TEST(testDefinePropertyIgnoredAttributes)
{
    JS::RootedObject obj(cx, JS_NewPlainObject(cx));
    JS::Rooted<JSPropertyDescriptor> desc(cx);
    JS::RootedValue defineValue(cx);

    // Try a getter. Allow it to fill in the defaults.
    CHECK(JS_DefineProperty(cx, obj, "foo", defineValue,
                            IgnoreAll | JSPROP_SHARED,
                            Getter));

    CHECK(JS_GetPropertyDescriptor(cx, obj, "foo", &desc));

    // Note that since JSPROP_READONLY means nothing for accessor properties, we will actually
    // claim to be writable, since the flag is not included in the mask.
    CHECK(CheckDescriptor(desc, false, true, false));

    // Install another configurable property, so we can futz with it.
    CHECK(JS_DefineProperty(cx, obj, "bar", defineValue,
                            AllowConfigure | JSPROP_SHARED,
                            Getter));
    CHECK(JS_GetPropertyDescriptor(cx, obj, "bar", &desc));
    CHECK(CheckDescriptor(desc, false, true, true));

    // Rewrite the descriptor to now be enumerable, ensuring that the lack of
    // configurablity stayed.
    CHECK(JS_DefineProperty(cx, obj, "bar", defineValue,
                            AllowEnumerate |
                            JSPROP_ENUMERATE |
                            JSPROP_SHARED,
                            Getter));
    CHECK(JS_GetPropertyDescriptor(cx, obj, "bar", &desc));
    CHECK(CheckDescriptor(desc, true, true, true));

    // Now try the same game with a value property
    defineValue.setObject(*obj);
    CHECK(JS_DefineProperty(cx, obj, "baz", defineValue, IgnoreWithValue));
    CHECK(JS_GetPropertyDescriptor(cx, obj, "baz", &desc));
    CHECK(CheckDescriptor(desc, false, false, false));

    // Now again with a configurable property
    CHECK(JS_DefineProperty(cx, obj, "quox", defineValue, ValueWithConfigurable));
    CHECK(JS_GetPropertyDescriptor(cx, obj, "quox", &desc));
    CHECK(CheckDescriptor(desc, false, false, true));

    // Just make it writable. Leave the old value and everythign else alone.
    defineValue.setUndefined();
    CHECK(JS_DefineProperty(cx, obj, "quox", defineValue, AllowWritable));
    CHECK(JS_GetPropertyDescriptor(cx, obj, "quox", &desc));
    CHECK(CheckDescriptor(desc, false, true, true));
    CHECK_SAME(JS::ObjectValue(*obj), desc.value());

    return true;
}
END_TEST(testDefinePropertyIgnoredAttributes)
