/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
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
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsbit.h"
#include "jsutil.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstaticcheck.h"
#include "jswrapper.h"

#include "vm/GlobalObject.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

#include "vm/Stack-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

/* Forward declarations for ErrorClass's initializer. */
static JSBool
Exception(JSContext *cx, uintN argc, Value *vp);

static void
exn_trace(JSTracer *trc, JSObject *obj);

static void
exn_finalize(JSContext *cx, JSObject *obj);

static JSBool
exn_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
            JSObject **objp);

Class js::ErrorClass = {
    js_Error_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Error),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    (JSResolveOp)exn_resolve,
    ConvertStub,
    exn_finalize,
    NULL,                 /* reserved0   */
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* construct   */
    NULL,                 /* xdrObject   */
    NULL,                 /* hasInstance */
    exn_trace
};

typedef struct JSStackTraceElem {
    JSString            *funName;
    size_t              argc;
    const char          *filename;
    uintN               ulineno;
} JSStackTraceElem;

typedef struct JSExnPrivate {
    /* A copy of the JSErrorReport originally generated. */
    JSErrorReport       *errorReport;
    JSString            *message;
    JSString            *filename;
    uintN               lineno;
    size_t              stackDepth;
    intN                exnType;
    JSStackTraceElem    stackElems[1];
} JSExnPrivate;

static JSString *
StackTraceToString(JSContext *cx, JSExnPrivate *priv);

static JSErrorReport *
CopyErrorReport(JSContext *cx, JSErrorReport *report)
{
    /*
     * We use a single malloc block to make a deep copy of JSErrorReport with
     * the following layout:
     *   JSErrorReport
     *   array of copies of report->messageArgs
     *   jschar array with characters for all messageArgs
     *   jschar array with characters for ucmessage
     *   jschar array with characters for uclinebuf and uctokenptr
     *   char array with characters for linebuf and tokenptr
     *   char array with characters for filename
     * Such layout together with the properties enforced by the following
     * asserts does not need any extra alignment padding.
     */
    JS_STATIC_ASSERT(sizeof(JSErrorReport) % sizeof(const char *) == 0);
    JS_STATIC_ASSERT(sizeof(const char *) % sizeof(jschar) == 0);

    size_t filenameSize;
    size_t linebufSize;
    size_t uclinebufSize;
    size_t ucmessageSize;
    size_t i, argsArraySize, argsCopySize, argSize;
    size_t mallocSize;
    JSErrorReport *copy;
    uint8 *cursor;

#define JS_CHARS_SIZE(jschars) ((js_strlen(jschars) + 1) * sizeof(jschar))

    filenameSize = report->filename ? strlen(report->filename) + 1 : 0;
    linebufSize = report->linebuf ? strlen(report->linebuf) + 1 : 0;
    uclinebufSize = report->uclinebuf ? JS_CHARS_SIZE(report->uclinebuf) : 0;
    ucmessageSize = 0;
    argsArraySize = 0;
    argsCopySize = 0;
    if (report->ucmessage) {
        ucmessageSize = JS_CHARS_SIZE(report->ucmessage);
        if (report->messageArgs) {
            for (i = 0; report->messageArgs[i]; ++i)
                argsCopySize += JS_CHARS_SIZE(report->messageArgs[i]);

            /* Non-null messageArgs should have at least one non-null arg. */
            JS_ASSERT(i != 0);
            argsArraySize = (i + 1) * sizeof(const jschar *);
        }
    }

    /*
     * The mallocSize can not overflow since it represents the sum of the
     * sizes of already allocated objects.
     */
    mallocSize = sizeof(JSErrorReport) + argsArraySize + argsCopySize +
                 ucmessageSize + uclinebufSize + linebufSize + filenameSize;
    cursor = (uint8 *)cx->malloc_(mallocSize);
    if (!cursor)
        return NULL;

    copy = (JSErrorReport *)cursor;
    memset(cursor, 0, sizeof(JSErrorReport));
    cursor += sizeof(JSErrorReport);

    if (argsArraySize != 0) {
        copy->messageArgs = (const jschar **)cursor;
        cursor += argsArraySize;
        for (i = 0; report->messageArgs[i]; ++i) {
            copy->messageArgs[i] = (const jschar *)cursor;
            argSize = JS_CHARS_SIZE(report->messageArgs[i]);
            memcpy(cursor, report->messageArgs[i], argSize);
            cursor += argSize;
        }
        copy->messageArgs[i] = NULL;
        JS_ASSERT(cursor == (uint8 *)copy->messageArgs[0] + argsCopySize);
    }

    if (report->ucmessage) {
        copy->ucmessage = (const jschar *)cursor;
        memcpy(cursor, report->ucmessage, ucmessageSize);
        cursor += ucmessageSize;
    }

    if (report->uclinebuf) {
        copy->uclinebuf = (const jschar *)cursor;
        memcpy(cursor, report->uclinebuf, uclinebufSize);
        cursor += uclinebufSize;
        if (report->uctokenptr) {
            copy->uctokenptr = copy->uclinebuf + (report->uctokenptr -
                                                  report->uclinebuf);
        }
    }

    if (report->linebuf) {
        copy->linebuf = (const char *)cursor;
        memcpy(cursor, report->linebuf, linebufSize);
        cursor += linebufSize;
        if (report->tokenptr) {
            copy->tokenptr = copy->linebuf + (report->tokenptr -
                                              report->linebuf);
        }
    }

    if (report->filename) {
        copy->filename = (const char *)cursor;
        memcpy(cursor, report->filename, filenameSize);
    }
    JS_ASSERT(cursor + filenameSize == (uint8 *)copy + mallocSize);

    /* Copy non-pointer members. */
    copy->lineno = report->lineno;
    copy->errorNumber = report->errorNumber;

    /* Note that this is before it gets flagged with JSREPORT_EXCEPTION */
    copy->flags = report->flags;

#undef JS_CHARS_SIZE
    return copy;
}

static jsval *
GetStackTraceValueBuffer(JSExnPrivate *priv)
{
    /*
     * We use extra memory after JSExnPrivateInfo.stackElems to store jsvals
     * that helps to produce more informative stack traces. The following
     * assert allows us to assume that no gap after stackElems is necessary to
     * align the buffer properly.
     */
    JS_STATIC_ASSERT(sizeof(JSStackTraceElem) % sizeof(jsval) == 0);

    return (jsval *)(priv->stackElems + priv->stackDepth);
}

static JSBool
InitExnPrivate(JSContext *cx, JSObject *exnObject, JSString *message,
               JSString *filename, uintN lineno, JSErrorReport *report, intN exnType)
{
    JSSecurityCallbacks *callbacks;
    CheckAccessOp checkAccess;
    JSErrorReporter older;
    JSExceptionState *state;
    jsid callerid;
    size_t stackDepth, valueCount, size;
    JSBool overflow;
    JSExnPrivate *priv;
    JSStackTraceElem *elem;
    jsval *values;

    JS_ASSERT(exnObject->getClass() == &ErrorClass);

    /*
     * Prepare stack trace data.
     *
     * Set aside any error reporter for cx and save its exception state
     * so we can suppress any checkAccess failures.  Such failures should stop
     * the backtrace procedure, not result in a failure of this constructor.
     */
    callbacks = JS_GetSecurityCallbacks(cx);
    checkAccess = callbacks
                  ? Valueify(callbacks->checkObjectAccess)
                  : NULL;
    older = JS_SetErrorReporter(cx, NULL);
    state = JS_SaveExceptionState(cx);

    callerid = ATOM_TO_JSID(cx->runtime->atomState.callerAtom);
    stackDepth = 0;
    valueCount = 0;
    FrameRegsIter firstPass(cx);
    for (; !firstPass.done(); ++firstPass) {
        StackFrame *fp = firstPass.fp();
        if (fp->compartment() != cx->compartment)
            break;
        if (fp->isNonEvalFunctionFrame()) {
            Value v = NullValue();
            if (checkAccess &&
                !checkAccess(cx, &fp->callee(), callerid, JSACC_READ, &v)) {
                break;
            }
            valueCount += fp->numActualArgs();
        }
        ++stackDepth;
    }
    JS_RestoreExceptionState(cx, state);
    JS_SetErrorReporter(cx, older);

    size = offsetof(JSExnPrivate, stackElems);
    overflow = (stackDepth > ((size_t)-1 - size) / sizeof(JSStackTraceElem));
    size += stackDepth * sizeof(JSStackTraceElem);
    overflow |= (valueCount > ((size_t)-1 - size) / sizeof(jsval));
    size += valueCount * sizeof(jsval);
    if (overflow) {
        js_ReportAllocationOverflow(cx);
        return JS_FALSE;
    }
    priv = (JSExnPrivate *)cx->malloc_(size);
    if (!priv)
        return JS_FALSE;

    /*
     * We initialize errorReport with a copy of report after setting the
     * private slot, to prevent GC accessing a junk value we clear the field
     * here.
     */
    priv->errorReport = NULL;
    priv->message = message;
    priv->filename = filename;
    priv->lineno = lineno;
    priv->stackDepth = stackDepth;
    priv->exnType = exnType;

    values = GetStackTraceValueBuffer(priv);
    elem = priv->stackElems;

    for (FrameRegsIter iter(cx); iter != firstPass; ++iter) {
        StackFrame *fp = iter.fp();
        if (fp->compartment() != cx->compartment)
            break;
        if (!fp->isNonEvalFunctionFrame()) {
            elem->funName = NULL;
            elem->argc = 0;
        } else {
            elem->funName = fp->fun()->atom
                            ? fp->fun()->atom
                            : cx->runtime->emptyString;
            elem->argc = fp->numActualArgs();
            fp->forEachCanonicalActualArg(CopyTo(Valueify(values)));
            values += elem->argc;
        }
        elem->ulineno = 0;
        elem->filename = NULL;
        if (fp->isScriptFrame()) {
            elem->filename = fp->script()->filename;
            elem->ulineno = js_FramePCToLineNumber(cx, fp, iter.pc());
        }
        ++elem;
    }
    JS_ASSERT(priv->stackElems + stackDepth == elem);
    JS_ASSERT(GetStackTraceValueBuffer(priv) + valueCount == values);

    exnObject->setPrivate(priv);

    if (report) {
        /*
         * Construct a new copy of the error report struct. We can't use the
         * error report struct that was passed in, because it's allocated on
         * the stack, and also because it may point to transient data in the
         * TokenStream.
         */
        priv->errorReport = CopyErrorReport(cx, report);
        if (!priv->errorReport) {
            /* The finalizer realeases priv since it is in the private slot. */
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

static inline JSExnPrivate *
GetExnPrivate(JSObject *obj)
{
    JS_ASSERT(obj->isError());
    return (JSExnPrivate *) obj->getPrivate();
}

static void
exn_trace(JSTracer *trc, JSObject *obj)
{
    JSExnPrivate *priv;
    JSStackTraceElem *elem;
    size_t vcount, i;
    jsval *vp, v;

    priv = GetExnPrivate(obj);
    if (priv) {
        if (priv->message)
            MarkString(trc, priv->message, "exception message");
        if (priv->filename)
            MarkString(trc, priv->filename, "exception filename");

        elem = priv->stackElems;
        for (vcount = i = 0; i != priv->stackDepth; ++i, ++elem) {
            if (elem->funName)
                MarkString(trc, elem->funName, "stack trace function name");
            if (IS_GC_MARKING_TRACER(trc) && elem->filename)
                js_MarkScriptFilename(elem->filename);
            vcount += elem->argc;
        }
        vp = GetStackTraceValueBuffer(priv);
        for (i = 0; i != vcount; ++i, ++vp) {
            v = *vp;
            JS_CALL_VALUE_TRACER(trc, v, "stack trace argument");
        }
    }
}

static void
exn_finalize(JSContext *cx, JSObject *obj)
{
    JSExnPrivate *priv;

    priv = GetExnPrivate(obj);
    if (priv) {
        if (priv->errorReport)
            cx->free_(priv->errorReport);
        cx->free_(priv);
    }
}

static JSBool
exn_resolve(JSContext *cx, JSObject *obj, jsid id, uintN flags,
            JSObject **objp)
{
    JSExnPrivate *priv;
    JSString *str;
    JSAtom *atom;
    JSString *stack;
    const char *prop;
    jsval v;
    uintN attrs;

    *objp = NULL;
    priv = GetExnPrivate(obj);
    if (priv && JSID_IS_ATOM(id)) {
        str = JSID_TO_STRING(id);

        atom = cx->runtime->atomState.messageAtom;
        if (str == atom) {
            prop = js_message_str;

            /*
             * Per ES5 15.11.1.1, if Error is called with no argument or with
             * undefined as the argument, it returns an Error object with no
             * own message property.
             */
            if (!priv->message)
                return true;

            v = STRING_TO_JSVAL(priv->message);
            attrs = 0;
            goto define;
        }

        atom = cx->runtime->atomState.fileNameAtom;
        if (str == atom) {
            prop = js_fileName_str;
            v = STRING_TO_JSVAL(priv->filename);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->runtime->atomState.lineNumberAtom;
        if (str == atom) {
            prop = js_lineNumber_str;
            v = INT_TO_JSVAL(priv->lineno);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->runtime->atomState.stackAtom;
        if (str == atom) {
            stack = StackTraceToString(cx, priv);
            if (!stack)
                return false;

            /* Allow to GC all things that were used to build stack trace. */
            priv->stackDepth = 0;
            prop = js_stack_str;
            v = STRING_TO_JSVAL(stack);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }
    }
    return true;

  define:
    if (!JS_DefineProperty(cx, obj, prop, v, NULL, NULL, attrs))
        return false;
    *objp = obj;
    return true;
}

JSErrorReport *
js_ErrorFromException(JSContext *cx, jsval exn)
{
    JSObject *obj;
    JSExnPrivate *priv;

    if (JSVAL_IS_PRIMITIVE(exn))
        return NULL;
    obj = JSVAL_TO_OBJECT(exn);
    if (!obj->isError())
        return NULL;
    priv = GetExnPrivate(obj);
    if (!priv)
        return NULL;
    return priv->errorReport;
}

static JSString *
ValueToShortSource(JSContext *cx, const Value &v)
{
    JSString *str;

    /* Avoid toSource bloat and fallibility for object types. */
    if (!v.isObject())
        return js_ValueToSource(cx, v);

    JSObject *obj = &v.toObject();
    AutoCompartment ac(cx, obj);
    if (!ac.enter())
        return NULL;

    if (obj->isFunction()) {
        /*
         * XXX Avoid function decompilation bloat for now.
         */
        str = JS_GetFunctionId(obj->getFunctionPrivate());
        if (!str && !(str = js_ValueToSource(cx, v))) {
            /*
             * Continue to soldier on if the function couldn't be
             * converted into a string.
             */
            JS_ClearPendingException(cx);
            str = JS_NewStringCopyZ(cx, "[unknown function]");
        }
    } else {
        /*
         * XXX Avoid toString on objects, it takes too long and uses too much
         * memory, for too many classes (see Mozilla bug 166743).
         */
        char buf[100];
        JS_snprintf(buf, sizeof buf, "[object %s]", obj->getClass()->name);
        str = JS_NewStringCopyZ(cx, buf);
    }

    ac.leave();

    if (!str || !cx->compartment->wrap(cx, &str))
        return NULL;
    return str;
}

static JSString *
StackTraceToString(JSContext *cx, JSExnPrivate *priv)
{
    jschar *stackbuf;
    size_t stacklen, stackmax;
    JSStackTraceElem *elem, *endElem;
    jsval *values;
    size_t i;
    JSString *str;
    const char *cp;
    char ulnbuf[11];

    /* After this point, failing control flow must goto bad. */
    stackbuf = NULL;
    stacklen = stackmax = 0;

/* Limit the stackbuf length to a reasonable value to avoid overflow checks. */
#define STACK_LENGTH_LIMIT JS_BIT(20)

#define APPEND_CHAR_TO_STACK(c)                                               \
    JS_BEGIN_MACRO                                                            \
        if (stacklen == stackmax) {                                           \
            void *ptr_;                                                       \
            if (stackmax >= STACK_LENGTH_LIMIT)                               \
                goto done;                                                    \
            stackmax = stackmax ? 2 * stackmax : 64;                          \
            ptr_ = cx->realloc_(stackbuf, (stackmax+1) * sizeof(jschar));      \
            if (!ptr_)                                                        \
                goto bad;                                                     \
            stackbuf = (jschar *) ptr_;                                       \
        }                                                                     \
        stackbuf[stacklen++] = (c);                                           \
    JS_END_MACRO

#define APPEND_STRING_TO_STACK(str)                                           \
    JS_BEGIN_MACRO                                                            \
        JSString *str_ = str;                                                 \
        size_t length_ = str_->length();                                      \
        const jschar *chars_ = str_->getChars(cx);                            \
        if (!chars_)                                                          \
            goto bad;                                                         \
                                                                              \
        if (length_ > stackmax - stacklen) {                                  \
            void *ptr_;                                                       \
            if (stackmax >= STACK_LENGTH_LIMIT ||                             \
                length_ >= STACK_LENGTH_LIMIT - stacklen) {                   \
                goto done;                                                    \
            }                                                                 \
            stackmax = JS_BIT(JS_CeilingLog2(stacklen + length_));            \
            ptr_ = cx->realloc_(stackbuf, (stackmax+1) * sizeof(jschar));      \
            if (!ptr_)                                                        \
                goto bad;                                                     \
            stackbuf = (jschar *) ptr_;                                       \
        }                                                                     \
        js_strncpy(stackbuf + stacklen, chars_, length_);                     \
        stacklen += length_;                                                  \
    JS_END_MACRO

    values = GetStackTraceValueBuffer(priv);
    elem = priv->stackElems;
    for (endElem = elem + priv->stackDepth; elem != endElem; elem++) {
        if (elem->funName) {
            APPEND_STRING_TO_STACK(elem->funName);
            APPEND_CHAR_TO_STACK('(');
            for (i = 0; i != elem->argc; i++, values++) {
                if (i > 0)
                    APPEND_CHAR_TO_STACK(',');
                str = ValueToShortSource(cx, Valueify(*values));
                if (!str)
                    goto bad;
                APPEND_STRING_TO_STACK(str);
            }
            APPEND_CHAR_TO_STACK(')');
        }
        APPEND_CHAR_TO_STACK('@');
        if (elem->filename) {
            for (cp = elem->filename; *cp; cp++)
                APPEND_CHAR_TO_STACK(*cp);
        }
        APPEND_CHAR_TO_STACK(':');
        JS_snprintf(ulnbuf, sizeof ulnbuf, "%u", elem->ulineno);
        for (cp = ulnbuf; *cp; cp++)
            APPEND_CHAR_TO_STACK(*cp);
        APPEND_CHAR_TO_STACK('\n');
    }
#undef APPEND_CHAR_TO_STACK
#undef APPEND_STRING_TO_STACK
#undef STACK_LENGTH_LIMIT

  done:
    if (stacklen == 0) {
        JS_ASSERT(!stackbuf);
        return cx->runtime->emptyString;
    }
    if (stacklen < stackmax) {
        /*
         * Realloc can fail when shrinking on some FreeBSD versions, so
         * don't use JS_realloc here; simply let the oversized allocation
         * be owned by the string in that rare case.
         */
        void *shrunk = cx->realloc_(stackbuf, (stacklen+1) * sizeof(jschar));
        if (shrunk)
            stackbuf = (jschar *) shrunk;
    }

    stackbuf[stacklen] = 0;
    str = js_NewString(cx, stackbuf, stacklen);
    if (str)
        return str;

  bad:
    if (stackbuf)
        cx->free_(stackbuf);
    return NULL;
}

/* XXXbe Consolidate the ugly truth that we don't treat filename as UTF-8
         with these two functions. */
static JSString *
FilenameToString(JSContext *cx, const char *filename)
{
    return JS_NewStringCopyZ(cx, filename);
}

enum {
    JSSLOT_ERROR_EXNTYPE = 0
};

static JSBool
Exception(JSContext *cx, uintN argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /*
     * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
     * called as functions, without operator new.  But as we do not give
     * each constructor a distinct JSClass, whose .name member is used by
     * NewNativeClassInstance to find the class prototype, we must get the
     * class prototype ourselves.
     */
    Value protov;
    jsid protoAtom = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
    if (!args.callee().getProperty(cx, protoAtom, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROTOTYPE, "Error");
        return false;
    }

    JSObject *errProto = &protov.toObject();
    JSObject *obj = NewNativeClassInstance(cx, &ErrorClass, errProto, errProto->getParent());
    if (!obj)
        return false;

    /* Set the 'message' property. */
    JSString *message;
    if (args.argc() != 0 && !args[0].isUndefined()) {
        message = js_ValueToString(cx, args[0]);
        if (!message)
            return false;
        args[0].setString(message);
    } else {
        message = NULL;
    }

    /* Find the scripted caller. */
    FrameRegsIter iter(cx);
    while (!iter.done() && !iter.fp()->isScriptFrame())
        ++iter;

    /* Set the 'fileName' property. */
    JSString *filename;
    if (args.argc() > 1) {
        filename = js_ValueToString(cx, args[1]);
        if (!filename)
            return false;
        args[1].setString(filename);
    } else {
        if (!iter.done()) {
            filename = FilenameToString(cx, iter.fp()->script()->filename);
            if (!filename)
                return false;
        } else {
            filename = cx->runtime->emptyString;
        }
    }

    /* Set the 'lineNumber' property. */
    uint32_t lineno;
    if (args.argc() > 2) {
        if (!ValueToECMAUint32(cx, args[2], &lineno))
            return false;
    } else {
        lineno = iter.done() ? 0 : js_FramePCToLineNumber(cx, iter.fp(), iter.pc());
    }

    intN exnType = args.callee().getReservedSlot(JSSLOT_ERROR_EXNTYPE).toInt32();
    if (!InitExnPrivate(cx, obj, message, filename, lineno, NULL, exnType))
        return false;

    args.rval().setObject(*obj);
    return true;
}

/*
 * Convert to string.
 *
 * This method only uses JavaScript-modifiable properties name, message.  It
 * is left to the host to check for private data and report filename and line
 * number information along with this message.
 */
static JSBool
exn_toString(JSContext *cx, uintN argc, Value *vp)
{
    jsval v;
    JSString *name, *message, *result;
    jschar *chars, *cp;
    size_t name_length, message_length, length;

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return JS_FALSE;
    if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.nameAtom), Valueify(&v)))
        return JS_FALSE;
    name = JSVAL_IS_STRING(v) ? JSVAL_TO_STRING(v) : cx->runtime->emptyString;
    vp->setString(name);

    if (!JS_GetProperty(cx, obj, js_message_str, &v))
        return JS_FALSE;
    message = JSVAL_IS_STRING(v) ? JSVAL_TO_STRING(v)
                                 : cx->runtime->emptyString;

    if (message->length() != 0) {
        name_length = name->length();
        message_length = message->length();
        length = (name_length ? name_length + 2 : 0) + message_length;
        cp = chars = (jschar *) cx->malloc_((length + 1) * sizeof(jschar));
        if (!chars)
            return JS_FALSE;

        if (name_length) {
            const jschar *name_chars = name->getChars(cx);
            if (!name_chars)
                return JS_FALSE;
            js_strncpy(cp, name_chars, name_length);
            cp += name_length;
            *cp++ = ':'; *cp++ = ' ';
        }
        const jschar *message_chars = message->getChars(cx);
        if (!message_chars)
            return JS_FALSE;
        js_strncpy(cp, message_chars, message_length);
        cp += message_length;
        *cp = 0;

        result = js_NewString(cx, chars, length);
        if (!result) {
            cx->free_(chars);
            return JS_FALSE;
        }
    } else {
        result = name;
    }

    vp->setString(result);
    return JS_TRUE;
}

#if JS_HAS_TOSOURCE
/*
 * Return a string that may eval to something similar to the original object.
 */
static JSBool
exn_toSource(JSContext *cx, uintN argc, Value *vp)
{
    JSString *name, *message, *filename, *lineno_as_str, *result;
    jsval localroots[3] = {JSVAL_NULL, JSVAL_NULL, JSVAL_NULL};
    size_t lineno_length, name_length, message_length, filename_length, length;
    jschar *chars, *cp;

    JSObject *obj = ToObject(cx, &vp[1]);
    if (!obj)
        return false;
    if (!obj->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.nameAtom), vp))
        return false;
    name = js_ValueToString(cx, *vp);
    if (!name)
        return false;
    vp->setString(name);

    {
        AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(localroots), Valueify(localroots));

#ifdef __GNUC__
        message = filename = NULL;
#endif
        if (!JS_GetProperty(cx, obj, js_message_str, &localroots[0]) ||
            !(message = js_ValueToSource(cx, Valueify(localroots[0])))) {
            return false;
        }
        localroots[0] = STRING_TO_JSVAL(message);

        if (!JS_GetProperty(cx, obj, js_fileName_str, &localroots[1]) ||
            !(filename = js_ValueToSource(cx, Valueify(localroots[1])))) {
            return false;
        }
        localroots[1] = STRING_TO_JSVAL(filename);

        if (!JS_GetProperty(cx, obj, js_lineNumber_str, &localroots[2]))
            return false;
        uint32_t lineno;
        if (!ValueToECMAUint32(cx, Valueify(localroots[2]), &lineno))
            return false;

        if (lineno != 0) {
            lineno_as_str = js_ValueToString(cx, Valueify(localroots[2]));
            if (!lineno_as_str)
                return false;
            lineno_length = lineno_as_str->length();
        } else {
            lineno_as_str = NULL;
            lineno_length = 0;
        }

        /* Magic 8, for the characters in ``(new ())''. */
        name_length = name->length();
        message_length = message->length();
        length = 8 + name_length + message_length;

        filename_length = filename->length();
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

        cp = chars = (jschar *) cx->malloc_((length + 1) * sizeof(jschar));
        if (!chars)
            return false;

        *cp++ = '('; *cp++ = 'n'; *cp++ = 'e'; *cp++ = 'w'; *cp++ = ' ';
        const jschar *name_chars = name->getChars(cx);
        if (!name_chars)
            return false;
        js_strncpy(cp, name_chars, name_length);
        cp += name_length;
        *cp++ = '(';
        const jschar *message_chars = message->getChars(cx);
        if (!message_chars)
            return false;
        if (message_length != 0) {
            js_strncpy(cp, message_chars, message_length);
            cp += message_length;
        }

        if (filename_length != 0) {
            /* append filename as ``, {filename}'' */
            *cp++ = ','; *cp++ = ' ';
            const jschar *filename_chars = filename->getChars(cx);
            if (!filename_chars)
                return false;
            js_strncpy(cp, filename_chars, filename_length);
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
            const jschar *lineno_chars = lineno_as_str->getChars(cx);
            if (!lineno_chars)
                return false;
            js_strncpy(cp, lineno_chars, lineno_length);
            cp += lineno_length;
        }

        *cp++ = ')'; *cp++ = ')'; *cp = 0;

        result = js_NewString(cx, chars, length);
        if (!result) {
            cx->free_(chars);
            return false;
        }
        vp->setString(result);
        return true;
    }
}
#endif

static JSFunctionSpec exception_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   exn_toSource,           0,0),
#endif
    JS_FN(js_toString_str,   exn_toString,           0,0),
    JS_FS_END
};

/* JSProto_ ordering for exceptions shall match JSEXN_ constants. */
JS_STATIC_ASSERT(JSEXN_ERR == 0);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_INTERNALERR  == JSProto_InternalError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_EVALERR      == JSProto_EvalError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_RANGEERR     == JSProto_RangeError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_REFERENCEERR == JSProto_ReferenceError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_SYNTAXERR    == JSProto_SyntaxError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_TYPEERR      == JSProto_TypeError);
JS_STATIC_ASSERT(JSProto_Error + JSEXN_URIERR       == JSProto_URIError);

static JS_INLINE JSProtoKey
GetExceptionProtoKey(intN exn)
{
    JS_ASSERT(JSEXN_ERR <= exn);
    JS_ASSERT(exn < JSEXN_LIMIT);
    return JSProtoKey(JSProto_Error + exn);
}

static JSObject *
InitErrorClass(JSContext *cx, GlobalObject *global, intN type, JSObject &proto)
{
    JSProtoKey key = GetExceptionProtoKey(type);
    JSAtom *name = cx->runtime->atomState.classAtoms[key];
    JSObject *errorProto = global->createBlankPrototypeInheriting(cx, &ErrorClass, proto);
    if (!errorProto)
        return NULL;

    Value empty = StringValue(cx->runtime->emptyString);
    jsid nameId = ATOM_TO_JSID(cx->runtime->atomState.nameAtom);
    jsid messageId = ATOM_TO_JSID(cx->runtime->atomState.messageAtom);
    jsid fileNameId = ATOM_TO_JSID(cx->runtime->atomState.fileNameAtom);
    jsid lineNumberId = ATOM_TO_JSID(cx->runtime->atomState.lineNumberAtom);
    if (!DefineNativeProperty(cx, errorProto, nameId, StringValue(name),
                              PropertyStub, StrictPropertyStub, 0, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, messageId, empty,
                              PropertyStub, StrictPropertyStub, 0, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, fileNameId, empty,
                              PropertyStub, StrictPropertyStub, JSPROP_ENUMERATE, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, lineNumberId, Int32Value(0),
                              PropertyStub, StrictPropertyStub, JSPROP_ENUMERATE, 0, 0))
    {
        return NULL;
    }

    /* Create the corresponding constructor. */
    JSFunction *ctor = global->createConstructor(cx, Exception, &ErrorClass, name, 1);
    if (!ctor)
        return NULL;
    ctor->setReservedSlot(JSSLOT_ERROR_EXNTYPE, Int32Value(int32(type)));

    if (!LinkConstructorAndPrototype(cx, ctor, errorProto))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, key, ctor, errorProto))
        return NULL;

    JS_ASSERT(!errorProto->getPrivate());

    return errorProto;
}

JSObject *
js_InitExceptionClasses(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isGlobal());
    JS_ASSERT(obj->isNative());

    GlobalObject *global = obj->asGlobal();

    JSObject *objectProto;
    if (!js_GetClassPrototype(cx, global, JSProto_Object, &objectProto))
        return NULL;

    /* Initialize the base Error class first. */
    JSObject *errorProto = InitErrorClass(cx, global, JSEXN_ERR, *objectProto);
    if (!errorProto)
        return NULL;

    /* |Error.prototype| alone has method properties. */
    if (!DefinePropertiesAndBrand(cx, errorProto, NULL, exception_methods))
        return NULL;

    /* Define all remaining *Error constructors. */
    for (intN i = JSEXN_ERR + 1; i < JSEXN_LIMIT; i++) {
        if (!InitErrorClass(cx, global, i, *errorProto))
            return NULL;
    }

    return errorProto;
}

const JSErrorFormatString*
js_GetLocalizedErrorMessage(JSContext* cx, void *userRef, const char *locale,
                            const uintN errorNumber)
{
    const JSErrorFormatString *errorString = NULL;

    if (cx->localeCallbacks && cx->localeCallbacks->localeGetErrorMessage) {
        errorString = cx->localeCallbacks
                        ->localeGetErrorMessage(userRef, locale, errorNumber);
    }
    if (!errorString)
        errorString = js_GetErrorMessage(userRef, locale, errorNumber);
    return errorString;
}

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
js_ErrorToException(JSContext *cx, const char *message, JSErrorReport *reportp,
                    JSErrorCallback callback, void *userRef)
{
    JSErrNum errorNumber;
    const JSErrorFormatString *errorString;
    JSExnType exn;
    jsval tv[4];
    JSBool ok;
    JSObject *errProto, *errObject;
    JSString *messageStr, *filenameStr;

    /*
     * Tell our caller to report immediately if this report is just a warning.
     */
    JS_ASSERT(reportp);
    if (JSREPORT_IS_WARNING(reportp->flags))
        return JS_FALSE;

    /* Find the exception index associated with this error. */
    errorNumber = (JSErrNum) reportp->errorNumber;
    if (!callback || callback == js_GetErrorMessage)
        errorString = js_GetLocalizedErrorMessage(cx, NULL, NULL, errorNumber);
    else
        errorString = callback(userRef, NULL, errorNumber);
    exn = errorString ? (JSExnType) errorString->exnType : JSEXN_NONE;
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
     * Prevent runaway recursion, via cx->generatingError.  If an out-of-memory
     * error occurs, no exception object will be created, but we don't assume
     * that OOM is the only kind of error that subroutines of this function
     * called below might raise.
     */
    if (cx->generatingError)
        return JS_FALSE;

    MUST_FLOW_THROUGH("out");
    cx->generatingError = JS_TRUE;

    /* Protect the newly-created strings below from nesting GCs. */
    PodArrayZero(tv);
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(tv), Valueify(tv));

    /*
     * Try to get an appropriate prototype by looking up the corresponding
     * exception constructor name in the scope chain of the current context's
     * top stack frame, or in the global object if no frame is active.
     */
    ok = js_GetClassPrototype(cx, NULL, GetExceptionProtoKey(exn), &errProto);
    if (!ok)
        goto out;
    tv[0] = OBJECT_TO_JSVAL(errProto);

    errObject = NewNativeClassInstance(cx, &ErrorClass, errProto, errProto->getParent());
    if (!errObject) {
        ok = JS_FALSE;
        goto out;
    }
    tv[1] = OBJECT_TO_JSVAL(errObject);

    messageStr = JS_NewStringCopyZ(cx, message);
    if (!messageStr) {
        ok = JS_FALSE;
        goto out;
    }
    tv[2] = STRING_TO_JSVAL(messageStr);

    filenameStr = JS_NewStringCopyZ(cx, reportp->filename);
    if (!filenameStr) {
        ok = JS_FALSE;
        goto out;
    }
    tv[3] = STRING_TO_JSVAL(filenameStr);

    ok = InitExnPrivate(cx, errObject, messageStr, filenameStr,
                        reportp->lineno, reportp, exn);
    if (!ok)
        goto out;

    JS_SetPendingException(cx, OBJECT_TO_JSVAL(errObject));

    /* Flag the error report passed in to indicate an exception was raised. */
    reportp->flags |= JSREPORT_EXCEPTION;

out:
    cx->generatingError = JS_FALSE;
    return ok;
}

JSBool
js_ReportUncaughtException(JSContext *cx)
{
    jsval exn;
    JSObject *exnObject;
    jsval roots[5];
    JSErrorReport *reportp, report;
    JSString *str;
    const char *bytes;

    if (!JS_IsExceptionPending(cx))
        return true;

    if (!JS_GetPendingException(cx, &exn))
        return false;

    PodArrayZero(roots);
    AutoArrayRooter tvr(cx, JS_ARRAY_LENGTH(roots), Valueify(roots));

    /*
     * Because js_ValueToString below could error and an exception object
     * could become unrooted, we must root exnObject.  Later, if exnObject is
     * non-null, we need to root other intermediates, so allocate an operand
     * stack segment to protect all of these values.
     */
    if (JSVAL_IS_PRIMITIVE(exn)) {
        exnObject = NULL;
    } else {
        exnObject = JSVAL_TO_OBJECT(exn);
        roots[0] = exn;
    }

    JS_ClearPendingException(cx);
    reportp = js_ErrorFromException(cx, exn);

    /* XXX L10N angels cry once again (see also jsemit.c, /L10N gaffes/) */
    str = js_ValueToString(cx, Valueify(exn));
    JSAutoByteString bytesStorage;
    if (!str) {
        bytes = "unknown (can't convert to string)";
    } else {
        roots[1] = STRING_TO_JSVAL(str);
        if (!bytesStorage.encode(cx, str))
            return false;
        bytes = bytesStorage.ptr();
    }

    JSAutoByteString filename;
    if (!reportp && exnObject && exnObject->isError()) {
        if (!JS_GetProperty(cx, exnObject, js_message_str, &roots[2]))
            return false;
        if (JSVAL_IS_STRING(roots[2])) {
            bytesStorage.clear();
            if (!bytesStorage.encode(cx, str))
                return false;
            bytes = bytesStorage.ptr();
        }

        if (!JS_GetProperty(cx, exnObject, js_fileName_str, &roots[3]))
            return false;
        str = js_ValueToString(cx, Valueify(roots[3]));
        if (!str || !filename.encode(cx, str))
            return false;

        if (!JS_GetProperty(cx, exnObject, js_lineNumber_str, &roots[4]))
            return false;
        uint32_t lineno;
        if (!ValueToECMAUint32(cx, Valueify(roots[4]), &lineno))
            return false;

        reportp = &report;
        PodZero(&report);
        report.filename = filename.ptr();
        report.lineno = (uintN) lineno;
        if (JSVAL_IS_STRING(roots[2])) {
            JSFixedString *fixed = JSVAL_TO_STRING(roots[2])->ensureFixed(cx);
            if (!fixed)
                return false;
            report.ucmessage = fixed->chars();
        }
    }

    if (!reportp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_UNCAUGHT_EXCEPTION, bytes);
    } else {
        /* Flag the error as an exception. */
        reportp->flags |= JSREPORT_EXCEPTION;

        /* Pass the exception object. */
        JS_SetPendingException(cx, exn);
        js_ReportErrorAgain(cx, bytes, reportp);
        JS_ClearPendingException(cx);
    }

    return true;
}

extern JSObject *
js_CopyErrorObject(JSContext *cx, JSObject *errobj, JSObject *scope)
{
    assertSameCompartment(cx, scope);
    JSExnPrivate *priv = GetExnPrivate(errobj);

    uint32 stackDepth = priv->stackDepth;
    size_t valueCount = 0;
    for (uint32 i = 0; i < stackDepth; i++)
        valueCount += priv->stackElems[i].argc;

    size_t size = offsetof(JSExnPrivate, stackElems) +
                  stackDepth * sizeof(JSStackTraceElem) +
                  valueCount * sizeof(jsval);

    JSExnPrivate *copy = (JSExnPrivate *)cx->malloc_(size);
    if (!copy)
        return NULL;

    struct AutoFree {
        JSContext *cx;
        JSExnPrivate *p;
        ~AutoFree() {
            if (p) {
                cx->free_(p->errorReport);
                cx->free_(p);
            }
        }
    } autoFree = {cx, copy};

    // Copy each field. Don't bother copying the stack elements.
    if (priv->errorReport) {
        copy->errorReport = CopyErrorReport(cx, priv->errorReport);
        if (!copy->errorReport)
            return NULL;
    } else {
        copy->errorReport = NULL;
    }
    copy->message = priv->message;
    if (!cx->compartment->wrap(cx, &copy->message))
        return NULL;
    JS::Anchor<JSString *> messageAnchor(copy->message);
    copy->filename = priv->filename;
    if (!cx->compartment->wrap(cx, &copy->filename))
        return NULL;
    JS::Anchor<JSString *> filenameAnchor(copy->filename);
    copy->lineno = priv->lineno;
    copy->stackDepth = 0;
    copy->exnType = priv->exnType;

    // Create the Error object.
    JSObject *proto;
    if (!js_GetClassPrototype(cx, scope->getGlobal(), GetExceptionProtoKey(copy->exnType), &proto))
        return NULL;
    JSObject *copyobj = NewNativeClassInstance(cx, &ErrorClass, proto, proto->getParent());
    copyobj->setPrivate(copy);
    autoFree.p = NULL;
    return copyobj;
}
