/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jsdate_h___
#define jsdate_h___
/*
 * JS Date class interface.
 */

PR_BEGIN_EXTERN_C

extern JSObject *
js_InitDateClass(JSContext *cx, JSObject *obj);

/*
 *  These functions provide a C interface to the date/time object
 */
extern JS_FRIEND_API(JSObject*)
js_NewDateObject(JSContext* cx, int year, int mon, int mday,
                                int hour, int min, int sec);

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

extern JS_FRIEND_API(void)
js_DateSetYear(JSContext *cx, JSObject *obj, int year);

extern JS_FRIEND_API(void)
js_DateSetMonth(JSContext *cx, JSObject *obj, int year);

extern JS_FRIEND_API(void)
js_DateSetDate(JSContext *cx, JSObject *obj, int date);

extern JS_FRIEND_API(void)
js_DateSetHours(JSContext *cx, JSObject *obj, int hours);

extern JS_FRIEND_API(void)
js_DateSetMinutes(JSContext *cx, JSObject *obj, int minutes);

extern JS_FRIEND_API(void)
js_DateSetSeconds(JSContext *cx, JSObject *obj, int seconds);


PR_END_EXTERN_C

#endif /* jsdate_h___ */
