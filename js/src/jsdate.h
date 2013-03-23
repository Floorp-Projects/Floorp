/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS Date class interface.
 */

#ifndef jsdate_h___
#define jsdate_h___

#include "mozilla/FloatingPoint.h"

#include <math.h>

#include "jstypes.h"

extern "C" {
class JSObject;
struct JSContext;
}

extern JSObject *
js_InitDateClass(JSContext *cx, js::HandleObject obj);

/*
 * These functions provide a C interface to the date/time object
 */

/*
 * Construct a new Date Object from a time value given in milliseconds UTC
 * since the epoch.
 */
extern JS_FRIEND_API(JSObject *)
js_NewDateObjectMsec(JSContext* cx, double msec_time);

/*
 * Construct a new Date Object from an exploded local time value.
 *
 * Assert that mon < 12 to help catch off-by-one user errors, which are common
 * due to the 0-based month numbering copied into JS from Java (java.util.Date
 * in 1995).
 */
extern JS_FRIEND_API(JSObject *)
js_NewDateObject(JSContext* cx, int year, int mon, int mday,
                 int hour, int min, int sec);

extern JS_FRIEND_API(int)
js_DateGetYear(JSContext *cx, JSRawObject obj);

extern JS_FRIEND_API(int)
js_DateGetMonth(JSContext *cx, JSRawObject obj);

extern JS_FRIEND_API(int)
js_DateGetDate(JSContext *cx, JSRawObject obj);

extern JS_FRIEND_API(int)
js_DateGetHours(JSContext *cx, JSRawObject obj);

extern JS_FRIEND_API(int)
js_DateGetMinutes(JSContext *cx, JSRawObject obj);

extern JS_FRIEND_API(int)
js_DateGetSeconds(JSRawObject obj);

/**
 * Checks that the this value provided meets the requirements for "this Date
 * object" in ES5.1, 15.9.5, and throws a TypeError if not.
 *
 * Usage: callFunction(date_CheckThisDate, this)
 */
extern JSBool
date_CheckThisDate(JSContext *cx, unsigned argc, js::Value *vp);

/* Date constructor native. Exposed only so the JIT can know its address. */
JSBool
js_Date(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsdate_h___ */
