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

/*
 * JS runtime exception classes.
 */

#ifndef jsexn_h___
#define jsexn_h___

JS_BEGIN_EXTERN_C

/*
 * Initialize exception object hierarchy.
 */
extern JSObject *
js_InitExceptionClasses(JSContext *cx, JSObject *obj);

/*
 * Given a JSErrorReport, check to see if there is an exception associated
 * with the error number.  If there is, then create an appropriate exception
 * object, set it as the pending exception, and set the JSREPORT_EXCEPTION
 * flag on the error report.  Exception-aware host error reporters will
 * know to ignore error reports so flagged.  Returns JS_TRUE if an associated
 * exception is found, JS_FALSE if none.
 */
extern JSBool
js_ErrorToException(JSContext *cx, JSErrorReport *reportp, const char *message);

JS_END_EXTERN_C

#endif /* jsexn_h___ */
