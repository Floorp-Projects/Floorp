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
 * JS standard exception implementation.
 */

#include "jsstddef.h"
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsbit.h"
#include "jsutil.h" /* Added by JSIFY */
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsconfig.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsinterp.h"
#include "jsopcode.h"
#include "jsnum.h"
#include "jsscript.h"

#if JS_HAS_ERROR_EXCEPTIONS
#if !JS_HAS_EXCEPTIONS
# error "JS_HAS_EXCEPTIONS must be defined to use JS_HAS_ERROR_EXCEPTIONS"
#endif

/* XXX consider adding rt->atomState.messageAtom */
static char js_message_str[]  = "message";
static char js_filename_str[] = "fileName";
static char js_lineno_str[]   = "lineNumber";
static char js_stack_str[]    = "stack";

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
        newReport->ucmessage = (const jschar *)
            JS_malloc(cx, capacity * sizeof(jschar));
        if (!newReport->ucmessage)
            goto error;
        js_strncpy((jschar *)newReport->ucmessage, report->ucmessage, capacity);

        if (report->messageArgs) {
            for (i = 0; report->messageArgs[i] != NULL; i++)
                continue;
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

    if (!JSVAL_IS_VOID(privateValue)) {
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
    if (JSVAL_IS_VOID(privateValue))
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
    {0,NULL,NULL}
};

static JSBool
InitExceptionObject(JSContext *cx, JSObject *obj, JSString *message,
                    JSString *filename, uintN lineno)
{
    JSCheckAccessOp checkAccess;
    JSErrorReporter older;
    JSExceptionState *state;
    jschar *stackbuf;
    size_t stacklen, stackmax;
    JSStackFrame *fp;
    jsval callerid, v;
    JSBool ok;
    JSString *argsrc, *stack;
    uintN i, ulineno;
    const char *cp;
    char ulnbuf[11];

    if (!JS_DefineProperty(cx, obj, js_message_str, STRING_TO_JSVAL(message),
                           NULL, NULL, JSPROP_ENUMERATE)) {
        return JS_FALSE;
    }

    if (!JS_DefineProperty(cx, obj, js_filename_str,
                           STRING_TO_JSVAL(filename),
                           NULL, NULL, JSPROP_ENUMERATE)) {
        return JS_FALSE;
    }

    if (!JS_DefineProperty(cx, obj, js_lineno_str,
                           INT_TO_JSVAL(lineno),
                           NULL, NULL, JSPROP_ENUMERATE)) {
        return JS_FALSE;
    }

    /*
     * Set the 'stack' property.
     *
     * First, set aside any error reporter for cx and save its exception state
     * so we can suppress any checkAccess failures.  Such failures should stop
     * the backtrace procedure, not result in a failure of this constructor.
     */
    checkAccess = cx->runtime->checkObjectAccess;
    if (checkAccess) {
        older = JS_SetErrorReporter(cx, NULL);
        state = JS_SaveExceptionState(cx);
    }
#ifdef __GNUC__         /* suppress bogus gcc warnings */
    else {
        older = NULL;
        state = NULL;
    }
#endif
    callerid = ATOM_KEY(cx->runtime->atomState.callerAtom);

    /*
     * Prepare to allocate a jschar buffer at stackbuf, where stacklen indexes
     * the next free jschar slot, and with room for at most stackmax non-null
     * jschars.  If stackbuf is non-null, it always contains an extra slot for
     * the null terminator we'll store at the end, as a backstop.
     *
     * All early returns must goto done after this point, till the after-loop
     * cleanup code has run!
     */
    stackbuf = NULL;
    stacklen = stackmax = 0;
    ok = JS_TRUE;

#define APPEND_CHAR_TO_STACK(c)                                               \
    JS_BEGIN_MACRO                                                            \
        if (stacklen == stackmax) {                                           \
            void *ptr_;                                                       \
            stackmax = stackmax ? 2 * stackmax : 64;                          \
            ptr_ = JS_realloc(cx, stackbuf, (stackmax+1) * sizeof(jschar));   \
            if (!ptr_) {                                                      \
                ok = JS_FALSE;                                                \
                goto done;                                                    \
            }                                                                 \
            stackbuf = ptr_;                                                  \
        }                                                                     \
        stackbuf[stacklen++] = (c);                                           \
    JS_END_MACRO

#define APPEND_STRING_TO_STACK(str)                                           \
    JS_BEGIN_MACRO                                                            \
        JSString *str_ = str;                                                 \
        size_t length_ = JSSTRING_LENGTH(str_);                               \
        if (stacklen + length_ > stackmax) {                                  \
            void *ptr_;                                                       \
            stackmax = JS_BIT(JS_CeilingLog2(stacklen + length_));            \
            ptr_ = JS_realloc(cx, stackbuf, (stackmax+1) * sizeof(jschar));   \
            if (!ptr_) {                                                      \
                ok = JS_FALSE;                                                \
                goto done;                                                    \
            }                                                                 \
            stackbuf = ptr_;                                                  \
        }                                                                     \
        js_strncpy(stackbuf + stacklen, JSSTRING_CHARS(str_), length_);       \
        stacklen += length_;                                                  \
    JS_END_MACRO

    for (fp = cx->fp; fp; fp = fp->down) {
        if (checkAccess) {
            v = (fp->fun && fp->argv) ? fp->argv[-2] : JSVAL_NULL;
            if (!JSVAL_IS_PRIMITIVE(v)) {
                ok = checkAccess(cx, fp->fun->object, callerid, JSACC_READ, &v);
                if (!ok) {
                    ok = JS_TRUE;
                    break;
                }
            }
        }

        if (fp->fun) {
            if (fp->fun->atom)
                APPEND_STRING_TO_STACK(ATOM_TO_STRING(fp->fun->atom));

            APPEND_CHAR_TO_STACK('(');
            for (i = 0; i < fp->argc; i++) {
                /* Avoid toSource bloat and fallibility for object types. */
                v = fp->argv[i];
                if (JSVAL_IS_PRIMITIVE(v)) {
                    argsrc = js_ValueToSource(cx, v);
                } else if (JSVAL_IS_FUNCTION(cx, v)) {
                    /* XXX Avoid function decompilation bloat for now. */
                    argsrc = JS_GetFunctionId(JS_ValueToFunction(cx, v));
                    if (!argsrc)
                        argsrc = js_ValueToSource(cx, v);
                } else {
                    /* XXX Avoid toString on objects, it takes too long and
                           uses too much memory, for too many classes (see
                           Mozilla bug 166743). */
                    char buf[100];
                    JS_snprintf(buf, sizeof buf, "[object %s]",
                                OBJ_GET_CLASS(cx, JSVAL_TO_OBJECT(v))->name);
                    argsrc = JS_NewStringCopyZ(cx, buf);
                }
                if (!argsrc) {
                    ok = JS_FALSE;
                    goto done;
                }
                if (i > 0)
                    APPEND_CHAR_TO_STACK(',');
                APPEND_STRING_TO_STACK(argsrc);
            }
            APPEND_CHAR_TO_STACK(')');
        }

        APPEND_CHAR_TO_STACK('@');
        if (fp->script && fp->script->filename) {
            for (cp = fp->script->filename; *cp; cp++)
                APPEND_CHAR_TO_STACK(*cp);
        }
        APPEND_CHAR_TO_STACK(':');
        if (fp->script && fp->pc) {
            ulineno = js_PCToLineNumber(cx, fp->script, fp->pc);
            JS_snprintf(ulnbuf, sizeof ulnbuf, "%u", ulineno);
            for (cp = ulnbuf; *cp; cp++)
                APPEND_CHAR_TO_STACK(*cp);
        } else {
            APPEND_CHAR_TO_STACK('0');
        }
        APPEND_CHAR_TO_STACK('\n');
    }

#undef APPEND_CHAR_TO_STACK
#undef APPEND_STRING_TO_STACK

done:
    if (checkAccess) {
        if (ok)
            JS_RestoreExceptionState(cx, state);
        else
            JS_DropExceptionState(cx, state);
        JS_SetErrorReporter(cx, older);
    }
    if (!ok) {
        JS_free(cx, stackbuf);
        return JS_FALSE;
    }

    if (!stackbuf) {
        stack = cx->runtime->emptyString;
    } else {
        /* NB: if stackbuf was allocated, it has room for the terminator. */
        JS_ASSERT(stacklen <= stackmax);
        if (stacklen < stackmax) {
            /*
             * Realloc can fail when shrinking on some FreeBSD versions, so
             * don't use JS_realloc here; simply let the oversized allocation
             * be owned by the string in that rare case.
             */
            void *shrunk = realloc(stackbuf, (stacklen+1) * sizeof(jschar));
            if (shrunk)
                stackbuf = shrunk;
        }
        stackbuf[stacklen] = 0;
        stack = js_NewString(cx, stackbuf, stacklen, 0);
        if (!stack) {
            JS_free(cx, stackbuf);
            return JS_FALSE;
        }
    }
    return JS_DefineProperty(cx, obj, js_stack_str,
                             STRING_TO_JSVAL(stack),
                             NULL, NULL, JSPROP_ENUMERATE);
}

static JSBool
Exception(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
    JSBool ok;
    jsval pval;
    int32 lineno;
    JSString *message, *filename;

    if (cx->creatingException)
        return JS_FALSE;
    cx->creatingException = JS_TRUE;

    if (!(cx->fp->flags & JSFRAME_CONSTRUCTING)) {
        /*
         * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
         * called as functions, without operator new.  But as we do not give
         * each constructor a distinct JSClass, whose .name member is used by
         * js_NewObject to find the class prototype, we must get the class
         * prototype ourselves.
         */
        ok = OBJ_GET_PROPERTY(cx, JSVAL_TO_OBJECT(argv[-2]),
                              ATOM_TO_JSID(cx->runtime->atomState
                                           .classPrototypeAtom),
                              &pval);
        if (!ok)
            goto out;
        obj = js_NewObject(cx, &ExceptionClass, JSVAL_TO_OBJECT(pval), NULL);
        if (!obj) {
            ok = JS_FALSE;
            goto out;
        }
        *rval = OBJECT_TO_JSVAL(obj);
    }

    /*
     * If it's a new object of class Exception, then null out the private
     * data so that the finalizer doesn't attempt to free it.
     */
    if (OBJ_GET_CLASS(cx, obj) == &ExceptionClass)
        OBJ_SET_SLOT(cx, obj, JSSLOT_PRIVATE, JSVAL_VOID);

    /* Set the 'message' property. */
    if (argc != 0) {
        message = js_ValueToString(cx, argv[0]);
        if (!message) {
            ok = JS_FALSE;
            goto out;
        }
        argv[0] = STRING_TO_JSVAL(message);
    } else {
        message = cx->runtime->emptyString;
    }

    /* Set the 'fileName' property. */
    if (argc > 1) {
        filename = js_ValueToString(cx, argv[1]);
        if (!filename) {
            ok = JS_FALSE;
            goto out;
        }
        argv[1] = STRING_TO_JSVAL(filename);
    } else {
        filename = cx->runtime->emptyString;
    }

    /* Set the 'lineNumber' property. */
    if (argc > 2) {
        ok = js_ValueToInt32(cx, argv[2], &lineno);
        if (!ok)
            goto out;
    } else {
        lineno = 0;
    }

    ok = InitExceptionObject(cx, obj, message, filename, lineno);

out:
    cx->creatingException = JS_FALSE;
    return ok;
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
    size_t name_length, message_length, length;

    if (!OBJ_GET_PROPERTY(cx, obj,
                          ATOM_TO_JSID(cx->runtime->atomState.nameAtom),
                          &v)) {
        return JS_FALSE;
    }
    name = JSVAL_IS_STRING(v) ? JSVAL_TO_STRING(v) : cx->runtime->emptyString;

    if (!JS_GetProperty(cx, obj, js_message_str, &v))
        return JS_FALSE;
    message = JSVAL_IS_STRING(v) ? JSVAL_TO_STRING(v)
                                 : cx->runtime->emptyString;

    if (JSSTRING_LENGTH(message) != 0) {
        name_length = JSSTRING_LENGTH(name);
        message_length = JSSTRING_LENGTH(message);
        length = (name_length ? name_length + 2 : 0) + message_length;
        cp = chars = (jschar*) JS_malloc(cx, (length + 1) * sizeof(jschar));
        if (!chars)
            return JS_FALSE;

        if (name_length) {
            js_strncpy(cp, JSSTRING_CHARS(name), name_length);
            cp += name_length;
            *cp++ = ':'; *cp++ = ' ';
        }
        js_strncpy(cp, JSSTRING_CHARS(message), message_length);
        cp += message_length;
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
    JSString *name, *message, *filename, *lineno_as_str, *result;
    int32 lineno;
    size_t lineno_length, name_length, message_length, filename_length, length;
    jschar *chars, *cp;

    if (!OBJ_GET_PROPERTY(cx, obj,
                          ATOM_TO_JSID(cx->runtime->atomState.nameAtom),
                          &v)) {
        return JS_FALSE;
    }
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
        lineno_length = JSSTRING_LENGTH(lineno_as_str);
    } else {
        lineno_as_str = NULL;
        lineno_length = 0;
    }

    /* Magic 8, for the characters in ``(new ())''. */
    name_length = JSSTRING_LENGTH(name);
    message_length = JSSTRING_LENGTH(message);
    length = 8 + name_length + message_length;

    filename_length = JSSTRING_LENGTH(filename);
    if (filename_length != 0) {
        /* append filename as ``, {filename}'' */
        length += 2 + filename_length;
        if (lineno_as_str) {
            /* append lineno as ``, {lineno_as_str}'' */
            length += 2 + lineno_length;
        }
    } else {
        if (lineno_as_str) {
            /*
             * no filename, but have line number,
             * need to append ``, "", {lineno_as_str}''
             */
            length += 6 + lineno_length;
        }
    }

    cp = chars = (jschar*) JS_malloc(cx, (length + 1) * sizeof(jschar));
    if (!chars)
        return JS_FALSE;

    *cp++ = '('; *cp++ = 'n'; *cp++ = 'e'; *cp++ = 'w'; *cp++ = ' ';
    js_strncpy(cp, JSSTRING_CHARS(name), name_length);
    cp += name_length;
    *cp++ = '(';
    if (message_length != 0) {
        js_strncpy(cp, JSSTRING_CHARS(message), message_length);
        cp += message_length;
    }

    if (filename_length != 0) {
        /* append filename as ``, {filename}'' */
        *cp++ = ','; *cp++ = ' ';
        js_strncpy(cp, JSSTRING_CHARS(filename), filename_length);
        cp += filename_length;
    } else {
        if (lineno_as_str) {
            /*
             * no filename, but have line number,
             * need to append ``, "", {lineno_as_str}''
             */
            *cp++ = ','; *cp++ = ' '; *cp++ = '"'; *cp++ = '"';
        }
    }
    if (lineno_as_str) {
        /* append lineno as ``, {lineno_as_str}'' */
        *cp++ = ','; *cp++ = ' ';
        js_strncpy(cp, JSSTRING_CHARS(lineno_as_str), lineno_length);
        cp += lineno_length;
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
        OBJ_SET_SLOT(cx, protos[i], JSSLOT_PRIVATE, JSVAL_VOID);

        atom = js_Atomize(cx, exceptions[i].name, strlen(exceptions[i].name), 0);
        if (!atom)
            return NULL;

        /* Make a constructor function for the current name. */
        fun = js_DefineFunction(cx, obj, atom, exceptions[i].native, 3, 0);
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
    JSExnType exn;
    JSBool ok;
    JSObject *errProto, *errObject;
    JSString *messageStr, *filenameStr;
    uintN lineno;
    JSExnPrivate *privateData;

    /*
     * Tell our caller to report immediately if cx has no active frames, or if
     * this report is just a warning.
     */
    JS_ASSERT(reportp);
    if (!cx->fp || JSREPORT_IS_WARNING(reportp->flags))
        return JS_FALSE;

    /* Find the exception index associated with this error. */
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
     * Prevent runaway recursion, just as the Exception native constructor
     * must do, via cx->creatingException.  If an out-of-memory error occurs,
     * no exception object will be created, but we don't assume that OOM is
     * the only kind of error that subroutines of this function called below
     * might raise.
     */
    if (cx->creatingException)
        return JS_FALSE;
    cx->creatingException = JS_TRUE;

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

        if (fp) {
            pc = fp->pc;
            fp->pc = NULL;
        }
        ok = js_GetClassPrototype(cx, exceptions[exn].name, &errProto);
        if (pc)
            fp->pc = pc;
        if (!ok)
            goto out;
    }

    errObject = js_NewObject(cx, &ExceptionClass, errProto, NULL);
    if (!errObject) {
        ok = JS_FALSE;
        goto out;
    }

    /*
     * Set the generated Exception object early, so it won't be GC'd by a last
     * ditch attempt to collect garbage, or a GC that otherwise nests or races
     * under any of the following calls.  If one of the following calls fails,
     * it will overwrite this exception object with one of its own (except in
     * case of OOM errors, of course).
     */
    JS_SetPendingException(cx, OBJECT_TO_JSVAL(errObject));

    messageStr = JS_NewStringCopyZ(cx, message);
    if (!messageStr) {
        ok = JS_FALSE;
        goto out;
    }

    if (reportp) {
        filenameStr = JS_NewStringCopyZ(cx, reportp->filename);
        if (!filenameStr) {
            ok = JS_FALSE;
            goto out;
        }
        lineno = reportp->lineno;
    } else {
        filenameStr = cx->runtime->emptyString;
        lineno = 0;
    }
    ok = InitExceptionObject(cx, errObject, messageStr, filenameStr, lineno);
    if (!ok)
        goto out;

    /*
     * Construct a new copy of the error report struct, and store it in the
     * exception object's private data.  We can't use the error report struct
     * that was passed in, because it's stack-allocated, and also because it
     * may point to transient data in the JSTokenStream.
     */
    privateData = exn_newPrivate(cx, reportp);
    if (!privateData) {
        ok = JS_FALSE;
        goto out;
    }
    OBJ_SET_SLOT(cx, errObject, JSSLOT_PRIVATE, PRIVATE_TO_JSVAL(privateData));

    /* Flag the error report passed in to indicate an exception was raised. */
    reportp->flags |= JSREPORT_EXCEPTION;

out:
    cx->creatingException = JS_FALSE;
    return ok;
}
#endif /* JS_HAS_ERROR_EXCEPTIONS */

#if JS_HAS_EXCEPTIONS

JSBool
js_ReportUncaughtException(JSContext *cx)
{
    JSObject *exnObject;
    JSString *str;
    jsval exn;
    JSErrorReport *reportp;
    const char *bytes;

    if (!JS_IsExceptionPending(cx))
        return JS_TRUE;

    if (!JS_GetPendingException(cx, &exn))
        return JS_FALSE;

    /*
     * Because js_ValueToString below could error and an exception object
     * could become unrooted, we must root exnObject.
     */
    if (JSVAL_IS_PRIMITIVE(exn)) {
        exnObject = NULL;
    } else {
        exnObject = JSVAL_TO_OBJECT(exn);
        if (!js_AddRoot(cx, &exnObject, "exn.report.root"))
            return JS_FALSE;
    }

#if JS_HAS_ERROR_EXCEPTIONS
    reportp = js_ErrorFromException(cx, exn);
#else
    reportp = NULL;
#endif

    str = js_ValueToString(cx, exn);
    bytes = str ? js_GetStringBytes(str) : "null";

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
    JS_ClearPendingException(cx);
    return JS_TRUE;
}

#endif /* JS_HAS_EXCEPTIONS */
