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

#ifndef jsbool_h___
#define jsbool_h___
/*
 * JS boolean interface.
 */

PR_BEGIN_EXTERN_C

extern JSObject *
js_InitBooleanClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_BooleanToObject(JSContext *cx, JSBool b);

extern JSString *
js_BooleanToString(JSContext *cx, JSBool b);

extern JSBool
js_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp);

PR_END_EXTERN_C

#endif /* jsbool_h___ */
