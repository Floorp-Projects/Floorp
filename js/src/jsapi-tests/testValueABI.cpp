/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi-tests/tests.h"

/*
 * Bug 689101 - jsval is technically a non-POD type because it has a private
 * data member. On gcc, this doesn't seem to matter. On MSVC, this prevents
 * returning a jsval from a function between C and C++ because it will use a
 * retparam in C++ and a direct return value in C.
 *
 * Bug 712289 - jsval alignment was different on 32-bit platforms between C and
 * C++ because the default alignments of js::Value and jsval_layout differ.
 */

extern "C" {

extern bool
C_ValueToObject(JSContext* cx, jsval v, JSObject** obj);

extern jsval
C_GetEmptyStringValue(JSContext* cx);

extern size_t
C_jsvalAlignmentTest();

}

BEGIN_TEST(testValueABI_retparam)
{
    JS::RootedObject obj(cx, JS::CurrentGlobalOrNull(cx));
    RootedValue v(cx, ObjectValue(*obj));
    obj = nullptr;
    CHECK(C_ValueToObject(cx, v, obj.address()));
    bool equal;
    RootedValue v2(cx, ObjectValue(*obj));
    CHECK(JS_StrictlyEqual(cx, v, v2, &equal));
    CHECK(equal);

    v = C_GetEmptyStringValue(cx);
    CHECK(v.isString());

    return true;
}
END_TEST(testValueABI_retparam)

BEGIN_TEST(testValueABI_alignment)
{
    typedef struct { char c; jsval v; } AlignTest;
    CHECK(C_jsvalAlignmentTest() == sizeof(AlignTest));

    return true;
}
END_TEST(testValueABI_alignment)
