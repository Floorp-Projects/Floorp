/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS standard exception implementation.
 */

#include "jsexn.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/PodOperations.h"

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

#include "jsobjinlines.h"

#include "vm/ErrorObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::ArrayLength;
using mozilla::PodArrayZero;
using mozilla::PodZero;

static void
exn_finalize(FreeOp *fop, JSObject *obj);

const Class ErrorObject::class_ = {
    js_Error_str,
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Error) |
    JSCLASS_HAS_RESERVED_SLOTS(ErrorObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    exn_finalize,
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr                  /* construct   */
};

JSErrorReport *
js::CopyErrorReport(JSContext *cx, JSErrorReport *report)
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
        return nullptr;

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
        copy->messageArgs[i] = nullptr;
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
    JS::AutoSaveExceptionState prevState;

    explicit SuppressErrorsGuard(JSContext *cx)
      : cx(cx),
        prevReporter(JS_SetErrorReporter(cx, nullptr)),
        prevState(cx)
    {}

    ~SuppressErrorsGuard()
    {
        JS_SetErrorReporter(cx, prevReporter);
    }
};

JSString *
js::ComputeStackString(JSContext *cx)
{
    StringBuffer sb(cx);

    {
        RootedAtom atom(cx);
        SuppressErrorsGuard seg(cx);
        for (NonBuiltinFrameIter i(cx, FrameIter::ALL_CONTEXTS, FrameIter::GO_THROUGH_SAVED,
                                   cx->compartment()->principals);
             !i.done();
             ++i)
        {
            /* First append the function name, if any. */
            if (i.isNonEvalFunctionFrame())
                atom = i.functionDisplayAtom();
            else
                atom = nullptr;
            if (atom && !sb.append(atom))
                return nullptr;

            /* Next a @ separating function name from source location. */
            if (!sb.append('@'))
                return nullptr;

            /* Now the filename. */
            const char *cfilename = i.scriptFilename();
            if (!cfilename)
                cfilename = "";
            if (!sb.appendInflated(cfilename, strlen(cfilename)))
                return nullptr;

            uint32_t column = 0;
            uint32_t line = i.computeLine(&column);
            // Now the line number
            if (!sb.append(':') || !NumberValueToStringBuffer(cx, NumberValue(line), sb))
                return nullptr;

            // Finally, : followed by the column number (1-based, as in other browsers)
            // and a newline.
            if (!sb.append(':') || !NumberValueToStringBuffer(cx, NumberValue(column + 1), sb) ||
                !sb.append('\n'))
            {
                return nullptr;
            }

            /*
             * Cut off the stack if it gets too deep (most commonly for
             * infinite recursion errors).
             */
            const size_t MaxReportedStackDepth = 1u << 20;
            if (sb.length() > MaxReportedStackDepth)
                break;
        }
    }

    return sb.finishString();
}

static void
exn_finalize(FreeOp *fop, JSObject *obj)
{
    if (JSErrorReport *report = obj->as<ErrorObject>().getErrorReport()) {
        /* These were held by ErrorObject::init. */
        if (JSPrincipals *prin = report->originPrincipals)
            JS_DropPrincipals(fop->runtime(), prin);
        fop->free_(report);
    }
}

JSErrorReport *
js_ErrorFromException(JSContext *cx, HandleObject objArg)
{
    // It's ok to UncheckedUnwrap here, since all we do is get the
    // JSErrorReport, and consumers are careful with the information they get
    // from that anyway.  Anyone doing things that would expose anything in the
    // JSErrorReport to page script either does a security check on the
    // JSErrorReport's principal or also tries to do toString on our object and
    // will fail if they can't unwrap it.
    RootedObject obj(cx, UncheckedUnwrap(objArg));
    if (!obj->is<ErrorObject>())
        return nullptr;

    return obj->as<ErrorObject>().getOrCreateErrorReport(cx);
}

static bool
Error(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Compute the error message, if any. */
    RootedString message(cx, nullptr);
    if (args.hasDefined(0)) {
        message = ToString<CanGC>(cx, args[0]);
        if (!message)
            return false;
    }

    /* Find the scripted caller. */
    NonBuiltinFrameIter iter(cx);

    /* Set the 'fileName' property. */
    RootedString fileName(cx);
    if (args.length() > 1) {
        fileName = ToString<CanGC>(cx, args[1]);
    } else {
        fileName = cx->runtime()->emptyString;
        if (!iter.done()) {
            if (const char *cfilename = iter.scriptFilename())
                fileName = JS_NewStringCopyZ(cx, cfilename);
        }
    }
    if (!fileName)
        return false;

    /* Set the 'lineNumber' property. */
    uint32_t lineNumber, columnNumber = 0;
    if (args.length() > 2) {
        if (!ToUint32(cx, args[2], &lineNumber))
            return false;
    } else {
        lineNumber = iter.done() ? 0 : iter.computeLine(&columnNumber);
    }

    Rooted<JSString*> stack(cx, ComputeStackString(cx));
    if (!stack)
        return false;

    /*
     * ECMA ed. 3, 15.11.1 requires Error, etc., to construct even when
     * called as functions, without operator new.  But as we do not give
     * each constructor a distinct JSClass, we must get the exception type
     * ourselves.
     */
    JSExnType exnType = JSExnType(args.callee().as<JSFunction>().getExtendedSlot(0).toInt32());

    RootedObject obj(cx, ErrorObject::create(cx, exnType, stack, fileName,
                                             lineNumber, columnNumber, nullptr, message));
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

/* ES5 15.11.4.4 (NB: with subsequent errata). */
static bool
exn_toString(JSContext *cx, unsigned argc, Value *vp)
{
    JS_CHECK_RECURSION(cx, return false);
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 2. */
    if (!args.thisv().isObject()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr, JSMSG_BAD_PROTOTYPE, "Error");
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
static bool
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

/* static */ ErrorObject *
ErrorObject::createProto(JSContext *cx, JS::Handle<GlobalObject*> global, JSExnType type,
                         JS::HandleObject proto)
{
    RootedObject errorProto(cx);
    errorProto = global->createBlankPrototypeInheriting(cx, &ErrorObject::class_, *proto);
    if (!errorProto)
        return nullptr;

    Rooted<ErrorObject*> err(cx, &errorProto->as<ErrorObject>());
    RootedString emptyStr(cx, cx->names().empty);
    if (!ErrorObject::init(cx, err, type, nullptr, emptyStr, emptyStr, 0, 0, emptyStr))
        return nullptr;

    // The various prototypes also have .name in addition to the normal error
    // instance properties.
    JSProtoKey key = GetExceptionProtoKey(type);
    RootedPropertyName name(cx, ClassName(key, cx));
    RootedValue nameValue(cx, StringValue(name));
    if (!JSObject::defineProperty(cx, err, cx->names().name, nameValue,
                                  JS_PropertyStub, JS_StrictPropertyStub, 0))
    {
        return nullptr;
    }

    // Create the corresponding constructor.
    RootedFunction ctor(cx, global->createConstructor(cx, Error, name, 1,
                                                      JSFunction::ExtendedFinalizeKind));
    if (!ctor)
        return nullptr;
    ctor->setExtendedSlot(0, Int32Value(int32_t(type)));

    if (!LinkConstructorAndPrototype(cx, ctor, err))
        return nullptr;

    if (!GlobalObject::initBuiltinConstructor(cx, global, key, ctor, err))
        return nullptr;

    return err;
}

JSObject *
js_InitExceptionClasses(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->is<GlobalObject>());
    JS_ASSERT(obj->isNative());

    Rooted<GlobalObject*> global(cx, &obj->as<GlobalObject>());

    RootedObject objProto(cx, global->getOrCreateObjectPrototype(cx));
    if (!objProto)
        return nullptr;

    /* Initialize the base Error class first. */
    RootedObject errorProto(cx, ErrorObject::createProto(cx, global, JSEXN_ERR, objProto));
    if (!errorProto)
        return nullptr;

    /* |Error.prototype| alone has method properties. */
    if (!DefinePropertiesAndBrand(cx, errorProto, nullptr, exception_methods))
        return nullptr;

    /* Define all remaining *Error constructors. */
    for (int i = JSEXN_ERR + 1; i < JSEXN_LIMIT; i++) {
        if (!ErrorObject::createProto(cx, global, JSExnType(i), errorProto))
            return nullptr;
    }

    return errorProto;
}

const JSErrorFormatString*
js_GetLocalizedErrorMessage(ExclusiveContext *cx, void *userRef, const char *locale,
                            const unsigned errorNumber)
{
    const JSErrorFormatString *errorString = nullptr;

    // The locale callbacks might not be thread safe, so don't call them if
    // we're not on the main thread. When used with XPConnect,
    // |localeGetErrorMessage| will be nullptr anyways.
    if (cx->isJSContext() &&
        cx->asJSContext()->runtime()->localeCallbacks &&
        cx->asJSContext()->runtime()->localeCallbacks->localeGetErrorMessage)
    {
        JSLocaleCallbacks *callbacks = cx->asJSContext()->runtime()->localeCallbacks;
        errorString = callbacks->localeGetErrorMessage(userRef, locale, errorNumber);
    }

    if (!errorString)
        errorString = js_GetErrorMessage(userRef, locale, errorNumber);
    return errorString;
}

JS_FRIEND_API(const jschar*)
js::GetErrorTypeName(JSRuntime* rt, int16_t exnType)
{
    /*
     * JSEXN_INTERNALERR returns null to prevent that "InternalError: "
     * is prepended before "uncaught exception: "
     */
    if (exnType <= JSEXN_NONE || exnType >= JSEXN_LIMIT ||
        exnType == JSEXN_INTERNALERR)
    {
        return nullptr;
    }
    JSProtoKey key = GetExceptionProtoKey(JSExnType(exnType));
    return ClassName(key, rt)->chars();
}

bool
js_ErrorToException(JSContext *cx, const char *message, JSErrorReport *reportp,
                    JSErrorCallback callback, void *userRef)
{
    // Tell our caller to report immediately if this report is just a warning.
    JS_ASSERT(reportp);
    if (JSREPORT_IS_WARNING(reportp->flags))
        return false;

    // Find the exception index associated with this error.
    JSErrNum errorNumber = static_cast<JSErrNum>(reportp->errorNumber);
    const JSErrorFormatString *errorString;
    if (!callback || callback == js_GetErrorMessage)
        errorString = js_GetLocalizedErrorMessage(cx, nullptr, nullptr, errorNumber);
    else
        errorString = callback(userRef, nullptr, errorNumber);
    JSExnType exnType = errorString ? static_cast<JSExnType>(errorString->exnType) : JSEXN_NONE;
    MOZ_ASSERT(exnType < JSEXN_LIMIT);

    // Return false (no exception raised) if no exception is associated
    // with the given error number.
    if (exnType == JSEXN_NONE)
        return false;

    // Prevent infinite recursion.
    if (cx->generatingError)
        return false;
    AutoScopedAssign<bool> asa(&cx->generatingError, true);

    // Create an exception object.
    RootedString messageStr(cx, reportp->ucmessage ? JS_NewUCStringCopyZ(cx, reportp->ucmessage)
                                                   : JS_NewStringCopyZ(cx, message));
    if (!messageStr)
        return cx->isExceptionPending();

    RootedString fileName(cx, JS_NewStringCopyZ(cx, reportp->filename));
    if (!fileName)
        return cx->isExceptionPending();

    uint32_t lineNumber = reportp->lineno;
    uint32_t columnNumber = reportp->column;

    RootedString stack(cx, ComputeStackString(cx));
    if (!stack)
        return cx->isExceptionPending();

    js::ScopedJSFreePtr<JSErrorReport> report(CopyErrorReport(cx, reportp));
    if (!report)
        return cx->isExceptionPending();

    RootedObject errObject(cx, ErrorObject::create(cx, exnType, stack, fileName,
                                                   lineNumber, columnNumber, &report, messageStr));
    if (!errObject)
        return cx->isExceptionPending();

    // Throw it.
    RootedValue errValue(cx, ObjectValue(*errObject));
    JS_SetPendingException(cx, errValue);

    // Flag the error report passed in to indicate an exception was raised.
    reportp->flags |= JSREPORT_EXCEPTION;
    return true;
}

static bool
IsDuckTypedErrorObject(JSContext *cx, HandleObject exnObject, const char **filename_strp)
{
    bool found;
    if (!JS_HasProperty(cx, exnObject, js_message_str, &found) || !found)
        return false;

    const char *filename_str = *filename_strp;
    if (!JS_HasProperty(cx, exnObject, filename_str, &found) || !found) {
        /* Now try "fileName", in case this quacks like an Error */
        filename_str = js_fileName_str;
        if (!JS_HasProperty(cx, exnObject, filename_str, &found) || !found)
            return false;
    }

    if (!JS_HasProperty(cx, exnObject, js_lineNumber_str, &found) || !found)
        return false;

    *filename_strp = filename_str;
    return true;
}

JS_FRIEND_API(JSString *)
js::ErrorReportToString(JSContext *cx, JSErrorReport *reportp)
{
    JSExnType type = static_cast<JSExnType>(reportp->exnType);
    RootedString str(cx, cx->runtime()->emptyString);
    if (type != JSEXN_NONE)
        str = ClassName(GetExceptionProtoKey(type), cx);
    RootedString toAppend(cx, JS_NewUCStringCopyN(cx, MOZ_UTF16(": "), 2));
    if (!str || !toAppend)
        return nullptr;
    str = ConcatStrings<CanGC>(cx, str, toAppend);
    if (!str)
        return nullptr;
    toAppend = JS_NewUCStringCopyZ(cx, reportp->ucmessage);
    if (toAppend)
        str = ConcatStrings<CanGC>(cx, str, toAppend);
    return str;
}

bool
js_ReportUncaughtException(JSContext *cx)
{
    if (!cx->isExceptionPending())
        return true;

    RootedValue exn(cx);
    if (!cx->getPendingException(&exn))
        return false;

    /*
     * Because ToString below could error and an exception object could become
     * unrooted, we must root exnObject.  Later, if exnObject is non-null, we
     * need to root other intermediates, so allocate an operand stack segment
     * to protect all of these values.
     */
    RootedObject exnObject(cx);
    if (exn.isPrimitive()) {
        exnObject = nullptr;
    } else {
        exnObject = exn.toObjectOrNull();
    }

    JS_ClearPendingException(cx);
    JSErrorReport *reportp = exnObject ? js_ErrorFromException(cx, exnObject)
                                       : nullptr;

    // Be careful not to invoke ToString if we've already successfully extracted
    // an error report, since the exception might be wrapped in a security
    // wrapper, and ToString-ing it might throw.
    RootedString str(cx);
    if (reportp)
        str = ErrorReportToString(cx, reportp);
    else
        str = ToString<CanGC>(cx, exn);

    JSErrorReport report;

    // If js_ErrorFromException didn't get us a JSErrorReport, then the object
    // was not an ErrorObject, security-wrapped or otherwise. However, it might
    // still quack like one. Give duck-typing a chance.  We start by looking for
    // "filename" (all lowercase), since that's where DOMExceptions store their
    // filename.  Then we check "fileName", which is where Errors store it.  We
    // have to do it in that order, because DOMExceptions have Error.prototype
    // on their proto chain, and hence also have a "fileName" property, but its
    // value is "".
    const char *filename_str = "filename";
    JSAutoByteString filename;
    if (!reportp && exnObject && IsDuckTypedErrorObject(cx, exnObject, &filename_str))
    {
        // Temporary value for pulling properties off of duck-typed objects.
        RootedValue val(cx);

        RootedString name(cx);
        if (JS_GetProperty(cx, exnObject, js_name_str, &val) && val.isString())
            name = val.toString();

        RootedString msg(cx);
        if (JS_GetProperty(cx, exnObject, js_message_str, &val) && val.isString())
            msg = val.toString();

        // If we have the right fields, override the ToString we performed on
        // the exception object above with something built out of its quacks
        // (i.e. as much of |NameQuack: MessageQuack| as we can make).
        //
        // It would be nice to use ErrorReportToString here, but we can't quite
        // do it - mostly because we'd need to figure out what JSExnType |name|
        // corresponds to, which may not be any JSExnType at all.
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

        if (JS_GetProperty(cx, exnObject, filename_str, &val)) {
            JSString *tmp = ToString<CanGC>(cx, val);
            if (tmp)
                filename.encodeLatin1(cx, tmp);
        }

        uint32_t lineno;
        if (!JS_GetProperty(cx, exnObject, js_lineNumber_str, &val) ||
            !ToUint32(cx, val, &lineno))
        {
            lineno = 0;
        }

        uint32_t column;
        if (!JS_GetProperty(cx, exnObject, js_columnNumber_str, &val) ||
            !ToUint32(cx, val, &column))
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
            // Note that using |str| for |ucmessage| here is kind of wrong,
            // because |str| is supposed to be of the format
            // |ErrorName: ErrorMessage|, and |ucmessage| is supposed to
            // correspond to |ErrorMessage|. But this is what we've historically
            // done for duck-typed error objects.
            //
            // If only this stuff could get specced one day...
            if (JSFlatString *flat = str->ensureFlat(cx))
                report.ucmessage = flat->chars();
        }
    }

    JSAutoByteString bytesStorage;
    const char *bytes = nullptr;
    if (str)
        bytes = bytesStorage.encodeLatin1(cx, str);
    if (!bytes)
        bytes = "unknown (can't convert to string)";

    if (!reportp) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                             JSMSG_UNCAUGHT_EXCEPTION, bytes);
    } else {
        /* Flag the error as an exception. */
        reportp->flags |= JSREPORT_EXCEPTION;

        /* Pass the exception object. */
        JS_SetPendingException(cx, exn);
        CallErrorReporter(cx, bytes, reportp);
    }

    JS_ClearPendingException(cx);
    return true;
}

JSObject *
js_CopyErrorObject(JSContext *cx, Handle<ErrorObject*> err, HandleObject scope)
{
    assertSameCompartment(cx, scope);

    js::ScopedJSFreePtr<JSErrorReport> copyReport;
    if (JSErrorReport *errorReport = err->getErrorReport()) {
        copyReport = CopyErrorReport(cx, errorReport);
        if (!copyReport)
            return nullptr;
    }

    RootedString message(cx, err->getMessage());
    if (message && !cx->compartment()->wrap(cx, message.address()))
        return nullptr;
    RootedString fileName(cx, err->fileName(cx));
    if (!cx->compartment()->wrap(cx, fileName.address()))
        return nullptr;
    RootedString stack(cx, err->stack(cx));
    if (!cx->compartment()->wrap(cx, stack.address()))
        return nullptr;
    uint32_t lineNumber = err->lineNumber();
    uint32_t columnNumber = err->columnNumber();
    JSExnType errorType = err->type();

    // Create the Error object.
    return ErrorObject::create(cx, errorType, stack, fileName,
                               lineNumber, columnNumber, &copyReport, message);
}

JS_PUBLIC_API(bool)
JS::CreateTypeError(JSContext *cx, HandleString stack, HandleString fileName,
                    uint32_t lineNumber, uint32_t columnNumber, JSErrorReport *report,
                    HandleString message, MutableHandleValue rval)
{
    assertSameCompartment(cx, stack, fileName, message);
    js::ScopedJSFreePtr<JSErrorReport> rep;
    if (report)
        rep = CopyErrorReport(cx, report);

    RootedObject obj(cx,
        js::ErrorObject::create(cx, JSEXN_TYPEERR, stack, fileName,
                                lineNumber, columnNumber, &rep, message));
    if (!obj)
        return false;

    rval.setObject(*obj);
    return true;
}
