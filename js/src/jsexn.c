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
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsexn.h"
#include "jsfun.h"

#if JS_HAS_ERROR_EXCEPTIONS
#if !JS_HAS_EXCEPTIONS
# error "JS_HAS_EXCEPTIONS must be defined to use JS_HAS_ERROR_EXCEPTIONS"
#endif

/* Declaration to resolve circular reference of exn_class by Exception. */
static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static void
exn_finalize(JSContext *cx, JSObject *obj);

static JSClass exn_class = {
    "Exception",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize,
    NULL,             NULL,             NULL,             Exception
};

/*
 * A copy of the JSErrorReport originally generated.
 */
typedef struct JSExnPrivate {
    JSErrorReport *errorReport;
} JSExnPrivate;

/*
 * Copy everything interesting about an error into allocated memory.
 */
static JSExnPrivate *
exn_initPrivate(JSContext *cx, JSErrorReport *report)
{
    JSExnPrivate *newPrivate;
    JSErrorReport *newReport;

    newPrivate = (JSExnPrivate *)JS_malloc(cx, sizeof (JSExnPrivate));

    /* Copy the error report */
    newReport = (JSErrorReport *)JS_malloc(cx, sizeof (JSErrorReport));

    if (report->filename != NULL) {
	newReport->filename =
	    (const char *)JS_malloc(cx, strlen(report->filename)+1);
	strcpy((char *)newReport->filename, report->filename);
    } else {
	newReport->filename = NULL;
    }

    newReport->lineno = report->lineno;

    /*
     * We don't need to copy linebuf and tokenptr, because they
     * point into the deflated string cache.  (currently?)
     */
    newReport->linebuf = report->linebuf;
    newReport->tokenptr = report->tokenptr;

    /*
     * But we do need to copy uclinebuf, uctokenptr, because they're
     * pointers into internal tokenstream structs, and may go away.
     *
     * NOTE nothing uses this and I'm not really maintaining it until
     * I know it's the desired API.
     */
    if (report->uclinebuf != NULL) {
	jsint len = js_strlen(report->uclinebuf) + 1;
	newReport->uclinebuf =
	    (const jschar *)JS_malloc(cx, len * sizeof(jschar));
	js_strncpy((jschar *)newReport->uclinebuf, report->uclinebuf, len);
	newReport->uctokenptr = newReport->uclinebuf + (report->uctokenptr -
							report->uclinebuf);
    } else
	newReport->uclinebuf = newReport->uctokenptr = NULL;
    
    if (report->ucmessage != NULL) {
        jsint len = js_strlen(report->ucmessage) + 1;
        newReport->ucmessage = (const jschar *)JS_malloc(cx, len);
        js_strncpy((jschar *)newReport->ucmessage, report->ucmessage, len);
        
        if (report->messageArgs) {
            intN i;
            
            for (i = 0; report->messageArgs[i] != NULL; i++)
                ;
            JS_ASSERT(i);
            newReport->messageArgs =
                (const jschar **)JS_malloc(cx, (i + 1) * sizeof(jschar *));
            for (i = 0; report->messageArgs[i] != NULL; i++) {
                len = js_strlen(report->messageArgs[i]) + 1;
                newReport->messageArgs[i] =
                    (const jschar *)JS_malloc(cx, len * sizeof(jschar));
                js_strncpy((jschar *)(newReport->messageArgs[i]),
                           report->messageArgs[i], len);
            }
            report->messageArgs[i] = NULL;
        } else {
            report->messageArgs = NULL;
        }
    } else {
        newReport->ucmessage = NULL;
        newReport->messageArgs = NULL;
    }
    newReport->errorNumber = report->errorNumber;

    /* Note that this is before it gets flagged with JSREPORT_EXCEPTION */
    newReport->flags = report->flags;

    newPrivate->errorReport = newReport;
    return newPrivate;
}

/*
 * Undo all the damage done by exn_initPrivate.
 */
static void
exn_destroyPrivate(JSContext *cx, JSExnPrivate *privateData)
{
    JSErrorReport *report;
    const jschar **args;
    
    report = privateData->errorReport;
    JS_ASSERT(report);
    if (report->uclinebuf)
	JS_free(cx, (void *)report->uclinebuf);
    if (report->filename)
	JS_free(cx, (void *)report->filename);
    if (report->ucmessage)
	JS_free(cx, (void *)report->ucmessage);
    if (report->messageArgs) {
        args = report->messageArgs;
        while(*args != NULL)
            JS_free(cx, (void *)*args++);
        JS_free(cx, (void *)report->messageArgs);
    }
    JS_free(cx, report);
    JS_free(cx, privateData);
}

static void
exn_finalize(JSContext *cx, JSObject *obj)
{
    JSExnPrivate *privateData;
    jsval private;

    private = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);

    if (private != JSVAL_NULL) {
        privateData = JSVAL_TO_PRIVATE(private);
        if (privateData)
            exn_destroyPrivate(cx, privateData);
    }
}

static JSErrorReport *
js_ErrorFromException(JSContext *cx, jsval exn)
{
    JSObject *obj;
    JSExnPrivate *privateData;
    jsval private;

    if (!JSVAL_IS_OBJECT(exn))
        return NULL;
    obj = JSVAL_TO_OBJECT(exn);
    if (OBJ_GET_CLASS(cx, obj) != &exn_class)
        return NULL;
    private = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    privateData = JSVAL_TO_PRIVATE(OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE));
    if (!privateData)
        return NULL;
    
    JS_ASSERT(privateData->errorReport);
    return privateData->errorReport;
}

/* This must be kept in synch with the exceptions array below. */
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

struct JSExnSpec {
    int protoIndex;
    const char *name;
};

static struct JSExnSpec exceptions[] = {
    { JSEXN_NONE,          "Exception"         },
    { JSEXN_EXCEPTION,     "Error"             },
    { JSEXN_ERR,           "InternalError"     },
    { JSEXN_ERR,           "SyntaxError"       },
    { JSEXN_ERR,           "ReferenceError"    },
    { JSEXN_ERR,           "CallError"         },
    { JSEXN_CALLERR,       "TargetError"       },
    { JSEXN_ERR,           "ConstructorError"  },
    { JSEXN_ERR,           "ConversionError"   },
    { JSEXN_CONVERSIONERR, "ToObjectError"     },
    { JSEXN_CONVERSIONERR, "ToPrimitiveError"  },
    { JSEXN_CONVERSIONERR, "DefaultValueError" },
    { JSEXN_ERR,           "ArrayError"        },
    {0}
};

static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *message;

    /*
     * Does it make sense to throw a not-a-function-error here?
     * A constructor that was not a function would be novel.
     */
    if (!cx->fp->constructing) {
	return JS_TRUE;
    }

    /*
     * If it's a new object of class Exception, then null out the private
     * data so that the finalizer doesn't attempt to free it.
     */
    if (OBJ_GET_CLASS(cx, obj) == &exn_class)
        OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, JSVAL_NULL);

    /*
     * Set the 'message' property.
     */
    if (argc > 0) {
	message = js_ValueToString(cx, argv[0]);
	if (!message)
	    return JS_FALSE;
    } else {
	message = cx->runtime->emptyString;
    }
    if (!JS_DefineProperty(cx, obj, "message", STRING_TO_JSVAL(message),
                           NULL, NULL, JSPROP_ENUMERATE));

    return JS_TRUE;
}

/*
 * Convert to string.
 *
 * This method only uses JavaScript-modifiable properties name, message; it is
 * left to the host to check for private data and report filename and line
 * number information along with this message.
 */
static JSBool
exn_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    JSString *name, *message, *result;
    jschar *chars, *cp;
    size_t length;
    jsid id;

    id = (jsid) cx->runtime->atomState.nameAtom;
    if (!OBJ_GET_PROPERTY(cx, obj, id, &v) || !(name = js_ValueToString(cx, v)))
        return JS_FALSE;

    if (!JS_GetProperty(cx, obj, "message", &v) ||
        !(message = js_ValueToString(cx, v)))
        return JS_FALSE;

    if (message->length > 0) {
        length = name->length + message->length + 2;
        cp = chars = JS_malloc(cx, (length + 1) * sizeof(jschar));
        if (!chars)
            return JS_FALSE;
        
        js_strncpy(cp, name->chars, name->length);
        cp += name->length;
        *cp++ = ':'; *cp++ = ' ';
        js_strncpy(cp, message->chars, message->length);
        cp += message->length;
        *cp = 0;
        
        result = js_NewString(cx, chars, length, 0);
        if (!result) {
            JS_free(cx, chars);
            return JS_FALSE;
        }
    } else {
        result = name;
    }

    *rval = STRING_TO_JSVAL(result);
    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
/*
 * Return a string that may eval to something similar to the original object.
 */
static JSBool
exn_toSource(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *name, *message, *result;
    jschar *chars, *cp;
    jsid id;
    size_t length;
    jsval v;

    id = (jsid) cx->runtime->atomState.nameAtom;
    if (!OBJ_GET_PROPERTY(cx, obj, id, &v) || !(name = js_ValueToString(cx, v)))
        return JS_FALSE;

    if (!JS_GetProperty(cx, obj, "message", &v) ||
        !(message = js_ValueToString(cx, v)))
        return JS_FALSE;

    length = (message->length > 0) ? name->length + message->length + 10
        : name->length + 8;

    cp = chars = JS_malloc(cx, (length + 1) * sizeof(jschar));
    if (!chars)
        return JS_FALSE;

    *cp++ = '('; *cp++ = 'n'; *cp++ = 'e'; *cp++ = 'w'; *cp++ = ' ';
    js_strncpy(cp, name->chars, name->length);
    cp += name->length;
    *cp++ = '(';
    if (message->length > 0) {
        *cp++ = '"';
        js_strncpy(cp, message->chars, message->length);
        cp += message->length;
        *cp++ = '"';
    }
    *cp++ = ')'; *cp++ = ')'; *cp = 0;

    result = js_NewString(cx, chars, length, 0);
    if (!result) {
        JS_free(cx, chars);
        return JS_FALSE;
    }

    *rval = STRING_TO_JSVAL(result);
    return JS_TRUE;
}
#endif

static JSFunctionSpec exception_methods[] = {
#if JS_HAS_TOSOURCE
    {js_toSource_str,   exn_toSource,           0},
#endif
    {js_toString_str,   exn_toString,           0},
    {0}
};

JSObject *
js_InitExceptionClasses(JSContext *cx, JSObject *obj)
{
    int i;
    JSObject *protos[JSEXN_LIMIT];

    /* Initialize the prototypes first. */
    for (i = 0; exceptions[i].name != 0; i++) {
        JSAtom *atom;
        JSFunction *fun;
        JSString *nameString;
        int protoidx = exceptions[i].protoIndex;
        
        /* Make the prototype for the current constructor name. */
        protos[i] = js_NewObject(cx, &exn_class,
                                 protoidx >= 0 ? protos[protoidx] : NULL,
                                 obj);
        if (!protos[i])
            return NULL;

        atom = js_Atomize(cx, exceptions[i].name, strlen(exceptions[i].name), 0);

        /* Make a constructor function for the current name. */
        fun = js_DefineFunction(cx, obj, atom, Exception, 1, 0);
        if (!fun)
            return NULL; /* XXX also remove named property from obj... */

        /* Make this constructor make objects of class Exception. */
        fun->clasp = &exn_class;

        /* Make the prototype and constructor links. */
        if (!js_SetClassPrototype(cx, fun->object, protos[i],
                                  JSPROP_READONLY | JSPROP_PERMANENT))
            return NULL;

        /* proto bootstrap bit from JS_InitClass omitted. */
        nameString = JS_NewStringCopyZ(cx, exceptions[i].name);
        if (nameString == NULL)
            return NULL;
        
        /* Add the name property to the prototype. */
        if (!JS_DefineProperty(cx, protos[i], js_name_str,
                               STRING_TO_JSVAL(nameString),
                               NULL, NULL,
                               JSPROP_ENUMERATE)) {
            /* release it? */
            return NULL;
        }   
        
        /* So finalize knows whether to. */
        OBJ_SET_SLOT(cx, protos[i], JSSLOT_PRIVATE, JSVAL_NULL);
    }
    
    /*
     * Add an empty message property.  (To Exception.prototype only,
     * because this property will be the same for all the exception
     * protos.)
     */
    if (!JS_DefineProperty(cx, protos[0], "message",
                           STRING_TO_JSVAL(cx->runtime->emptyString),
                           NULL, NULL, JSPROP_ENUMERATE))
        return NULL;
    
    /*
     * Add methods only to Exception.prototype, because ostensibly all
     * exception types delegate to that.
     */
    if (!JS_DefineFunctions(cx, protos[0], exception_methods))
        return NULL;

    return protos[0];
}

static JSExnType errorToExceptionNum[] = {
#define MSG_DEF(name, number, count, exception, format) \
    exception,
#include "js.msg"
#undef MSG_DEF
};

#if defined ( DEBUG_mccabe ) && defined ( PRINTNAMES )
/* For use below... get character strings for error name and exception name */
static struct exnname { char *name; char *exception; } errortoexnname[] = {
#define MSG_DEF(name, number, count, exception, format) \
    {#name, #exception},
#include "js.msg"
#undef MSG_DEF
};
#endif /* DEBUG */

JSBool
js_ErrorToException(JSContext *cx, const char *message, JSErrorReport *reportp)
{
    JSErrNum errorNumber;
    JSObject *errObject, *errProto;
    JSExnType exn;
    JSExnPrivate *privateData;
    JSString *msgstr;

    /* Find the exception index associated with this error. */
    JS_ASSERT(reportp);
    errorNumber = reportp->errorNumber;
    exn = errorToExceptionNum[errorNumber];
    JS_ASSERT(exn < JSEXN_LIMIT);

#if defined( DEBUG_mccabe ) && defined ( PRINTNAMES )
    /* Print the error name and the associated exception name to stderr */
    fprintf(stderr, "%s\t%s\n",
	    errortoexnname[errorNumber].name,
	    errortoexnname[errorNumber].exception);
#endif

    /*
     * Return false (no exception raised) if no exception is associated
     * with the given error number.
     */
    if (exn == JSEXN_NONE)
	return JS_FALSE;

    /*
     * Try to get an appropriate prototype by looking up the corresponding
     * exception constructor name in the current context.  If the constructor
     * has been deleted or overwritten, this may fail or return NULL, and
     * js_NewObject will fall back to using Object.prototype.
     */
    if (!js_GetClassPrototype(cx, exceptions[exn].name, &errProto))
        errProto = NULL;

    /*
     * Use js_NewObject instead of js_ConstructObject, because
     * js_ConstructObject seems to require a frame.
     */
    errObject = js_NewObject(cx, &exn_class, errProto, NULL);

    /* Store 'message' as a javascript-visible value. */
    msgstr = JS_NewStringCopyZ(cx, message);
    if (!JS_DefineProperty(cx, errObject, "message", STRING_TO_JSVAL(msgstr),
                           NULL, NULL,
                           JSPROP_ENUMERATE))
        return JS_FALSE;

    /*
     * Construct a new copy of the error report, and store it in the
     * exception objects' private data.  We can't use the error report
     * handed in, because it's stack-allocated, and may point to transient
     * data in the JSTokenStream.
     */
    privateData = exn_initPrivate(cx, reportp);
    OBJ_SET_SLOT(cx, errObject, JSSLOT_PRIVATE, PRIVATE_TO_JSVAL(privateData));

    /* Set the generated Exception object as the current exception. */
    JS_SetPendingException(cx, OBJECT_TO_JSVAL(errObject));

    /* Flag the error report passed in to indicate an exception was raised. */
    reportp->flags |= JSREPORT_EXCEPTION;

    return JS_TRUE;
}
#endif /* JS_HAS_ERROR_EXCEPTIONS */

#if JS_HAS_EXCEPTIONS

extern JSBool
js_ReportUncaughtException(JSContext *cx)
{
    JSObject *exnObject;
    JSString *str;
    jsval exn;
    JSErrorReport *reportp;
    
    if (!JS_IsExceptionPending(cx))
        return JS_FALSE;
    
    if (!JS_GetPendingException(cx, &exn))
        return JS_FALSE;

    /*
     * Because js_ValueToString below could error and an exception object
     * could become unrooted, we root it here.
     */
    if (JSVAL_IS_OBJECT(exn) && exn != JSVAL_NULL) {
        exnObject = JSVAL_TO_OBJECT(exn);
        if (!js_AddRoot(cx, &exnObject, "exn.report.root"))
            return JS_FALSE;
    } else {
        exnObject = NULL;
    }
    str = js_ValueToString(cx, exn);

#if JS_HAS_ERROR_EXCEPTIONS
    reportp = js_ErrorFromException(cx, exn);
#else
    reportp = NULL;
#endif

    if (reportp == NULL) {
        /*
         * XXXmccabe todo: Instead of doing this, synthesize an error report
         * struct that includes the filename, lineno where the exception was
         * originally thrown.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNCAUGHT_EXCEPTION, js_GetStringBytes(str));
    } else {
        /* Flag the error as an exception. */
        reportp->flags |= JSREPORT_EXCEPTION;
        js_ReportErrorAgain(cx, js_GetStringBytes(str), reportp);
    }

    if (exnObject != NULL)
        js_RemoveRoot(cx, &exnObject);
    return JS_TRUE;
}

#endif	    /* JS_HAS_EXCEPTIONS */
