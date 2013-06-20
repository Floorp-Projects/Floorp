/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS runtime exception classes.
 */

#ifndef jsexn_h
#define jsexn_h

#include "jsapi.h"

/*
 * Initialize the exception constructor/prototype hierarchy.
 */
extern JSObject *
js_InitExceptionClasses(JSContext *cx, js::HandleObject obj);

/*
 * Given a JSErrorReport, check to see if there is an exception associated with
 * the error number.  If there is, then create an appropriate exception object,
 * set it as the pending exception, and set the JSREPORT_EXCEPTION flag on the
 * error report.  Exception-aware host error reporters should probably ignore
 * error reports so flagged.  Returns JS_TRUE if an associated exception is
 * found and set, JS_FALSE otherwise.
 */
extern JSBool
js_ErrorToException(JSContext *cx, const char *message, JSErrorReport *reportp,
                    JSErrorCallback callback, void *userRef);

/*
 * Called if a JS API call to js_Execute or js_InternalCall fails; calls the
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

extern JSErrorReport *
js_ErrorFromException(jsval exn);

extern const JSErrorFormatString *
js_GetLocalizedErrorMessage(JSContext* cx, void *userRef, const char *locale,
                            const unsigned errorNumber);

/*
 * Make a copy of errobj parented to scope.
 *
 * cx must be in the same compartment as scope. errobj may be in a different
 * compartment, but it must be an Error object (not a wrapper of one) and it
 * must not be one of the prototype objects created by js_InitExceptionClasses
 * (errobj->getPrivate() must not be NULL).
 */
extern JSObject *
js_CopyErrorObject(JSContext *cx, js::HandleObject errobj, js::HandleObject scope);

static JS_INLINE JSProtoKey
GetExceptionProtoKey(int exn)
{
    JS_ASSERT(JSEXN_ERR <= exn);
    JS_ASSERT(exn < JSEXN_LIMIT);
    return JSProtoKey(JSProto_Error + exn);
}

#endif /* jsexn_h */
