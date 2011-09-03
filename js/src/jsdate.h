/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * JS Date class interface.
 */

#ifndef jsdate_h___
#define jsdate_h___

#include "jsobj.h"

extern js::Class js_DateClass;

inline bool
JSObject::isDate() const
{
    return getClass() == &js_DateClass;
}

#define HalfTimeDomain  8.64e15

#define TIMECLIP(d) ((JSDOUBLE_IS_FINITE(d) \
                      && !((d < 0 ? -d : d) > HalfTimeDomain)) \
                     ? js_DoubleToInteger(d + (+0.)) : js_NaN)

extern JSObject *
js_InitDateClass(JSContext *cx, JSObject *obj);

/*
 * These functions provide a C interface to the date/time object
 */

/*
 * Construct a new Date Object from a time value given in milliseconds UTC
 * since the epoch.
 */
extern JS_FRIEND_API(JSObject*)
js_NewDateObjectMsec(JSContext* cx, jsdouble msec_time);

/*
 * Construct a new Date Object from an exploded local time value.
 *
 * Assert that mon < 12 to help catch off-by-one user errors, which are common
 * due to the 0-based month numbering copied into JS from Java (java.util.Date
 * in 1995).
 */
extern JS_FRIEND_API(JSObject*)
js_NewDateObject(JSContext* cx, int year, int mon, int mday,
                 int hour, int min, int sec);

/*
 * Detect whether the internal date value is NaN.  (Because failure is
 * out-of-band for js_DateGet*)
 */
extern JS_FRIEND_API(JSBool)
js_DateIsValid(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(int)
js_DateGetYear(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(int)
js_DateGetMonth(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(int)
js_DateGetDate(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(int)
js_DateGetHours(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(int)
js_DateGetMinutes(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(int)
js_DateGetSeconds(JSContext *cx, JSObject* obj);

extern JS_FRIEND_API(jsdouble)
js_DateGetMsecSinceEpoch(JSContext *cx, JSObject *obj);

typedef uint32 JSIntervalTime;

extern JS_FRIEND_API(JSIntervalTime)
js_IntervalNow();

/* Date constructor native. Exposed only so the JIT can know its address. */
JSBool
js_Date(JSContext *cx, uintN argc, js::Value *vp);

#endif /* jsdate_h___ */
