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
 * Given a JSErrorReport, check to see if there is an exception associated with
 * the error number.  If there is, then create an appropriate exception object,
 * set it as the pending exception, and set the JSREPORT_EXCEPTION flag on the
 * error report.  Exception-aware host error reporters should probably ignore
 * error reports so flagged.  Returns JS_TRUE if an associated exception is
 * found and set, JS_FALSE otherwise..
 */
extern JSBool
js_ErrorToException(JSContext *cx, const char *message, JSErrorReport *reportp);

/*
 * Called if an api call to js_Execute or js_CallFunctionValue fails; calls the
 * error reporter with the error report associated with any uncaught exception
 * that has been raised.  Returns true if there was an exception pending, and
 * the error reporter was actually called.
 *
 * The JSErrorReport * that the error reporter is called with is currently
 * associated with a JavaScript object, and is not guaranteed to persist after
 * the object is collected.  Any persistent uses of the JSErrorReport contents
 * should make their own copy.
 *
 * The flags field of the JSErrorReport will have the JSREPORT_EXCEPTION flag
 * set; embeddings that want to silently propagate JavaScript exceptions to
 * other contexts may want to use an error reporter that ignores errors with
 * this flag.
 */
extern JSBool
js_ReportUncaughtException(JSContext *cx);

JS_END_EXTERN_C

#endif /* jsexn_h___ */
