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
 * JavaScript API.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h" /* Added by JSIFY */
#include "jsutil.h" /* Added by JSIFY */
#include "jsclist.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdate.h"
#include "jsdtoa.h"
#include "jsemit.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "json.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsregexp.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstask.h"
#include "jstracer.h"
#include "jsdbgapi.h"
#include "prmjtime.h"
#include "jsstaticcheck.h"
#include "jsvector.h"
#include "jstypedarray.h"

#include "jsatominlines.h"
#include "jsscopeinlines.h"

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

using namespace js;

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) ((va_list *)(ap))
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

#if defined(JS_THREADSAFE)
#define CHECK_REQUEST(cx)                                                   \
    JS_ASSERT((cx)->requestDepth || (cx)->thread == (cx)->runtime->gcThread)
#else
#define CHECK_REQUEST(cx)       ((void)0)
#endif

/* Check that we can cast JSObject* as jsval without tag bit manipulations. */
JS_STATIC_ASSERT(JSVAL_OBJECT == 0);

/* Check that JSVAL_TRACE_KIND works. */
JS_STATIC_ASSERT(JSVAL_TRACE_KIND(JSVAL_OBJECT) == JSTRACE_OBJECT);
JS_STATIC_ASSERT(JSVAL_TRACE_KIND(JSVAL_DOUBLE) == JSTRACE_DOUBLE);
JS_STATIC_ASSERT(JSVAL_TRACE_KIND(JSVAL_STRING) == JSTRACE_STRING);

JS_PUBLIC_API(int64)
JS_Now()
{
    return PRMJ_Now();
}

JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    return cx->runtime->NaNValue;
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    return cx->runtime->negativeInfinityValue;
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    return cx->runtime->positiveInfinityValue;
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    return STRING_TO_JSVAL(cx->runtime->emptyString);
}

static JSBool
TryArgumentFormatter(JSContext *cx, const char **formatp, JSBool fromJS,
                     jsval **vpp, va_list *app)
{
    const char *format;
    JSArgumentFormatMap *map;

    format = *formatp;
    for (map = cx->argumentFormatMap; map; map = map->next) {
        if (!strncmp(format, map->format, map->length)) {
            *formatp = format + map->length;
            return map->formatter(cx, format, fromJS, vpp, app);
        }
    }
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_CHAR, format);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArguments(JSContext *cx, uintN argc, jsval *argv, const char *format,
                    ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, format);
    ok = JS_ConvertArgumentsVA(cx, argc, argv, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArgumentsVA(JSContext *cx, uintN argc, jsval *argv,
                      const char *format, va_list ap)
{
    jsval *sp;
    JSBool required;
    char c;
    JSFunction *fun;
    jsdouble d;
    JSString *str;
    JSObject *obj;

    CHECK_REQUEST(cx);
    sp = argv;
    required = JS_TRUE;
    while ((c = *format++) != '\0') {
        if (isspace(c))
            continue;
        if (c == '/') {
            required = JS_FALSE;
            continue;
        }
        if (sp == argv + argc) {
            if (required) {
                fun = js_ValueToFunction(cx, &argv[-2], 0);
                if (fun) {
                    char numBuf[12];
                    JS_snprintf(numBuf, sizeof numBuf, "%u", argc);
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_MORE_ARGS_NEEDED,
                                         JS_GetFunctionName(fun), numBuf,
                                         (argc == 1) ? "" : "s");
                }
                return JS_FALSE;
            }
            break;
        }
        switch (c) {
          case 'b':
            *va_arg(ap, JSBool *) = js_ValueToBoolean(*sp);
            break;
          case 'c':
            if (!JS_ValueToUint16(cx, *sp, va_arg(ap, uint16 *)))
                return JS_FALSE;
            break;
          case 'i':
            if (!JS_ValueToECMAInt32(cx, *sp, va_arg(ap, int32 *)))
                return JS_FALSE;
            break;
          case 'u':
            if (!JS_ValueToECMAUint32(cx, *sp, va_arg(ap, uint32 *)))
                return JS_FALSE;
            break;
          case 'j':
            if (!JS_ValueToInt32(cx, *sp, va_arg(ap, int32 *)))
                return JS_FALSE;
            break;
          case 'd':
            if (!JS_ValueToNumber(cx, *sp, va_arg(ap, jsdouble *)))
                return JS_FALSE;
            break;
          case 'I':
            if (!JS_ValueToNumber(cx, *sp, &d))
                return JS_FALSE;
            *va_arg(ap, jsdouble *) = js_DoubleToInteger(d);
            break;
          case 's':
          case 'S':
          case 'W':
            str = js_ValueToString(cx, *sp);
            if (!str)
                return JS_FALSE;
            *sp = STRING_TO_JSVAL(str);
            if (c == 's') {
                const char *bytes = js_GetStringBytes(cx, str);
                if (!bytes)
                    return JS_FALSE;
                *va_arg(ap, const char **) = bytes;
            } else if (c == 'W') {
                const jschar *chars = js_GetStringChars(cx, str);
                if (!chars)
                    return JS_FALSE;
                *va_arg(ap, const jschar **) = chars;
            } else {
                *va_arg(ap, JSString **) = str;
            }
            break;
          case 'o':
            if (!js_ValueToObject(cx, *sp, &obj))
                return JS_FALSE;
            *sp = OBJECT_TO_JSVAL(obj);
            *va_arg(ap, JSObject **) = obj;
            break;
          case 'f':
            obj = js_ValueToFunctionObject(cx, sp, 0);
            if (!obj)
                return JS_FALSE;
            *sp = OBJECT_TO_JSVAL(obj);
            *va_arg(ap, JSFunction **) = GET_FUNCTION_PRIVATE(cx, obj);
            break;
          case 'v':
            *va_arg(ap, jsval *) = *sp;
            break;
          case '*':
            break;
          default:
            format--;
            if (!TryArgumentFormatter(cx, &format, JS_TRUE, &sp,
                                      JS_ADDRESSOF_VA_LIST(ap))) {
                return JS_FALSE;
            }
            /* NB: the formatter already updated sp, so we continue here. */
            continue;
        }
        sp++;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_AddArgumentFormatter(JSContext *cx, const char *format,
                        JSArgumentFormatter formatter)
{
    size_t length;
    JSArgumentFormatMap **mpp, *map;

    length = strlen(format);
    mpp = &cx->argumentFormatMap;
    while ((map = *mpp) != NULL) {
        /* Insert before any shorter string to match before prefixes. */
        if (map->length < length)
            break;
        if (map->length == length && !strcmp(map->format, format))
            goto out;
        mpp = &map->next;
    }
    map = (JSArgumentFormatMap *) cx->malloc(sizeof *map);
    if (!map)
        return JS_FALSE;
    map->format = format;
    map->length = length;
    map->next = *mpp;
    *mpp = map;
out:
    map->formatter = formatter;
    return JS_TRUE;
}

JS_PUBLIC_API(void)
JS_RemoveArgumentFormatter(JSContext *cx, const char *format)
{
    size_t length;
    JSArgumentFormatMap **mpp, *map;

    length = strlen(format);
    mpp = &cx->argumentFormatMap;
    while ((map = *mpp) != NULL) {
        if (map->length == length && !strcmp(map->format, format)) {
            *mpp = map->next;
            cx->free(map);
            return;
        }
        mpp = &map->next;
    }
}

JS_PUBLIC_API(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp)
{
    JSBool ok;
    JSObject *obj;
    JSString *str;
    jsdouble d, *dp;

    CHECK_REQUEST(cx);
    switch (type) {
      case JSTYPE_VOID:
        *vp = JSVAL_VOID;
        ok = JS_TRUE;
        break;
      case JSTYPE_OBJECT:
        ok = js_ValueToObject(cx, v, &obj);
        if (ok)
            *vp = OBJECT_TO_JSVAL(obj);
        break;
      case JSTYPE_FUNCTION:
        *vp = v;
        obj = js_ValueToFunctionObject(cx, vp, JSV2F_SEARCH_STACK);
        ok = (obj != NULL);
        break;
      case JSTYPE_STRING:
        str = js_ValueToString(cx, v);
        ok = (str != NULL);
        if (ok)
            *vp = STRING_TO_JSVAL(str);
        break;
      case JSTYPE_NUMBER:
        ok = JS_ValueToNumber(cx, v, &d);
        if (ok) {
            dp = js_NewWeaklyRootedDouble(cx, d);
            ok = (dp != NULL);
            if (ok)
                *vp = DOUBLE_TO_JSVAL(dp);
        }
        break;
      case JSTYPE_BOOLEAN:
        *vp = BOOLEAN_TO_JSVAL(js_ValueToBoolean(v));
        return JS_TRUE;
      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", (int)type);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_TYPE,
                             numBuf);
        ok = JS_FALSE;
        break;
      }
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    CHECK_REQUEST(cx);
    return js_ValueToObject(cx, v, objp);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToFunction(cx, &v, JSV2F_SEARCH_STACK);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToFunction(cx, &v, JSV2F_SEARCH_STACK);
}

JS_PUBLIC_API(JSString *)
JS_ValueToString(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToString(cx, v);
}

JS_PUBLIC_API(JSString *)
JS_ValueToSource(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ValueToSource(cx, v);
}

JS_PUBLIC_API(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    CHECK_REQUEST(cx);

    AutoValueRooter tvr(cx, v);
    *dp = js_ValueToNumber(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

JS_PUBLIC_API(JSBool)
JS_DoubleIsInt32(jsdouble d, jsint *ip)
{
    return JSDOUBLE_IS_INT(d, *ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);

    AutoValueRooter tvr(cx, v);
    *ip = js_ValueToECMAInt32(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip)
{
    CHECK_REQUEST(cx);

    AutoValueRooter tvr(cx, v);
    *ip = js_ValueToECMAUint32(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

JS_PUBLIC_API(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);

    AutoValueRooter tvr(cx, v);
    *ip = js_ValueToInt32(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

JS_PUBLIC_API(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    CHECK_REQUEST(cx);

    AutoValueRooter tvr(cx, v);
    *ip = js_ValueToUint16(cx, tvr.addr());
    return !JSVAL_IS_NULL(tvr.value());
}

JS_PUBLIC_API(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
    CHECK_REQUEST(cx);
    *bp = js_ValueToBoolean(v);
    return JS_TRUE;
}

JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, jsval v)
{
    JSType type;
    JSObject *obj;

    CHECK_REQUEST(cx);
    if (JSVAL_IS_OBJECT(v)) {
        obj = JSVAL_TO_OBJECT(v);
        if (obj)
            return obj->map->ops->typeOf(cx, obj);
        type = JSTYPE_OBJECT;
    } else if (JSVAL_IS_NUMBER(v)) {
        type = JSTYPE_NUMBER;
    } else if (JSVAL_IS_STRING(v)) {
        type = JSTYPE_STRING;
    } else if (JSVAL_IS_BOOLEAN(v)) {
        type = JSTYPE_BOOLEAN;
    } else {
        type = JSTYPE_VOID;
    }
    return type;
}

JS_PUBLIC_API(const char *)
JS_GetTypeName(JSContext *cx, JSType type)
{
    if ((uintN)type >= (uintN)JSTYPE_LIMIT)
        return NULL;
    return JS_TYPE_STR(type);
}

JS_PUBLIC_API(JSBool)
JS_StrictlyEqual(JSContext *cx, jsval v1, jsval v2)
{
    return js_StrictlyEqual(cx, v1, v2);
}

JS_PUBLIC_API(JSBool)
JS_SameValue(JSContext *cx, jsval v1, jsval v2)
{
    return js_SameValue(v1, v2, cx);
}

/************************************************************************/

/*
 * Has a new runtime ever been created?  This flag is used to detect unsafe
 * changes to js_CStringsAreUTF8 after a runtime has been created, and to
 * ensure that "first checks" on runtime creation are run only once.
 */
#ifdef DEBUG
static JSBool js_NewRuntimeWasCalled = JS_FALSE;
#endif

JSRuntime::JSRuntime()
{
    /* Initialize infallibly first, so we can goto bad and JS_DestroyRuntime. */
    JS_INIT_CLIST(&contextList);
    JS_INIT_CLIST(&trapList);
    JS_INIT_CLIST(&watchPointList);
}

bool
JSRuntime::init(uint32 maxbytes)
{
    if (!js_InitGC(this, maxbytes) || !js_InitAtomState(this))
        return false;

    deflatedStringCache = new js::DeflatedStringCache();
    if (!deflatedStringCache || !deflatedStringCache->init())
        return false;

#ifdef JS_THREADSAFE
    gcLock = JS_NEW_LOCK();
    if (!gcLock)
        return false;
    gcDone = JS_NEW_CONDVAR(gcLock);
    if (!gcDone)
        return false;
    requestDone = JS_NEW_CONDVAR(gcLock);
    if (!requestDone)
        return false;
    /* this is asymmetric with JS_ShutDown: */
    if (!js_SetupLocks(8, 16))
        return false;
    rtLock = JS_NEW_LOCK();
    if (!rtLock)
        return false;
    stateChange = JS_NEW_CONDVAR(gcLock);
    if (!stateChange)
        return false;
    titleSharingDone = JS_NEW_CONDVAR(gcLock);
    if (!titleSharingDone)
        return false;
    titleSharingTodo = NO_TITLE_SHARING_TODO;
    debuggerLock = JS_NEW_LOCK();
    if (!debuggerLock)
        return false;
    deallocatorThread = new JSBackgroundThread();
    if (!deallocatorThread || !deallocatorThread->init())
        return false;
#endif
    return propertyTree.init() && js_InitThreads(this);
}

JSRuntime::~JSRuntime()
{
#ifdef DEBUG
    /* Don't hurt everyone in leaky ol' Mozilla with a fatal JS_ASSERT! */
    if (!JS_CLIST_IS_EMPTY(&contextList)) {
        JSContext *cx, *iter = NULL;
        uintN cxcount = 0;
        while ((cx = js_ContextIterator(this, JS_TRUE, &iter)) != NULL) {
            fprintf(stderr,
"JS API usage error: found live context at %p\n",
                    (void *) cx);
            cxcount++;
        }
        fprintf(stderr,
"JS API usage error: %u context%s left in runtime upon JS_DestroyRuntime.\n",
                cxcount, (cxcount == 1) ? "" : "s");
    }
#endif

    js_FinishThreads(this);
    js_FreeRuntimeScriptState(this);
    js_FinishAtomState(this);

    /*
     * Finish the deflated string cache after the last GC and after
     * calling js_FinishAtomState, which finalizes strings.
     */
    delete deflatedStringCache;
    js_FinishGC(this);
#ifdef JS_THREADSAFE
    if (gcLock)
        JS_DESTROY_LOCK(gcLock);
    if (gcDone)
        JS_DESTROY_CONDVAR(gcDone);
    if (requestDone)
        JS_DESTROY_CONDVAR(requestDone);
    if (rtLock)
        JS_DESTROY_LOCK(rtLock);
    if (stateChange)
        JS_DESTROY_CONDVAR(stateChange);
    if (titleSharingDone)
        JS_DESTROY_CONDVAR(titleSharingDone);
    if (debuggerLock)
        JS_DESTROY_LOCK(debuggerLock);
    if (deallocatorThread) {
        deallocatorThread->cancel();
        delete deallocatorThread;
    }
#endif
    propertyTree.finish();
}


JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32 maxbytes)
{
#ifdef DEBUG
    if (!js_NewRuntimeWasCalled) {
        /*
         * This code asserts that the numbers associated with the error names
         * in jsmsg.def are monotonically increasing.  It uses values for the
         * error names enumerated in jscntxt.c.  It's not a compile-time check
         * but it's better than nothing.
         */
        int errorNumber = 0;
#define MSG_DEF(name, number, count, exception, format)                       \
    JS_ASSERT(name == errorNumber++);
#include "js.msg"
#undef MSG_DEF

#define MSG_DEF(name, number, count, exception, format)                       \
    JS_BEGIN_MACRO                                                            \
        uintN numfmtspecs = 0;                                                \
        const char *fmt;                                                      \
        for (fmt = format; *fmt != '\0'; fmt++) {                             \
            if (*fmt == '{' && isdigit(fmt[1]))                               \
                ++numfmtspecs;                                                \
        }                                                                     \
        JS_ASSERT(count == numfmtspecs);                                      \
    JS_END_MACRO;
#include "js.msg"
#undef MSG_DEF

        /*
         * If it were possible for pure inline function calls with constant
         * arguments to be computed at compile time, these would be static
         * assertions, but since it isn't, this is the best we can do.
         */
        JS_ASSERT(JSVAL_NULL == OBJECT_TO_JSVAL(NULL));
        JS_ASSERT(JSVAL_ZERO == INT_TO_JSVAL(0));
        JS_ASSERT(JSVAL_ONE == INT_TO_JSVAL(1));
        JS_ASSERT(JSVAL_FALSE == BOOLEAN_TO_JSVAL(JS_FALSE));
        JS_ASSERT(JSVAL_TRUE == BOOLEAN_TO_JSVAL(JS_TRUE));

        JS_ASSERT(JSVAL_TO_SPECIAL(JSVAL_VOID) == 2);
        JS_ASSERT(JSVAL_TO_SPECIAL(JSVAL_HOLE) == (2 | (JSVAL_HOLE_FLAG >> JSVAL_TAGBITS)));
        JS_ASSERT(JSVAL_TO_SPECIAL(JSVAL_ARETURN) == 8);

        js_NewRuntimeWasCalled = JS_TRUE;
    }
#endif /* DEBUG */

    void *mem = js_calloc(sizeof(JSRuntime));
    if (!mem)
        return NULL;

    JSRuntime *rt = new (mem) JSRuntime();
    if (!rt->init(maxbytes)) {
        JS_DestroyRuntime(rt);
        return NULL;
    }

    return rt;
}

JS_PUBLIC_API(void)
JS_CommenceRuntimeShutDown(JSRuntime *rt)
{
    rt->gcFlushCodeCaches = true;
}

JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt)
{
    rt->~JSRuntime();

    js_free(rt);
}

#ifdef JS_REPRMETER
namespace reprmeter {
    extern void js_DumpReprMeter();
}
#endif

JS_PUBLIC_API(void)
JS_ShutDown(void)
{
#ifdef MOZ_TRACEVIS
    StopTraceVis();
#endif

#ifdef JS_OPMETER
    extern void js_DumpOpMeters();
    js_DumpOpMeters();
#endif

#ifdef JS_REPRMETER
    reprmeter::js_DumpReprMeter();
#endif

#ifdef JS_THREADSAFE
    js_CleanupLocks();
#endif
    PRMJ_NowShutdown();
}

JS_PUBLIC_API(void *)
JS_GetRuntimePrivate(JSRuntime *rt)
{
    return rt->data;
}

JS_PUBLIC_API(void)
JS_SetRuntimePrivate(JSRuntime *rt, void *data)
{
    rt->data = data;
}

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
    if (!cx->requestDepth) {
        JSRuntime *rt = cx->runtime;
        JS_LOCK_GC(rt);

        /* Wait until the GC is finished. */
        if (rt->gcThread != cx->thread) {
            while (rt->gcLevel > 0)
                JS_AWAIT_GC_DONE(rt);
        }

        /* Indicate that a request is running. */
        rt->requestCount++;
        cx->requestDepth = 1;
        cx->outstandingRequests++;
        JS_UNLOCK_GC(rt);
        return;
    }
    cx->requestDepth++;
    cx->outstandingRequests++;
#endif
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JSRuntime *rt;

    CHECK_REQUEST(cx);
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
    JS_ASSERT(cx->requestDepth > 0);
    JS_ASSERT(cx->outstandingRequests > 0);
    if (cx->requestDepth == 1) {
        LeaveTrace(cx);  /* for GC safety */

        /* Lock before clearing to interlock with ClaimScope, in jslock.c. */
        rt = cx->runtime;
        JS_LOCK_GC(rt);
        cx->requestDepth = 0;
        cx->outstandingRequests--;

        js_ShareWaitingTitles(cx);

        /* Give the GC a chance to run if this was the last request running. */
        JS_ASSERT(rt->requestCount > 0);
        rt->requestCount--;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);

        JS_UNLOCK_GC(rt);
        return;
    }

    cx->requestDepth--;
    cx->outstandingRequests--;
#endif
}

/* Yield to pending GC operations, regardless of request depth */
JS_PUBLIC_API(void)
JS_YieldRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread);
    CHECK_REQUEST(cx);
    JS_ResumeRequest(cx, JS_SuspendRequest(cx));
#endif
}

JS_PUBLIC_API(jsrefcount)
JS_SuspendRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    jsrefcount saveDepth = cx->requestDepth;

    while (cx->requestDepth) {
        cx->outstandingRequests++;  /* compensate for JS_EndRequest */
        JS_EndRequest(cx);
    }
    return saveDepth;
#else
    return 0;
#endif
}

JS_PUBLIC_API(void)
JS_ResumeRequest(JSContext *cx, jsrefcount saveDepth)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(!cx->requestDepth);
    while (--saveDepth >= 0) {
        JS_BeginRequest(cx);
        cx->outstandingRequests--;  /* compensate for JS_BeginRequest */
    }
#endif
}

JS_PUBLIC_API(void)
JS_TransferRequest(JSContext *cx, JSContext *another)
{
    JS_ASSERT(cx != another);
    JS_ASSERT(cx->runtime == another->runtime);
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread);
    JS_ASSERT(another->thread);
    JS_ASSERT(cx->thread == another->thread);
    JS_ASSERT(cx->requestDepth != 0);
    JS_ASSERT(another->requestDepth == 0);

    /* Serialize access to JSContext::requestDepth from other threads. */
    JS_LOCK_GC(cx->runtime);
    another->requestDepth = cx->requestDepth;
    cx->requestDepth = 0;
    JS_UNLOCK_GC(cx->runtime);
#endif
}

JS_PUBLIC_API(void)
JS_Lock(JSRuntime *rt)
{
    JS_LOCK_RUNTIME(rt);
}

JS_PUBLIC_API(void)
JS_Unlock(JSRuntime *rt)
{
    JS_UNLOCK_RUNTIME(rt);
}

JS_PUBLIC_API(JSContextCallback)
JS_SetContextCallback(JSRuntime *rt, JSContextCallback cxCallback)
{
    JSContextCallback old;

    old = rt->cxCallback;
    rt->cxCallback = cxCallback;
    return old;
}

JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    return js_NewContext(rt, stackChunkSize);
}

JS_PUBLIC_API(void)
JS_DestroyContext(JSContext *cx)
{
    js_DestroyContext(cx, JSDCM_FORCE_GC);
}

JS_PUBLIC_API(void)
JS_DestroyContextNoGC(JSContext *cx)
{
    js_DestroyContext(cx, JSDCM_NO_GC);
}

JS_PUBLIC_API(void)
JS_DestroyContextMaybeGC(JSContext *cx)
{
    js_DestroyContext(cx, JSDCM_MAYBE_GC);
}

JS_PUBLIC_API(void *)
JS_GetContextPrivate(JSContext *cx)
{
    return cx->data;
}

JS_PUBLIC_API(void)
JS_SetContextPrivate(JSContext *cx, void *data)
{
    cx->data = data;
}

JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    return cx->runtime;
}

JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    return js_ContextIterator(rt, JS_TRUE, iterp);
}

JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx)
{
    return JSVERSION_NUMBER(cx);
}

JS_PUBLIC_API(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version)
{
    JSVersion oldVersion;

    JS_ASSERT(version != JSVERSION_UNKNOWN);
    JS_ASSERT((version & ~JSVERSION_MASK) == 0);

    oldVersion = JSVERSION_NUMBER(cx);
    if (version == oldVersion)
        return oldVersion;

    /* We no longer support 1.4 or below. */
    if (version != JSVERSION_DEFAULT && version <= JSVERSION_1_4)
        return oldVersion;

    cx->version = (cx->version & ~JSVERSION_MASK) | version;
    js_OnVersionChange(cx);
    return oldVersion;
}

static struct v2smap {
    JSVersion   version;
    const char  *string;
} v2smap[] = {
    {JSVERSION_1_0,     "1.0"},
    {JSVERSION_1_1,     "1.1"},
    {JSVERSION_1_2,     "1.2"},
    {JSVERSION_1_3,     "1.3"},
    {JSVERSION_1_4,     "1.4"},
    {JSVERSION_ECMA_3,  "ECMAv3"},
    {JSVERSION_1_5,     "1.5"},
    {JSVERSION_1_6,     "1.6"},
    {JSVERSION_1_7,     "1.7"},
    {JSVERSION_1_8,     "1.8"},
    {JSVERSION_ECMA_5,  "ECMAv5"},
    {JSVERSION_DEFAULT, js_default_str},
    {JSVERSION_UNKNOWN, NULL},          /* must be last, NULL is sentinel */
};

JS_PUBLIC_API(const char *)
JS_VersionToString(JSVersion version)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (v2smap[i].version == version)
            return v2smap[i].string;
    return "unknown";
}

JS_PUBLIC_API(JSVersion)
JS_StringToVersion(const char *string)
{
    int i;

    for (i = 0; v2smap[i].string; i++)
        if (strcmp(v2smap[i].string, string) == 0)
            return v2smap[i].version;
    return JSVERSION_UNKNOWN;
}

JS_PUBLIC_API(uint32)
JS_GetOptions(JSContext *cx)
{
    return cx->options;
}

JS_PUBLIC_API(uint32)
JS_SetOptions(JSContext *cx, uint32 options)
{
    JS_LOCK_GC(cx->runtime);
    uint32 oldopts = cx->options;
    cx->options = options;
    js_SyncOptionsToVersion(cx);
    cx->updateJITEnabled();
    JS_UNLOCK_GC(cx->runtime);
    return oldopts;
}

JS_PUBLIC_API(uint32)
JS_ToggleOptions(JSContext *cx, uint32 options)
{
    JS_LOCK_GC(cx->runtime);
    uint32 oldopts = cx->options;
    cx->options ^= options;
    js_SyncOptionsToVersion(cx);
    cx->updateJITEnabled();
    JS_UNLOCK_GC(cx->runtime);
    return oldopts;
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "JavaScript-C 1.8.0 pre-release 1 2007-10-03";
}


JS_PUBLIC_API(JSObject *)
JS_GetGlobalObject(JSContext *cx)
{
    return cx->globalObject;
}

JS_PUBLIC_API(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    cx->globalObject = obj;

#if JS_HAS_XML_SUPPORT
    cx->xmlSettingFlags = 0;
#endif
}

JS_BEGIN_EXTERN_C

JSObject *
js_InitFunctionAndObjectClasses(JSContext *cx, JSObject *obj)
{
    JSDHashTable *table;
    JSBool resolving;
    JSRuntime *rt;
    JSResolvingKey key;
    JSResolvingEntry *entry;
    JSObject *fun_proto, *obj_proto;

    /* If cx has no global object, use obj so prototypes can be found. */
    if (!cx->globalObject)
        JS_SetGlobalObject(cx, obj);

    /* Record Function and Object in cx->resolvingTable, if we are resolving. */
    table = cx->resolvingTable;
    resolving = (table && table->entryCount);
    rt = cx->runtime;
    key.obj = obj;
    if (resolving) {
        key.id = ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Function]);
        entry = (JSResolvingEntry *)
                JS_DHashTableOperate(table, &key, JS_DHASH_ADD);
        if (entry && entry->key.obj && (entry->flags & JSRESFLAG_LOOKUP)) {
            /* Already resolving Function, record Object too. */
            JS_ASSERT(entry->key.obj == obj);
            key.id = ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Object]);
            entry = (JSResolvingEntry *)
                    JS_DHashTableOperate(table, &key, JS_DHASH_ADD);
        }
        if (!entry) {
            JS_ReportOutOfMemory(cx);
            return NULL;
        }
        JS_ASSERT(!entry->key.obj && entry->flags == 0);
        entry->key = key;
        entry->flags = JSRESFLAG_LOOKUP;
    } else {
        key.id = ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Object]);
        if (!js_StartResolving(cx, &key, JSRESFLAG_LOOKUP, &entry))
            return NULL;

        key.id = ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Function]);
        if (!js_StartResolving(cx, &key, JSRESFLAG_LOOKUP, &entry)) {
            key.id = ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Object]);
            JS_DHashTableOperate(table, &key, JS_DHASH_REMOVE);
            return NULL;
        }

        table = cx->resolvingTable;
    }

    /* Initialize the function class first so constructors can be made. */
    if (!js_GetClassPrototype(cx, obj, JSProto_Function, &fun_proto)) {
        fun_proto = NULL;
        goto out;
    }
    if (!fun_proto) {
        fun_proto = js_InitFunctionClass(cx, obj);
        if (!fun_proto)
            goto out;
    } else {
        JSObject *ctor;

        ctor = JS_GetConstructor(cx, fun_proto);
        if (!ctor) {
            fun_proto = NULL;
            goto out;
        }
        obj->defineProperty(cx, ATOM_TO_JSID(CLASS_ATOM(cx, Function)),
                            OBJECT_TO_JSVAL(ctor), 0, 0, 0);
    }

    /* Initialize the object class next so Object.prototype works. */
    if (!js_GetClassPrototype(cx, obj, JSProto_Object, &obj_proto)) {
        fun_proto = NULL;
        goto out;
    }
    if (!obj_proto)
        obj_proto = js_InitObjectClass(cx, obj);
    if (!obj_proto) {
        fun_proto = NULL;
        goto out;
    }

    /* Function.prototype and the global object delegate to Object.prototype. */
    fun_proto->setProto(obj_proto);
    if (!obj->getProto())
        obj->setProto(obj_proto);

out:
    /* If resolving, remove the other entry (Object or Function) from table. */
    JS_DHashTableOperate(table, &key, JS_DHASH_REMOVE);
    if (!resolving) {
        /* If not resolving, remove the first entry added above, for Object. */
        JS_ASSERT(key.id ==                                                   \
                  ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Function]));
        key.id = ATOM_TO_JSID(rt->atomState.classAtoms[JSProto_Object]);
        JS_DHashTableOperate(table, &key, JS_DHASH_REMOVE);
    }
    return fun_proto;
}

JS_END_EXTERN_C

JS_PUBLIC_API(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);

    /* Define a top-level property 'undefined' with the undefined value. */
    atom = cx->runtime->atomState.typeAtoms[JSTYPE_VOID];
    if (!obj->defineProperty(cx, ATOM_TO_JSID(atom), JSVAL_VOID,
                             JS_PropertyStub, JS_PropertyStub,
                             JSPROP_PERMANENT | JSPROP_READONLY)) {
        return JS_FALSE;
    }

    /* Function and Object require cooperative bootstrapping magic. */
    if (!js_InitFunctionAndObjectClasses(cx, obj))
        return JS_FALSE;

    /* Initialize the rest of the standard objects and functions. */
    return js_InitArrayClass(cx, obj) &&
           js_InitBooleanClass(cx, obj) &&
           js_InitExceptionClasses(cx, obj) &&
           js_InitMathClass(cx, obj) &&
           js_InitNumberClass(cx, obj) &&
           js_InitJSONClass(cx, obj) &&
           js_InitRegExpClass(cx, obj) &&
           js_InitStringClass(cx, obj) &&
           js_InitEval(cx, obj) &&
           js_InitTypedArrayClasses(cx, obj) &&
#if JS_HAS_XML_SUPPORT
           js_InitXMLClasses(cx, obj) &&
#endif
#if JS_HAS_GENERATORS
           js_InitIteratorClasses(cx, obj) &&
#endif
           js_InitDateClass(cx, obj);
}

#define CLASP(name)                 (&js_##name##Class)
#define XCLASP(name)                (&js_##name##Class.base)
#define EAGER_ATOM(name)            ATOM_OFFSET(name), NULL
#define EAGER_CLASS_ATOM(name)      CLASS_ATOM_OFFSET(name), NULL
#define EAGER_ATOM_AND_CLASP(name)  EAGER_CLASS_ATOM(name), CLASP(name)
#define EAGER_ATOM_AND_XCLASP(name) EAGER_CLASS_ATOM(name), XCLASP(name)
#define LAZY_ATOM(name)             ATOM_OFFSET(lazy.name), js_##name##_str

typedef struct JSStdName {
    JSObjectOp  init;
    size_t      atomOffset;     /* offset of atom pointer in JSAtomState */
    const char  *name;          /* null if atom is pre-pinned, else name */
    JSClass     *clasp;
} JSStdName;

static JSAtom *
StdNameToAtom(JSContext *cx, JSStdName *stdn)
{
    size_t offset;
    JSAtom *atom;
    const char *name;

    offset = stdn->atomOffset;
    atom = OFFSET_TO_ATOM(cx->runtime, offset);
    if (!atom) {
        name = stdn->name;
        if (name) {
            atom = js_Atomize(cx, name, strlen(name), ATOM_PINNED);
            OFFSET_TO_ATOM(cx->runtime, offset) = atom;
        }
    }
    return atom;
}

/*
 * Table of class initializers and their atom offsets in rt->atomState.
 * If you add a "standard" class, remember to update this table.
 */
static JSStdName standard_class_atoms[] = {
    {js_InitFunctionAndObjectClasses,   EAGER_ATOM_AND_CLASP(Function)},
    {js_InitFunctionAndObjectClasses,   EAGER_ATOM_AND_CLASP(Object)},
    {js_InitArrayClass,                 EAGER_ATOM_AND_CLASP(Array)},
    {js_InitBooleanClass,               EAGER_ATOM_AND_CLASP(Boolean)},
    {js_InitDateClass,                  EAGER_ATOM_AND_CLASP(Date)},
    {js_InitMathClass,                  EAGER_ATOM_AND_CLASP(Math)},
    {js_InitNumberClass,                EAGER_ATOM_AND_CLASP(Number)},
    {js_InitStringClass,                EAGER_ATOM_AND_CLASP(String)},
    {js_InitExceptionClasses,           EAGER_ATOM_AND_CLASP(Error)},
    {js_InitRegExpClass,                EAGER_ATOM_AND_CLASP(RegExp)},
#if JS_HAS_XML_SUPPORT
    {js_InitXMLClass,                   EAGER_ATOM_AND_CLASP(XML)},
    {js_InitNamespaceClass,             EAGER_ATOM_AND_XCLASP(Namespace)},
    {js_InitQNameClass,                 EAGER_ATOM_AND_XCLASP(QName)},
#endif
#if JS_HAS_GENERATORS
    {js_InitIteratorClasses,            EAGER_ATOM_AND_CLASP(StopIteration)},
#endif
    {js_InitJSONClass,                  EAGER_ATOM_AND_CLASP(JSON)},
    {js_InitTypedArrayClasses,          EAGER_CLASS_ATOM(ArrayBuffer), &js::ArrayBuffer::jsclass},
    {NULL,                              0, NULL, NULL}
};

/*
 * Table of top-level function and constant names and their init functions.
 * If you add a "standard" global function or property, remember to update
 * this table.
 */
static JSStdName standard_class_names[] = {
    /* ECMA requires that eval be a direct property of the global object. */
    {js_InitEval,               EAGER_ATOM(eval), NULL},

    /* Global properties and functions defined by the Number class. */
    {js_InitNumberClass,        LAZY_ATOM(NaN), NULL},
    {js_InitNumberClass,        LAZY_ATOM(Infinity), NULL},
    {js_InitNumberClass,        LAZY_ATOM(isNaN), NULL},
    {js_InitNumberClass,        LAZY_ATOM(isFinite), NULL},
    {js_InitNumberClass,        LAZY_ATOM(parseFloat), NULL},
    {js_InitNumberClass,        LAZY_ATOM(parseInt), NULL},

    /* String global functions. */
    {js_InitStringClass,        LAZY_ATOM(escape), NULL},
    {js_InitStringClass,        LAZY_ATOM(unescape), NULL},
    {js_InitStringClass,        LAZY_ATOM(decodeURI), NULL},
    {js_InitStringClass,        LAZY_ATOM(encodeURI), NULL},
    {js_InitStringClass,        LAZY_ATOM(decodeURIComponent), NULL},
    {js_InitStringClass,        LAZY_ATOM(encodeURIComponent), NULL},
#if JS_HAS_UNEVAL
    {js_InitStringClass,        LAZY_ATOM(uneval), NULL},
#endif

    /* Exception constructors. */
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(Error), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(InternalError), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(EvalError), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(RangeError), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(ReferenceError), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(SyntaxError), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(TypeError), CLASP(Error)},
    {js_InitExceptionClasses,   EAGER_CLASS_ATOM(URIError), CLASP(Error)},

#if JS_HAS_XML_SUPPORT
    {js_InitAnyNameClass,       EAGER_ATOM_AND_CLASP(AnyName)},
    {js_InitAttributeNameClass, EAGER_ATOM_AND_CLASP(AttributeName)},
    {js_InitXMLClass,           LAZY_ATOM(XMLList), &js_XMLClass},
    {js_InitXMLClass,           LAZY_ATOM(isXMLName), NULL},
#endif

#if JS_HAS_GENERATORS
    {js_InitIteratorClasses,    EAGER_ATOM_AND_CLASP(Iterator)},
    {js_InitIteratorClasses,    EAGER_ATOM_AND_CLASP(Generator)},
#endif

    /* Typed Arrays */
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(ArrayBuffer), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Int8Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint8Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Int16Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint16Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Int32Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint32Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Float32Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Float64Array), NULL},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint8ClampedArray), NULL},

    {NULL,                      0, NULL, NULL}
};

static JSStdName object_prototype_names[] = {
    /* Object.prototype properties (global delegates to Object.prototype). */
    {js_InitObjectClass,        EAGER_ATOM(proto), NULL},
    {js_InitObjectClass,        EAGER_ATOM(parent), NULL},
#if JS_HAS_TOSOURCE
    {js_InitObjectClass,        EAGER_ATOM(toSource), NULL},
#endif
    {js_InitObjectClass,        EAGER_ATOM(toString), NULL},
    {js_InitObjectClass,        EAGER_ATOM(toLocaleString), NULL},
    {js_InitObjectClass,        EAGER_ATOM(valueOf), NULL},
#if JS_HAS_OBJ_WATCHPOINT
    {js_InitObjectClass,        LAZY_ATOM(watch), NULL},
    {js_InitObjectClass,        LAZY_ATOM(unwatch), NULL},
#endif
    {js_InitObjectClass,        LAZY_ATOM(hasOwnProperty), NULL},
    {js_InitObjectClass,        LAZY_ATOM(isPrototypeOf), NULL},
    {js_InitObjectClass,        LAZY_ATOM(propertyIsEnumerable), NULL},
#if JS_HAS_GETTER_SETTER
    {js_InitObjectClass,        LAZY_ATOM(defineGetter), NULL},
    {js_InitObjectClass,        LAZY_ATOM(defineSetter), NULL},
    {js_InitObjectClass,        LAZY_ATOM(lookupGetter), NULL},
    {js_InitObjectClass,        LAZY_ATOM(lookupSetter), NULL},
#endif

    {NULL,                      0, NULL, NULL}
};

JS_PUBLIC_API(JSBool)
JS_ResolveStandardClass(JSContext *cx, JSObject *obj, jsval id,
                        JSBool *resolved)
{
    JSString *idstr;
    JSRuntime *rt;
    JSAtom *atom;
    JSStdName *stdnm;
    uintN i;

    CHECK_REQUEST(cx);
    *resolved = JS_FALSE;

    rt = cx->runtime;
    JS_ASSERT(rt->state != JSRTS_DOWN);
    if (rt->state == JSRTS_LANDING || !JSVAL_IS_STRING(id))
        return JS_TRUE;

    idstr = JSVAL_TO_STRING(id);

    /* Check whether we're resolving 'undefined', and define it if so. */
    atom = rt->atomState.typeAtoms[JSTYPE_VOID];
    if (idstr == ATOM_TO_STRING(atom)) {
        *resolved = JS_TRUE;
        return obj->defineProperty(cx, ATOM_TO_JSID(atom), JSVAL_VOID,
                                   JS_PropertyStub, JS_PropertyStub,
                                   JSPROP_PERMANENT | JSPROP_READONLY);
    }

    /* Try for class constructors/prototypes named by well-known atoms. */
    stdnm = NULL;
    for (i = 0; standard_class_atoms[i].init; i++) {
        atom = OFFSET_TO_ATOM(rt, standard_class_atoms[i].atomOffset);
        if (idstr == ATOM_TO_STRING(atom)) {
            stdnm = &standard_class_atoms[i];
            break;
        }
    }

    if (!stdnm) {
        /* Try less frequently used top-level functions and constants. */
        for (i = 0; standard_class_names[i].init; i++) {
            atom = StdNameToAtom(cx, &standard_class_names[i]);
            if (!atom)
                return JS_FALSE;
            if (idstr == ATOM_TO_STRING(atom)) {
                stdnm = &standard_class_names[i];
                break;
            }
        }

        if (!stdnm && !obj->getProto()) {
            /*
             * Try even less frequently used names delegated from the global
             * object to Object.prototype, but only if the Object class hasn't
             * yet been initialized.
             */
            for (i = 0; object_prototype_names[i].init; i++) {
                atom = StdNameToAtom(cx, &object_prototype_names[i]);
                if (!atom)
                    return JS_FALSE;
                if (idstr == ATOM_TO_STRING(atom)) {
                    stdnm = &standard_class_names[i];
                    break;
                }
            }
        }
    }

    if (stdnm) {
        /*
         * If this standard class is anonymous and obj advertises itself as a
         * global object (in order to reserve slots for standard class object
         * pointers), then we don't want to resolve by name.
         *
         * If inversely, either id does not name a class, or id does not name
         * an anonymous class, or the global does not reserve slots for class
         * objects, then we must call the init hook here.
         */
        if (stdnm->clasp &&
            (stdnm->clasp->flags & JSCLASS_IS_ANONYMOUS) &&
            (OBJ_GET_CLASS(cx, obj)->flags & JSCLASS_IS_GLOBAL)) {
            return JS_TRUE;
        }

        if (!stdnm->init(cx, obj))
            return JS_FALSE;
        *resolved = JS_TRUE;
    }
    return JS_TRUE;
}

static JSBool
AlreadyHasOwnProperty(JSContext *cx, JSObject *obj, JSAtom *atom)
{
    JS_LOCK_OBJ(cx, obj);
    JSScope *scope = OBJ_SCOPE(obj);
    bool found = scope->hasProperty(ATOM_TO_JSID(atom));
    JS_UNLOCK_SCOPE(cx, scope);
    return found;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStandardClasses(JSContext *cx, JSObject *obj)
{
    JSRuntime *rt;
    JSAtom *atom;
    uintN i;

    CHECK_REQUEST(cx);
    rt = cx->runtime;

    /* Check whether we need to bind 'undefined' and define it if so. */
    atom = rt->atomState.typeAtoms[JSTYPE_VOID];
    if (!AlreadyHasOwnProperty(cx, obj, atom) &&
        !obj->defineProperty(cx, ATOM_TO_JSID(atom), JSVAL_VOID,
                             JS_PropertyStub, JS_PropertyStub,
                             JSPROP_PERMANENT | JSPROP_READONLY)) {
        return JS_FALSE;
    }

    /* Initialize any classes that have not been resolved yet. */
    for (i = 0; standard_class_atoms[i].init; i++) {
        atom = OFFSET_TO_ATOM(rt, standard_class_atoms[i].atomOffset);
        if (!AlreadyHasOwnProperty(cx, obj, atom) &&
            !standard_class_atoms[i].init(cx, obj)) {
            return JS_FALSE;
        }
    }

    return JS_TRUE;
}

static JSIdArray *
NewIdArray(JSContext *cx, jsint length)
{
    JSIdArray *ida;

    ida = (JSIdArray *)
        cx->malloc(offsetof(JSIdArray, vector) + length * sizeof(jsval));
    if (ida)
        ida->length = length;
    return ida;
}

/*
 * Unlike realloc(3), this function frees ida on failure.
 */
static JSIdArray *
SetIdArrayLength(JSContext *cx, JSIdArray *ida, jsint length)
{
    JSIdArray *rida;

    rida = (JSIdArray *)
           JS_realloc(cx, ida,
                      offsetof(JSIdArray, vector) + length * sizeof(jsval));
    if (!rida)
        JS_DestroyIdArray(cx, ida);
    else
        rida->length = length;
    return rida;
}

static JSIdArray *
AddAtomToArray(JSContext *cx, JSAtom *atom, JSIdArray *ida, jsint *ip)
{
    jsint i, length;

    i = *ip;
    length = ida->length;
    if (i >= length) {
        ida = SetIdArrayLength(cx, ida, JS_MAX(length * 2, 8));
        if (!ida)
            return NULL;
        JS_ASSERT(i < ida->length);
    }
    ida->vector[i] = ATOM_TO_JSID(atom);
    *ip = i + 1;
    return ida;
}

static JSIdArray *
EnumerateIfResolved(JSContext *cx, JSObject *obj, JSAtom *atom, JSIdArray *ida,
                    jsint *ip, JSBool *foundp)
{
    *foundp = AlreadyHasOwnProperty(cx, obj, atom);
    if (*foundp)
        ida = AddAtomToArray(cx, atom, ida, ip);
    return ida;
}

JS_PUBLIC_API(JSIdArray *)
JS_EnumerateResolvedStandardClasses(JSContext *cx, JSObject *obj,
                                    JSIdArray *ida)
{
    JSRuntime *rt;
    jsint i, j, k;
    JSAtom *atom;
    JSBool found;
    JSObjectOp init;

    CHECK_REQUEST(cx);
    rt = cx->runtime;
    if (ida) {
        i = ida->length;
    } else {
        ida = NewIdArray(cx, 8);
        if (!ida)
            return NULL;
        i = 0;
    }

    /* Check whether 'undefined' has been resolved and enumerate it if so. */
    atom = rt->atomState.typeAtoms[JSTYPE_VOID];
    ida = EnumerateIfResolved(cx, obj, atom, ida, &i, &found);
    if (!ida)
        return NULL;

    /* Enumerate only classes that *have* been resolved. */
    for (j = 0; standard_class_atoms[j].init; j++) {
        atom = OFFSET_TO_ATOM(rt, standard_class_atoms[j].atomOffset);
        ida = EnumerateIfResolved(cx, obj, atom, ida, &i, &found);
        if (!ida)
            return NULL;

        if (found) {
            init = standard_class_atoms[j].init;

            for (k = 0; standard_class_names[k].init; k++) {
                if (standard_class_names[k].init == init) {
                    atom = StdNameToAtom(cx, &standard_class_names[k]);
                    ida = AddAtomToArray(cx, atom, ida, &i);
                    if (!ida)
                        return NULL;
                }
            }

            if (init == js_InitObjectClass) {
                for (k = 0; object_prototype_names[k].init; k++) {
                    atom = StdNameToAtom(cx, &object_prototype_names[k]);
                    ida = AddAtomToArray(cx, atom, ida, &i);
                    if (!ida)
                        return NULL;
                }
            }
        }
    }

    /* Trim to exact length. */
    return SetIdArrayLength(cx, ida, i);
}

#undef CLASP
#undef EAGER_ATOM
#undef EAGER_CLASS_ATOM
#undef EAGER_ATOM_CLASP
#undef LAZY_ATOM

JS_PUBLIC_API(JSBool)
JS_GetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key,
                  JSObject **objp)
{
    CHECK_REQUEST(cx);
    return js_GetClassObject(cx, obj, key, objp);
}

JS_PUBLIC_API(JSObject *)
JS_GetScopeChain(JSContext *cx)
{
    JSStackFrame *fp;

    CHECK_REQUEST(cx);
    fp = js_GetTopStackFrame(cx);
    if (!fp) {
        /*
         * There is no code active on this context. In place of an actual
         * scope chain, use the context's global object, which is set in
         * js_InitFunctionAndObjectClasses, and which represents the default
         * scope chain for the embedding. See also js_FindClassObject.
         *
         * For embeddings that use the inner and outer object hooks, the inner
         * object represents the ultimate global object, with the outer object
         * acting as a stand-in.
         */
        JSObject *obj = cx->globalObject;
        if (!obj) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INACTIVE);
            return NULL;
        }

        OBJ_TO_INNER_OBJECT(cx, obj);
        return obj;
    }
    return js_GetScopeChain(cx, fp);
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForObject(JSContext *cx, JSObject *obj)
{
    return obj->getGlobal();
}

JS_PUBLIC_API(jsval)
JS_ComputeThis(JSContext *cx, jsval *vp)
{
    if (!js_ComputeThis(cx, vp + 2))
        return JSVAL_NULL;
    return vp[1];
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    return cx->malloc(nbytes);
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
    return cx->realloc(p, nbytes);
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
    return cx->free(p);
}

JS_PUBLIC_API(void)
JS_updateMallocCounter(JSContext *cx, size_t nbytes)
{
    return cx->updateMallocCounter(nbytes);
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    size_t n;
    void *p;

    n = strlen(s) + 1;
    p = cx->malloc(n);
    if (!p)
        return NULL;
    return (char *)memcpy(p, s, n);
}

JS_PUBLIC_API(jsdouble *)
JS_NewDouble(JSContext *cx, jsdouble d)
{
    CHECK_REQUEST(cx);
    return js_NewWeaklyRootedDouble(cx, d);
}

JS_PUBLIC_API(JSBool)
JS_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval)
{
    jsdouble *dp;

    CHECK_REQUEST(cx);
    dp = js_NewWeaklyRootedDouble(cx, d);
    if (!dp)
        return JS_FALSE;
    *rval = DOUBLE_TO_JSVAL(dp);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval)
{
    CHECK_REQUEST(cx);
    return js_NewWeaklyRootedNumber(cx, d, rval);
}

#undef JS_AddRoot
JS_PUBLIC_API(JSBool)
JS_AddRoot(JSContext *cx, void *rp)
{
    CHECK_REQUEST(cx);
    return js_AddRoot(cx, rp, NULL);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRootRT(JSRuntime *rt, void *rp, const char *name)
{
    return js_AddRootRT(rt, rp, name);
}

JS_PUBLIC_API(JSBool)
JS_RemoveRoot(JSContext *cx, void *rp)
{
    CHECK_REQUEST(cx);
    return js_RemoveRoot(cx->runtime, rp);
}

JS_PUBLIC_API(JSBool)
JS_RemoveRootRT(JSRuntime *rt, void *rp)
{
    return js_RemoveRoot(rt, rp);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRoot(JSContext *cx, void *rp, const char *name)
{
    CHECK_REQUEST(cx);
    return js_AddRoot(cx, rp, name);
}

JS_PUBLIC_API(void)
JS_ClearNewbornRoots(JSContext *cx)
{
    JS_CLEAR_WEAK_ROOTS(&cx->weakRoots);
}

JS_PUBLIC_API(JSBool)
JS_EnterLocalRootScope(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return js_EnterLocalRootScope(cx);
}

JS_PUBLIC_API(void)
JS_LeaveLocalRootScope(JSContext *cx)
{
    CHECK_REQUEST(cx);
    js_LeaveLocalRootScope(cx);
}

JS_PUBLIC_API(void)
JS_LeaveLocalRootScopeWithResult(JSContext *cx, jsval rval)
{
    CHECK_REQUEST(cx);
    js_LeaveLocalRootScopeWithResult(cx, rval);
}

JS_PUBLIC_API(void)
JS_ForgetLocalRoot(JSContext *cx, void *thing)
{
    CHECK_REQUEST(cx);
    js_ForgetLocalRoot(cx, (jsval) thing);
}

#ifdef DEBUG

JS_PUBLIC_API(void)
JS_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, void *data),
                  void *data)
{
    js_DumpNamedRoots(rt, dump, data);
}

#endif /* DEBUG */

JS_PUBLIC_API(uint32)
JS_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    return js_MapGCRoots(rt, map, data);
}

JS_PUBLIC_API(JSBool)
JS_LockGCThing(JSContext *cx, void *thing)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    ok = js_LockGCThingRT(cx->runtime, thing);
    if (!ok)
        JS_ReportOutOfMemory(cx);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_LockGCThingRT(JSRuntime *rt, void *thing)
{
    return js_LockGCThingRT(rt, thing);
}

JS_PUBLIC_API(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing)
{
    CHECK_REQUEST(cx);
    js_UnlockGCThingRT(cx->runtime, thing);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_UnlockGCThingRT(JSRuntime *rt, void *thing)
{
    js_UnlockGCThingRT(rt, thing);
    return true;
}

JS_PUBLIC_API(void)
JS_SetExtraGCRoots(JSRuntime *rt, JSTraceDataOp traceOp, void *data)
{
    rt->gcExtraRootsTraceOp = traceOp;
    rt->gcExtraRootsData = data;
}

JS_PUBLIC_API(void)
JS_TraceRuntime(JSTracer *trc)
{
    JSBool allAtoms = trc->context->runtime->gcKeepAtoms != 0;

    LeaveTrace(trc->context);
    js_TraceRuntime(trc, allAtoms);
}

JS_PUBLIC_API(void)
JS_CallTracer(JSTracer *trc, void *thing, uint32 kind)
{
    js_CallGCMarker(trc, thing, kind);
}

#ifdef DEBUG

#ifdef HAVE_XPCONNECT
#include "dump_xpc.h"
#endif

JS_PUBLIC_API(void)
JS_PrintTraceThingInfo(char *buf, size_t bufsize, JSTracer *trc,
                       void *thing, uint32 kind, JSBool details)
{
    const char *name;
    size_t n;

    if (bufsize == 0)
        return;

    switch (kind) {
      case JSTRACE_OBJECT:
      {
        JSObject *obj = (JSObject *)thing;
        JSClass *clasp = obj->getClass();

        name = clasp->name;
#ifdef HAVE_XPCONNECT
        if (clasp->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS) {
            void *privateThing = obj->getPrivate();
            if (privateThing) {
                const char *xpcClassName = GetXPCObjectClassName(privateThing);
                if (xpcClassName)
                    name = xpcClassName;
            }
        }
#endif
        break;
      }

      case JSTRACE_STRING:
        name = ((JSString *)thing)->isDependent()
               ? "substring"
               : "string";
        break;

      case JSTRACE_DOUBLE:
        name = "double";
        break;

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        name = "xml";
        break;
#endif
      default:
        JS_ASSERT(0);
        return;
        break;
    }

    n = strlen(name);
    if (n > bufsize - 1)
        n = bufsize - 1;
    memcpy(buf, name, n + 1);
    buf += n;
    bufsize -= n;

    if (details && bufsize > 2) {
        *buf++ = ' ';
        bufsize--;

        switch (kind) {
          case JSTRACE_OBJECT:
          {
            JSObject  *obj = (JSObject *)thing;
            JSClass *clasp = obj->getClass();
            if (clasp == &js_FunctionClass) {
                JSFunction *fun = GET_FUNCTION_PRIVATE(trc->context, obj);
                if (!fun) {
                    JS_snprintf(buf, bufsize, "<newborn>");
                } else if (FUN_OBJECT(fun) != obj) {
                    JS_snprintf(buf, bufsize, "%p", fun);
                } else {
                    if (fun->atom && ATOM_IS_STRING(fun->atom))
                        js_PutEscapedString(buf, bufsize,
                                            ATOM_TO_STRING(fun->atom), 0);
                }
            } else if (clasp->flags & JSCLASS_HAS_PRIVATE) {
                JS_snprintf(buf, bufsize, "%p", obj->getPrivate());
            } else {
                JS_snprintf(buf, bufsize, "<no private>");
            }
            break;
          }

          case JSTRACE_STRING:
            js_PutEscapedString(buf, bufsize, (JSString *)thing, 0);
            break;

          case JSTRACE_DOUBLE:
            JS_snprintf(buf, bufsize, "%g", *(jsdouble *)thing);
            break;

#if JS_HAS_XML_SUPPORT
          case JSTRACE_XML:
          {
            extern const char *js_xml_class_str[];
            JSXML *xml = (JSXML *)thing;

            JS_snprintf(buf, bufsize, "%s", js_xml_class_str[xml->xml_class]);
            break;
          }
#endif
          default:
            JS_ASSERT(0);
            break;
        }
    }
    buf[bufsize - 1] = '\0';
}

typedef struct JSHeapDumpNode JSHeapDumpNode;

struct JSHeapDumpNode {
    void            *thing;
    uint32          kind;
    JSHeapDumpNode  *next;          /* next sibling */
    JSHeapDumpNode  *parent;        /* node with the thing that refer to thing
                                       from this node */
    char            edgeName[1];    /* name of the edge from parent->thing
                                       into thing */
};

typedef struct JSDumpingTracer {
    JSTracer            base;
    JSDHashTable        visited;
    JSBool              ok;
    void                *startThing;
    void                *thingToFind;
    void                *thingToIgnore;
    JSHeapDumpNode      *parentNode;
    JSHeapDumpNode      **lastNodep;
    char                buffer[200];
} JSDumpingTracer;

static void
DumpNotify(JSTracer *trc, void *thing, uint32 kind)
{
    JSDumpingTracer *dtrc;
    JSContext *cx;
    JSDHashEntryStub *entry;
    JSHeapDumpNode *node;
    const char *edgeName;
    size_t edgeNameSize;

    JS_ASSERT(trc->callback == DumpNotify);
    dtrc = (JSDumpingTracer *)trc;

    if (!dtrc->ok || thing == dtrc->thingToIgnore)
        return;

    cx = trc->context;

    /*
     * Check if we have already seen thing unless it is thingToFind to include
     * it to the graph each time we reach it and print all live things that
     * refer to thingToFind.
     *
     * This does not print all possible paths leading to thingToFind since
     * when a thing A refers directly or indirectly to thingToFind and A is
     * present several times in the graph, we will print only the first path
     * leading to A and thingToFind, other ways to reach A will be ignored.
     */
    if (dtrc->thingToFind != thing) {
        /*
         * The startThing check allows to avoid putting startThing into the
         * hash table before tracing startThing in JS_DumpHeap.
         */
        if (thing == dtrc->startThing)
            return;
        entry = (JSDHashEntryStub *)
            JS_DHashTableOperate(&dtrc->visited, thing, JS_DHASH_ADD);
        if (!entry) {
            JS_ReportOutOfMemory(cx);
            dtrc->ok = JS_FALSE;
            return;
        }
        if (entry->key)
            return;
        entry->key = thing;
    }

    if (dtrc->base.debugPrinter) {
        dtrc->base.debugPrinter(trc, dtrc->buffer, sizeof(dtrc->buffer));
        edgeName = dtrc->buffer;
    } else if (dtrc->base.debugPrintIndex != (size_t)-1) {
        JS_snprintf(dtrc->buffer, sizeof(dtrc->buffer), "%s[%lu]",
                    (const char *)dtrc->base.debugPrintArg,
                    dtrc->base.debugPrintIndex);
        edgeName = dtrc->buffer;
    } else {
        edgeName = (const char*)dtrc->base.debugPrintArg;
    }

    edgeNameSize = strlen(edgeName) + 1;
    node = (JSHeapDumpNode *)
        cx->malloc(offsetof(JSHeapDumpNode, edgeName) + edgeNameSize);
    if (!node) {
        dtrc->ok = JS_FALSE;
        return;
    }

    node->thing = thing;
    node->kind = kind;
    node->next = NULL;
    node->parent = dtrc->parentNode;
    memcpy(node->edgeName, edgeName, edgeNameSize);

    JS_ASSERT(!*dtrc->lastNodep);
    *dtrc->lastNodep = node;
    dtrc->lastNodep = &node->next;
}

/* Dump node and the chain that leads to thing it contains. */
static JSBool
DumpNode(JSDumpingTracer *dtrc, FILE* fp, JSHeapDumpNode *node)
{
    JSHeapDumpNode *prev, *following;
    size_t chainLimit;
    JSBool ok;
    enum { MAX_PARENTS_TO_PRINT = 10 };

    JS_PrintTraceThingInfo(dtrc->buffer, sizeof dtrc->buffer,
                           &dtrc->base, node->thing, node->kind, JS_TRUE);
    if (fprintf(fp, "%p %-22s via ", node->thing, dtrc->buffer) < 0)
        return JS_FALSE;

    /*
     * We need to print the parent chain in the reverse order. To do it in
     * O(N) time where N is the chain length we first reverse the chain while
     * searching for the top and then print each node while restoring the
     * chain order.
     */
    chainLimit = MAX_PARENTS_TO_PRINT;
    prev = NULL;
    for (;;) {
        following = node->parent;
        node->parent = prev;
        prev = node;
        node = following;
        if (!node)
            break;
        if (chainLimit == 0) {
            if (fputs("...", fp) < 0)
                return JS_FALSE;
            break;
        }
        --chainLimit;
    }

    node = prev;
    prev = following;
    ok = JS_TRUE;
    do {
        /* Loop must continue even when !ok to restore the parent chain. */
        if (ok) {
            if (!prev) {
                /* Print edge from some runtime root or startThing. */
                if (fputs(node->edgeName, fp) < 0)
                    ok = JS_FALSE;
            } else {
                JS_PrintTraceThingInfo(dtrc->buffer, sizeof dtrc->buffer,
                                       &dtrc->base, prev->thing, prev->kind,
                                       JS_FALSE);
                if (fprintf(fp, "(%p %s).%s",
                           prev->thing, dtrc->buffer, node->edgeName) < 0) {
                    ok = JS_FALSE;
                }
            }
        }
        following = node->parent;
        node->parent = prev;
        prev = node;
        node = following;
    } while (node);

    return ok && putc('\n', fp) >= 0;
}

JS_PUBLIC_API(JSBool)
JS_DumpHeap(JSContext *cx, FILE *fp, void* startThing, uint32 startKind,
            void *thingToFind, size_t maxDepth, void *thingToIgnore)
{
    JSDumpingTracer dtrc;
    JSHeapDumpNode *node, *children, *next, *parent;
    size_t depth;
    JSBool thingToFindWasTraced;

    if (maxDepth == 0)
        return JS_TRUE;

    JS_TRACER_INIT(&dtrc.base, cx, DumpNotify);
    if (!JS_DHashTableInit(&dtrc.visited, JS_DHashGetStubOps(),
                           NULL, sizeof(JSDHashEntryStub),
                           JS_DHASH_DEFAULT_CAPACITY(100))) {
        JS_ReportOutOfMemory(cx);
        return JS_FALSE;
    }
    dtrc.ok = JS_TRUE;
    dtrc.startThing = startThing;
    dtrc.thingToFind = thingToFind;
    dtrc.thingToIgnore = thingToIgnore;
    dtrc.parentNode = NULL;
    node = NULL;
    dtrc.lastNodep = &node;
    if (!startThing) {
        JS_ASSERT(startKind == 0);
        JS_TraceRuntime(&dtrc.base);
    } else {
        JS_TraceChildren(&dtrc.base, startThing, startKind);
    }

    depth = 1;
    if (!node)
        goto dump_out;

    thingToFindWasTraced = thingToFind && thingToFind == startThing;
    for (;;) {
        /*
         * Loop must continue even when !dtrc.ok to free all nodes allocated
         * so far.
         */
        if (dtrc.ok) {
            if (thingToFind == NULL || thingToFind == node->thing)
                dtrc.ok = DumpNode(&dtrc, fp, node);

            /* Descend into children. */
            if (dtrc.ok &&
                depth < maxDepth &&
                (thingToFind != node->thing || !thingToFindWasTraced)) {
                dtrc.parentNode = node;
                children = NULL;
                dtrc.lastNodep = &children;
                JS_TraceChildren(&dtrc.base, node->thing, node->kind);
                if (thingToFind == node->thing)
                    thingToFindWasTraced = JS_TRUE;
                if (children != NULL) {
                    ++depth;
                    node = children;
                    continue;
                }
            }
        }

        /* Move to next or parents next and free the node. */
        for (;;) {
            next = node->next;
            parent = node->parent;
            cx->free(node);
            node = next;
            if (node)
                break;
            if (!parent)
                goto dump_out;
            JS_ASSERT(depth > 1);
            --depth;
            node = parent;
        }
    }

  dump_out:
    JS_ASSERT(depth == 1);
    JS_DHashTableFinish(&dtrc.visited);
    return dtrc.ok;
}

#endif /* DEBUG */

JS_PUBLIC_API(void)
JS_MarkGCThing(JSContext *cx, void *thing, const char *name, void *arg)
{
    JSTracer *trc;

    trc = (JSTracer *)arg;
    if (!trc)
        trc = cx->runtime->gcMarkingTracer;
    else
        JS_ASSERT(trc == cx->runtime->gcMarkingTracer);

#ifdef JS_THREADSAFE
    JS_ASSERT(cx->runtime->gcThread == trc->context->thread);
#endif
    JS_SET_TRACING_NAME(trc, name ? name : "unknown");
    js_CallValueTracerIfGCThing(trc, (jsval)thing);
}

extern JS_PUBLIC_API(JSBool)
JS_IsGCMarkingTracer(JSTracer *trc)
{
    return IS_GC_MARKING_TRACER(trc);
}

JS_PUBLIC_API(void)
JS_GC(JSContext *cx)
{
    LeaveTrace(cx);

    /* Don't nuke active arenas if executing or compiling. */
    if (cx->stackPool.current == &cx->stackPool.first)
        JS_FinishArenaPool(&cx->stackPool);
    if (cx->tempPool.current == &cx->tempPool.first)
        JS_FinishArenaPool(&cx->tempPool);
    js_GC(cx, GC_NORMAL);
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
    JSRuntime *rt;
    uint32 bytes, lastBytes;

    rt = cx->runtime;

#ifdef JS_GC_ZEAL
    if (rt->gcZeal > 0) {
        JS_GC(cx);
        return;
    }
#endif

    bytes = rt->gcBytes;
    lastBytes = rt->gcLastBytes;

    /*
     * We run the GC if we used all available free GC cells and had to
     * allocate extra 1/3 of GC arenas since the last run of GC, or if
     * we have malloc'd more bytes through JS_malloc than we were told
     * to allocate by JS_NewRuntime.
     *
     * The reason for
     *   bytes > 4/3 lastBytes
     * condition is the following. Bug 312238 changed bytes and lastBytes
     * to mean the total amount of memory that the GC uses now and right
     * after the last GC.
     *
     * Before the bug the variables meant the size of allocated GC things
     * now and right after the last GC. That size did not include the
     * memory taken by free GC cells and the condition was
     *   bytes > 3/2 lastBytes.
     * That is, we run the GC if we have half again as many bytes of
     * GC-things as the last time we GC'd. To be compatible we need to
     * express that condition through the new meaning of bytes and
     * lastBytes.
     *
     * We write the original condition as
     *   B*(1-F) > 3/2 Bl*(1-Fl)
     * where B is the total memory size allocated by GC and F is the free
     * cell density currently and Sl and Fl are the size and the density
     * right after GC. The density by definition is memory taken by free
     * cells divided by total amount of memory. In other words, B and Bl
     * are bytes and lastBytes with the new meaning and B*(1-F) and
     * Bl*(1-Fl) are bytes and lastBytes with the original meaning.
     *
     * Our task is to exclude F and Fl from the last statement. According
     * to the stats from bug 331966 comment 23, Fl is about 10-25% for a
     * typical run of the browser. It means that the original condition
     * implied that we did not run GC unless we exhausted the pool of
     * free cells. Indeed if we still have free cells, then B == Bl since
     * we did not yet allocated any new arenas and the condition means
     *   1 - F > 3/2 (1-Fl) or 3/2Fl > 1/2 + F
     * That implies 3/2 Fl > 1/2 or Fl > 1/3. That cannot be fulfilled
     * for the state described by the stats. So we can write the original
     * condition as:
     *   F == 0 && B > 3/2 Bl(1-Fl)
     * Again using the stats we see that Fl is about 11% when the browser
     * starts up and when we are far from hitting rt->gcMaxBytes. With
     * this F we have
     * F == 0 && B > 3/2 Bl(1-0.11)
     * or approximately F == 0 && B > 4/3 Bl.
     */
    if ((bytes > 8192 && bytes > lastBytes + lastBytes / 3) ||
        rt->isGCMallocLimitReached()) {
        JS_GC(cx);
    }
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallback(JSContext *cx, JSGCCallback cb)
{
    CHECK_REQUEST(cx);
    return JS_SetGCCallbackRT(cx->runtime, cb);
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallbackRT(JSRuntime *rt, JSGCCallback cb)
{
    JSGCCallback oldcb;

    oldcb = rt->gcCallback;
    rt->gcCallback = cb;
    return oldcb;
}

JS_PUBLIC_API(JSBool)
JS_IsAboutToBeFinalized(JSContext *cx, void *thing)
{
    JS_ASSERT(thing);
    return js_IsAboutToBeFinalized(thing);
}

JS_PUBLIC_API(void)
JS_SetGCParameter(JSRuntime *rt, JSGCParamKey key, uint32 value)
{
    switch (key) {
      case JSGC_MAX_BYTES:
        rt->gcMaxBytes = value;
        break;
      case JSGC_MAX_MALLOC_BYTES:
        rt->setGCMaxMallocBytes(value);
        break;
      case JSGC_STACKPOOL_LIFESPAN:
        rt->gcEmptyArenaPoolLifespan = value;
        break;
      default:
        JS_ASSERT(key == JSGC_TRIGGER_FACTOR);
        JS_ASSERT(value >= 100);
        rt->setGCTriggerFactor(value);
        return;
    }
}

JS_PUBLIC_API(uint32)
JS_GetGCParameter(JSRuntime *rt, JSGCParamKey key)
{
    switch (key) {
      case JSGC_MAX_BYTES:
        return rt->gcMaxBytes;
      case JSGC_MAX_MALLOC_BYTES:
        return rt->gcMaxMallocBytes;
      case JSGC_STACKPOOL_LIFESPAN:
        return rt->gcEmptyArenaPoolLifespan;
      case JSGC_TRIGGER_FACTOR:
        return rt->gcTriggerFactor;
      case JSGC_BYTES:
        return rt->gcBytes;
      default:
        JS_ASSERT(key == JSGC_NUMBER);
        return rt->gcNumber;
    }
}

JS_PUBLIC_API(void)
JS_SetGCParameterForThread(JSContext *cx, JSGCParamKey key, uint32 value)
{
    JS_ASSERT(key == JSGC_MAX_CODE_CACHE_BYTES);
#ifdef JS_TRACER
    SetMaxCodeCacheBytes(cx, value);
#endif
}

JS_PUBLIC_API(uint32)
JS_GetGCParameterForThread(JSContext *cx, JSGCParamKey key)
{
    JS_ASSERT(key == JSGC_MAX_CODE_CACHE_BYTES);
#ifdef JS_TRACER
    return JS_THREAD_DATA(cx)->traceMonitor.maxCodeCacheBytes;
#else
    return 0;
#endif
}

JS_PUBLIC_API(void)
JS_FlushCaches(JSContext *cx)
{
#ifdef JS_TRACER
    FlushJITCache(cx);
#endif
}

JS_PUBLIC_API(intN)
JS_AddExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    return js_ChangeExternalStringFinalizer(NULL, finalizer);
}

JS_PUBLIC_API(intN)
JS_RemoveExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    return js_ChangeExternalStringFinalizer(finalizer, NULL);
}

JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, jschar *chars, size_t length, intN type)
{
    CHECK_REQUEST(cx);
    JS_ASSERT(uintN(type) < JS_EXTERNAL_STRING_LIMIT);

    JSString *str = js_NewGCExternalString(cx, uintN(type));
    if (!str)
        return NULL;
    str->initFlat(chars, length);
    cx->updateMallocCounter((length + 1) * sizeof(jschar));
    return str;
}

JS_PUBLIC_API(intN)
JS_GetExternalStringGCType(JSRuntime *rt, JSString *str)
{
    /*
     * No need to test this in js_GetExternalStringGCType, which asserts its
     * inverse instead of wasting cycles on testing a condition we can ensure
     * by auditing in-VM calls to the js_... helper.
     */
    if (JSString::isStatic(str))
        return -1;

    return js_GetExternalStringGCType(str);
}

JS_PUBLIC_API(void)
JS_SetThreadStackLimit(JSContext *cx, jsuword limitAddr)
{
#if JS_STACK_GROWTH_DIRECTION > 0
    if (limitAddr == 0)
        limitAddr = (jsuword)-1;
#endif
    cx->stackLimit = limitAddr;
}

JS_PUBLIC_API(void)
JS_SetScriptStackQuota(JSContext *cx, size_t quota)
{
    cx->scriptStackQuota = quota;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
    cx->free(ida);
}

JS_PUBLIC_API(JSBool)
JS_ValueToId(JSContext *cx, jsval v, jsid *idp)
{
    CHECK_REQUEST(cx);
    if (JSVAL_IS_INT(v)) {
        *idp = INT_JSVAL_TO_JSID(v);
        return JS_TRUE;
    }

#if JS_HAS_XML_SUPPORT
    if (!JSVAL_IS_PRIMITIVE(v)) {
        JSClass *clasp = JSVAL_TO_OBJECT(v)->getClass();
        if (JS_UNLIKELY(clasp == &js_QNameClass.base ||
                        clasp == &js_AttributeNameClass ||
                        clasp == &js_AnyNameClass)) {
            *idp = OBJECT_JSVAL_TO_JSID(v);
            return JS_TRUE;
        }
    }
#endif

    return js_ValueToStringId(cx, v, idp);
}

JS_PUBLIC_API(JSBool)
JS_IdToValue(JSContext *cx, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    *vp = ID_TO_VALUE(id);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsval id)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    return js_TryValueOf(cx, obj, type, vp);
}

JS_PUBLIC_API(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj)
{
}

JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    CHECK_REQUEST(cx);
    return js_InitClass(cx, obj, parent_proto, clasp, constructor, nargs,
                        ps, fs, static_ps, static_fs);
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSContext *cx, JSObject *obj)
{
    return obj->getClass();
}
#else
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSObject *obj)
{
    return obj->getClass();
}
#endif

JS_PUBLIC_API(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
{
    JSFunction *fun;

    CHECK_REQUEST(cx);
    if (obj && OBJ_GET_CLASS(cx, obj) == clasp)
        return JS_TRUE;
    if (argv) {
        fun = js_ValueToFunction(cx, &argv[-2], 0);
        if (fun) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                 JSMSG_INCOMPATIBLE_PROTO,
                                 clasp->name, JS_GetFunctionName(fun),
                                 obj
                                 ? OBJ_GET_CLASS(cx, obj)->name
                                 : js_null_str);
        }
    }
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    return js_HasInstance(cx, obj, v, bp);
}

JS_PUBLIC_API(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj)
{
    return obj->getPrivate();
}

JS_PUBLIC_API(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data)
{
    obj->setPrivate(data);
    return true;
}

JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
                      jsval *argv)
{
    if (!JS_InstanceOf(cx, obj, clasp, argv))
        return NULL;
    return obj->getPrivate();
}

JS_PUBLIC_API(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj)
{
    JSObject *proto;

    CHECK_REQUEST(cx);
    proto = obj->getProto();

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return proto && proto->map ? proto : NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto)
{
    CHECK_REQUEST(cx);
    return js_SetProtoOrParent(cx, obj, JSSLOT_PROTO, proto, JS_FALSE);
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj)
{
    JSObject *parent = obj->getParent();

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return parent && parent->map ? parent : NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent)
{
    CHECK_REQUEST(cx);
    return js_SetProtoOrParent(cx, obj, JSSLOT_PARENT, parent, JS_FALSE);
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto)
{
    jsval cval;

    CHECK_REQUEST(cx);
    {
        JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

        if (!proto->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.constructorAtom), &cval))
            return NULL;
    }
    if (!VALUE_IS_FUNCTION(cx, cval)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR,
                             OBJ_GET_CLASS(cx, proto)->name);
        return NULL;
    }
    return JSVAL_TO_OBJECT(cval);
}

JS_PUBLIC_API(JSBool)
JS_GetObjectId(JSContext *cx, JSObject *obj, jsid *idp)
{
    JS_ASSERT(JSID_IS_OBJECT(obj));
    *idp = OBJECT_TO_JSID(obj);
    return JS_TRUE;
}

JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    CHECK_REQUEST(cx);
    if (!clasp)
        clasp = &js_ObjectClass;    /* default class is Object */
    return js_NewObject(cx, clasp, proto, parent);
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectWithGivenProto(JSContext *cx, JSClass *clasp, JSObject *proto,
                           JSObject *parent)
{
    CHECK_REQUEST(cx);
    if (!clasp)
        clasp = &js_ObjectClass;    /* default class is Object */
    return js_NewObjectWithGivenProto(cx, clasp, proto, parent);
}

JS_PUBLIC_API(JSBool)
JS_SealObject(JSContext *cx, JSObject *obj, JSBool deep)
{
    JSScope *scope;
    JSIdArray *ida;
    uint32 nslots, i;
    jsval v;

    if (obj->isDenseArray() && !js_MakeArraySlow(cx, obj))
        return JS_FALSE;

    if (!obj->isNative()) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_CANT_SEAL_OBJECT,
                             OBJ_GET_CLASS(cx, obj)->name);
        return JS_FALSE;
    }

    scope = OBJ_SCOPE(obj);

#if defined JS_THREADSAFE && defined DEBUG
    /* Insist on scope being used exclusively by cx's thread. */
    if (scope->title.ownercx != cx) {
        JS_LOCK_OBJ(cx, obj);
        JS_ASSERT(OBJ_SCOPE(obj) == scope);
        JS_ASSERT(scope->title.ownercx == cx);
        JS_UNLOCK_SCOPE(cx, scope);
    }
#endif

    /* Nothing to do if obj's scope is already sealed. */
    if (scope->sealed())
        return JS_TRUE;

    /* XXX Enumerate lazy properties now, as they can't be added later. */
    ida = JS_Enumerate(cx, obj);
    if (!ida)
        return JS_FALSE;
    JS_DestroyIdArray(cx, ida);

    /* Ensure that obj has its own, mutable scope, and seal that scope. */
    JS_LOCK_OBJ(cx, obj);
    scope = js_GetMutableScope(cx, obj);
    if (scope)
        scope->seal(cx);
    JS_UNLOCK_OBJ(cx, obj);
    if (!scope)
        return JS_FALSE;

    /* If we are not sealing an entire object graph, we're done. */
    if (!deep)
        return JS_TRUE;

    /* Walk slots in obj and if any value is a non-null object, seal it. */
    nslots = scope->freeslot;
    for (i = 0; i != nslots; ++i) {
        v = obj->getSlot(i);
        if (i == JSSLOT_PRIVATE && (obj->getClass()->flags & JSCLASS_HAS_PRIVATE))
            continue;
        if (JSVAL_IS_PRIMITIVE(v))
            continue;
        if (!JS_SealObject(cx, JSVAL_TO_OBJECT(v), deep))
            return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent)
{
    CHECK_REQUEST(cx);
    if (!clasp)
        clasp = &js_ObjectClass;    /* default class is Object */
    return js_ConstructObject(cx, clasp, proto, parent, 0, NULL);
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObjectWithArguments(JSContext *cx, JSClass *clasp, JSObject *proto,
                                JSObject *parent, uintN argc, jsval *argv)
{
    CHECK_REQUEST(cx);
    if (!clasp)
        clasp = &js_ObjectClass;    /* default class is Object */
    return js_ConstructObject(cx, clasp, proto, parent, argc, argv);
}

static JSBool
DefinePropertyById(JSContext *cx, JSObject *obj, jsid id, jsval value,
                   JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                   uintN flags, intN tinyid)
{
    if (flags != 0 && obj->isNative()) {
        JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DECLARING);
        return !!js_DefineNativeProperty(cx, obj, id, value, getter, setter,
                                         attrs, flags, tinyid, NULL);
    }
    return obj->defineProperty(cx, id, value, getter, setter, attrs);
}

static JSBool
DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
               JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
               uintN flags, intN tinyid)
{
    jsid id;
    JSAtom *atom;

    if (attrs & JSPROP_INDEX) {
        id = INT_TO_JSID(intptr_t(name));
        atom = NULL;
        attrs &= ~JSPROP_INDEX;
    } else {
        atom = js_Atomize(cx, name, strlen(name), 0);
        if (!atom)
            return JS_FALSE;
        id = ATOM_TO_JSID(atom);
    }
    return DefinePropertyById(cx, obj, id, value, getter, setter, attrs,
                              flags, tinyid);
}

#define AUTO_NAMELEN(s,n)   (((n) == (size_t)-1) ? js_strlen(s) : (n))

static JSBool
DefineUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen, jsval value,
                 JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                 uintN flags, intN tinyid)
{
    JSAtom *atom;

    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return JS_FALSE;
    if (flags != 0 && obj->isNative()) {
        JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DECLARING);
        return !!js_DefineNativeProperty(cx, obj, ATOM_TO_JSID(atom), value,
                                         getter, setter, attrs, flags, tinyid,
                                         NULL);
    }
    return obj->defineProperty(cx, ATOM_TO_JSID(atom), value, getter, setter, attrs);
}

JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *clasp,
                JSObject *proto, uintN attrs)
{
    JSObject *nobj;

    CHECK_REQUEST(cx);
    if (!clasp)
        clasp = &js_ObjectClass;    /* default class is Object */
    nobj = js_NewObject(cx, clasp, proto, obj);
    if (!nobj)
        return NULL;
    if (!DefineProperty(cx, obj, name, OBJECT_TO_JSVAL(nobj), NULL, NULL, attrs,
                        0, 0)) {
        return NULL;
    }
    return nobj;
}

JS_PUBLIC_API(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds)
{
    JSBool ok;
    jsval value;
    uintN attrs;

    CHECK_REQUEST(cx);
    for (ok = JS_TRUE; cds->name; cds++) {
        ok = js_NewNumberInRootedValue(cx, cds->dval, &value);
        if (!ok)
            break;
        attrs = cds->flags;
        if (!attrs)
            attrs = JSPROP_READONLY | JSPROP_PERMANENT;
        ok = DefineProperty(cx, obj, cds->name, value, NULL, NULL, attrs, 0, 0);
        if (!ok)
            break;
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_DefineProperties(JSContext *cx, JSObject *obj, JSPropertySpec *ps)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    for (ok = JS_TRUE; ps->name; ps++) {
        ok = DefineProperty(cx, obj, ps->name, JSVAL_VOID,
                            ps->getter, ps->setter, ps->flags,
                            JSScopeProperty::HAS_SHORTID, ps->tinyid);
        if (!ok)
            break;
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefineProperty(cx, obj, name, value, getter, setter, attrs, 0, 0);
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyById(JSContext *cx, JSObject *obj, jsid id, jsval value,
                      JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefinePropertyById(cx, obj, id, value, getter, setter, attrs, 0, 0);
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
                            int8 tinyid, jsval value,
                            JSPropertyOp getter, JSPropertyOp setter,
                            uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefineProperty(cx, obj, name, value, getter, setter, attrs,
                          JSScopeProperty::HAS_SHORTID, tinyid);
}

JS_PUBLIC_API(JSBool)
JS_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id, jsval descriptor, JSBool *bp)
{
    CHECK_REQUEST(cx);
    return js_DefineOwnProperty(cx, obj, id, descriptor, bp);
}

static JSBool
LookupPropertyById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                   JSObject **objp, JSProperty **propp)
{
    JSAutoResolveFlags rf(cx, flags);
    id = js_CheckForStringIndex(id);
    return obj->lookupProperty(cx, id, objp, propp);
}

static JSBool
LookupProperty(JSContext *cx, JSObject *obj, const char *name, uintN flags,
               JSObject **objp, JSProperty **propp)
{
    JSAtom *atom;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;
    return LookupPropertyById(cx, obj, ATOM_TO_JSID(atom), flags, objp, propp);
}

static JSBool
LookupUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen, uintN flags,
                 JSObject **objp, JSProperty **propp)
{
    JSAtom *atom;

    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return JS_FALSE;
    return LookupPropertyById(cx, obj, ATOM_TO_JSID(atom), flags, objp, propp);
}

JS_PUBLIC_API(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
                 const char *alias)
{
    JSObject *obj2;
    JSProperty *prop;
    JSAtom *atom;
    JSBool ok;
    JSScopeProperty *sprop;

    CHECK_REQUEST(cx);
    if (!LookupProperty(cx, obj, name, JSRESOLVE_QUALIFIED, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        js_ReportIsNotDefined(cx, name);
        return JS_FALSE;
    }
    if (obj2 != obj || !obj->isNative()) {
        obj2->dropProperty(cx, prop);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_ALIAS,
                             alias, name, OBJ_GET_CLASS(cx, obj2)->name);
        return JS_FALSE;
    }
    atom = js_Atomize(cx, alias, strlen(alias), 0);
    if (!atom) {
        ok = JS_FALSE;
    } else {
        sprop = (JSScopeProperty *)prop;
        ok = (js_AddNativeProperty(cx, obj, ATOM_TO_JSID(atom),
                                   sprop->getter(), sprop->setter(), sprop->slot,
                                   sprop->attributes(), sprop->getFlags() | JSScopeProperty::ALIAS,
                                   sprop->shortid)
              != NULL);
    }
    obj->dropProperty(cx, prop);
    return ok;
}

static JSBool
LookupResult(JSContext *cx, JSObject *obj, JSObject *obj2, JSProperty *prop,
             jsval *vp)
{
    if (!prop) {
        /* XXX bad API: no way to tell "not defined" from "void value" */
        *vp = JSVAL_VOID;
        return JS_TRUE;
    }

    JSBool ok = JS_TRUE;
    if (obj2->isNative()) {
        JSScopeProperty *sprop = (JSScopeProperty *) prop;

        if (sprop->isMethod()) {
            AutoScopePropertyRooter root(cx, sprop);
            JS_UNLOCK_OBJ(cx, obj2);
            *vp = sprop->methodValue();
            return OBJ_SCOPE(obj2)->methodReadBarrier(cx, sprop, vp);
        }

        /* Peek at the native property's slot value, without doing a Get. */
        *vp = SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj2))
               ? LOCKED_OBJ_GET_SLOT(obj2, sprop->slot)
               : JSVAL_TRUE;
    } else if (obj2->isDenseArray()) {
        ok = js_GetDenseArrayElementValue(cx, obj2, prop, vp);
    } else {
        /* XXX bad API: no way to return "defined but value unknown" */
        *vp = JSVAL_TRUE;
    }
    obj2->dropProperty(cx, prop);
    return ok;
}

static JSBool
GetPropertyAttributesById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                          JSBool own, JSPropertyDescriptor *desc)
{
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;

    if (!LookupPropertyById(cx, obj, id, flags, &obj2, &prop))
        return JS_FALSE;

    if (!prop || (own && obj != obj2)) {
        desc->obj = NULL;
        desc->attrs = 0;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->value = JSVAL_VOID;
        if (prop)
            obj2->dropProperty(cx, prop);
        return JS_TRUE;
    }

    desc->obj = obj2;

    ok = obj2->getAttributes(cx, id, prop, &desc->attrs);
    if (ok) {
        if (obj2->isNative()) {
            JSScopeProperty *sprop = (JSScopeProperty *) prop;

            desc->getter = sprop->getter();
            desc->setter = sprop->setter();
            desc->value = SPROP_HAS_VALID_SLOT(sprop, OBJ_SCOPE(obj2))
                          ? LOCKED_OBJ_GET_SLOT(obj2, sprop->slot)
                          : JSVAL_VOID;
        } else {
            desc->getter = NULL;
            desc->setter = NULL;
            desc->value = JSVAL_VOID;
        }
    }
    obj2->dropProperty(cx, prop);
    return ok;
}

static JSBool
GetPropertyAttributes(JSContext *cx, JSObject *obj, JSAtom *atom,
                      uintN *attrsp, JSBool *foundp,
                      JSPropertyOp *getterp, JSPropertyOp *setterp)

{
    if (!atom)
        return JS_FALSE;

    JSPropertyDescriptor desc;
    if (!GetPropertyAttributesById(cx, obj, ATOM_TO_JSID(atom),
                                   JSRESOLVE_QUALIFIED, JS_FALSE, &desc)) {
        return JS_FALSE;
    }

    *attrsp = desc.attrs;
    *foundp = (desc.obj != NULL);
    if (getterp)
        *getterp = desc.getter;
    if (setterp)
        *setterp = desc.setter;
    return JS_TRUE;
}

static JSBool
SetPropertyAttributes(JSContext *cx, JSObject *obj, JSAtom *atom,
                      uintN attrs, JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok;

    if (!atom)
        return JS_FALSE;
    if (!LookupPropertyById(cx, obj, ATOM_TO_JSID(atom), JSRESOLVE_QUALIFIED,
                            &obj2, &prop)) {
        return JS_FALSE;
    }
    if (!prop || obj != obj2) {
        *foundp = JS_FALSE;
        if (prop)
            obj2->dropProperty(cx, prop);
        return JS_TRUE;
    }

    *foundp = JS_TRUE;
    ok = obj->setAttributes(cx, ATOM_TO_JSID(atom), prop, &attrs);
    obj->dropProperty(cx, prop);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN *attrsp, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return GetPropertyAttributes(cx, obj,
                                 js_Atomize(cx, name, strlen(name), 0),
                                 attrsp, foundp, NULL, NULL);
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyAttrsGetterAndSetter(JSContext *cx, JSObject *obj,
                                   const char *name,
                                   uintN *attrsp, JSBool *foundp,
                                   JSPropertyOp *getterp,
                                   JSPropertyOp *setterp)
{
    CHECK_REQUEST(cx);
    return GetPropertyAttributes(cx, obj,
                                 js_Atomize(cx, name, strlen(name), 0),
                                 attrsp, foundp, getterp, setterp);
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyAttrsGetterAndSetterById(JSContext *cx, JSObject *obj,
                                       jsid id,
                                       uintN *attrsp, JSBool *foundp,
                                       JSPropertyOp *getterp,
                                       JSPropertyOp *setterp)
{
    CHECK_REQUEST(cx);

    JSPropertyDescriptor desc;
    if (!GetPropertyAttributesById(cx, obj, id, JSRESOLVE_QUALIFIED, JS_FALSE, &desc))
        return JS_FALSE;

    *attrsp = desc.attrs;
    *foundp = (desc.obj != NULL);
    if (getterp)
        *getterp = desc.getter;
    if (setterp)
        *setterp = desc.setter;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN attrs, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return SetPropertyAttributes(cx, obj,
                                 js_Atomize(cx, name, strlen(name), 0),
                                 attrs, foundp);
}

static JSBool
AlreadyHasOwnPropertyHelper(JSContext *cx, JSObject *obj, jsid id,
                            JSBool *foundp)
{
    JSScope *scope;

    if (!obj->isNative()) {
        JSObject *obj2;
        JSProperty *prop;

        if (!LookupPropertyById(cx, obj, id,
                                JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                                &obj2, &prop)) {
            return JS_FALSE;
        }
        *foundp = (obj == obj2);
        if (prop)
            obj2->dropProperty(cx, prop);
        return JS_TRUE;
    }

    JS_LOCK_OBJ(cx, obj);
    scope = OBJ_SCOPE(obj);
    *foundp = scope->hasProperty(id);
    JS_UNLOCK_SCOPE(cx, scope);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnProperty(JSContext *cx, JSObject *obj, const char *name,
                         JSBool *foundp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;
    return AlreadyHasOwnPropertyHelper(cx, obj, ATOM_TO_JSID(atom), foundp);
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnPropertyById(JSContext *cx, JSObject *obj, jsid id,
                             JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return AlreadyHasOwnPropertyHelper(cx, obj, id, foundp);
}

JS_PUBLIC_API(JSBool)
JS_HasProperty(JSContext *cx, JSObject *obj, const char *name, JSBool *foundp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = LookupProperty(cx, obj, name,
                        JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                        &obj2, &prop);
    if (ok) {
        *foundp = (prop != NULL);
        if (prop)
            obj2->dropProperty(cx, prop);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_HasPropertyById(JSContext *cx, JSObject *obj, jsid id, JSBool *foundp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = LookupPropertyById(cx, obj, id,
                            JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                            &obj2, &prop);
    if (ok) {
       *foundp = (prop != NULL);
       if (prop)
           obj2->dropProperty(cx, prop);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    return LookupProperty(cx, obj, name, JSRESOLVE_QUALIFIED, &obj2, &prop) &&
           LookupResult(cx, obj, obj2, prop, vp);
}

JS_PUBLIC_API(JSBool)
JS_LookupPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    return LookupPropertyById(cx, obj, id, JSRESOLVE_QUALIFIED, &obj2, &prop) &&
           LookupResult(cx, obj, obj2, prop, vp);
}

JS_PUBLIC_API(JSBool)
JS_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, const char *name,
                           uintN flags, jsval *vp)
{
    JSAtom *atom;
    JSObject *obj2;

    atom = js_Atomize(cx, name, strlen(name), 0);
    return atom &&
           JS_LookupPropertyWithFlagsById(cx, obj, ATOM_TO_JSID(atom), flags,
                                          &obj2, vp);
}

JS_PUBLIC_API(JSBool)
JS_LookupPropertyWithFlagsById(JSContext *cx, JSObject *obj, jsid id,
                               uintN flags, JSObject **objp, jsval *vp)
{
    JSBool ok;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = obj->isNative()
         ? js_LookupPropertyWithFlags(cx, obj, id, flags, objp, &prop) >= 0
         : obj->lookupProperty(cx, id, objp, &prop);
    if (ok)
        ok = LookupResult(cx, obj, *objp, prop, vp);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescriptorById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                             JSPropertyDescriptor *desc)
{
    CHECK_REQUEST(cx);

    return GetPropertyAttributesById(cx, obj, id, flags, JS_FALSE, desc);
}

JS_PUBLIC_API(JSBool)
JS_GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    return js_GetOwnPropertyDescriptor(cx, obj, id, vp);
}

JS_PUBLIC_API(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->getProperty(cx, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->getProperty(cx, id, vp);
}

JS_PUBLIC_API(JSBool)
JS_GetMethodById(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                 jsval *vp)
{
    CHECK_REQUEST(cx);
    if (!js_GetMethod(cx, obj, id, JSGET_METHOD_BARRIER, vp))
        return JS_FALSE;
    if (objp)
        *objp = obj;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetMethod(JSContext *cx, JSObject *obj, const char *name, JSObject **objp,
             jsval *vp)
{
    JSAtom *atom;

    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;
    return JS_GetMethodById(cx, obj, ATOM_TO_JSID(atom), objp, vp);
}

JS_PUBLIC_API(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING);
    return obj->setProperty(cx, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING);
    return obj->setProperty(cx, id, vp);
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name)
{
    jsval junk;

    return JS_DeleteProperty2(cx, obj, name, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty2(JSContext *cx, JSObject *obj, const char *name,
                   jsval *rval)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return JS_FALSE;

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->deleteProperty(cx, ATOM_TO_JSID(atom), rval);
}

JS_PUBLIC_API(JSBool)
JS_DeletePropertyById(JSContext *cx, JSObject *obj, jsid id)
{
    jsval junk;

    return JS_DeletePropertyById2(cx, obj, id, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeletePropertyById2(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    CHECK_REQUEST(cx);
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->deleteProperty(cx, id, rval);
}

JS_PUBLIC_API(JSBool)
JS_DefineUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, jsval value,
                    JSPropertyOp getter, JSPropertyOp setter,
                    uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter,
                            attrs, 0, 0);
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN *attrsp, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return GetPropertyAttributes(cx, obj,
                    js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0),
                    attrsp, foundp, NULL, NULL);
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttrsGetterAndSetter(JSContext *cx, JSObject *obj,
                                     const jschar *name, size_t namelen,
                                     uintN *attrsp, JSBool *foundp,
                                     JSPropertyOp *getterp,
                                     JSPropertyOp *setterp)
{
    CHECK_REQUEST(cx);
    return GetPropertyAttributes(cx, obj,
                    js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0),
                    attrsp, foundp, getterp, setterp);
}

JS_PUBLIC_API(JSBool)
JS_SetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN attrs, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    return SetPropertyAttributes(cx, obj,
                    js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0),
                    attrs, foundp);
}

JS_PUBLIC_API(JSBool)
JS_DefineUCPropertyWithTinyId(JSContext *cx, JSObject *obj,
                              const jschar *name, size_t namelen,
                              int8 tinyid, jsval value,
                              JSPropertyOp getter, JSPropertyOp setter,
                              uintN attrs)
{
    CHECK_REQUEST(cx);
    return DefineUCProperty(cx, obj, name, namelen, value, getter, setter,
                            attrs, JSScopeProperty::HAS_SHORTID, tinyid);
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnUCProperty(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           JSBool *foundp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return JS_FALSE;
    return AlreadyHasOwnPropertyHelper(cx, obj, ATOM_TO_JSID(atom), foundp);
}

JS_PUBLIC_API(JSBool)
JS_HasUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 JSBool *vp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = LookupUCProperty(cx, obj, name, namelen,
                          JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                          &obj2, &prop);
    if (ok) {
        *vp = (prop != NULL);
        if (prop)
            obj2->dropProperty(cx, prop);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_LookupUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen,
                    jsval *vp)
{
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    return LookupUCProperty(cx, obj, name, namelen, JSRESOLVE_QUALIFIED,
                            &obj2, &prop) &&
           LookupResult(cx, obj, obj2, prop, vp);
}

JS_PUBLIC_API(JSBool)
JS_GetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return JS_FALSE;

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->getProperty(cx, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_SetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return JS_FALSE;

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING);
    return obj->setProperty(cx, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_DeleteUCProperty2(JSContext *cx, JSObject *obj,
                     const jschar *name, size_t namelen,
                     jsval *rval)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return JS_FALSE;

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->deleteProperty(cx, ATOM_TO_JSID(atom), rval);
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector)
{
    CHECK_REQUEST(cx);
    /* NB: jsuint cast does ToUint32. */
    return js_NewArrayObject(cx, (jsuint)length, vector);
}

JS_PUBLIC_API(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj)
{
    return js_GetWrappedObject(cx, obj)->isArray();
}

JS_PUBLIC_API(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    CHECK_REQUEST(cx);
    return js_GetLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsuint length)
{
    CHECK_REQUEST(cx);
    return js_SetLengthProperty(cx, obj, length);
}

JS_PUBLIC_API(JSBool)
JS_HasArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    CHECK_REQUEST(cx);
    return js_HasLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
                 JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DECLARING);

    CHECK_REQUEST(cx);
    return obj->defineProperty(cx, INT_TO_JSID(index), value, getter, setter, attrs);
}

JS_PUBLIC_API(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias)
{
    JSObject *obj2;
    JSProperty *prop;
    JSScopeProperty *sprop;
    JSBool ok;

    CHECK_REQUEST(cx);
    if (!LookupProperty(cx, obj, name, JSRESOLVE_QUALIFIED, &obj2, &prop))
        return JS_FALSE;
    if (!prop) {
        js_ReportIsNotDefined(cx, name);
        return JS_FALSE;
    }
    if (obj2 != obj || !obj->isNative()) {
        char numBuf[12];
        obj2->dropProperty(cx, prop);
        JS_snprintf(numBuf, sizeof numBuf, "%ld", (long)alias);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_ALIAS,
                             numBuf, name, OBJ_GET_CLASS(cx, obj2)->name);
        return JS_FALSE;
    }
    sprop = (JSScopeProperty *)prop;
    ok = (js_AddNativeProperty(cx, obj, INT_TO_JSID(alias),
                               sprop->getter(), sprop->setter(), sprop->slot,
                               sprop->attributes(), sprop->getFlags() | JSScopeProperty::ALIAS,
                               sprop->shortid)
          != NULL);
    obj->dropProperty(cx, prop);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnElement(JSContext *cx, JSObject *obj, jsint index,
                        JSBool *foundp)
{
    return AlreadyHasOwnPropertyHelper(cx, obj, INT_TO_JSID(index), foundp);
}

JS_PUBLIC_API(JSBool)
JS_HasElement(JSContext *cx, JSObject *obj, jsint index, JSBool *foundp)
{
    JSBool ok;
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    ok = LookupPropertyById(cx, obj, INT_TO_JSID(index),
                            JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                            &obj2, &prop);
    if (ok) {
        *foundp = (prop != NULL);
        if (prop)
            obj2->dropProperty(cx, prop);
    }
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JSObject *obj2;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    return LookupPropertyById(cx, obj, INT_TO_JSID(index), JSRESOLVE_QUALIFIED,
                              &obj2, &prop) &&
           LookupResult(cx, obj, obj2, prop, vp);
}

JS_PUBLIC_API(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

    CHECK_REQUEST(cx);
    return obj->getProperty(cx, INT_TO_JSID(index), vp);
}

JS_PUBLIC_API(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING);

    CHECK_REQUEST(cx);
    return obj->setProperty(cx, INT_TO_JSID(index), vp);
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index)
{
    jsval junk;

    return JS_DeleteElement2(cx, obj, index, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement2(JSContext *cx, JSObject *obj, jsint index, jsval *rval)
{
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

    CHECK_REQUEST(cx);
    return obj->deleteProperty(cx, INT_TO_JSID(index), rval);
}

JS_PUBLIC_API(void)
JS_ClearScope(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);

    if (obj->map->ops->clear)
        obj->map->ops->clear(cx, obj);

    /* Clear cached class objects on the global object. */
    if (OBJ_GET_CLASS(cx, obj)->flags & JSCLASS_IS_GLOBAL) {
        int key;

        for (key = JSProto_Null; key < JSProto_LIMIT; key++)
            JS_SetReservedSlot(cx, obj, key, JSVAL_VOID);
    }
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *obj)
{
    jsint i, n;
    jsid id;
    JSIdArray *ida;
    jsval *vector;

    CHECK_REQUEST(cx);

    ida = NULL;
    AutoEnumStateRooter iterState(cx, obj);

    /* Get the number of properties to enumerate. */
    jsval num_properties;
    if (!obj->enumerate(cx, JSENUMERATE_INIT, iterState.addr(), &num_properties))
        goto error;
    if (!JSVAL_IS_INT(num_properties)) {
        JS_ASSERT(0);
        goto error;
    }

    /* Grow as needed if we don't know the exact amount ahead of time. */
    n = JSVAL_TO_INT(num_properties);
    if (n <= 0)
        n = 8;

    /* Create an array of jsids large enough to hold all the properties */
    ida = NewIdArray(cx, n);
    if (!ida)
        goto error;

    i = 0;
    vector = &ida->vector[0];
    for (;;) {
        if (!obj->enumerate(cx, JSENUMERATE_NEXT, iterState.addr(), &id))
            goto error;

        /* No more jsid's to enumerate ? */
        if (iterState.state() == JSVAL_NULL)
            break;

        if (i == ida->length) {
            ida = SetIdArrayLength(cx, ida, ida->length * 2);
            if (!ida)
                goto error;
            vector = &ida->vector[0];
        }
        vector[i++] = id;
    }
    return SetIdArrayLength(cx, ida, i);

error:
    if (ida)
        JS_DestroyIdArray(cx, ida);
    return NULL;
}

/*
 * XXX reverse iterator for properties, unreverse and meld with jsinterp.c's
 *     prop_iterator_class somehow...
 * + preserve the obj->enumerate API while optimizing the native object case
 * + native case here uses a JSScopeProperty *, but that iterates in reverse!
 * + so we make non-native match, by reverse-iterating after JS_Enumerating
 */
const uint32 JSSLOT_ITER_INDEX = JSSLOT_PRIVATE + 1;
JS_STATIC_ASSERT(JSSLOT_ITER_INDEX < JS_INITIAL_NSLOTS);

static void
prop_iter_finalize(JSContext *cx, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    if (!pdata)
        return;

    if (JSVAL_TO_INT(obj->fslots[JSSLOT_ITER_INDEX]) >= 0) {
        /* Non-native case: destroy the ida enumerated when obj was created. */
        JSIdArray *ida = (JSIdArray *) pdata;
        JS_DestroyIdArray(cx, ida);
    }
}

static void
prop_iter_trace(JSTracer *trc, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    if (!pdata)
        return;

    if (JSVAL_TO_INT(obj->fslots[JSSLOT_ITER_INDEX]) < 0) {
        /* Native case: just mark the next property to visit. */
        ((JSScopeProperty *) pdata)->trace(trc);
    } else {
        /* Non-native case: mark each id in the JSIdArray private. */
        JSIdArray *ida = (JSIdArray *) pdata;
        for (jsint i = 0, n = ida->length; i < n; i++)
            js_TraceId(trc, ida->vector[i]);
    }
}

static JSClass prop_iter_class = {
    "PropertyIterator",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1) |
    JSCLASS_MARK_IS_TRACE,
    JS_PropertyStub,  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
    JS_EnumerateStub, JS_ResolveStub,  JS_ConvertStub,  prop_iter_finalize,
    NULL,             NULL,            NULL,            NULL,
    NULL,             NULL,            JS_CLASS_TRACE(prop_iter_trace), NULL
};

JS_PUBLIC_API(JSObject *)
JS_NewPropertyIterator(JSContext *cx, JSObject *obj)
{
    JSObject *iterobj;
    JSScope *scope;
    void *pdata;
    jsint index;
    JSIdArray *ida;

    CHECK_REQUEST(cx);
    iterobj = js_NewObject(cx, &prop_iter_class, NULL, obj);
    if (!iterobj)
        return NULL;

    if (obj->isNative()) {
        /* Native case: start with the last property in obj's own scope. */
        scope = OBJ_SCOPE(obj);
        pdata = scope->lastProperty();
        index = -1;
    } else {
        /*
         * Non-native case: enumerate a JSIdArray and keep it via private.
         *
         * Note: we have to make sure that we root obj around the call to
         * JS_Enumerate to protect against multiple allocations under it.
         */
        AutoValueRooter tvr(cx, iterobj);
        ida = JS_Enumerate(cx, obj);
        if (!ida)
            return NULL;
        pdata = ida;
        index = ida->length;
    }

    /* iterobj cannot escape to other threads here. */
    iterobj->setPrivate(pdata);
    iterobj->fslots[JSSLOT_ITER_INDEX] = INT_TO_JSVAL(index);
    return iterobj;
}

JS_PUBLIC_API(JSBool)
JS_NextProperty(JSContext *cx, JSObject *iterobj, jsid *idp)
{
    jsint i;
    JSObject *obj;
    JSScope *scope;
    JSScopeProperty *sprop;
    JSIdArray *ida;

    CHECK_REQUEST(cx);
    i = JSVAL_TO_INT(iterobj->fslots[JSSLOT_ITER_INDEX]);
    if (i < 0) {
        /* Native case: private data is a property tree node pointer. */
        obj = iterobj->getParent();
        JS_ASSERT(obj->isNative());
        scope = OBJ_SCOPE(obj);
        sprop = (JSScopeProperty *) iterobj->getPrivate();

        /*
         * If the next property mapped by scope in the property tree ancestor
         * line is not enumerable, or it's an alias, skip it and keep on trying
         * to find an enumerable property that is still in scope.
         */
        while (sprop && (!sprop->enumerable() || sprop->isAlias()))
            sprop = sprop->parent;

        if (!sprop) {
            *idp = JSVAL_VOID;
        } else {
            iterobj->setPrivate(sprop->parent);
            *idp = sprop->id;
        }
    } else {
        /* Non-native case: use the ida enumerated when iterobj was created. */
        ida = (JSIdArray *) iterobj->getPrivate();
        JS_ASSERT(i <= ida->length);
        if (i == 0) {
            *idp = JSVAL_VOID;
        } else {
            *idp = ida->vector[--i];
            iterobj->setSlot(JSSLOT_ITER_INDEX, INT_TO_JSVAL(i));
        }
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp)
{
    CHECK_REQUEST(cx);
    return obj->checkAccess(cx, id, mode, vp, attrsp);
}

JS_PUBLIC_API(JSBool)
JS_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    CHECK_REQUEST(cx);
    return js_GetReservedSlot(cx, obj, index, vp);
}

JS_PUBLIC_API(JSBool)
JS_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v)
{
    CHECK_REQUEST(cx);
    return js_SetReservedSlot(cx, obj, index, v);
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(jsrefcount)
JS_HoldPrincipals(JSContext *cx, JSPrincipals *principals)
{
    return JS_ATOMIC_INCREMENT(&principals->refcount);
}

JS_PUBLIC_API(jsrefcount)
JS_DropPrincipals(JSContext *cx, JSPrincipals *principals)
{
    jsrefcount rc = JS_ATOMIC_DECREMENT(&principals->refcount);
    if (rc == 0)
        principals->destroy(cx, principals);
    return rc;
}
#endif

JS_PUBLIC_API(JSSecurityCallbacks *)
JS_SetRuntimeSecurityCallbacks(JSRuntime *rt, JSSecurityCallbacks *callbacks)
{
    JSSecurityCallbacks *oldcallbacks;

    oldcallbacks = rt->securityCallbacks;
    rt->securityCallbacks = callbacks;
    return oldcallbacks;
}

JS_PUBLIC_API(JSSecurityCallbacks *)
JS_GetRuntimeSecurityCallbacks(JSRuntime *rt)
{
  return rt->securityCallbacks;
}

JS_PUBLIC_API(JSSecurityCallbacks *)
JS_SetContextSecurityCallbacks(JSContext *cx, JSSecurityCallbacks *callbacks)
{
    JSSecurityCallbacks *oldcallbacks;

    oldcallbacks = cx->securityCallbacks;
    cx->securityCallbacks = callbacks;
    return oldcallbacks;
}

JS_PUBLIC_API(JSSecurityCallbacks *)
JS_GetSecurityCallbacks(JSContext *cx)
{
  return cx->securityCallbacks
         ? cx->securityCallbacks
         : cx->runtime->securityCallbacks;
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative native, uintN nargs, uintN flags,
               JSObject *parent, const char *name)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);

    if (!name) {
        atom = NULL;
    } else {
        atom = js_Atomize(cx, name, strlen(name), 0);
        if (!atom)
            return NULL;
    }
    return js_NewFunction(cx, NULL, native, nargs, flags, parent, atom);
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent)
{
    CHECK_REQUEST(cx);
    if (!parent) {
        if (cx->fp)
            parent = js_GetScopeChain(cx, cx->fp);
        if (!parent)
            parent = cx->globalObject;
        JS_ASSERT(parent);
    }

    if (OBJ_GET_CLASS(cx, funobj) != &js_FunctionClass) {
        /*
         * We cannot clone this object, so fail (we used to return funobj, bad
         * idea, but we changed incompatibly to teach any abusers a lesson!).
         */
        jsval v = OBJECT_TO_JSVAL(funobj);
        js_ReportIsNotFunction(cx, &v, 0);
        return NULL;
    }

    JSFunction *fun = GET_FUNCTION_PRIVATE(cx, funobj);
    JSObject *clone = CloneFunctionObject(cx, fun, parent);
    if (!clone)
        return NULL;

    /*
     * A flat closure carries its own environment, so why clone it? In case
     * someone wants to mutate its fixed slots or add ad-hoc properties. API
     * compatibility suggests we not return funobj and let callers mutate the
     * returned object at will.
     *
     * But it's worse than that: API compatibility according to the test for
     * bug 300079 requires we get "upvars" from parent and its ancestors! So
     * we do that (grudgingly!). The scope chain ancestors are searched as if
     * they were activations, respecting the skip field in each upvar's cookie
     * but looking up the property by name instead of frame slot.
     */
    if (FUN_FLAT_CLOSURE(fun)) {
        JS_ASSERT(funobj->dslots);
        if (!js_EnsureReservedSlots(cx, clone,
                                    fun->countInterpretedReservedSlots())) {
            return NULL;
        }

        JSUpvarArray *uva = fun->u.i.script->upvars();
        JS_ASSERT(uva->length <= size_t(clone->dslots[-1]));

        void *mark = JS_ARENA_MARK(&cx->tempPool);
        jsuword *names = js_GetLocalNameArray(cx, fun, &cx->tempPool);
        if (!names)
            return NULL;

        uint32 i = 0, n = uva->length;
        for (; i < n; i++) {
            JSObject *obj = parent;
            int skip = UPVAR_FRAME_SKIP(uva->vector[i]);
            while (--skip > 0) {
                if (!obj) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_BAD_CLONE_FUNOBJ_SCOPE);
                    goto break2;
                }
                obj = obj->getParent();
            }

            JSAtom *atom = JS_LOCAL_NAME_TO_ATOM(names[i]);
            if (!obj->getProperty(cx, ATOM_TO_JSID(atom), &clone->dslots[i]))
                break;
        }

      break2:
        JS_ARENA_RELEASE(&cx->tempPool, mark);
        if (i < n)
            return NULL;
    }

    return clone;
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return FUN_OBJECT(fun);
}

JS_PUBLIC_API(const char *)
JS_GetFunctionName(JSFunction *fun)
{
    return fun->atom
           ? JS_GetStringBytes(ATOM_TO_STRING(fun->atom))
           : js_anonymous_str;
}

JS_PUBLIC_API(JSString *)
JS_GetFunctionId(JSFunction *fun)
{
    return fun->atom ? ATOM_TO_STRING(fun->atom) : NULL;
}

JS_PUBLIC_API(uintN)
JS_GetFunctionFlags(JSFunction *fun)
{
    return fun->flags;
}

JS_PUBLIC_API(uint16)
JS_GetFunctionArity(JSFunction *fun)
{
    return fun->nargs;
}

JS_PUBLIC_API(JSBool)
JS_ObjectIsFunction(JSContext *cx, JSObject *obj)
{
    return OBJ_GET_CLASS(cx, obj) == &js_FunctionClass;
}

JS_BEGIN_EXTERN_C
static JSBool
js_generic_fast_native_method_dispatcher(JSContext *cx, uintN argc, jsval *vp)
{
    jsval fsv;
    JSFunctionSpec *fs;
    JSObject *tmp;
    JSFastNative native;

    if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(*vp), 0, &fsv))
        return JS_FALSE;
    fs = (JSFunctionSpec *) JSVAL_TO_PRIVATE(fsv);
    JS_ASSERT((~fs->flags & (JSFUN_FAST_NATIVE | JSFUN_GENERIC_NATIVE)) == 0);

    /*
     * We know that vp[2] is valid because JS_DefineFunctions, which is our
     * only (indirect) referrer, defined us as requiring at least one argument
     * (notice how it passes fs->nargs + 1 as the next-to-last argument to
     * JS_DefineFunction).
     */
    if (JSVAL_IS_PRIMITIVE(vp[2])) {
        /*
         * Make sure that this is an object or null, as required by the generic
         * functions.
         */
        if (!js_ValueToObject(cx, vp[2], &tmp))
            return JS_FALSE;
        vp[2] = OBJECT_TO_JSVAL(tmp);
    }

    /*
     * Copy all actual (argc) arguments down over our |this| parameter, vp[1],
     * which is almost always the class constructor object, e.g. Array.  Then
     * call the corresponding prototype native method with our first argument
     * passed as |this|.
     */
    memmove(vp + 1, vp + 2, argc * sizeof(jsval));

    /*
     * Follow Function.prototype.apply and .call by using the global object as
     * the 'this' param if no args.
     */
    if (!js_ComputeThis(cx, vp + 2))
        return JS_FALSE;
    /*
     * Protect against argc underflowing. By calling js_ComputeThis, we made
     * it as if the static was called with one parameter, the explicit |this|
     * object.
     */
    if (argc != 0) {
        /* Clear the last parameter in case too few arguments were passed. */
        vp[2 + --argc] = JSVAL_VOID;
    }

    native =
#ifdef JS_TRACER
             (fs->flags & JSFUN_TRCINFO)
             ? JS_FUNC_TO_DATA_PTR(JSNativeTraceInfo *, fs->call)->native
             :
#endif
               (JSFastNative) fs->call;
    return native(cx, argc, vp);
}

static JSBool
js_generic_native_method_dispatcher(JSContext *cx, JSObject *obj,
                                    uintN argc, jsval *argv, jsval *rval)
{
    jsval fsv;
    JSFunctionSpec *fs;
    JSObject *tmp;

    if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(argv[-2]), 0, &fsv))
        return JS_FALSE;
    fs = (JSFunctionSpec *) JSVAL_TO_PRIVATE(fsv);
    JS_ASSERT((fs->flags & (JSFUN_FAST_NATIVE | JSFUN_GENERIC_NATIVE)) ==
              JSFUN_GENERIC_NATIVE);

    /*
     * We know that argv[0] is valid because JS_DefineFunctions, which is our
     * only (indirect) referrer, defined us as requiring at least one argument
     * (notice how it passes fs->nargs + 1 as the next-to-last argument to
     * JS_DefineFunction).
     */
    if (JSVAL_IS_PRIMITIVE(argv[0])) {
        /*
         * Make sure that this is an object or null, as required by the generic
         * functions.
         */
        if (!js_ValueToObject(cx, argv[0], &tmp))
            return JS_FALSE;
        argv[0] = OBJECT_TO_JSVAL(tmp);
    }

    /*
     * Copy all actual (argc) arguments down over our |this| parameter,
     * argv[-1], which is almost always the class constructor object, e.g.
     * Array.  Then call the corresponding prototype native method with our
     * first argument passed as |this|.
     */
    memmove(argv - 1, argv, argc * sizeof(jsval));

    /*
     * Follow Function.prototype.apply and .call by using the global object as
     * the 'this' param if no args.
     */
    if (!js_ComputeThis(cx, argv))
        return JS_FALSE;
    js_GetTopStackFrame(cx)->thisv = argv[-1];
    JS_ASSERT(cx->fp->argv == argv);

    /*
     * Protect against argc underflowing. By calling js_ComputeThis, we made
     * it as if the static was called with one parameter, the explicit |this|
     * object.
     */
    if (argc != 0) {
        /* Clear the last parameter in case too few arguments were passed. */
        argv[--argc] = JSVAL_VOID;
    }

    return fs->call(cx, JSVAL_TO_OBJECT(argv[-1]), argc, argv, rval);
}
JS_END_EXTERN_C

JS_PUBLIC_API(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs)
{
    uintN flags;
    JSObject *ctor;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    ctor = NULL;
    for (; fs->name; fs++) {
        flags = fs->flags;

        /*
         * Define a generic arity N+1 static method for the arity N prototype
         * method if flags contains JSFUN_GENERIC_NATIVE.
         */
        if (flags & JSFUN_GENERIC_NATIVE) {
            if (!ctor) {
                ctor = JS_GetConstructor(cx, obj);
                if (!ctor)
                    return JS_FALSE;
            }

            flags &= ~JSFUN_GENERIC_NATIVE;
            fun = JS_DefineFunction(cx, ctor, fs->name,
                                    (flags & JSFUN_FAST_NATIVE)
                                    ? (JSNative)
                                      js_generic_fast_native_method_dispatcher
                                    : js_generic_native_method_dispatcher,
                                    fs->nargs + 1,
                                    flags & ~JSFUN_TRCINFO);
            if (!fun)
                return JS_FALSE;
            fun->u.n.extra = (uint16)fs->extra;

            /*
             * As jsapi.h notes, fs must point to storage that lives as long
             * as fun->object lives.
             */
            if (!JS_SetReservedSlot(cx, FUN_OBJECT(fun), 0, PRIVATE_TO_JSVAL(fs)))
                return JS_FALSE;
        }

        JS_ASSERT(!(flags & JSFUN_FAST_NATIVE) ||
                  (uint16)(fs->extra >> 16) <= fs->nargs);
        fun = JS_DefineFunction(cx, obj, fs->name, fs->call, fs->nargs, flags);
        if (!fun)
            return JS_FALSE;
        fun->u.n.extra = (uint16)fs->extra;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
                  uintN nargs, uintN attrs)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return NULL;
    return js_DefineFunction(cx, obj, atom, call, nargs, attrs);
}

JS_PUBLIC_API(JSFunction *)
JS_DefineUCFunction(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, JSNative call,
                    uintN nargs, uintN attrs)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen), 0);
    if (!atom)
        return NULL;
    return js_DefineFunction(cx, obj, atom, call, nargs, attrs);
}

JS_PUBLIC_API(JSScript *)
JS_CompileScript(JSContext *cx, JSObject *obj,
                 const char *bytes, size_t length,
                 const char *filename, uintN lineno)
{
    jschar *chars;
    JSScript *script;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    script = JS_CompileUCScript(cx, obj, chars, length, filename, lineno);
    cx->free(chars);
    return script;
}

JS_PUBLIC_API(JSScript *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
                              JSPrincipals *principals,
                              const char *bytes, size_t length,
                              const char *filename, uintN lineno)
{
    jschar *chars;
    JSScript *script;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    script = JS_CompileUCScriptForPrincipals(cx, obj, principals,
                                             chars, length, filename, lineno);
    cx->free(chars);
    return script;
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
                   const jschar *chars, size_t length,
                   const char *filename, uintN lineno)
{
    CHECK_REQUEST(cx);
    return JS_CompileUCScriptForPrincipals(cx, obj, NULL, chars, length,
                                           filename, lineno);
}

#define LAST_FRAME_EXCEPTION_CHECK(cx,result)                                 \
    JS_BEGIN_MACRO                                                            \
        if (!(result) && !((cx)->options & JSOPTION_DONT_REPORT_UNCAUGHT))    \
            js_ReportUncaughtException(cx);                                   \
    JS_END_MACRO

#define LAST_FRAME_CHECKS(cx,result)                                          \
    JS_BEGIN_MACRO                                                            \
        if (!JS_IsRunning(cx)) {                                              \
            (cx)->weakRoots.lastInternalResult = JSVAL_NULL;                  \
            LAST_FRAME_EXCEPTION_CHECK(cx, result);                           \
        }                                                                     \
    JS_END_MACRO

#define JS_OPTIONS_TO_TCFLAGS(cx)                                             \
    ((((cx)->options & JSOPTION_COMPILE_N_GO) ? TCF_COMPILE_N_GO : 0) |       \
     (((cx)->options & JSOPTION_NO_SCRIPT_RVAL) ? TCF_NO_SCRIPT_RVAL : 0))

JS_PUBLIC_API(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals,
                                const jschar *chars, size_t length,
                                const char *filename, uintN lineno)
{
    uint32 tcflags;
    JSScript *script;

    CHECK_REQUEST(cx);
    tcflags = JS_OPTIONS_TO_TCFLAGS(cx) | TCF_NEED_MUTABLE_SCRIPT;
    script = JSCompiler::compileScript(cx, obj, NULL, principals, tcflags,
                                       chars, length, NULL, filename, lineno);
    LAST_FRAME_CHECKS(cx, script);
    return script;
}

JS_PUBLIC_API(JSBool)
JS_BufferIsCompilableUnit(JSContext *cx, JSObject *obj,
                          const char *bytes, size_t length)
{
    jschar *chars;
    JSBool result;
    JSExceptionState *exnState;
    JSErrorReporter older;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return JS_TRUE;

    /*
     * Return true on any out-of-memory error, so our caller doesn't try to
     * collect more buffered source.
     */
    result = JS_TRUE;
    exnState = JS_SaveExceptionState(cx);
    {
        JSCompiler jsc(cx);
        if (jsc.init(chars, length, NULL, NULL, 1)) {
            older = JS_SetErrorReporter(cx, NULL);
            if (!jsc.parse(obj) &&
                (jsc.tokenStream.flags & TSF_UNEXPECTED_EOF)) {
                /*
                 * We ran into an error. If it was because we ran out of
                 * source, we return false so our caller knows to try to
                 * collect more buffered source.
                 */
                result = JS_FALSE;
            }
            JS_SetErrorReporter(cx, older);
        }
    }
    cx->free(chars);
    JS_RestoreExceptionState(cx, exnState);
    return result;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename)
{
    FILE *fp;
    uint32 tcflags;
    JSScript *script;

    CHECK_REQUEST(cx);
    if (!filename || strcmp(filename, "-") == 0) {
        fp = stdin;
    } else {
        fp = fopen(filename, "r");
        if (!fp) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_OPEN,
                                 filename, "No such file or directory");
            return NULL;
        }
    }

    tcflags = JS_OPTIONS_TO_TCFLAGS(cx);
    script = JSCompiler::compileScript(cx, obj, NULL, NULL, tcflags,
                                       NULL, 0, fp, filename, 1);
    if (fp != stdin)
        fclose(fp);
    LAST_FRAME_CHECKS(cx, script);
    return script;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandle(JSContext *cx, JSObject *obj, const char *filename,
                     FILE *file)
{
    return JS_CompileFileHandleForPrincipals(cx, obj, filename, file, NULL);
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandleForPrincipals(JSContext *cx, JSObject *obj,
                                  const char *filename, FILE *file,
                                  JSPrincipals *principals)
{
    uint32 tcflags;
    JSScript *script;

    CHECK_REQUEST(cx);
    tcflags = JS_OPTIONS_TO_TCFLAGS(cx);
    script = JSCompiler::compileScript(cx, obj, NULL, principals, tcflags,
                                       NULL, 0, file, filename, 1);
    LAST_FRAME_CHECKS(cx, script);
    return script;
}

JS_PUBLIC_API(JSObject *)
JS_NewScriptObject(JSContext *cx, JSScript *script)
{
    JSObject *obj;

    CHECK_REQUEST(cx);
    if (!script)
        return js_NewObject(cx, &js_ScriptClass, NULL, NULL);

    JS_ASSERT(!script->u.object);

    {
        AutoScriptRooter root(cx, script);

        obj = js_NewObject(cx, &js_ScriptClass, NULL, NULL);
        if (obj) {
            obj->setPrivate(script);
            script->u.object = obj;
#ifdef CHECK_SCRIPT_OWNER
            script->owner = NULL;
#endif
        }
    }

    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_GetScriptObject(JSScript *script)
{
    return script->u.object;
}

JS_PUBLIC_API(void)
JS_DestroyScript(JSContext *cx, JSScript *script)
{
    CHECK_REQUEST(cx);
    js_DestroyScript(cx, script);
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
                   uintN nargs, const char **argnames,
                   const char *bytes, size_t length,
                   const char *filename, uintN lineno)
{
    jschar *chars;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    fun = JS_CompileUCFunction(cx, obj, name, nargs, argnames, chars, length,
                               filename, lineno);
    cx->free(chars);
    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals, const char *name,
                                uintN nargs, const char **argnames,
                                const char *bytes, size_t length,
                                const char *filename, uintN lineno)
{
    jschar *chars;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    fun = JS_CompileUCFunctionForPrincipals(cx, obj, principals, name,
                                            nargs, argnames, chars, length,
                                            filename, lineno);
    cx->free(chars);
    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
                     uintN nargs, const char **argnames,
                     const jschar *chars, size_t length,
                     const char *filename, uintN lineno)
{
    CHECK_REQUEST(cx);
    return JS_CompileUCFunctionForPrincipals(cx, obj, NULL, name,
                                             nargs, argnames,
                                             chars, length,
                                             filename, lineno);
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                  JSPrincipals *principals, const char *name,
                                  uintN nargs, const char **argnames,
                                  const jschar *chars, size_t length,
                                  const char *filename, uintN lineno)
{
    JSFunction *fun;
    JSAtom *funAtom, *argAtom;
    uintN i;

    CHECK_REQUEST(cx);
    if (!name) {
        funAtom = NULL;
    } else {
        funAtom = js_Atomize(cx, name, strlen(name), 0);
        if (!funAtom) {
            fun = NULL;
            goto out2;
        }
    }
    fun = js_NewFunction(cx, NULL, NULL, 0, JSFUN_INTERPRETED, obj, funAtom);
    if (!fun)
        goto out2;

    {
        AutoValueRooter tvr(cx, FUN_OBJECT(fun));
        MUST_FLOW_THROUGH("out");

        for (i = 0; i < nargs; i++) {
            argAtom = js_Atomize(cx, argnames[i], strlen(argnames[i]), 0);
            if (!argAtom) {
                fun = NULL;
                goto out;
            }
            if (!js_AddLocal(cx, fun, argAtom, JSLOCAL_ARG)) {
                fun = NULL;
                goto out;
            }
        }

        if (!JSCompiler::compileFunctionBody(cx, fun, principals,
                                             chars, length, filename, lineno)) {
            fun = NULL;
            goto out;
        }

        if (obj && funAtom &&
            !obj->defineProperty(cx, ATOM_TO_JSID(funAtom), OBJECT_TO_JSVAL(FUN_OBJECT(fun)),
                                 NULL, NULL, JSPROP_ENUMERATE)) {
            fun = NULL;
        }

#ifdef JS_SCOPE_DEPTH_METER
        if (fun && obj) {
            JSObject *pobj = obj;
            uintN depth = 1;

            while ((pobj = pobj->getParent()) != NULL)
                ++depth;
            JS_BASIC_STATS_ACCUM(&cx->runtime->hostenvScopeDepthStats, depth);
        }
#endif

      out:
        cx->weakRoots.finalizableNewborns[FINALIZE_FUNCTION] = fun;
    }

  out2:
    LAST_FRAME_CHECKS(cx, fun);
    return fun;
}

JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name,
                   uintN indent)
{
    JSPrinter *jp;
    JSString *str;

    CHECK_REQUEST(cx);
    jp = js_NewPrinter(cx, name, NULL,
                       indent & ~JS_DONT_PRETTY_PRINT,
                       !(indent & JS_DONT_PRETTY_PRINT),
                       false, false);
    if (!jp)
        return NULL;
    if (js_DecompileScript(jp, script))
        str = js_GetPrinterOutput(jp);
    else
        str = NULL;
    js_DestroyPrinter(jp);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent)
{
    CHECK_REQUEST(cx);
    return js_DecompileToString(cx, "JS_DecompileFunction", fun,
                                indent & ~JS_DONT_PRETTY_PRINT,
                                !(indent & JS_DONT_PRETTY_PRINT),
                                false, false, js_DecompileFunction);
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent)
{
    CHECK_REQUEST(cx);
    return js_DecompileToString(cx, "JS_DecompileFunctionBody", fun,
                                indent & ~JS_DONT_PRETTY_PRINT,
                                !(indent & JS_DONT_PRETTY_PRINT),
                                false, false, js_DecompileFunctionBody);
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    ok = js_Execute(cx, obj, script, NULL, 0, rval);
    LAST_FRAME_CHECKS(cx, ok);
    return ok;
}

/* Ancient uintN nbytes is part of API/ABI, so use size_t length local. */
JS_PUBLIC_API(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
                  const char *bytes, uintN nbytes,
                  const char *filename, uintN lineno,
                  jsval *rval)
{
    size_t length = nbytes;
    jschar *chars;
    JSBool ok;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return JS_FALSE;
    ok = JS_EvaluateUCScript(cx, obj, chars, length, filename, lineno, rval);
    cx->free(chars);
    return ok;
}

/* Ancient uintN nbytes is part of API/ABI, so use size_t length local. */
JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
                               JSPrincipals *principals,
                               const char *bytes, uintN nbytes,
                               const char *filename, uintN lineno,
                               jsval *rval)
{
    size_t length = nbytes;
    jschar *chars;
    JSBool ok;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return JS_FALSE;
    ok = JS_EvaluateUCScriptForPrincipals(cx, obj, principals, chars, length,
                                          filename, lineno, rval);
    cx->free(chars);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
                    const jschar *chars, uintN length,
                    const char *filename, uintN lineno,
                    jsval *rval)
{
    CHECK_REQUEST(cx);
    return JS_EvaluateUCScriptForPrincipals(cx, obj, NULL, chars, length,
                                            filename, lineno, rval);
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                 JSPrincipals *principals,
                                 const jschar *chars, uintN length,
                                 const char *filename, uintN lineno,
                                 jsval *rval)
{
    JSScript *script;
    JSBool ok;

    CHECK_REQUEST(cx);
    script = JSCompiler::compileScript(cx, obj, NULL, principals,
                                       !rval
                                       ? TCF_COMPILE_N_GO | TCF_NO_SCRIPT_RVAL
                                       : TCF_COMPILE_N_GO,
                                       chars, length, NULL, filename, lineno);
    if (!script) {
        LAST_FRAME_CHECKS(cx, script);
        return JS_FALSE;
    }
    ok = js_Execute(cx, obj, script, NULL, 0, rval);
    LAST_FRAME_CHECKS(cx, ok);
    JS_DestroyScript(cx, script);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
                jsval *argv, jsval *rval)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    ok = js_InternalCall(cx, obj, OBJECT_TO_JSVAL(FUN_OBJECT(fun)), argc, argv,
                         rval);
    LAST_FRAME_CHECKS(cx, ok);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
                    jsval *argv, jsval *rval)
{
    CHECK_REQUEST(cx);

    AutoValueRooter tvr(cx);
    JSAtom *atom = js_Atomize(cx, name, strlen(name), 0);
    JSBool ok = atom &&
                js_GetMethod(cx, obj, ATOM_TO_JSID(atom),
                             JSGET_NO_METHOD_BARRIER, tvr.addr()) &&
                js_InternalCall(cx, obj, tvr.value(), argc, argv, rval);
    LAST_FRAME_CHECKS(cx, ok);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
                     jsval *argv, jsval *rval)
{
    JSBool ok;

    CHECK_REQUEST(cx);
    ok = js_InternalCall(cx, obj, fval, argc, argv, rval);
    LAST_FRAME_CHECKS(cx, ok);
    return ok;
}

JS_PUBLIC_API(JSObject *)
JS_New(JSContext *cx, JSObject *ctor, uintN argc, jsval *argv)
{
    CHECK_REQUEST(cx);

    // This is not a simple variation of JS_CallFunctionValue because JSOP_NEW
    // is not a simple variation of JSOP_CALL. We have to determine what class
    // of object to create, create it, and clamp the return value to an object,
    // among other details. js_InvokeConstructor does the hard work.
    void *mark;
    jsval *vp = js_AllocStack(cx, 2 + argc, &mark);
    if (!vp)
        return NULL;
    vp[0] = OBJECT_TO_JSVAL(ctor);
    vp[1] = JSVAL_NULL;
    memcpy(vp + 2, argv, argc * sizeof(jsval));

    JSBool ok = js_InvokeConstructor(cx, argc, JS_TRUE, vp);
    JSObject *obj = ok ? JSVAL_TO_OBJECT(vp[0]) : NULL;

    js_FreeStack(cx, mark);
    LAST_FRAME_CHECKS(cx, ok);
    return obj;
}

JS_PUBLIC_API(JSOperationCallback)
JS_SetOperationCallback(JSContext *cx, JSOperationCallback callback)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
#endif
    JSOperationCallback old = cx->operationCallback;
    cx->operationCallback = callback;
    return old;
}

JS_PUBLIC_API(JSOperationCallback)
JS_GetOperationCallback(JSContext *cx)
{
    return cx->operationCallback;
}

JS_PUBLIC_API(void)
JS_TriggerOperationCallback(JSContext *cx)
{
    /*
     * Use JS_ATOMIC_SET in the hope that it will make sure the write
     * will become immediately visible to other processors polling
     * cx->operationCallbackFlag. Note that we only care about
     * visibility here, not read/write ordering.
     */
    JS_ATOMIC_SET(&cx->operationCallbackFlag, 1);
}

JS_PUBLIC_API(void)
JS_TriggerAllOperationCallbacks(JSRuntime *rt)
{
    js_TriggerAllOperationCallbacks(rt, JS_FALSE);
}

JS_PUBLIC_API(JSBool)
JS_IsRunning(JSContext *cx)
{
    /*
     * The use of cx->fp below is safe. Rationale: Here we don't care if the
     * interpreter state is stale. We just want to know if there *is* any
     * interpreter state.
     */
    VOUCH_DOES_NOT_REQUIRE_STACK();

#ifdef JS_TRACER
    JS_ASSERT_IF(JS_TRACE_MONITOR(cx).tracecx == cx, cx->fp);
#endif
    return cx->fp != NULL;
}



JS_PUBLIC_API(JSBool)
JS_IsConstructing(JSContext *cx)
{
    return cx->isConstructing();
}

JS_PUBLIC_API(JSStackFrame *)
JS_SaveFrameChain(JSContext *cx)
{
    CHECK_REQUEST(cx);
    JSStackFrame *fp = js_GetTopStackFrame(cx);
    if (!fp)
        return NULL;
    cx->saveActiveCallStack();
    return fp;
}

JS_PUBLIC_API(void)
JS_RestoreFrameChain(JSContext *cx, JSStackFrame *fp)
{
    CHECK_REQUEST(cx);
    JS_ASSERT_NOT_ON_TRACE(cx);
    JS_ASSERT(!cx->fp);
    if (!fp)
        return;
    cx->restoreCallStack();
}

/************************************************************************/

JS_PUBLIC_API(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t nbytes)
{
    size_t length = nbytes;
    jschar *chars;
    JSString *str;

    CHECK_REQUEST(cx);

    /* Make a UTF-16 vector from the 8-bit char codes in bytes. */
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;

    /* Free chars (but not bytes, which caller frees on error) if we fail. */
    str = js_NewString(cx, chars, length);
    if (!str) {
        cx->free(chars);
        return NULL;
    }

    /* Hand off bytes to the deflated string cache, if possible. */
    if (!cx->runtime->deflatedStringCache->setBytes(cx, str, bytes))
        cx->free(bytes);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    jschar *js;
    JSString *str;

    CHECK_REQUEST(cx);
    js = js_InflateString(cx, s, &n);
    if (!js)
        return NULL;
    str = js_NewString(cx, js, n);
    if (!str)
        cx->free(js);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    size_t n;
    jschar *js;
    JSString *str;

    CHECK_REQUEST(cx);
    if (!s)
        return cx->runtime->emptyString;
    n = strlen(s);
    js = js_InflateString(cx, s, &n);
    if (!js)
        return NULL;
    str = js_NewString(cx, js, n);
    if (!str)
        cx->free(js);
    return str;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_Atomize(cx, s, strlen(s), ATOM_INTERNED);
    if (!atom)
        return NULL;
    return ATOM_TO_STRING(atom);
}

JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length)
{
    CHECK_REQUEST(cx);
    return js_NewString(cx, chars, length);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    CHECK_REQUEST(cx);
    return js_NewStringCopyN(cx, s, n);
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s)
{
    CHECK_REQUEST(cx);
    if (!s)
        return cx->runtime->emptyString;
    return js_NewStringCopyZ(cx, s);
}

JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const jschar *s, size_t length)
{
    JSAtom *atom;

    CHECK_REQUEST(cx);
    atom = js_AtomizeChars(cx, s, length, ATOM_INTERNED);
    if (!atom)
        return NULL;
    return ATOM_TO_STRING(atom);
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
    return JS_InternUCStringN(cx, s, js_strlen(s));
}

JS_PUBLIC_API(char *)
JS_GetStringBytes(JSString *str)
{
    const char *bytes;

    bytes = js_GetStringBytes(NULL, str);
    return (char *)(bytes ? bytes : "");
}

JS_PUBLIC_API(jschar *)
JS_GetStringChars(JSString *str)
{
    size_t n, size;
    jschar *s;

    /*
     * API botch (again, shades of JS_GetStringBytes): we have no cx to report
     * out-of-memory when undepending strings, so we replace js_UndependString
     * with explicit malloc call and ignore its errors.
     *
     * If we fail to convert a dependent string into an independent one, our
     * caller will not be guaranteed a \u0000 terminator as a backstop.  This
     * may break some clients who already misbehave on embedded NULs.
     *
     * The gain of dependent strings, which cure quadratic and cubic growth
     * rate bugs in string concatenation, is worth this slight loss in API
     * compatibility.
     */
    if (str->isDependent()) {
        n = str->dependentLength();
        size = (n + 1) * sizeof(jschar);
        s = (jschar *) js_malloc(size);
        if (s) {
            memcpy(s, str->dependentChars(), n * sizeof *s);
            s[n] = 0;
            str->initFlat(s, n);
        } else {
            s = str->dependentChars();
        }
    } else {
        str->flatClearMutable();
        s = str->flatChars();
    }
    return s;
}

JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str)
{
    return str->length();
}

JS_PUBLIC_API(const char *)
JS_GetStringBytesZ(JSContext *cx, JSString *str)
{
    return js_GetStringBytes(cx, str);
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsZ(JSContext *cx, JSString *str)
{
    return js_UndependString(cx, str);
}

JS_PUBLIC_API(intN)
JS_CompareStrings(JSString *str1, JSString *str2)
{
    return js_CompareStrings(str1, str2);
}

JS_PUBLIC_API(JSString *)
JS_NewGrowableString(JSContext *cx, jschar *chars, size_t length)
{
    JSString *str;

    CHECK_REQUEST(cx);
    str = js_NewString(cx, chars, length);
    if (!str)
        return str;
    str->flatSetMutable();
    return str;
}

JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, JSString *str, size_t start,
                      size_t length)
{
    CHECK_REQUEST(cx);
    return js_NewDependentString(cx, str, start, length);
}

JS_PUBLIC_API(JSString *)
JS_ConcatStrings(JSContext *cx, JSString *left, JSString *right)
{
    CHECK_REQUEST(cx);
    return js_ConcatStrings(cx, left, right);
}

JS_PUBLIC_API(const jschar *)
JS_UndependString(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    return js_UndependString(cx, str);
}

JS_PUBLIC_API(JSBool)
JS_MakeStringImmutable(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    return js_MakeStringImmutable(cx, str);
}

JS_PUBLIC_API(JSBool)
JS_EncodeCharacters(JSContext *cx, const jschar *src, size_t srclen, char *dst,
                    size_t *dstlenp)
{
    size_t n;

    if (!dst) {
        n = js_GetDeflatedStringLength(cx, src, srclen);
        if (n == (size_t)-1) {
            *dstlenp = 0;
            return JS_FALSE;
        }
        *dstlenp = n;
        return JS_TRUE;
    }

    return js_DeflateStringToBuffer(cx, src, srclen, dst, dstlenp);
}

JS_PUBLIC_API(JSBool)
JS_DecodeBytes(JSContext *cx, const char *src, size_t srclen, jschar *dst,
               size_t *dstlenp)
{
    return js_InflateStringToBuffer(cx, src, srclen, dst, dstlenp);
}

JS_PUBLIC_API(char *)
JS_EncodeString(JSContext *cx, JSString *str)
{
    return js_DeflateString(cx, str->chars(), str->length());
}

JS_PUBLIC_API(JSBool)
JS_Stringify(JSContext *cx, jsval *vp, JSObject *replacer, jsval space,
             JSONWriteCallback callback, void *data)
{
    CHECK_REQUEST(cx);
    JSCharBuffer cb(cx);
    if (!js_Stringify(cx, vp, replacer, space, cb))
        return false;
    return callback(cb.begin(), cb.length(), data);
}

JS_PUBLIC_API(JSBool)
JS_TryJSON(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    return js_TryJSON(cx, vp);
}

JS_PUBLIC_API(JSONParser *)
JS_BeginJSONParse(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    return js_BeginJSONParse(cx, vp);
}

JS_PUBLIC_API(JSBool)
JS_ConsumeJSONText(JSContext *cx, JSONParser *jp, const jschar *data, uint32 len)
{
    CHECK_REQUEST(cx);
    return js_ConsumeJSONText(cx, jp, data, len);
}

JS_PUBLIC_API(JSBool)
JS_FinishJSONParse(JSContext *cx, JSONParser *jp, jsval reviver)
{
    CHECK_REQUEST(cx);
    return js_FinishJSONParse(cx, jp, reviver);
}

/*
 * The following determines whether C Strings are to be treated as UTF-8
 * or ISO-8859-1.  For correct operation, it must be set prior to the
 * first call to JS_NewRuntime.
 */
#ifndef JS_C_STRINGS_ARE_UTF8
JSBool js_CStringsAreUTF8 = JS_FALSE;
#endif

JS_PUBLIC_API(JSBool)
JS_CStringsAreUTF8()
{
    return js_CStringsAreUTF8;
}

JS_PUBLIC_API(void)
JS_SetCStringsAreUTF8()
{
    JS_ASSERT(!js_NewRuntimeWasCalled);

#ifndef JS_C_STRINGS_ARE_UTF8
    js_CStringsAreUTF8 = JS_TRUE;
#endif
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    js_ReportErrorVA(cx, JSREPORT_ERROR, format, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...)
{
    va_list ap;

    va_start(ap, errorNumber);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
                           errorNumber, JS_TRUE, ap);
    va_end(ap);
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...)
{
    va_list ap;

    va_start(ap, errorNumber);
    js_ReportErrorNumberVA(cx, JSREPORT_ERROR, errorCallback, userRef,
                           errorNumber, JS_FALSE, ap);
    va_end(ap);
}

JS_PUBLIC_API(JSBool)
JS_ReportWarning(JSContext *cx, const char *format, ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, format);
    ok = js_ReportErrorVA(cx, JSREPORT_WARNING, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, uintN flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const uintN errorNumber, ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, JS_TRUE, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, uintN flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const uintN errorNumber, ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, errorNumber);
    ok = js_ReportErrorNumberVA(cx, flags, errorCallback, userRef,
                                errorNumber, JS_FALSE, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx)
{
    js_ReportOutOfMemory(cx);
}

JS_PUBLIC_API(void)
JS_ReportAllocationOverflow(JSContext *cx)
{
    js_ReportAllocationOverflow(cx);
}

JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er)
{
    JSErrorReporter older;

    older = cx->errorReporter;
    cx->errorReporter = er;
    return older;
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags)
{
    jschar *chars;
    JSObject *obj;

    CHECK_REQUEST(cx);
    chars = js_InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    obj = js_NewRegExpObject(cx, NULL, chars, length, flags);
    cx->free(chars);
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
    CHECK_REQUEST(cx);
    return js_NewRegExpObject(cx, NULL, chars, length, flags);
}

JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline)
{
    JSRegExpStatics *res;

    CHECK_REQUEST(cx);
    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = input;
    res->multiline = multiline;
    cx->runtime->gcPoke = JS_TRUE;
}

JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx)
{
    JSRegExpStatics *res;

    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = NULL;
    res->multiline = JS_FALSE;
    res->parenCount = 0;
    res->lastMatch = res->lastParen = js_EmptySubString;
    res->leftContext = res->rightContext = js_EmptySubString;
    if (res->moreParens) {
      cx->free(res->moreParens);
      res->moreParens = NULL;
    }
    cx->runtime->gcPoke = JS_TRUE;
}

JS_PUBLIC_API(void)
JS_ClearRegExpRoots(JSContext *cx)
{
    JSRegExpStatics *res;

    /* No locking required, cx is thread-private and input must be live. */
    res = &cx->regExpStatics;
    res->input = NULL;
    cx->runtime->gcPoke = JS_TRUE;
}

/* TODO: compile, execute, get/set other statics... */

/************************************************************************/

JS_PUBLIC_API(void)
JS_SetLocaleCallbacks(JSContext *cx, JSLocaleCallbacks *callbacks)
{
    cx->localeCallbacks = callbacks;
}

JS_PUBLIC_API(JSLocaleCallbacks *)
JS_GetLocaleCallbacks(JSContext *cx)
{
    return cx->localeCallbacks;
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_IsExceptionPending(JSContext *cx)
{
    return (JSBool) cx->throwing;
}

JS_PUBLIC_API(JSBool)
JS_GetPendingException(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    if (!cx->throwing)
        return JS_FALSE;
    *vp = cx->exception;
    return JS_TRUE;
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    cx->throwing = JS_TRUE;
    cx->exception = v;
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
    cx->throwing = JS_FALSE;
    cx->exception = JSVAL_VOID;
}

JS_PUBLIC_API(JSBool)
JS_ReportPendingException(JSContext *cx)
{
    JSBool ok;
    JSPackedBool save;

    CHECK_REQUEST(cx);

    /*
     * Set cx->generatingError to suppress the standard error-to-exception
     * conversion done by all {js,JS}_Report* functions except for OOM.  The
     * cx->generatingError flag was added to suppress recursive divergence
     * under js_ErrorToException, but it serves for our purposes here too.
     */
    save = cx->generatingError;
    cx->generatingError = JS_TRUE;
    ok = js_ReportUncaughtException(cx);
    cx->generatingError = save;
    return ok;
}

struct JSExceptionState {
    JSBool throwing;
    jsval  exception;
};

JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx)
{
    JSExceptionState *state;

    CHECK_REQUEST(cx);
    state = (JSExceptionState *) cx->malloc(sizeof(JSExceptionState));
    if (state) {
        state->throwing = JS_GetPendingException(cx, &state->exception);
        if (state->throwing && JSVAL_IS_GCTHING(state->exception))
            js_AddRoot(cx, &state->exception, "JSExceptionState.exception");
    }
    return state;
}

JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state)
{
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing)
            JS_SetPendingException(cx, state->exception);
        else
            JS_ClearPendingException(cx);
        JS_DropExceptionState(cx, state);
    }
}

JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state)
{
    CHECK_REQUEST(cx);
    if (state) {
        if (state->throwing && JSVAL_IS_GCTHING(state->exception))
            JS_RemoveRoot(cx, &state->exception);
        cx->free(state);
    }
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return js_ErrorFromException(cx, v);
}

JS_PUBLIC_API(JSBool)
JS_ThrowReportedError(JSContext *cx, const char *message,
                      JSErrorReport *reportp)
{
    return JS_IsRunning(cx) &&
           js_ErrorToException(cx, message, reportp, NULL, NULL);
}

JS_PUBLIC_API(JSBool)
JS_ThrowStopIteration(JSContext *cx)
{
    return js_ThrowStopIteration(cx);
}

/*
 * Get the owning thread id of a context. Returns 0 if the context is not
 * owned by any thread.
 */
JS_PUBLIC_API(jsword)
JS_GetContextThread(JSContext *cx)
{
#ifdef JS_THREADSAFE
    return JS_THREAD_ID(cx);
#else
    return 0;
#endif
}

/*
 * Set the current thread as the owning thread of a context. Returns the
 * old owning thread id, or -1 if the operation failed.
 */
JS_PUBLIC_API(jsword)
JS_SetContextThread(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->requestDepth == 0);
    if (cx->thread) {
        JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
        return cx->thread->id;
    }

    if (!js_InitContextThread(cx)) {
        js_ReportOutOfMemory(cx);
        return -1;
    }

    /* Here the GC lock is still held after js_InitContextThread took it. */
    JS_UNLOCK_GC(cx->runtime);
#endif
    return 0;
}

JS_PUBLIC_API(jsword)
JS_ClearContextThread(JSContext *cx)
{
#ifdef JS_THREADSAFE
    /*
     * This must be called outside a request and, if cx is associated with a
     * thread, this must be called only from that thread.  If not, this is a
     * harmless no-op.
     */
    JS_ASSERT(cx->requestDepth == 0);
    if (!cx->thread)
        return 0;
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread));
    jsword old = cx->thread->id;

    /*
     * We must not race with a GC that accesses cx->thread for all threads,
     * see bug 476934.
     */
    JSRuntime *rt = cx->runtime;
    JS_LOCK_GC(rt);
    js_WaitForGC(rt);
    js_ClearContextThread(cx);
    JS_UNLOCK_GC(rt);
    return old;
#else
    return 0;
#endif
}

#ifdef JS_GC_ZEAL
JS_PUBLIC_API(void)
JS_SetGCZeal(JSContext *cx, uint8 zeal)
{
    cx->runtime->gcZeal = zeal;
}
#endif

/************************************************************************/

#if !defined(STATIC_JS_API) && defined(XP_WIN) && !defined (WINCE)

#include <windows.h>

/*
 * Initialization routine for the JS DLL.
 */
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#endif
