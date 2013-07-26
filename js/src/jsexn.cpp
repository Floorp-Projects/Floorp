/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS standard exception implementation.
 */

#include "jsexn.h"

#include "mozilla/PodOperations.h"
#include "mozilla/Util.h"

#include <string.h>

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jstypes.h"
#include "jsutil.h"
#include "jswrapper.h"

#include "gc/Marking.h"
#include "vm/ErrorObject.h"
#include "vm/GlobalObject.h"
#include "vm/StringBuffer.h"

#include "jsfuninlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::ArrayLength;
using mozilla::PodArrayZero;
using mozilla::PodZero;

/* Forward declarations for ErrorObject::class_'s initializer. */
static JSBool
Exception(JSContext *cx, unsigned argc, Value *vp);

static void
exn_trace(JSTracer *trc, JSObject *obj);

static void
exn_finalize(FreeOp *fop, JSObject *obj);

static JSBool
exn_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp);

Class ErrorObject::class_ = {
    js_Error_str,
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS | JSCLASS_NEW_RESOLVE |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Error),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    (JSResolveOp)exn_resolve,
    JS_ConvertStub,
    exn_finalize,
    NULL,                 /* checkAccess */
    NULL,                 /* call        */
    NULL,                 /* hasInstance */
    NULL,                 /* construct   */
    exn_trace
};

template <typename T>
struct JSStackTraceElemImpl
{
    T                   funName;
    const char          *filename;
    unsigned            ulineno;
};

typedef JSStackTraceElemImpl<HeapPtrString> JSStackTraceElem;
typedef JSStackTraceElemImpl<JSString *>    JSStackTraceStackElem;

struct JSExnPrivate
{
    /* A copy of the JSErrorReport originally generated. */
    JSErrorReport       *errorReport;
    js::HeapPtrString   message;
    js::HeapPtrString   filename;
    unsigned            lineno;
    unsigned            column;
    size_t              stackDepth;
    int                 exnType;
    JSStackTraceElem    stackElems[1];
};

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
    uint8_t *cursor;

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
    cursor = cx->pod_malloc<uint8_t>(mallocSize);
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
            js_memcpy(cursor, report->messageArgs[i], argSize);
            cursor += argSize;
        }
        copy->messageArgs[i] = NULL;
        JS_ASSERT(cursor == (uint8_t *)copy->messageArgs[0] + argsCopySize);
    }

    if (report->ucmessage) {
        copy->ucmessage = (const jschar *)cursor;
        js_memcpy(cursor, report->ucmessage, ucmessageSize);
        cursor += ucmessageSize;
    }

    if (report->uclinebuf) {
        copy->uclinebuf = (const jschar *)cursor;
        js_memcpy(cursor, report->uclinebuf, uclinebufSize);
        cursor += uclinebufSize;
        if (report->uctokenptr) {
            copy->uctokenptr = copy->uclinebuf + (report->uctokenptr -
                                                  report->uclinebuf);
        }
    }

    if (report->linebuf) {
        copy->linebuf = (const char *)cursor;
        js_memcpy(cursor, report->linebuf, linebufSize);
        cursor += linebufSize;
        if (report->tokenptr) {
            copy->tokenptr = copy->linebuf + (report->tokenptr -
                                              report->linebuf);
        }
    }

    if (report->filename) {
        copy->filename = (const char *)cursor;
        js_memcpy(cursor, report->filename, filenameSize);
    }
    JS_ASSERT(cursor + filenameSize == (uint8_t *)copy + mallocSize);

    /* HOLD called by the destination error object. */
    copy->originPrincipals = report->originPrincipals;

    /* Copy non-pointer members. */
    copy->lineno = report->lineno;
    copy->column = report->column;
    copy->errorNumber = report->errorNumber;
    copy->exnType = report->exnType;

    /* Note that this is before it gets flagged with JSREPORT_EXCEPTION */
    copy->flags = report->flags;

#undef JS_CHARS_SIZE
    return copy;
}

struct SuppressErrorsGuard
{
    JSContext *cx;
    JSErrorReporter prevReporter;
    JSExceptionState *prevState;

    SuppressErrorsGuard(JSContext *cx)
      : cx(cx),
        prevReporter(JS_SetErrorReporter(cx, NULL)),
        prevState(JS_SaveExceptionState(cx))
    {}

    ~SuppressErrorsGuard()
    {
        JS_RestoreExceptionState(cx, prevState);
        JS_SetErrorReporter(cx, prevReporter);
    }
};

static void
SetExnPrivate(ErrorObject &exnObject, JSExnPrivate *priv);

static bool
InitExnPrivate(JSContext *cx, HandleObject exnObject, HandleString message,
               HandleString filename, unsigned lineno, unsigned column,
               JSErrorReport *report, int exnType)
{
    JS_ASSERT(exnObject->is<ErrorObject>());
    JS_ASSERT(!exnObject->getPrivate());

    JSCheckAccessOp checkAccess = cx->runtime()->securityCallbacks->checkObjectAccess;

    Vector<JSStackTraceStackElem> frames(cx);
    {
        SuppressErrorsGuard seg(cx);
        for (NonBuiltinScriptFrameIter i(cx); !i.done(); ++i) {

            /* Ask the crystal CAPS ball whether we can see across compartments. */
            if (checkAccess && i.isNonEvalFunctionFrame()) {
                RootedValue v(cx);
                RootedId callerid(cx, NameToId(cx->names().caller));
                RootedObject obj(cx, i.callee());
                if (!checkAccess(cx, obj, callerid, JSACC_READ, &v))
                    break;
            }

            if (!frames.growBy(1))
                return false;
            JSStackTraceStackElem &frame = frames.back();
            if (i.isNonEvalFunctionFrame()) {
                JSAtom *atom = i.callee()->displayAtom();
                if (atom == NULL)
                    atom = cx->runtime()->emptyString;
                frame.funName = atom;
            } else {
                frame.funName = NULL;
            }
            RootedScript script(cx, i.script());
            const char *cfilename = script->filename();
            if (!cfilename)
                cfilename = "";
            frame.filename = cfilename;
            frame.ulineno = PCToLineNumber(script, i.pc());
        }
    }

    /* Do not need overflow check: the vm stack is already bigger. */
    JS_STATIC_ASSERT(sizeof(JSStackTraceElem) <= sizeof(StackFrame));

    size_t nbytes = offsetof(JSExnPrivate, stackElems) +
                    frames.length() * sizeof(JSStackTraceElem);

    JSExnPrivate *priv = (JSExnPrivate *)cx->malloc_(nbytes);
    if (!priv)
        return false;

    /* Initialize to zero so that write barriers don't witness undefined values. */
    memset(priv, 0, nbytes);

    if (report) {
        /*
         * Construct a new copy of the error report struct. We can't use the
         * error report struct that was passed in, because it's allocated on
         * the stack, and also because it may point to transient data in the
         * TokenStream.
         */
        priv->errorReport = CopyErrorReport(cx, report);
        if (!priv->errorReport) {
            js_free(priv);
            return false;
        }
    } else {
        priv->errorReport = NULL;
    }

    priv->message.init(message);
    priv->filename.init(filename);
    priv->lineno = lineno;
    priv->column = column;
    priv->stackDepth = frames.length();
    priv->exnType = exnType;
    for (size_t i = 0; i < frames.length(); ++i) {
        priv->stackElems[i].funName.init(frames[i].funName);
        priv->stackElems[i].filename = JS_strdup(cx, frames[i].filename);
        if (!priv->stackElems[i].filename)
            return false;
        priv->stackElems[i].ulineno = frames[i].ulineno;
    }

    SetExnPrivate(exnObject->as<ErrorObject>(), priv);
    return true;
}

static void
exn_trace(JSTracer *trc, JSObject *obj)
{
    if (JSExnPrivate *priv = obj->as<ErrorObject>().getExnPrivate()) {
        if (priv->message)
            MarkString(trc, &priv->message, "exception message");
        if (priv->filename)
            MarkString(trc, &priv->filename, "exception filename");

        for (size_t i = 0; i != priv->stackDepth; ++i) {
            JSStackTraceElem &elem = priv->stackElems[i];
            if (elem.funName)
                MarkString(trc, &elem.funName, "stack trace function name");
        }
    }
}

/* NB: An error object's private must be set through this function. */
static void
SetExnPrivate(ErrorObject &exnObject, JSExnPrivate *priv)
{
    JS_ASSERT(!exnObject.getExnPrivate());
    if (JSErrorReport *report = priv->errorReport) {
        if (JSPrincipals *prin = report->originPrincipals)
            JS_HoldPrincipals(prin);
    }
    exnObject.setPrivate(priv);
}

static void
exn_finalize(FreeOp *fop, JSObject *obj)
{
    if (JSExnPrivate *priv = obj->as<ErrorObject>().getExnPrivate()) {
        if (JSErrorReport *report = priv->errorReport) {
            /* HOLD called by SetExnPrivate. */
            if (JSPrincipals *prin = report->originPrincipals)
                JS_DropPrincipals(fop->runtime(), prin);
            fop->free_(report);
        }
        for (size_t i = 0; i < priv->stackDepth; i++)
            js_free(const_cast<char *>(priv->stackElems[i].filename));
        fop->free_(priv);
    }
}

static JSBool
exn_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    JSExnPrivate *priv;
    const char *prop;
    jsval v;
    unsigned attrs;

    objp.set(NULL);
    priv = obj->as<ErrorObject>().getExnPrivate();
    if (priv && JSID_IS_ATOM(id)) {
        RootedString str(cx, JSID_TO_STRING(id));

        RootedAtom atom(cx, cx->names().message);
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

        atom = cx->names().fileName;
        if (str == atom) {
            prop = js_fileName_str;
            v = STRING_TO_JSVAL(priv->filename);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->names().lineNumber;
        if (str == atom) {
            prop = js_lineNumber_str;
            v = UINT_TO_JSVAL(priv->lineno);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->names().columnNumber;
        if (str == atom) {
            prop = js_columnNumber_str;
            v = UINT_TO_JSVAL(priv->column);
            attrs = JSPROP_ENUMERATE;
            goto define;
        }

        atom = cx->names().stack;
        if (str == atom) {
            JSString *stack = StackTraceToString(cx, priv);
            if (!stack)
                return false;

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
    objp.set(obj);
    return true;
}

JSErrorReport *
js_ErrorFromException(jsval exn)
{
    if (JSVAL_IS_PRIMITIVE(exn))
        return NULL;

    // It's ok to UncheckedUnwrap here, since all we do is get the
    // JSErrorReport, and consumers are careful with the information they get
    // from that anyway.  Anyone doing things that would expose anything in the
    // JSErrorReport to page script either does a security check on the
    // JSErrorReport's principal or also tries to do toString on our object and
    // will fail if they can't unwrap it.
    JSObject *obj = UncheckedUnwrap(JSVAL_TO_OBJECT(exn));
    if (!obj->is<ErrorObject>())
        return NULL;

    JSExnPrivate *priv = obj->as<ErrorObject>().getExnPrivate();
    if (!priv)
        return NULL;

    return priv->errorReport;
}

static JSString *
StackTraceToString(JSContext *cx, JSExnPrivate *priv)
{
    StringBuffer sb(cx);

    JSStackTraceElem *element = priv->stackElems, *end = element + priv->stackDepth;
    for (; element < end; element++) {
        /* Try to reserve required space upfront, so we don't fail inbetween. */
        size_t length = ((element->funName ? element->funName->length() : 0) +
                         (element->filename ? strlen(element->filename) * 2 : 0) +
                         13); /* "@" + ":" + "4294967295" + "\n" */

        if (!sb.reserve(length) || sb.length() > JS_BIT(20))
            break; /* Return as much as we got. */

        if (element->funName) {
            if (!sb.append(element->funName))
                return NULL;
        }
        if (!sb.append('@'))
            return NULL;
        if (element->filename) {
            if (!sb.appendInflated(element->filename, strlen(element->filename)))
                return NULL;
        }
        if (!sb.append(':') || !NumberValueToStringBuffer(cx, NumberValue(element->ulineno), sb) || 
            !sb.append('\n'))
        {
            return NULL;
        }
    }

    return sb.finishString();
}

/* XXXbe Consolidate the ugly truth that we don't treat filename as UTF-8
         with these two functions. */
static JSString *
FilenameToString(JSContext *cx, const char *filename)
{
    return JS_NewStringCopyZ(cx, filename);
}

static JSBool
Exception(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /*
     * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
     * called as functions, without operator new.  But as we do not give
     * each constructor a distinct JSClass, whose .name member is used by
     * NewNativeClassInstance to find the class prototype, we must get the
     * class prototype ourselves.
     */
    RootedObject callee(cx, &args.callee());
    RootedValue protov(cx);
    if (!JSObject::getProperty(cx, callee, callee, cx->names().classPrototype, &protov))
        return false;

    if (!protov.isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROTOTYPE, "Error");
        return false;
    }

    RootedObject obj(cx, NewObjectWithGivenProto(cx, &ErrorObject::class_, &protov.toObject(),
                     NULL));
    if (!obj)
        return false;

    /* Set the 'message' property. */
    RootedString message(cx);
    if (args.hasDefined(0)) {
        message = ToString<CanGC>(cx, args[0]);
        if (!message)
            return false;
        args[0].setString(message);
    } else {
        message = NULL;
    }

    /* Find the scripted caller. */
    NonBuiltinScriptFrameIter iter(cx);

    /* Set the 'fileName' property. */
    RootedScript script(cx, iter.done() ? NULL : iter.script());
    RootedString filename(cx);
    if (args.length() > 1) {
        filename = ToString<CanGC>(cx, args[1]);
        if (!filename)
            return false;
        args[1].setString(filename);
    } else {
        filename = cx->runtime()->emptyString;
        if (!iter.done()) {
            if (const char *cfilename = script->filename()) {
                filename = FilenameToString(cx, cfilename);
                if (!filename)
                    return false;
            }
        }
    }

    /* Set the 'lineNumber' property. */
    uint32_t lineno, column = 0;
    if (args.length() > 2) {
        if (!ToUint32(cx, args[2], &lineno))
            return false;
    } else {
        lineno = iter.done() ? 0 : PCToLineNumber(script, iter.pc(), &column);
    }

    int exnType = args.callee().as<JSFunction>().getExtendedSlot(0).toInt32();
    if (!InitExnPrivate(cx, obj, message, filename, lineno, column, NULL, exnType))
        return false;

    args.rval().setObject(*obj);
    return true;
}

/* ES5 15.11.4.4 (NB: with subsequent errata). */
static JSBool
exn_toString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 2. */
    if (!args.thisv().isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_PROTOTYPE, "Error");
        return false;
    }

    /* Step 1. */
    RootedObject obj(cx, &args.thisv().toObject());

    /* Step 3. */
    RootedValue nameVal(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().name, &nameVal))
        return false;

    /* Step 4. */
    RootedString name(cx);
    if (nameVal.isUndefined()) {
        name = cx->names().Error;
    } else {
        name = ToString<CanGC>(cx, nameVal);
        if (!name)
            return false;
    }

    /* Step 5. */
    RootedValue msgVal(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().message, &msgVal))
        return false;

    /* Step 6. */
    RootedString message(cx);
    if (msgVal.isUndefined()) {
        message = cx->runtime()->emptyString;
    } else {
        message = ToString<CanGC>(cx, msgVal);
        if (!message)
            return false;
    }

    /* Step 7. */
    if (name->empty() && message->empty()) {
        args.rval().setString(cx->names().Error);
        return true;
    }

    /* Step 8. */
    if (name->empty()) {
        args.rval().setString(message);
        return true;
    }

    /* Step 9. */
    if (message->empty()) {
        args.rval().setString(name);
        return true;
    }

    /* Step 10. */
    StringBuffer sb(cx);
    if (!sb.append(name) || !sb.append(": ") || !sb.append(message))
        return false;

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

#if JS_HAS_TOSOURCE
/*
 * Return a string that may eval to something similar to the original object.
 */
static JSBool
exn_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedValue nameVal(cx);
    RootedString name(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().name, &nameVal) ||
        !(name = ToString<CanGC>(cx, nameVal)))
    {
        return false;
    }

    RootedValue messageVal(cx);
    RootedString message(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().message, &messageVal) ||
        !(message = ValueToSource(cx, messageVal)))
    {
        return false;
    }

    RootedValue filenameVal(cx);
    RootedString filename(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().fileName, &filenameVal) ||
        !(filename = ValueToSource(cx, filenameVal)))
    {
        return false;
    }

    RootedValue linenoVal(cx);
    uint32_t lineno;
    if (!JSObject::getProperty(cx, obj, obj, cx->names().lineNumber, &linenoVal) ||
        !ToUint32(cx, linenoVal, &lineno))
    {
        return false;
    }

    StringBuffer sb(cx);
    if (!sb.append("(new ") || !sb.append(name) || !sb.append("("))
        return false;

    if (!sb.append(message))
        return false;

    if (!filename->empty()) {
        if (!sb.append(", ") || !sb.append(filename))
            return false;
    }
    if (lineno != 0) {
        /* We have a line, but no filename, add empty string */
        if (filename->empty() && !sb.append(", \"\""))
                return false;

        JSString *linenumber = ToString<CanGC>(cx, linenoVal);
        if (!linenumber)
            return false;
        if (!sb.append(", ") || !sb.append(linenumber))
            return false;
    }

    if (!sb.append("))"))
        return false;

    JSString *str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}
#endif

static const JSFunctionSpec exception_methods[] = {
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

static JSObject *
InitErrorClass(JSContext *cx, Handle<GlobalObject*> global, int type, HandleObject proto)
{
    JSProtoKey key = GetExceptionProtoKey(type);
    RootedAtom name(cx, ClassName(key, cx));
    RootedObject errorProto(cx, global->createBlankPrototypeInheriting(cx, &ErrorObject::class_,
                            *proto));
    if (!errorProto)
        return NULL;

    RootedValue nameValue(cx, StringValue(name));
    RootedValue zeroValue(cx, Int32Value(0));
    RootedValue empty(cx, StringValue(cx->runtime()->emptyString));
    RootedId nameId(cx, NameToId(cx->names().name));
    RootedId messageId(cx, NameToId(cx->names().message));
    RootedId fileNameId(cx, NameToId(cx->names().fileName));
    RootedId lineNumberId(cx, NameToId(cx->names().lineNumber));
    RootedId columnNumberId(cx, NameToId(cx->names().columnNumber));
    if (!DefineNativeProperty(cx, errorProto, nameId, nameValue,
                              JS_PropertyStub, JS_StrictPropertyStub, 0, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, messageId, empty,
                              JS_PropertyStub, JS_StrictPropertyStub, 0, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, fileNameId, empty,
                              JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, lineNumberId, zeroValue,
                              JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE, 0, 0) ||
        !DefineNativeProperty(cx, errorProto, columnNumberId, zeroValue,
                              JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE, 0, 0))
    {
        return NULL;
    }

    /* Create the corresponding constructor. */
    RootedFunction ctor(cx, global->createConstructor(cx, Exception, name, 1,
                                                      JSFunction::ExtendedFinalizeKind));
    if (!ctor)
        return NULL;
    ctor->setExtendedSlot(0, Int32Value(int32_t(type)));

    if (!LinkConstructorAndPrototype(cx, ctor, errorProto))
        return NULL;

    if (!DefineConstructorAndPrototype(cx, global, key, ctor, errorProto))
        return NULL;

    JS_ASSERT(!errorProto->getPrivate());

    return errorProto;
}

JSObject *
js_InitExceptionClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedObject objectProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objectProto)
        return NULL;

    /* Initialize the base Error class first. */
    RootedObject errorProto(cx, InitErrorClass(cx, global, JSEXN_ERR, objectProto));
    if (!errorProto)
        return NULL;

    /* |Error.prototype| alone has method properties. */
    if (!DefinePropertiesAndBrand(cx, errorProto, NULL, exception_methods))
        return NULL;

    /* Define all remaining *Error constructors. */
    for (int i = JSEXN_ERR + 1; i < JSEXN_LIMIT; i++) {
        if (!InitErrorClass(cx, global, i, errorProto))
            return NULL;
    }

    return errorProto;
}

const JSErrorFormatString*
js_GetLocalizedErrorMessage(JSContext* cx, void *userRef, const char *locale,
                            const unsigned errorNumber)
{
    const JSErrorFormatString *errorString = NULL;

    if (cx->runtime()->localeCallbacks && cx->runtime()->localeCallbacks->localeGetErrorMessage) {
        errorString = cx->runtime()->localeCallbacks
                                   ->localeGetErrorMessage(userRef, locale, errorNumber);
    }
    if (!errorString)
        errorString = js_GetErrorMessage(userRef, locale, errorNumber);
    return errorString;
}

JS_FRIEND_API(const jschar*)
js::GetErrorTypeName(JSContext* cx, int16_t exnType)
{
    /*
     * JSEXN_INTERNALERR returns null to prevent that "InternalError: "
     * is prepended before "uncaught exception: "
     */
    if (exnType <= JSEXN_NONE || exnType >= JSEXN_LIMIT ||
        exnType == JSEXN_INTERNALERR)
    {
        return NULL;
    }
    JSProtoKey key = GetExceptionProtoKey(exnType);
    return ClassName(key, cx)->chars();
}

#if defined ( DEBUG_mccabe ) && defined ( PRINTNAMES )
/* For use below... get character strings for error name and exception name */
static const struct exnname { char *name; char *exception; } errortoexnname[] = {
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

    /*
     * Tell our caller to report immediately if this report is just a warning.
     */
    JS_ASSERT(reportp);
    if (JSREPORT_IS_WARNING(reportp->flags))
        return false;

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
        return false;

    /* Prevent infinite recursion. */
    if (cx->generatingError)
        return false;
    AutoScopedAssign<bool> asa(&cx->generatingError, true);

    /* Protect the newly-created strings below from nesting GCs. */
    PodArrayZero(tv);
    AutoArrayRooter tvr(cx, ArrayLength(tv), tv);

    /*
     * Try to get an appropriate prototype by looking up the corresponding
     * exception constructor name in the scope chain of the current context's
     * top stack frame, or in the global object if no frame is active.
     */
    RootedObject errProto(cx);
    if (!js_GetClassPrototype(cx, GetExceptionProtoKey(exn), &errProto))
        return false;
    tv[0] = OBJECT_TO_JSVAL(errProto);

    RootedObject errObject(cx, NewObjectWithGivenProto(cx, &ErrorObject::class_, errProto, NULL));
    if (!errObject)
        return false;
    tv[1] = OBJECT_TO_JSVAL(errObject);

    RootedString messageStr(cx, reportp->ucmessage ? JS_NewUCStringCopyZ(cx, reportp->ucmessage)
                                                   : JS_NewStringCopyZ(cx, message));
    if (!messageStr)
        return false;
    tv[2] = STRING_TO_JSVAL(messageStr);

    RootedString filenameStr(cx, JS_NewStringCopyZ(cx, reportp->filename));
    if (!filenameStr)
        return false;
    tv[3] = STRING_TO_JSVAL(filenameStr);

    if (!InitExnPrivate(cx, errObject, messageStr, filenameStr,
                        reportp->lineno, reportp->column, reportp, exn)) {
        return false;
    }

    JS_SetPendingException(cx, OBJECT_TO_JSVAL(errObject));

    /* Flag the error report passed in to indicate an exception was raised. */
    reportp->flags |= JSREPORT_EXCEPTION;
    return true;
}

static bool
IsDuckTypedErrorObject(JSContext *cx, HandleObject exnObject, const char **filename_strp)
{
    JSBool found;
    if (!JS_HasProperty(cx, exnObject, js_message_str, &found) || !found)
        return false;

    const char *filename_str = *filename_strp;
    if (!JS_HasProperty(cx, exnObject, filename_str, &found) || !found) {
        /* DOMException duck quacks "filename" (all lowercase) */
        filename_str = "filename";
        if (!JS_HasProperty(cx, exnObject, filename_str, &found) || !found)
            return false;
    }

    if (!JS_HasProperty(cx, exnObject, js_lineNumber_str, &found) || !found)
        return false;

    *filename_strp = filename_str;
    return true;
}

JSBool
js_ReportUncaughtException(JSContext *cx)
{
    jsval roots[6];
    JSErrorReport *reportp, report;

    if (!JS_IsExceptionPending(cx))
        return true;

    RootedValue exn(cx);
    if (!JS_GetPendingException(cx, exn.address()))
        return false;

    PodArrayZero(roots);
    AutoArrayRooter tvr(cx, ArrayLength(roots), roots);

    /*
     * Because ToString below could error and an exception object could become
     * unrooted, we must root exnObject.  Later, if exnObject is non-null, we
     * need to root other intermediates, so allocate an operand stack segment
     * to protect all of these values.
     */
    RootedObject exnObject(cx);
    if (JSVAL_IS_PRIMITIVE(exn)) {
        exnObject = NULL;
    } else {
        exnObject = JSVAL_TO_OBJECT(exn);
        roots[0] = exn;
    }

    JS_ClearPendingException(cx);
    reportp = js_ErrorFromException(exn);

    /* XXX L10N angels cry once again. see also everywhere else */
    RootedString str(cx, ToString<CanGC>(cx, exn));
    if (str)
        roots[1] = StringValue(str);

    const char *filename_str = js_fileName_str;
    JSAutoByteString filename;
    if (!reportp && exnObject &&
        (exnObject->is<ErrorObject>() || IsDuckTypedErrorObject(cx, exnObject, &filename_str)))
    {
        RootedString name(cx);
        if (JS_GetProperty(cx, exnObject, js_name_str, tvr.handleAt(2)) &&
            JSVAL_IS_STRING(roots[2]))
        {
            name = JSVAL_TO_STRING(roots[2]);
        }

        RootedString msg(cx);
        if (JS_GetProperty(cx, exnObject, js_message_str, tvr.handleAt(3)) &&
            JSVAL_IS_STRING(roots[3]))
        {
            msg = JSVAL_TO_STRING(roots[3]);
        }

        if (name && msg) {
            RootedString colon(cx, JS_NewStringCopyZ(cx, ": "));
            if (!colon)
                return false;
            RootedString nameColon(cx, ConcatStrings<CanGC>(cx, name, colon));
            if (!nameColon)
                return false;
            str = ConcatStrings<CanGC>(cx, nameColon, msg);
            if (!str)
                return false;
        } else if (name) {
            str = name;
        } else if (msg) {
            str = msg;
        }

        if (JS_GetProperty(cx, exnObject, filename_str, tvr.handleAt(4))) {
            JSString *tmp = ToString<CanGC>(cx, HandleValue::fromMarkedLocation(&roots[4]));
            if (tmp)
                filename.encodeLatin1(cx, tmp);
        }

        uint32_t lineno;
        if (!JS_GetProperty(cx, exnObject, js_lineNumber_str, tvr.handleAt(5)) ||
            !ToUint32(cx, roots[5], &lineno))
        {
            lineno = 0;
        }

        uint32_t column;
        if (!JS_GetProperty(cx, exnObject, js_columnNumber_str, tvr.handleAt(5)) ||
            !ToUint32(cx, roots[5], &column))
        {
            column = 0;
        }

        reportp = &report;
        PodZero(&report);
        report.filename = filename.ptr();
        report.lineno = (unsigned) lineno;
        report.exnType = int16_t(JSEXN_NONE);
        report.column = (unsigned) column;
        if (str) {
            if (JSStableString *stable = str->ensureStable(cx))
                report.ucmessage = stable->chars().get();
        }
    }

    JSAutoByteString bytesStorage;
    const char *bytes = NULL;
    if (str)
        bytes = bytesStorage.encodeLatin1(cx, str);
    if (!bytes)
        bytes = "unknown (can't convert to string)";

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
js_CopyErrorObject(JSContext *cx, HandleObject errobj, HandleObject scope)
{
    assertSameCompartment(cx, scope);
    JSExnPrivate *priv = errobj->as<ErrorObject>().getExnPrivate();

    size_t size = offsetof(JSExnPrivate, stackElems) +
                  priv->stackDepth * sizeof(JSStackTraceElem);

    ScopedJSFreePtr<JSExnPrivate> copy(static_cast<JSExnPrivate *>(cx->malloc_(size)));
    if (!copy)
        return NULL;

    if (priv->errorReport) {
        copy->errorReport = CopyErrorReport(cx, priv->errorReport);
        if (!copy->errorReport)
            return NULL;
    } else {
        copy->errorReport = NULL;
    }
    ScopedJSFreePtr<JSErrorReport> autoFreeErrorReport(copy->errorReport);

    copy->message.init(priv->message);
    if (!cx->compartment()->wrap(cx, &copy->message))
        return NULL;
    JS::Anchor<JSString *> messageAnchor(copy->message);
    copy->filename.init(priv->filename);
    if (!cx->compartment()->wrap(cx, &copy->filename))
        return NULL;
    JS::Anchor<JSString *> filenameAnchor(copy->filename);
    copy->lineno = priv->lineno;
    copy->column = priv->column;
    copy->stackDepth = 0;
    copy->exnType = priv->exnType;

    // Create the Error object.
    RootedObject proto(cx, scope->global().getOrCreateCustomErrorPrototype(cx, copy->exnType));
    if (!proto)
        return NULL;
    RootedObject copyobj(cx, NewObjectWithGivenProto(cx, &ErrorObject::class_, proto, NULL));
    if (!copyobj)
        return NULL;
    SetExnPrivate(copyobj->as<ErrorObject>(), copy);
    copy.forget();
    autoFreeErrorReport.forget();
    return copyobj;
}
