/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=c:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"

/* See testValueABI.cpp */

JSBool
C_ValueToObject(JSContext *cx, jsval v, JSObject **obj)
{
    return JS_ValueToObject(cx, v, obj);
}

jsval
C_GetEmptyStringValue(JSContext *cx)
{
    return JS_GetEmptyStringValue(cx);
}

size_t
C_jsvalAlignmentTest()
{
    typedef struct { char c; jsval v; } AlignTest;
    return sizeof(AlignTest);
}
