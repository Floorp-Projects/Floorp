/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
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
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/*
 * JS standard exception implementation.
 */

#include <string.h>
#include "jsstddef.h"
#include "jstypes.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsopcode.h"
#include "jsnum.h"

#if JS_HAS_ERROR_EXCEPTIONS
#if !JS_HAS_EXCEPTIONS
# error "JS_HAS_EXCEPTIONS must be defined to use JS_HAS_ERROR_EXCEPTIONS"
#endif

/* XXX consider adding rt->atomState.messageAtom */
static char js_message_str[]  = "message";
static char js_filename_str[] = "fileName";
static char js_lineno_str[]   = "lineNumber";

/* Forward declarations for ExceptionClass's initializer. */
static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

static void
exn_finalize(JSContext *cx, JSObject *obj);

static JSClass ExceptionClass = {
    "Error",
    JSCLASS_HAS_PRIVATE,
    JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,  JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,   JS_ConvertStub,   exn_finalize,
    NULL,             NULL,             NULL,             Exception,
    NULL,             NULL,             NULL,             0
};

/*
 * A copy of the JSErrorReport originally generated.
 */
typedef struct JSExnPrivate {
    JSErrorReport *errorReport;
} JSExnPrivate;

/*
 * Undo all the damage done by exn_newPrivate.
 */
static void
exn_destroyPrivate(JSContext *cx, JSExnPrivate *privateData)
{
    JSErrorReport *report;
    const jschar **args;

    if (!privateData)
        return;
    report = privateData->errorReport;
    if (report) {
        if (report->uclinebuf)
	    JS_free(cx, (void *)report->uclinebuf);
        if (report->filename)
	    JS_free(cx, (void *)report->filename);
        if (report->ucmessage)
	    JS_free(cx, (void *)report->ucmessage);
        if (report->messageArgs) {
            args = report->messageArgs;
            while (*args != NULL)
                JS_free(cx, (void *)*args++);
            JS_free(cx, (void *)report->messageArgs);
        }
        JS_free(cx, report);
    }
    JS_free(cx, privateData);
}

/*
 * Copy everything interesting about an error into allocated memory.
 */
static JSExnPrivate *
exn_newPrivate(JSContext *cx, JSErrorReport *report)
{
    intN i;
    JSExnPrivate *newPrivate;
    JSErrorReport *newReport;
    size_t capacity;

    newPrivate = (JSExnPrivate *)JS_malloc(cx, sizeof (JSExnPrivate));
    if (!newPrivate)
        return NULL;
    memset(newPrivate, 0, sizeof (JSExnPrivate));

    /* Copy the error report */
    newReport = (JSErrorReport *)JS_malloc(cx, sizeof (JSErrorReport));
    if (!newReport)
        goto error;
    memset(newReport, 0, sizeof (JSErrorReport));
    newPrivate->errorReport = newReport;

    if (report->filename != NULL) {
        newReport->filename = JS_strdup(cx, report->filename);
        if (!newReport->filename)
            goto error;
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
	capacity = js_strlen(report->uclinebuf) + 1;
        newReport->uclinebuf =
            (const jschar *)JS_malloc(cx, capacity * sizeof(jschar));
        if (!newReport->uclinebuf)
            goto error;
	js_strncpy((jschar *)newReport->uclinebuf, report->uclinebuf, capacity);
	newReport->uctokenptr = newReport->uclinebuf + (report->uctokenptr -
							report->uclinebuf);
    } else {
	newReport->uclinebuf = newReport->uctokenptr = NULL;
    }

    if (report->ucmessage != NULL) {
        capacity = js_strlen(report->ucmessage) + 1;
        newReport->ucmessage = (const jschar *)JS_malloc(cx, capacity * sizeof(jschar));
        if (!newReport->ucmessage)
            goto error;
        js_strncpy((jschar *)newReport->ucmessage, report->ucmessage, capacity);

        if (report->messageArgs) {
            for (i = 0; report->messageArgs[i] != NULL; i++)
                ;
            JS_ASSERT(i);
            newReport->messageArgs =
                (const jschar **)JS_malloc(cx, (i + 1) * sizeof(jschar *));
            if (!newReport->messageArgs)
                goto error;
            for (i = 0; report->messageArgs[i] != NULL; i++) {
                capacity = js_strlen(report->messageArgs[i]) + 1;
                newReport->messageArgs[i] =
                    (const jschar *)JS_malloc(cx, capacity * sizeof(jschar));
                if (!newReport->messageArgs[i])
                    goto error;
                js_strncpy((jschar *)(newReport->messageArgs[i]),
                           report->messageArgs[i], capacity);
            }
            newReport->messageArgs[i] = NULL;
        } else {
            newReport->messageArgs = NULL;
        }
    } else {
        newReport->ucmessage = NULL;
        newReport->messageArgs = NULL;
    }
    newReport->errorNumber = report->errorNumber;

    /* Note that this is before it gets flagged with JSREPORT_EXCEPTION */
    newReport->flags = report->flags;

    return newPrivate;
error:
    exn_destroyPrivate(cx, newPrivate);
    return NULL;
}

static void
exn_finalize(JSContext *cx, JSObject *obj)
{
    JSExnPrivate *privateData;
    jsval privateValue;

    privateValue = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);

    if (privateValue != JSVAL_NULL) {
        privateData = (JSExnPrivate*) JSVAL_TO_PRIVATE(privateValue);
        if (privateData)
            exn_destroyPrivate(cx, privateData);
    }
}

JSErrorReport *
js_ErrorFromException(JSContext *cx, jsval exn)
{
    JSObject *obj;
    JSExnPrivate *privateData;
    jsval privateValue;

    if (JSVAL_IS_PRIMITIVE(exn))
        return NULL;
    obj = JSVAL_TO_OBJECT(exn);
    if (OBJ_GET_CLASS(cx, obj) != &ExceptionClass)
        return NULL;
    privateValue = OBJ_GET_SLOT(cx, obj, JSSLOT_PRIVATE);
    if (privateValue == JSVAL_NULL)
        return NULL;
    privateData = (JSExnPrivate*) JSVAL_TO_PRIVATE(privateValue);
    if (!privateData)
        return NULL;

    JS_ASSERT(privateData->errorReport);
    return privateData->errorReport;
}

/*
 * This must be kept in synch with the exceptions array below.
 * XXX use a jsexn.tbl file a la jsopcode.tbl
 */
typedef enum JSExnType {
    JSEXN_NONE = -1,
      JSEXN_ERR,
	JSEXN_INTERNALERR,
	JSEXN_EVALERR,
	JSEXN_RANGEERR,
	JSEXN_REFERENCEERR,
	JSEXN_SYNTAXERR,
	JSEXN_TYPEERR,
	JSEXN_URIERR,
	JSEXN_LIMIT
} JSExnType;

struct JSExnSpec {
    int protoIndex;
    const char *name;
    JSNative native;
};

/*
 * All *Error constructors share the same JSClass, ExceptionClass.  But each
 * constructor function for an *Error class must have a distinct native 'call'
 * function pointer, in order for instanceof to work properly across multiple
 * standard class sets.  See jsfun.c:fun_hasInstance.
 */
#define MAKE_EXCEPTION_CTOR(name)                                             \
const char js_##name##_str[] = #name;                                         \
static JSBool                                                                 \
name(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)      \
{                                                                             \
    return Exception(cx, obj, argc, argv, rval);                              \
}

MAKE_EXCEPTION_CTOR(Error)
MAKE_EXCEPTION_CTOR(InternalError)
MAKE_EXCEPTION_CTOR(EvalError)
MAKE_EXCEPTION_CTOR(RangeError)
MAKE_EXCEPTION_CTOR(ReferenceError)
MAKE_EXCEPTION_CTOR(SyntaxError)
MAKE_EXCEPTION_CTOR(TypeError)
MAKE_EXCEPTION_CTOR(URIError)

#undef MAKE_EXCEPTION_CTOR

static struct JSExnSpec exceptions[] = {
    { JSEXN_NONE,       js_Error_str,           Error },
    { JSEXN_ERR,        js_InternalError_str,   InternalError },
    { JSEXN_ERR,        js_EvalError_str,       EvalError },
    { JSEXN_ERR,        js_RangeError_str,      RangeError },
    { JSEXN_ERR,        js_ReferenceError_str,  ReferenceError },
    { JSEXN_ERR,        js_SyntaxError_str,     SyntaxError },
    { JSEXN_ERR,        js_TypeError_str,       TypeError },
    { JSEXN_ERR,        js_URIError_str,        URIError },
    {0,0,NULL}
};

static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok;
    jsval pval;
    int32 lineno;
    JSString *message, *filename;

    if (!cx->fp->constructing) {
        /*
         * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
         * called as functions, without operator new.  But as we do not give
         * each constructor a distinct JSClass, whose .name member is used by
         * js_NewObject to find the class prototype, we must get the class
         * prototype ourselves.
         */
        if (!OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(argv[-2]),
                              (jsid)cx->runtime->atomState.classPrototypeAtom,
                              &pval)) {
            return JS_FALSE;
        }
        obj = js_NewObject(cx, &ExceptionClass, JSVAL_TO_OBJECT(pval), NULL);
        if (!obj)
            return JS_FALSE;
        *rval = OBJECT_TO_JSVAL(obj);
    }

    /*
     * If it's a new object of class Exception, then null out the private
     * data so that the finalizer doesn't attempt to free it.
     */
    if (OBJ_GET_CLASS(cx, obj) == &ExceptionClass)
        OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, JSVAL_NULL);

    /* Set the 'message' property. */
    if (argc != 0) {
        message = js_ValueToString(cx, argv[0]);
        if (!message)
            return JS_FALSE;
    } else {
        message = cx->runtime->emptyString;
    }
    ok = JS_DefineProperty(cx, obj, js_message_str, STRING_TO_JSVAL(message),
                           NULL, NULL, JSPROP_ENUMERATE);
    if (!ok)
        return JS_FALSE;

    /* Set the 'fileName' property. */
    if (argc > 1) {
        filename = js_ValueToString(cx, argv[1]);
        if (!filename)
            return JS_FALSE;
    } else {
        filename = cx->runtime->emptyString;
    }
    ok = JS_DefineProperty(cx, obj, js_filename_str,
                           STRING_TO_JSVAL(filename),
                           NULL, NULL, JSPROP_ENUMERATE);
    if (!ok)
        return JS_FALSE;

    /* Set the 'lineNumber' property. */
    if (argc > 2) {
        ok = js_ValueToInt32(cx, argv[2], &lineno);
        if (!ok)
            return JS_FALSE;
    } else {
        lineno = 0;
    }
    return JS_DefineProperty(cx, obj, js_lineno_str,
                             INT_TO_JSVAL(lineno),
                             NULL, NULL, JSPROP_ENUMERATE);

}

/*
 * Convert to string.
 *
 * This method only uses JavaScript-modifiable properties name, message.  It
 * is left to the host to check for private data and report filename and line
 * number information along with this message.
 */
static JSBool
exn_toString(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    jsval v;
    JSString *name, *message, *result;
    jschar *chars, *cp;
    size_t length;

    if (!OBJ_GET_PROPERTY(cx, obj, (jsid)cx->runtime->atomState.nameAtom, &v))
        return JS_FALSE;
    name = js_ValueToString(cx, v);
    if (!name)
        return JS_FALSE;

    if (!JS_GetProperty(cx, obj, js_message_str, &v) ||
        !(message = js_ValueToString(cx, v))) {
        return JS_FALSE;
    }

    if (message->length != 0) {
        length = name->length + message->length + 2;
        cp = chars = (jschar*) JS_malloc(cx, (length + 1) * sizeof(jschar));
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
    jsval v;
    int32 lineno;
    JSString *name, *message, *filename, *lineno_as_str, *result;
    jschar *chars, *cp;
    size_t length;

    if (!OBJ_GET_PROPERTY(cx, obj, (jsid)cx->runtime->atomState.nameAtom, &v))
        return JS_FALSE;
    name = js_ValueToString(cx, v);
    if (!name)
        return JS_FALSE;

    if (!JS_GetProperty(cx, obj, js_message_str, &v) ||
        !(message = js_ValueToSource(cx, v))) {
        return JS_FALSE;
    }

    if (!JS_GetProperty(cx, obj, js_filename_str, &v) ||
        !(filename = js_ValueToSource(cx, v))) {
        return JS_FALSE;
    }

    if (!JS_GetProperty(cx, obj, js_lineno_str, &v) ||
        !js_ValueToInt32 (cx, v, &lineno)) {
        return JS_FALSE;
    }

    if (lineno != 0) {
        if (!(lineno_as_str = js_ValueToString(cx, v))) {
            return JS_FALSE;
        }
    } else {
        lineno_as_str = NULL;
    }

    /* Magic 8, for the characters in ``(new ())''. */
    length = 8 + name->length + message->length;

    if (filename->length != 0) {
        /* append filename as ``, {filename}'' */
        length += 2 + filename->length;
        if (lineno_as_str) {
            /* append lineno as ``, {lineno_as_str}'' */
            length += 2 + lineno_as_str->length;
        }
    } else {
        if (lineno_as_str) {
            /*
             * no filename, but have line number,
             * need to append ``, "", {lineno_as_str}''
             */
            length += 6 + lineno_as_str->length;
        }
    }

    cp = chars = (jschar*) JS_malloc(cx, (length + 1) * sizeof(jschar));
    if (!chars)
        return JS_FALSE;

    *cp++ = '('; *cp++ = 'n'; *cp++ = 'e'; *cp++ = 'w'; *cp++ = ' ';
    js_strncpy(cp, name->chars, name->length);
    cp += name->length;
    *cp++ = '(';
    if (message->length != 0) {
        js_strncpy(cp, message->chars, message->length);
        cp += message->length;
    }

    if (filename->length != 0) {
        /* append filename as ``, {filename}'' */
        *cp++ = ','; *cp++ = ' ';
        js_strncpy(cp, filename->chars, filename->length);
        cp += filename->length;
        if (lineno_as_str) {
            /* append lineno as ``, {lineno_as_str}'' */
            *cp++ = ','; *cp++ = ' ';
            js_strncpy(cp, lineno_as_str->chars, lineno_as_str->length);
            cp += lineno_as_str->length;
        }
    } else {
        if (lineno_as_str) {
            /*
             * no filename, but have line number,
             * need to append ``, "", {lineno_as_str}''
             */
            *cp++ = ','; *cp++ = ' '; *cp++ = '"'; *cp++ = '"';
            *cp++ = ','; *cp++ = ' ';
            js_strncpy(cp, lineno_as_str->chars, lineno_as_str->length);
            cp += lineno_as_str->length;
        }
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
    {js_toSource_str,   exn_toSource,           0,0,0},
#endif
    {js_toString_str,   exn_toString,           0,0,0},
    {0,0,0,0,0}
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
        int protoIndex = exceptions[i].protoIndex;

        /* Make the prototype for the current constructor name. */
        protos[i] = js_NewObject(cx, &ExceptionClass,
                                 (protoIndex != JSEXN_NONE)
                                 ? protos[protoIndex]
                                 : NULL,
                                 obj);
        if (!protos[i])
            return NULL;

        /* So exn_finalize knows whether to destroy private data. */
        OBJ_SET_SLOT(cx, protos[i], JSSLOT_PRIVATE, JSVAL_NULL);

        atom = js_Atomize(cx, exceptions[i].name, strlen(exceptions[i].name), 0);
        if (!atom)
            return NULL;

        /* Make a constructor function for the current name. */
        fun = js_DefineFunction(cx, obj, atom, exceptions[i].native, 1, 0);
        if (!fun)
            return NULL;

        /* Make this constructor make objects of class Exception. */
        fun->clasp = &ExceptionClass;

        /* Make the prototype and constructor links. */
        if (!js_SetClassPrototype(cx, fun->object, protos[i],
                                  JSPROP_READONLY | JSPROP_PERMANENT)) {
            return NULL;
        }

        /* proto bootstrap bit from JS_InitClass omitted. */
        nameString = JS_NewStringCopyZ(cx, exceptions[i].name);
        if (!nameString)
            return NULL;

        /* Add the name property to the prototype. */
        if (!JS_DefineProperty(cx, protos[i], js_name_str,
                               STRING_TO_JSVAL(nameString),
                               NULL, NULL,
                               JSPROP_ENUMERATE)) {
            return NULL;
        }
    }

    /*
     * Add an empty message property.  (To Exception.prototype only,
     * because this property will be the same for all the exception
     * protos.)
     */
    if (!JS_DefineProperty(cx, protos[0], js_message_str,
                           STRING_TO_JSVAL(cx->runtime->emptyString),
                           NULL, NULL, JSPROP_ENUMERATE)) {
        return NULL;
    }
    if (!JS_DefineProperty(cx, protos[0], js_filename_str,
                           STRING_TO_JSVAL(cx->runtime->emptyString),
                           NULL, NULL, JSPROP_ENUMERATE)) {
        return NULL;
    }
    if (!JS_DefineProperty(cx, protos[0], js_lineno_str,
                           INT_TO_JSVAL(0),
                           NULL, NULL, JSPROP_ENUMERATE)) {
        return NULL;
    }

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
    JSString *msgstr, *fnamestr;

    /* Find the exception index associated with this error. */
    JS_ASSERT(reportp);
    if (JSREPORT_IS_WARNING(reportp->flags))
        return JS_FALSE;
    errorNumber = (JSErrNum) reportp->errorNumber;
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
     * exception constructor name in the scope chain of the current context's
     * top stack frame, or in the global object if no frame is active.
     *
     * XXXbe hack around JSCLASS_NEW_RESOLVE code in js_LookupProperty that
     *       checks cx->fp, cx->fp->pc, and js_CodeSpec[*cx->fp->pc] in order
     *       to compute resolve flags such as JSRESOLVE_ASSIGNING.  The bug
     *       is that this "internal" js_GetClassPrototype call may trigger a
     *       resolve of exceptions[exn].name if the global object uses a lazy
     *       standard class resolver (see JS_ResolveStandardClass), but the
     *       current frame and bytecode end up affecting the resolve flags.
     */
    {
        JSStackFrame *fp = cx->fp;
        jsbytecode *pc = NULL;
        JSBool ok;

        if (fp) {
            pc = fp->pc;
            fp->pc = NULL;
        }
        ok = js_GetClassPrototype(cx, exceptions[exn].name, &errProto);
        if (pc)
            fp->pc = pc;
        if (!ok)
            return JS_FALSE;
    }

    /*
     * Use js_NewObject instead of js_ConstructObject, because
     * js_ConstructObject seems to require a frame.
     */
    errObject = js_NewObject(cx, &ExceptionClass, errProto, NULL);
    if (!errObject)
        return JS_FALSE;

    /* Store 'message' as a javascript-visible value. */
    msgstr = JS_NewStringCopyZ(cx, message);
    if (!msgstr)
        return JS_FALSE;
    if (!JS_DefineProperty(cx, errObject, js_message_str,
                           STRING_TO_JSVAL(msgstr), NULL, NULL,
                           JSPROP_ENUMERATE)) {
        return JS_FALSE;
    }

    if (reportp) {
        if (reportp->filename) {
            fnamestr = JS_NewStringCopyZ(cx, reportp->filename);
            if (!fnamestr)
                return JS_FALSE;
            /* Store 'filename' as a javascript-visible value. */
            if (!JS_DefineProperty(cx, errObject, js_filename_str,
                                   STRING_TO_JSVAL(fnamestr), NULL, NULL,
                                   JSPROP_ENUMERATE)) {
                return JS_FALSE;
            }
            /* Store 'lineno' as a javascript-visible value. */
            if (!JS_DefineProperty(cx, errObject, js_lineno_str,
                                   INT_TO_JSVAL((int)reportp->lineno), NULL,
                                   NULL, JSPROP_ENUMERATE)) {
                return JS_FALSE;
            }
        }

    }

    /*
     * Construct a new copy of the error report, and store it in the
     * exception objects' private data.  We can't use the error report
     * handed in, because it's stack-allocated, and may point to transient
     * data in the JSTokenStream.
     */
    privateData = exn_newPrivate(cx, reportp);
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
    const char *bytes;

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

    if (str != NULL) {
	bytes = js_GetStringBytes(str);
    }
    else {
	bytes = "null";
    }

    if (reportp == NULL) {
        /*
         * XXXmccabe todo: Instead of doing this, synthesize an error report
         * struct that includes the filename, lineno where the exception was
         * originally thrown.
         */
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNCAUGHT_EXCEPTION, bytes);
    } else {
        /* Flag the error as an exception. */
        reportp->flags |= JSREPORT_EXCEPTION;
        js_ReportErrorAgain(cx, bytes, reportp);
    }

    if (exnObject != NULL)
        js_RemoveRoot(cx->runtime, &exnObject);
    return JS_TRUE;
}

#endif	    /* JS_HAS_EXCEPTIONS */
