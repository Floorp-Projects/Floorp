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
 * JS standard exception implementation.
 */

#include "jsstddef.h"
#include "prtypes.h"
#include "prassert.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsexn.h"

#if JS_HAS_ERROR_EXCEPTIONS
#if !JS_HAS_EXCEPTIONS
# error "JS_HAS_EXCEPTIONS must be defined to use JS_HAS_ERROR_EXCEPTIONS"
#endif

/* Maybe #define RANDOM_CLASS(name, prototype) 4 here? */

struct JSExnSpec {
    int protoIndex;
    JSClass theclass;
};

/* This must be kept in synch with the exceptions array */

typedef enum JSExnType {
    JSEXN_NONE = -1,
    JSEXN_EXCEPTION,
      JSEXN_ERR,
        JSEXN_INTERNALERR,
        JSEXN_SYNTAXERR,
        JSEXN_REFERENCEERR,
        JSEXN_CALLERR,
          JSEXN_TARGETERR,
        JSEXN_CONSTRUCTORERR,        
        JSEXN_CONVERSIONERR,
          JSEXN_TOOBJECTERR,
          JSEXN_TOPRIMITIVEERR,        
          JSEXN_DEFAULTVALUEERR,
        JSEXN_ARRAYERR,
        JSEXN_LIMIT
} JSExnType;

#define FLAGS JSCLASS_HAS_PRIVATE

static struct JSExnSpec exceptions[] = {
    { JSEXN_NONE, /* No proto? */ {
        "Exception", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_EXCEPTION, {
        "Error", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "InternalError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "SyntaxError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "ReferenceError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "CallError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_CALLERR, {
        "TargetError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "ConstructorError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "ConversionError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_CONVERSIONERR, {
        "ToObjectError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_CONVERSIONERR, {
        "ToPrimitiveError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_CONVERSIONERR, {
        "DefaultValueError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    { JSEXN_ERR, {
        "ArrayError", FLAGS,
        JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
        JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   JS_FinalizeStub
    } },
    {0}
};

static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval bval;

    bval = JSVAL_FALSE;
    if (!cx->fp->constructing) {
	*rval = bval;
	return JS_TRUE;
    }
    return JS_TRUE;
}

static JSFunctionSpec exception_methods[] = {
    {0}
};

JSObject *
js_InitExceptionClasses(JSContext *cx, JSObject *obj)
{

    JSObject *protos[JSEXN_LIMIT];
    int i;

    for (i = 0; exceptions[i].theclass.name != 0; i++) {
        int protoidx = exceptions[i].protoIndex;
        protos[i] = JS_InitClass(cx, obj,
                                 ((protoidx >= 0) ? protos[protoidx] : NULL),
                                 &(exceptions[i].theclass),
                                 Exception, 1,
                                 NULL,
                                 exception_methods,
                                 NULL,
                                 NULL);
    }

    /*
     * JS_InitClass magically replaces a null prototype with Object.prototype,
     * so we need to explicitly assign to the proto slot to get null.
     *
     * Temporarily disabled until I do toString for Exception.
     */

/*     protos[0]->slots[JSSLOT_PROTO] = JSVAL_NULL; */
    
    return protos[0];
}

static JSExnType errorToException[] = {
#define MSG_DEF(name, number, count, exception, format) \
    exception,
#include "jsmsg.def"
#undef MSG_DEF
};

JSBool
js_ErrorToException(JSContext *cx, JSErrorReport *reportp) {
    JSErrNum errorNumber; 
    JSObject *errobj;
    JSExnType exn;

    PR_ASSERT(reportp);
    errorNumber = reportp->errorNumber;
    exn = errorToException[errorNumber];
    PR_ASSERT(exn < JSEXN_LIMIT);

    if (exn > JSEXN_NONE) {
        errobj = js_ConstructObject(cx,
                                    &(exceptions[exn].theclass),
                                    NULL, NULL);
        
        JS_SetPendingException(cx, OBJECT_TO_JSVAL(errobj));
        reportp->flags |= JSREPORT_EXCEPTION;
        return JS_TRUE;
    }
    return JS_FALSE;
}

#endif /* JS_HAS_ERROR_EXCEPTIONS */
