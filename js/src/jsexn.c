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


/*
 * This is currently very messy, and in flux.  Don't anybody think
 * I'm going to leave it like this.  No no.
 */


#include "jsstddef.h"
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsexn.h"

#if JS_HAS_ERROR_EXCEPTIONS
#if !JS_HAS_EXCEPTIONS
# error "JS_HAS_EXCEPTIONS must be defined to use JS_HAS_ERROR_EXCEPTIONS"
#endif

JSObject *tlobj;

/*
 * This could end up just being the error report, but I
 * want to learn how to support random garbage here.
 *
 * If I go with it, a rambling comment mentioning GC, memory management etc. is
 * needed.
 *
 * I'd rather have the errorReport inline, to avoid the extra malloc'd object
 * dangle.  But I'll figure that out later, if I still need it.
 */
typedef struct JSExnPrivate {
    JSErrorReport *errorReport;
    const char *message;
} JSExnPrivate;


/*
 * Copy everything interesting about an error into allocated memory.
 */
static JSExnPrivate *
exn_initPrivate(JSContext *cx, JSErrorReport *report, const char *message)
{
    JSExnPrivate *newPrivate;
    JSErrorReport * newReport;
    char *newMessage;

    newPrivate = (JSExnPrivate *)JS_malloc(cx, sizeof (JSExnPrivate));

    JS_ASSERT(message);
    newMessage = (char *)JS_malloc(cx, strlen(message)+1);
    strcpy(newMessage, message);
    newPrivate->message = newMessage;

    /* Copy the error report */
    newReport = (JSErrorReport *)JS_malloc(cx, sizeof (JSErrorReport));

    if (report->filename) {
	newReport->filename =
	    (const char *)JS_malloc(cx, strlen(report->filename)+1);
	/* Ack.  Const! */
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
     * But only if they're non-null...
     *
     * NOTE nothing uses this and I'm not really maintaining it until
     * I know it's the desired API.
     *
     * Temporarily disabled, because uclinebuf is 0x10 when I evaluate 'Math()'!
     */

#if 0
    if (report->uclinebuf) {
	size_t len = js_strlen(report->uclinebuf)+1;
	newReport->uclinebuf =
	    (const jschar *)JS_malloc(cx, len);
	js_strncpy(newReport->uclinebuf, report->uclinebuf, len);
	newReport->uctokenptr = newReport->uclinebuf + (report->uctokenptr -
							report->uclinebuf);
    } else
#endif
	newReport->uclinebuf = newReport->uctokenptr = NULL;

    /* Note that this is before it gets flagged with JSREPORT_EXCEPTION */
    newReport->flags = report->flags;

    /* Skipping *ucmessage, **messageArgs for now.  My guess is that it'll just
     * mean copying the pointers, and adding another GC root.  Then de-rooting
     * them in the finalizer.  Dunno if they're rooted in the first place -
     * maybe it's only relevant for an exception that goes where it pleases,
     * and not for the formerly stack-bound use of the error report.
     */

    newPrivate->errorReport = newReport;

    return newPrivate;
}

/*
 * Undo all the damage done by exn_initPrivate.
 */
static void
exn_destroyPrivate(JSContext *cx, JSExnPrivate *privateData)
{
    JS_ASSERT(privateData->message);
    /* ! what does const do? */
    JS_free(cx, (void *)privateData->message);

    JS_ASSERT(privateData->errorReport);
    if (privateData->errorReport->uclinebuf)
	JS_free(cx, (void *)privateData->errorReport->uclinebuf);

    if (privateData->errorReport->filename)
	JS_free(cx, (void *)privateData->errorReport->filename);

    JS_free(cx, privateData->errorReport);

    JS_free(cx, privateData);
}

/* Destroy associated data... */
static void
exn_finalize(JSContext *cx, JSObject *obj)
{
    JSExnPrivate *privateData;

    privateData = (JSExnPrivate *)
	JSVAL_TO_PRIVATE(OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE));

    if (privateData) {
	exn_destroyPrivate(cx, privateData);
    }
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

#define FLAGS JSCLASS_HAS_PRIVATE

/* Maybe #define RANDOM_CLASS(name, prototype) 4 here? */
struct JSExnSpec {
    int protoIndex;
    JSClass theclass;
};

/*
 * I want to replace all of these with just one class.  All we really care
 * about is the prototypes, and the constructor names.
 */
static struct JSExnSpec exceptions[] = {
    { JSEXN_NONE, /* No proto? */ {
	"Exception", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_EXCEPTION, {
	"Error", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"InternalError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"SyntaxError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"ReferenceError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"CallError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_CALLERR, {
	"TargetError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"ConstructorError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"ConversionError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_CONVERSIONERR, {
	"ToObjectError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_CONVERSIONERR, {
	"ToPrimitiveError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_CONVERSIONERR, {
	"DefaultValueError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    { JSEXN_ERR, {
	"ArrayError", FLAGS,
	JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
	JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize
    } },
    {0}
};

static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    if (!cx->fp->constructing) {
	return JS_TRUE;
    }

    /* gotta null out that private data */
    OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, JSVAL_NULL);
    return JS_TRUE;
}

/*
 * Convert to string.  Much of this is taken from js.c.
 *
 * I should rewrite this to use message, line, file etc. from
 * javascript-modifiable properties (which might be lazily created
 * from the encapsulated error report.)
 */
static JSBool
exn_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSString *str;
    JSExnPrivate *privateData;
    JSErrorReport *report;
    jsval v;
    char *name;
    JSClass *theclass;

    /* Check needed against incompatible target... */

    /* Try to include the exception name in the error message. */
    theclass = OBJ_GET_CLASS(cx, obj);
    name = theclass->name;

    v = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);

    if (!JSVAL_IS_NULL(v)) {
	char *msgbuf, *tmp;

	privateData = JSVAL_TO_PRIVATE(v);
	report = privateData->errorReport;

	msgbuf = JS_smprintf("%s:", name);

	if (report->filename) {
	    tmp = msgbuf;
	    msgbuf = JS_smprintf("%s%s:", tmp, report->filename);
	    JS_free(cx, tmp);
	}
	if (report->lineno) {
	    tmp = msgbuf;
	    msgbuf = JS_smprintf("%s%u: ", tmp ? tmp : "", report->lineno);
	    if (tmp)
		JS_free(cx, tmp);
	}
	JS_ASSERT(privateData->message);
	tmp = msgbuf;
	msgbuf = JS_smprintf("%s%s", tmp ? tmp : "", privateData->message);
	if(tmp)
	    JS_free(cx, tmp);

	str = JS_NewStringCopyZ(cx, msgbuf);
    } else {
	str = JS_NewStringCopyZ(cx, "some non-engine-thrown exception");
    }
    if (!str)
	return JS_FALSE;
    *rval = STRING_TO_JSVAL(str);
    return JS_TRUE;
}

static JSFunctionSpec exception_methods[] = {
    {js_toString_str,   exn_toString,           0},
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

	/* So finalize knows whether to. */
	OBJ_SET_SLOT(cx, protos[i], JSSLOT_PRIVATE, JSVAL_NULL);
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

static JSErrorReport *
js_GetErrorFromException(JSContext *cx, JSObject *errobj)
{
    JSExnPrivate *privateData;
#if 0
    {
	JSClass *errobjclass;
	/* Assert that we have an Exception object */
	/* This assert does the right thing, but we can't use it yet, because
	 * we're throwing lots of different exception classes. */
	errobjclass = OBJ_GET_CLASS(cx, errobj);
	JS_ASSERT(errobjclass == &(exceptions[JSEXN_CALLERR].theclass));
    }
#endif
    privateData = JSVAL_TO_PRIVATE(OBJ_GET_SLOT(cx, errobj, JSSLOT_PRIVATE));

    /* Still OK to return NULL, tho. */
    return privateData->errorReport;
}

static JSExnType errorToException[] = {
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
js_ErrorToException(JSContext *cx, JSErrorReport *reportp, const char *message)
{
    JSErrNum errorNumber;
    JSObject *errobj;
    JSExnType exn;
    JSExnPrivate *privateData;

    JS_ASSERT(reportp);
    errorNumber = reportp->errorNumber;
    exn = errorToException[errorNumber];
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
     * Should (?) be js_ConstructObject... switching to NewObject
     * in the speculation that it won't require a frame.  DefaultValue trouble.
     * And it seems to work????  For the record, the trouble was that
     * cx->fp was null when trying to construct the object...
     */
    errobj = js_NewObject(cx,
			  &(exceptions[exn].theclass),
			  NULL, NULL);

    /*
     * Construct a new copy of the error report, and store it in the
     * exception objects' private data.  We can't use the error report
     * handed in, because it's stack-allocated, and may point to transient
     * data in the JSTokenStream.
     */

    /* XXX report failure? */
    privateData = exn_initPrivate(cx, reportp, message);
    OBJ_SET_SLOT(cx, errobj, JSSLOT_PRIVATE, PRIVATE_TO_JSVAL(privateData));

    JS_SetPendingException(cx, OBJECT_TO_JSVAL(errobj));
    reportp->flags |= JSREPORT_EXCEPTION;
    return JS_TRUE;
}

#endif /* JS_HAS_ERROR_EXCEPTIONS */

