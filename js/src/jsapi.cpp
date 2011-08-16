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
#include <sys/stat.h>
#include "jstypes.h"
#include "jsstdint.h"
#include "jsarena.h"
#include "jsutil.h"
#include "jsclist.h"
#include "jsdhash.h"
#include "jsprf.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jsbuiltins.h"
#include "jsclone.h"
#include "jscntxt.h"
#include "jsversion.h"
#include "jsdate.h"
#include "jsemit.h"
#include "jsexn.h"
#include "jsfun.h"
#include "jsgc.h"
#include "jsgcmark.h"
#include "jsinterp.h"
#include "jsiter.h"
#include "jslock.h"
#include "jsmath.h"
#include "jsnum.h"
#include "json.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jsparse.h"
#include "jsprobes.h"
#include "jsproxy.h"
#include "jsregexp.h"
#include "jsscope.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jstracer.h"
#include "prmjtime.h"
#include "jsstaticcheck.h"
#include "jsvector.h"
#include "jsweakmap.h"
#include "jswrapper.h"
#include "jstypedarray.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jsregexpinlines.h"
#include "jsscriptinlines.h"
#include "assembler/wtf/Platform.h"

#include "vm/Stack-inl.h"
#include "vm/String-inl.h"

#if ENABLE_YARR_JIT
#include "assembler/jit/ExecutableAllocator.h"
#include "methodjit/Logging.h"
#endif

#if JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

using namespace js;
using namespace js::gc;
using namespace js::types;

/*
 * This class is a version-establising barrier at the head of a VM entry or
 * re-entry. It ensures that:
 *
 * - |newVersion| is the starting (default) version used for the context.
 * - The starting version state is not an override.
 * - Overrides in the VM session are not propagated to the caller.
 */
class AutoVersionAPI
{
    JSContext   * const cx;
    JSVersion   oldDefaultVersion;
    bool        oldHasVersionOverride;
    JSVersion   oldVersionOverride;
#ifdef DEBUG
    uintN       oldCompileOptions;
#endif
    JSVersion   newVersion;

  public:
    explicit AutoVersionAPI(JSContext *cx, JSVersion newVersion)
      : cx(cx),
        oldDefaultVersion(cx->getDefaultVersion()),
        oldHasVersionOverride(cx->isVersionOverridden()),
        oldVersionOverride(oldHasVersionOverride ? cx->findVersion() : JSVERSION_UNKNOWN)
#ifdef DEBUG
        , oldCompileOptions(cx->getCompileOptions())
#endif
    {
        this->newVersion = newVersion;
        cx->clearVersionOverride();
        cx->setDefaultVersion(newVersion);
    }

    ~AutoVersionAPI() {
        cx->setDefaultVersion(oldDefaultVersion);
        if (oldHasVersionOverride)
            cx->overrideVersion(oldVersionOverride);
        else
            cx->clearVersionOverride();
        JS_ASSERT(oldCompileOptions == cx->getCompileOptions());
    }

    /* The version that this scoped-entity establishes. */
    JSVersion version() const { return newVersion; }
};

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) ((va_list *)(ap))
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

#ifdef JS_USE_JSVAL_JSID_STRUCT_TYPES
JS_PUBLIC_DATA(jsid) JS_DEFAULT_XML_NAMESPACE_ID = { size_t(JSID_TYPE_DEFAULT_XML_NAMESPACE) };
JS_PUBLIC_DATA(jsid) JSID_VOID  = { size_t(JSID_TYPE_VOID) };
JS_PUBLIC_DATA(jsid) JSID_EMPTY = { size_t(JSID_TYPE_OBJECT) };
#endif

#ifdef JS_USE_JSVAL_JSID_STRUCT_TYPES
JS_PUBLIC_DATA(jsval) JSVAL_NULL  = { BUILD_JSVAL(JSVAL_TAG_NULL,      0) };
JS_PUBLIC_DATA(jsval) JSVAL_ZERO  = { BUILD_JSVAL(JSVAL_TAG_INT32,     0) };
JS_PUBLIC_DATA(jsval) JSVAL_ONE   = { BUILD_JSVAL(JSVAL_TAG_INT32,     1) };
JS_PUBLIC_DATA(jsval) JSVAL_FALSE = { BUILD_JSVAL(JSVAL_TAG_BOOLEAN,   JS_FALSE) };
JS_PUBLIC_DATA(jsval) JSVAL_TRUE  = { BUILD_JSVAL(JSVAL_TAG_BOOLEAN,   JS_TRUE) };
JS_PUBLIC_DATA(jsval) JSVAL_VOID  = { BUILD_JSVAL(JSVAL_TAG_UNDEFINED, 0) };
#endif

/* Make sure that jschar is two bytes unsigned integer */
JS_STATIC_ASSERT((jschar)-1 > 0);
JS_STATIC_ASSERT(sizeof(jschar) == 2);

JS_PUBLIC_API(int64)
JS_Now()
{
    return PRMJ_Now();
}

JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    return Jsvalify(cx->runtime->NaNValue);
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    return Jsvalify(cx->runtime->negativeInfinityValue);
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    return Jsvalify(cx->runtime->positiveInfinityValue);
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    return STRING_TO_JSVAL(cx->runtime->emptyString);
}

JS_PUBLIC_API(JSString *)
JS_GetEmptyString(JSRuntime *rt)
{
    JS_ASSERT(rt->state == JSRTS_UP);
    return rt->emptyString;
}

static JSBool
TryArgumentFormatter(JSContext *cx, const char **formatp, JSBool fromJS, jsval **vpp, va_list *app)
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
JS_ConvertArguments(JSContext *cx, uintN argc, jsval *argv, const char *format, ...)
{
    va_list ap;
    JSBool ok;

    va_start(ap, format);
    ok = JS_ConvertArgumentsVA(cx, argc, argv, format, ap);
    va_end(ap);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArgumentsVA(JSContext *cx, uintN argc, jsval *argv, const char *format, va_list ap)
{
    jsval *sp;
    JSBool required;
    char c;
    JSFunction *fun;
    jsdouble d;
    JSString *str;
    JSObject *obj;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, JSValueArray(argv - 2, argc + 2));
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
                fun = js_ValueToFunction(cx, Valueify(&argv[-2]), 0);
                if (fun) {
                    char numBuf[12];
                    JS_snprintf(numBuf, sizeof numBuf, "%u", argc);
                    JSAutoByteString funNameBytes;
                    if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_MORE_ARGS_NEEDED,
                                             name, numBuf, (argc == 1) ? "" : "s");
                    }
                }
                return JS_FALSE;
            }
            break;
        }
        switch (c) {
          case 'b':
            *va_arg(ap, JSBool *) = js_ValueToBoolean(Valueify(*sp));
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
          case 'S':
          case 'W':
            str = js_ValueToString(cx, Valueify(*sp));
            if (!str)
                return JS_FALSE;
            *sp = STRING_TO_JSVAL(str);
            if (c == 'W') {
                JSFixedString *fixed = str->ensureFixed(cx);
                if (!fixed)
                    return JS_FALSE;
                *va_arg(ap, const jschar **) = fixed->chars();
            } else {
                *va_arg(ap, JSString **) = str;
            }
            break;
          case 'o':
            if (!js_ValueToObjectOrNull(cx, Valueify(*sp), &obj))
                return JS_FALSE;
            *sp = OBJECT_TO_JSVAL(obj);
            *va_arg(ap, JSObject **) = obj;
            break;
          case 'f':
            obj = js_ValueToFunctionObject(cx, Valueify(sp), 0);
            if (!obj)
                return JS_FALSE;
            *sp = OBJECT_TO_JSVAL(obj);
            *va_arg(ap, JSFunction **) = obj->getFunctionPrivate();
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
JS_AddArgumentFormatter(JSContext *cx, const char *format, JSArgumentFormatter formatter)
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
    map = (JSArgumentFormatMap *) cx->malloc_(sizeof *map);
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
            cx->free_(map);
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
    jsdouble d;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    switch (type) {
      case JSTYPE_VOID:
        *vp = JSVAL_VOID;
        ok = JS_TRUE;
        break;
      case JSTYPE_OBJECT:
        ok = js_ValueToObjectOrNull(cx, Valueify(v), &obj);
        if (ok)
            *vp = OBJECT_TO_JSVAL(obj);
        break;
      case JSTYPE_FUNCTION:
        *vp = v;
        obj = js_ValueToFunctionObject(cx, Valueify(vp), JSV2F_SEARCH_STACK);
        ok = (obj != NULL);
        break;
      case JSTYPE_STRING:
        str = js_ValueToString(cx, Valueify(v));
        ok = (str != NULL);
        if (ok)
            *vp = STRING_TO_JSVAL(str);
        break;
      case JSTYPE_NUMBER:
        ok = JS_ValueToNumber(cx, v, &d);
        if (ok)
            *vp = DOUBLE_TO_JSVAL(d);
        break;
      case JSTYPE_BOOLEAN:
        *vp = BOOLEAN_TO_JSVAL(js_ValueToBoolean(Valueify(v)));
        return JS_TRUE;
      default: {
        char numBuf[12];
        JS_snprintf(numBuf, sizeof numBuf, "%d", (int)type);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_TYPE, numBuf);
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
    assertSameCompartment(cx, v);
    return js_ValueToObjectOrNull(cx, Valueify(v), objp);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    return js_ValueToFunction(cx, Valueify(&v), JSV2F_SEARCH_STACK);
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    return js_ValueToFunction(cx, Valueify(&v), JSV2F_SEARCH_STACK);
}

JS_PUBLIC_API(JSString *)
JS_ValueToString(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    return js_ValueToString(cx, Valueify(v));
}

JS_PUBLIC_API(JSString *)
JS_ValueToSource(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    return js_ValueToSource(cx, Valueify(v));
}

JS_PUBLIC_API(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);

    AutoValueRooter tvr(cx, Valueify(v));
    return ToNumber(cx, tvr.value(), dp);
}

JS_PUBLIC_API(JSBool)
JS_DoubleIsInt32(jsdouble d, jsint *ip)
{
    return JSDOUBLE_IS_INT32(d, (int32_t *)ip);
}

JS_PUBLIC_API(int32)
JS_DoubleToInt32(jsdouble d)
{
    return js_DoubleToECMAInt32(d);
}

JS_PUBLIC_API(uint32)
JS_DoubleToUint32(jsdouble d)
{
    return js_DoubleToECMAUint32(d);
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);

    AutoValueRooter tvr(cx, Valueify(v));
    return ValueToECMAInt32(cx, tvr.value(), (int32_t *)ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);

    AutoValueRooter tvr(cx, Valueify(v));
    return ValueToECMAUint32(cx, tvr.value(), (uint32_t *)ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);

    AutoValueRooter tvr(cx, Valueify(v));
    return ValueToInt32(cx, tvr.value(), (int32_t *)ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);

    AutoValueRooter tvr(cx, Valueify(v));
    return ValueToUint16(cx, tvr.value(), (uint16_t *)ip);
}

JS_PUBLIC_API(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    *bp = js_ValueToBoolean(Valueify(v));
    return JS_TRUE;
}

JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    return TypeOfValue(cx, Valueify(v));
}

JS_PUBLIC_API(const char *)
JS_GetTypeName(JSContext *cx, JSType type)
{
    if ((uintN)type >= (uintN)JSTYPE_LIMIT)
        return NULL;
    return JS_TYPE_STR(type);
}

JS_PUBLIC_API(JSBool)
JS_StrictlyEqual(JSContext *cx, jsval v1, jsval v2, JSBool *equal)
{
    assertSameCompartment(cx, v1, v2);
    return StrictlyEqual(cx, Valueify(v1), Valueify(v2), equal);
}

JS_PUBLIC_API(JSBool)
JS_LooselyEqual(JSContext *cx, jsval v1, jsval v2, JSBool *equal)
{
    assertSameCompartment(cx, v1, v2);
    return LooselyEqual(cx, Valueify(v1), Valueify(v2), equal);
}

JS_PUBLIC_API(JSBool)
JS_SameValue(JSContext *cx, jsval v1, jsval v2, JSBool *same)
{
    assertSameCompartment(cx, v1, v2);
    return SameValue(cx, Valueify(v1), Valueify(v2), same);
}

JS_PUBLIC_API(JSBool)
JS_IsBuiltinEvalFunction(JSFunction *fun)
{
    return IsAnyBuiltinEval(fun);
}

JS_PUBLIC_API(JSBool)
JS_IsBuiltinFunctionConstructor(JSFunction *fun)
{
    return IsBuiltinFunctionConstructor(fun);
}

/************************************************************************/

/*
 * Has a new runtime ever been created?  This flag is used to detect unsafe
 * changes to js_CStringsAreUTF8 after a runtime has been created, and to
 * control things that should happen only once across all runtimes.
 */
static JSBool js_NewRuntimeWasCalled = JS_FALSE;

JSRuntime::JSRuntime()
  : trustedPrincipals_(NULL)
{
    /* Initialize infallibly first, so we can goto bad and JS_DestroyRuntime. */
    JS_INIT_CLIST(&contextList);
    JS_INIT_CLIST(&debuggerList);
}

bool
JSRuntime::init(uint32 maxbytes)
{
#ifdef JS_METHODJIT_SPEW
    JMCheckLogging();
#endif

#ifdef JS_TRACER
    InitJIT();
#endif

    if (!js_InitGC(this, maxbytes))
        return false;

    if (!(atomsCompartment = this->new_<JSCompartment>(this)) ||
        !atomsCompartment->init(NULL) ||
        !compartments.append(atomsCompartment)) {
        Foreground::delete_(atomsCompartment);
        return false;
    }

    atomsCompartment->isSystemCompartment = true;
    atomsCompartment->setGCLastBytes(8192, GC_NORMAL);

    if (!js_InitAtomState(this))
        return false;

    wrapObjectCallback = js::TransparentObjectWrapper;

#ifdef JS_THREADSAFE
    /* this is asymmetric with JS_ShutDown: */
    if (!js_SetupLocks(8, 16))
        return false;
    rtLock = JS_NEW_LOCK();
    if (!rtLock)
        return false;
    stateChange = JS_NEW_CONDVAR(gcLock);
    if (!stateChange)
        return false;
#endif

    debugMode = false;
    if (!js_InitThreads(this))
        return false;
    if (!InitRuntimeNumberState(this))
        return false;

    return true;
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

#ifdef JS_TRACER
    FinishJIT();
#endif

    FinishRuntimeNumberState(this);
    js_FinishThreads(this);
    js_FinishAtomState(this);

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
#endif
}

JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32 maxbytes)
{
    if (!js_NewRuntimeWasCalled) {
#ifdef DEBUG
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
#endif /* DEBUG */

        js_NewRuntimeWasCalled = JS_TRUE;
    }

    void *mem = OffTheBooks::calloc_(sizeof(JSRuntime));
    if (!mem)
        return NULL;

    JSRuntime *rt = new (mem) JSRuntime();
    if (!rt->init(maxbytes)) {
        JS_DestroyRuntime(rt);
        return NULL;
    }

    Probes::createRuntime(rt);
    return rt;
}

JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt)
{
    Probes::destroyRuntime(rt);
    Foreground::delete_(rt);
}

JS_PUBLIC_API(void)
JS_ShutDown(void)
{
    Probes::shutdown();

#ifdef MOZ_TRACEVIS
    StopTraceVis();
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

#ifdef JS_THREADSAFE
static void
StartRequest(JSContext *cx)
{
    JSThread *t = cx->thread();
    JS_ASSERT(CURRENT_THREAD_IS_ME(t));

    if (t->data.requestDepth) {
        t->data.requestDepth++;
    } else {
        JSRuntime *rt = cx->runtime;
        AutoLockGC lock(rt);

        /* Wait until the GC is finished. */
        if (rt->gcThread != cx->thread()) {
            while (rt->gcThread)
                JS_AWAIT_GC_DONE(rt);
        }

        /* Indicate that a request is running. */
        rt->requestCount++;
        t->data.requestDepth = 1;

        /*
         * Adjust rt->interruptCounter to reflect any interrupts added while the
         * thread was suspended.
         */
        if (t->data.interruptFlags)
            JS_ATOMIC_INCREMENT(&rt->interruptCounter);

        if (rt->requestCount == 1 && rt->activityCallback)
            rt->activityCallback(rt->activityCallbackArg, true);
    }
}

static void
StopRequest(JSContext *cx)
{
    JSThread *t = cx->thread();
    JS_ASSERT(CURRENT_THREAD_IS_ME(t));
    JS_ASSERT(t->data.requestDepth != 0);
    if (t->data.requestDepth != 1) {
        t->data.requestDepth--;
    } else {
        LeaveTrace(cx);  /* for GC safety */

        t->data.conservativeGC.updateForRequestEnd(t->suspendCount);

        /* Lock before clearing to interlock with ClaimScope, in jslock.c. */
        JSRuntime *rt = cx->runtime;
        AutoLockGC lock(rt);

        t->data.requestDepth = 0;

        /*
         * Adjust rt->interruptCounter to reflect any interrupts added while the
         * thread still had active requests.
         */
        if (t->data.interruptFlags)
            JS_ATOMIC_DECREMENT(&rt->interruptCounter);

        /* Give the GC a chance to run if this was the last request running. */
        JS_ASSERT(rt->requestCount > 0);
        rt->requestCount--;
        if (rt->requestCount == 0) {
            JS_NOTIFY_REQUEST_DONE(rt);
            if (rt->activityCallback)
                rt->activityCallback(rt->activityCallbackArg, false);
        }
    }
}
#endif /* JS_THREADSAFE */

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    cx->outstandingRequests++;
    StartRequest(cx);
#endif
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->outstandingRequests != 0);
    cx->outstandingRequests--;
    StopRequest(cx);
#endif
}

/* Yield to pending GC operations, regardless of request depth */
JS_PUBLIC_API(void)
JS_YieldRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    CHECK_REQUEST(cx);
    JS_ResumeRequest(cx, JS_SuspendRequest(cx));
#endif
}

JS_PUBLIC_API(jsrefcount)
JS_SuspendRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JSThread *t = cx->thread();
    JS_ASSERT(CURRENT_THREAD_IS_ME(t));

    jsrefcount saveDepth = t->data.requestDepth;
    if (!saveDepth)
        return 0;

    t->suspendCount++;
    t->data.requestDepth = 1;
    StopRequest(cx);
    return saveDepth;
#else
    return 0;
#endif
}

JS_PUBLIC_API(void)
JS_ResumeRequest(JSContext *cx, jsrefcount saveDepth)
{
#ifdef JS_THREADSAFE
    JSThread *t = cx->thread();
    JS_ASSERT(CURRENT_THREAD_IS_ME(t));
    if (saveDepth == 0)
        return;
    JS_ASSERT(saveDepth >= 1);
    JS_ASSERT(!t->data.requestDepth);
    JS_ASSERT(t->suspendCount);
    StartRequest(cx);
    t->data.requestDepth = saveDepth;
    t->suspendCount--;
#endif
}

JS_PUBLIC_API(JSBool)
JS_IsInRequest(JSContext *cx)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread()));
    return JS_THREAD_DATA(cx)->requestDepth != 0;
#else
    return false;
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
    return VersionNumber(cx->findVersion());
}

JS_PUBLIC_API(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion newVersion)
{
    JS_ASSERT(VersionIsKnown(newVersion));
    JS_ASSERT(!VersionHasFlags(newVersion));
    JSVersion newVersionNumber = newVersion;

#ifdef DEBUG
    uintN coptsBefore = cx->getCompileOptions();
#endif
    JSVersion oldVersion = cx->findVersion();
    JSVersion oldVersionNumber = VersionNumber(oldVersion);
    if (oldVersionNumber == newVersionNumber)
        return oldVersionNumber; /* No override actually occurs! */

    /* We no longer support 1.4 or below. */
    if (newVersionNumber != JSVERSION_DEFAULT && newVersionNumber <= JSVERSION_1_4)
        return oldVersionNumber;

    VersionCopyFlags(&newVersion, oldVersion);
    cx->maybeOverrideVersion(newVersion);
    JS_ASSERT(cx->getCompileOptions() == coptsBefore);
    return oldVersionNumber;
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
    /*
     * Can't check option/version synchronization here.
     * We may have been synchronized with a script version that was formerly on
     * the stack, but has now been popped.
     */
    return cx->allOptions();
}

static uintN
SetOptionsCommon(JSContext *cx, uintN options)
{
    JS_ASSERT((options & JSALLOPTION_MASK) == options);
    uintN oldopts = cx->allOptions();
    uintN newropts = options & JSRUNOPTION_MASK;
    uintN newcopts = options & JSCOMPILEOPTION_MASK;
    cx->setRunOptions(newropts);
    cx->setCompileOptions(newcopts);
    cx->updateJITEnabled();
    return oldopts;
}

JS_PUBLIC_API(uint32)
JS_SetOptions(JSContext *cx, uint32 options)
{
    AutoLockGC lock(cx->runtime);
    return SetOptionsCommon(cx, options);
}

JS_PUBLIC_API(uint32)
JS_ToggleOptions(JSContext *cx, uint32 options)
{
    AutoLockGC lock(cx->runtime);
    uintN oldopts = cx->allOptions();
    uintN newopts = oldopts ^ options;
    return SetOptionsCommon(cx, newopts);
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "JavaScript-C 1.8.5+ 2011-04-16";
}

JS_PUBLIC_API(JSCompartmentCallback)
JS_SetCompartmentCallback(JSRuntime *rt, JSCompartmentCallback callback)
{
    JSCompartmentCallback old = rt->compartmentCallback;
    rt->compartmentCallback = callback;
    return old;
}

JS_PUBLIC_API(JSWrapObjectCallback)
JS_SetWrapObjectCallbacks(JSRuntime *rt,
                          JSWrapObjectCallback callback,
                          JSPreWrapCallback precallback)
{
    JSWrapObjectCallback old = rt->wrapObjectCallback;
    rt->wrapObjectCallback = callback;
    rt->preWrapObjectCallback = precallback;
    return old;
}

JS_PUBLIC_API(JSCrossCompartmentCall *)
JS_EnterCrossCompartmentCall(JSContext *cx, JSObject *target)
{
    CHECK_REQUEST(cx);

    JS_ASSERT(target);
    AutoCompartment *call = cx->new_<AutoCompartment>(cx, target);
    if (!call)
        return NULL;
    if (!call->enter()) {
        Foreground::delete_(call);
        return NULL;
    }
    return reinterpret_cast<JSCrossCompartmentCall *>(call);
}

// Declared in jscompartment.h
JSClass js_dummy_class = {
    "jdummy",
    JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub,  JS_PropertyStub,
    JS_PropertyStub,  JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub,
    JS_ConvertStub,   NULL,
    JSCLASS_NO_OPTIONAL_MEMBERS
};

JS_PUBLIC_API(JSCrossCompartmentCall *)
JS_EnterCrossCompartmentCallScript(JSContext *cx, JSScript *target)
{
    CHECK_REQUEST(cx);

    JSObject *scriptObject = target->u.object;
    if (!scriptObject) {
        SwitchToCompartment sc(cx, target->compartment());
        scriptObject = JS_NewGlobalObject(cx, &js_dummy_class);
        if (!scriptObject)
            return NULL;
    }
    return JS_EnterCrossCompartmentCall(cx, scriptObject);
}

JS_PUBLIC_API(JSCrossCompartmentCall *)
JS_EnterCrossCompartmentCallStackFrame(JSContext *cx, JSStackFrame *target)
{
    CHECK_REQUEST(cx);

    StackFrame *frame = Valueify(target);
    return JS_EnterCrossCompartmentCall(cx, frame->scopeChain().getGlobal());
}

JS_PUBLIC_API(void)
JS_LeaveCrossCompartmentCall(JSCrossCompartmentCall *call)
{
    AutoCompartment *realcall = reinterpret_cast<AutoCompartment *>(call);
    CHECK_REQUEST(realcall->context);
    realcall->leave();
    Foreground::delete_(realcall);
}

bool
JSAutoEnterCompartment::enter(JSContext *cx, JSObject *target)
{
    JS_ASSERT(!call);
    if (cx->compartment == target->getCompartment()) {
        call = reinterpret_cast<JSCrossCompartmentCall*>(1);
        return true;
    }
    call = JS_EnterCrossCompartmentCall(cx, target);
    return call != NULL;
}

void
JSAutoEnterCompartment::enterAndIgnoreErrors(JSContext *cx, JSObject *target)
{
    (void) enter(cx, target);
}

namespace JS {

bool
AutoEnterScriptCompartment::enter(JSContext *cx, JSScript *target)
{
    JS_ASSERT(!call);
    if (cx->compartment == target->compartment()) {
        call = reinterpret_cast<JSCrossCompartmentCall*>(1);
        return true;
    }
    call = JS_EnterCrossCompartmentCallScript(cx, target);
    return call != NULL;
}

bool
AutoEnterFrameCompartment::enter(JSContext *cx, JSStackFrame *target)
{
    JS_ASSERT(!call);
    js::StackFrame *fp = Valueify(target);
    if (cx->compartment == fp->scopeChain().compartment()) {
        call = reinterpret_cast<JSCrossCompartmentCall*>(1);
        return true;
    }
    call = JS_EnterCrossCompartmentCallStackFrame(cx, target);
    return call != NULL;
}

} /* namespace JS */

JS_PUBLIC_API(void *)
JS_SetCompartmentPrivate(JSContext *cx, JSCompartment *compartment, void *data)
{
    CHECK_REQUEST(cx);
    void *old = compartment->data;
    compartment->data = data;
    return old;
}

JS_PUBLIC_API(void *)
JS_GetCompartmentPrivate(JSContext *cx, JSCompartment *compartment)
{
    CHECK_REQUEST(cx);
    return compartment->data;
}

JS_PUBLIC_API(JSBool)
JS_WrapObject(JSContext *cx, JSObject **objp)
{
    CHECK_REQUEST(cx);
    return cx->compartment->wrap(cx, objp);
}

JS_PUBLIC_API(JSBool)
JS_WrapValue(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    return cx->compartment->wrap(cx, Valueify(vp));
}

JS_PUBLIC_API(JSObject *)
JS_TransplantObject(JSContext *cx, JSObject *origobj, JSObject *target)
{
     // This function is called when an object moves between two
     // different compartments. In that case, we need to "move" the
     // window from origobj's compartment to target's compartment.
    JSCompartment *destination = target->getCompartment();
    WrapperMap &map = destination->crossCompartmentWrappers;
    Value origv = ObjectValue(*origobj);
    JSObject *obj;

    if (origobj->getCompartment() == destination) {
        // If the original object is in the same compartment as the
        // destination, then we know that we won't find wrapper in the
        // destination's cross compartment map and that the same
        // object will continue to work.  Note the rare case where
        // |origobj == target|. In that case, we can just treat this
        // as a same compartment navigation. The effect is to clear
        // all of the wrappers and their holders if they have
        // them. This would be cleaner as a separate API.
        if (origobj != target && !origobj->swap(cx, target))
            return NULL;
        obj = origobj;
    } else if (WrapperMap::Ptr p = map.lookup(origv)) {
        // There might already be a wrapper for the original object in
        // the new compartment. If there is, make it the primary outer
        // window proxy around the inner (accomplished by swapping
        // target's innards with the old, possibly security wrapper,
        // innards).
        obj = &p->value.toObject();
        map.remove(p);
        if (!obj->swap(cx, target))
            return NULL;
    } else {
        // Otherwise, this is going to be our outer window proxy in
        // the new compartment.
        obj = target;
    }

    // Now, iterate through other scopes looking for references to the
    // old outer window. They need to be updated to point at the new
    // outer window.  They also might transition between different
    // types of security wrappers based on whether the new compartment
    // is same origin with them.
    Value targetv = ObjectValue(*obj);
    CompartmentVector &vector = cx->runtime->compartments;
    AutoValueVector toTransplant(cx);
    if (!toTransplant.reserve(vector.length()))
        return NULL;

    for (JSCompartment **p = vector.begin(), **end = vector.end(); p != end; ++p) {
        WrapperMap &pmap = (*p)->crossCompartmentWrappers;
        if (WrapperMap::Ptr wp = pmap.lookup(origv)) {
            // We found a wrapper. Remember and root it.
            toTransplant.infallibleAppend(wp->value);
        }
    }

    for (Value *begin = toTransplant.begin(), *end = toTransplant.end(); begin != end; ++begin) {
        JSObject *wobj = &begin->toObject();
        JSCompartment *wcompartment = wobj->compartment();
        WrapperMap &pmap = wcompartment->crossCompartmentWrappers;
        JS_ASSERT(pmap.lookup(origv));
        pmap.remove(origv);

        // First, we wrap it in the new compartment. This will return
        // a new wrapper.
        AutoCompartment ac(cx, wobj);
        JSObject *tobj = obj;
        if (!ac.enter() || !wcompartment->wrap(cx, &tobj))
            return NULL;

        // Now, because we need to maintain object identity, we do a
        // brain transplant on the old object. At the same time, we
        // update the entry in the compartment's wrapper map to point
        // to the old wrapper.
        JS_ASSERT(tobj != wobj);
        if (!wobj->swap(cx, tobj))
            return NULL;
        pmap.put(targetv, ObjectValue(*wobj));
    }

    // Lastly, update the original object to point to the new one.
    if (origobj->getCompartment() != destination) {
        AutoCompartment ac(cx, origobj);
        JSObject *tobj = obj;
        if (!ac.enter() || !JS_WrapObject(cx, &tobj))
            return NULL;
        if (!origobj->swap(cx, tobj))
            return NULL;
        origobj->getCompartment()->crossCompartmentWrappers.put(targetv, origv);
    }

    return obj;
}

/*
 * The location object is special. There is the location object itself and
 * then the location object wrapper. Because there are no direct references to
 * the location object itself, we don't want the old obj (|origobj| here) to
 * become the new wrapper but the wrapper itself instead. This leads to very
 * subtle differences between js_TransplantObjectWithWrapper and
 * JS_TransplantObject.
 */
JS_FRIEND_API(JSObject *)
js_TransplantObjectWithWrapper(JSContext *cx,
                               JSObject *origobj,
                               JSObject *origwrapper,
                               JSObject *targetobj,
                               JSObject *targetwrapper)
{
    JSObject *obj;
    JSCompartment *destination = targetobj->getCompartment();
    WrapperMap &map = destination->crossCompartmentWrappers;

    // |origv| is the map entry we're looking up. The map entries are going to
    // be for the location object itself.
    Value origv = ObjectValue(*origobj);

    // There might already be a wrapper for the original object in the new
    // compartment.
    if (WrapperMap::Ptr p = map.lookup(origv)) {
        // There is. Make the existing wrapper a same compartment location
        // wrapper (swapping it with the given new wrapper).
        obj = &p->value.toObject();
        map.remove(p);
        if (!obj->swap(cx, targetwrapper))
            return NULL;
    } else {
        // Otherwise, use the passed-in wrapper as the same compartment
        // location wrapper.
        obj = targetwrapper;
    }

    // Now, iterate through other scopes looking for references to the old
    // location object. Note that the entries in the maps are for |origobj|
    // and not |origwrapper|. They need to be updated to point at the new
    // location object.
    Value targetv = ObjectValue(*targetobj);
    CompartmentVector &vector = cx->runtime->compartments;
    AutoValueVector toTransplant(cx);
    if (!toTransplant.reserve(vector.length()))
        return NULL;

    for (JSCompartment **p = vector.begin(), **end = vector.end(); p != end; ++p) {
        WrapperMap &pmap = (*p)->crossCompartmentWrappers;
        if (WrapperMap::Ptr wp = pmap.lookup(origv)) {
            // We found a wrapper. Remember and root it.
            toTransplant.infallibleAppend(wp->value);
        }
    }

    for (Value *begin = toTransplant.begin(), *end = toTransplant.end(); begin != end; ++begin) {
        JSObject *wobj = &begin->toObject();
        JSCompartment *wcompartment = wobj->compartment();
        WrapperMap &pmap = wcompartment->crossCompartmentWrappers;
        JS_ASSERT(pmap.lookup(origv));
        pmap.remove(origv);

        // First, we wrap it in the new compartment. This will return a
        // new wrapper.
        AutoCompartment ac(cx, wobj);

        JSObject *tobj = targetobj;
        if (!ac.enter() || !wcompartment->wrap(cx, &tobj))
            return NULL;

        // Now, because we need to maintain object identity, we do a brain
        // transplant on the old object. At the same time, we update the
        // entry in the compartment's wrapper map to point to the old
        // wrapper.
        JS_ASSERT(tobj != wobj);
        if (!wobj->swap(cx, tobj))
            return NULL;
        pmap.put(targetv, ObjectValue(*wobj));
    }

    // Lastly, update the original object to point to the new one. However, as
    // mentioned above, we do the transplant on the wrapper, not the object
    // itself, since all of the references are to the object itself.
    {
        AutoCompartment ac(cx, origobj);
        JSObject *tobj = obj;
        if (!ac.enter() || !JS_WrapObject(cx, &tobj))
            return NULL;
        if (!origwrapper->swap(cx, tobj))
            return NULL;
        origwrapper->getCompartment()->crossCompartmentWrappers.put(targetv,
                                                                    ObjectValue(*origwrapper));
    }

    return obj;
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
    if (!cx->hasfp())
        cx->resetCompartment();
}

JS_PUBLIC_API(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);

    /*
     * JS_SetGlobalObject might or might not change cx's compartment, so call
     * it before assertSameCompartment. (The API contract is that *after* this,
     * cx and obj must be in the same compartment.)
     */
    if (!cx->globalObject)
        JS_SetGlobalObject(cx, obj);

    assertSameCompartment(cx, obj);

    return obj->asGlobal()->initStandardClasses(cx);
}

#define CLASP(name)                 (&name##Class)
#define TYPED_ARRAY_CLASP(type)     (&TypedArray::fastClasses[TypedArray::type])
#define EAGER_ATOM(name)            ATOM_OFFSET(name), NULL
#define EAGER_CLASS_ATOM(name)      CLASS_ATOM_OFFSET(name), NULL
#define EAGER_ATOM_AND_CLASP(name)  EAGER_CLASS_ATOM(name), CLASP(name)
#define LAZY_ATOM(name)             ATOM_OFFSET(lazy.name), js_##name##_str

typedef struct JSStdName {
    JSObjectOp  init;
    size_t      atomOffset;     /* offset of atom pointer in JSAtomState */
    const char  *name;          /* null if atom is pre-pinned, else name */
    Class       *clasp;
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
            atom = js_Atomize(cx, name, strlen(name), InternAtom);
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
    {js_InitNamespaceClass,             EAGER_ATOM_AND_CLASP(Namespace)},
    {js_InitQNameClass,                 EAGER_ATOM_AND_CLASP(QName)},
#endif
#if JS_HAS_GENERATORS
    {js_InitIteratorClasses,            EAGER_ATOM_AND_CLASP(StopIteration)},
#endif
    {js_InitJSONClass,                  EAGER_ATOM_AND_CLASP(JSON)},
    {js_InitTypedArrayClasses,          EAGER_CLASS_ATOM(ArrayBuffer), &js::ArrayBuffer::slowClass},
    {js_InitWeakMapClass,               EAGER_CLASS_ATOM(WeakMap), &js::WeakMapClass},
    {NULL,                              0, NULL, NULL}
};

/*
 * Table of top-level function and constant names and their init functions.
 * If you add a "standard" global function or property, remember to update
 * this table.
 */
static JSStdName standard_class_names[] = {
    {js_InitObjectClass,        EAGER_ATOM(eval), CLASP(Object)},

    /* Global properties and functions defined by the Number class. */
    {js_InitNumberClass,        EAGER_ATOM(NaN), CLASP(Number)},
    {js_InitNumberClass,        EAGER_ATOM(Infinity), CLASP(Number)},
    {js_InitNumberClass,        LAZY_ATOM(isNaN), CLASP(Number)},
    {js_InitNumberClass,        LAZY_ATOM(isFinite), CLASP(Number)},
    {js_InitNumberClass,        LAZY_ATOM(parseFloat), CLASP(Number)},
    {js_InitNumberClass,        LAZY_ATOM(parseInt), CLASP(Number)},

    /* String global functions. */
    {js_InitStringClass,        LAZY_ATOM(escape), CLASP(String)},
    {js_InitStringClass,        LAZY_ATOM(unescape), CLASP(String)},
    {js_InitStringClass,        LAZY_ATOM(decodeURI), CLASP(String)},
    {js_InitStringClass,        LAZY_ATOM(encodeURI), CLASP(String)},
    {js_InitStringClass,        LAZY_ATOM(decodeURIComponent), CLASP(String)},
    {js_InitStringClass,        LAZY_ATOM(encodeURIComponent), CLASP(String)},
#if JS_HAS_UNEVAL
    {js_InitStringClass,        LAZY_ATOM(uneval), CLASP(String)},
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
    {js_InitXMLClass,           LAZY_ATOM(XMLList), CLASP(XML)},
    {js_InitXMLClass,           LAZY_ATOM(isXMLName), CLASP(XML)},
#endif

#if JS_HAS_GENERATORS
    {js_InitIteratorClasses,    EAGER_ATOM_AND_CLASP(Iterator)},
#endif

    /* Typed Arrays */
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(ArrayBuffer),  &ArrayBufferClass},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Int8Array),    TYPED_ARRAY_CLASP(TYPE_INT8)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint8Array),   TYPED_ARRAY_CLASP(TYPE_UINT8)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Int16Array),   TYPED_ARRAY_CLASP(TYPE_INT16)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint16Array),  TYPED_ARRAY_CLASP(TYPE_UINT16)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Int32Array),   TYPED_ARRAY_CLASP(TYPE_INT32)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint32Array),  TYPED_ARRAY_CLASP(TYPE_UINT32)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Float32Array), TYPED_ARRAY_CLASP(TYPE_FLOAT32)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Float64Array), TYPED_ARRAY_CLASP(TYPE_FLOAT64)},
    {js_InitTypedArrayClasses,  EAGER_CLASS_ATOM(Uint8ClampedArray),
                                TYPED_ARRAY_CLASP(TYPE_UINT8_CLAMPED)},

    {js_InitProxyClass,         EAGER_ATOM_AND_CLASP(Proxy)},

    {NULL,                      0, NULL, NULL}
};

static JSStdName object_prototype_names[] = {
    /* Object.prototype properties (global delegates to Object.prototype). */
    {js_InitObjectClass,        EAGER_ATOM(proto), CLASP(Object)},
#if JS_HAS_TOSOURCE
    {js_InitObjectClass,        EAGER_ATOM(toSource), CLASP(Object)},
#endif
    {js_InitObjectClass,        EAGER_ATOM(toString), CLASP(Object)},
    {js_InitObjectClass,        EAGER_ATOM(toLocaleString), CLASP(Object)},
    {js_InitObjectClass,        EAGER_ATOM(valueOf), CLASP(Object)},
#if JS_HAS_OBJ_WATCHPOINT
    {js_InitObjectClass,        LAZY_ATOM(watch), CLASP(Object)},
    {js_InitObjectClass,        LAZY_ATOM(unwatch), CLASP(Object)},
#endif
    {js_InitObjectClass,        LAZY_ATOM(hasOwnProperty), CLASP(Object)},
    {js_InitObjectClass,        LAZY_ATOM(isPrototypeOf), CLASP(Object)},
    {js_InitObjectClass,        LAZY_ATOM(propertyIsEnumerable), CLASP(Object)},
#if OLD_GETTER_SETTER_METHODS
    {js_InitObjectClass,        LAZY_ATOM(defineGetter), CLASP(Object)},
    {js_InitObjectClass,        LAZY_ATOM(defineSetter), CLASP(Object)},
    {js_InitObjectClass,        LAZY_ATOM(lookupGetter), CLASP(Object)},
    {js_InitObjectClass,        LAZY_ATOM(lookupSetter), CLASP(Object)},
#endif

    {NULL,                      0, NULL, NULL}
};

JS_PUBLIC_API(JSBool)
JS_ResolveStandardClass(JSContext *cx, JSObject *obj, jsid id, JSBool *resolved)
{
    JSString *idstr;
    JSRuntime *rt;
    JSAtom *atom;
    JSStdName *stdnm;
    uintN i;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    *resolved = JS_FALSE;

    rt = cx->runtime;
    JS_ASSERT(rt->state != JSRTS_DOWN);
    if (rt->state == JSRTS_LANDING || !JSID_IS_ATOM(id))
        return JS_TRUE;

    idstr = JSID_TO_STRING(id);

    /* Check whether we're resolving 'undefined', and define it if so. */
    atom = rt->atomState.typeAtoms[JSTYPE_VOID];
    if (idstr == atom) {
        *resolved = JS_TRUE;
        return obj->defineProperty(cx, ATOM_TO_JSID(atom), UndefinedValue(),
                                   PropertyStub, StrictPropertyStub,
                                   JSPROP_PERMANENT | JSPROP_READONLY);
    }

    /* Try for class constructors/prototypes named by well-known atoms. */
    stdnm = NULL;
    for (i = 0; standard_class_atoms[i].init; i++) {
        JS_ASSERT(standard_class_atoms[i].clasp);
        atom = OFFSET_TO_ATOM(rt, standard_class_atoms[i].atomOffset);
        if (idstr == atom) {
            stdnm = &standard_class_atoms[i];
            break;
        }
    }

    if (!stdnm) {
        /* Try less frequently used top-level functions and constants. */
        for (i = 0; standard_class_names[i].init; i++) {
            JS_ASSERT(standard_class_names[i].clasp);
            atom = StdNameToAtom(cx, &standard_class_names[i]);
            if (!atom)
                return JS_FALSE;
            if (idstr == atom) {
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
                JS_ASSERT(object_prototype_names[i].clasp);
                atom = StdNameToAtom(cx, &object_prototype_names[i]);
                if (!atom)
                    return JS_FALSE;
                if (idstr == atom) {
                    stdnm = &object_prototype_names[i];
                    break;
                }
            }
        }
    }

    if (stdnm) {
        /*
         * If this standard class is anonymous, then we don't want to resolve
         * by name.
         */
        JS_ASSERT(obj->isGlobal());
        if (stdnm->clasp->flags & JSCLASS_IS_ANONYMOUS)
            return JS_TRUE;

        if (IsStandardClassResolved(obj, stdnm->clasp))
            return JS_TRUE;

        if (!stdnm->init(cx, obj))
            return JS_FALSE;
        *resolved = JS_TRUE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStandardClasses(JSContext *cx, JSObject *obj)
{
    JSRuntime *rt;
    JSAtom *atom;
    uintN i;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    rt = cx->runtime;

    /*
     * Check whether we need to bind 'undefined' and define it if so.
     * Since ES5 15.1.1.3 undefined can't be deleted.
     */
    atom = rt->atomState.typeAtoms[JSTYPE_VOID];
    if (!obj->nativeContains(cx, ATOM_TO_JSID(atom)) &&
        !obj->defineProperty(cx, ATOM_TO_JSID(atom), UndefinedValue(),
                             PropertyStub, StrictPropertyStub,
                             JSPROP_PERMANENT | JSPROP_READONLY)) {
        return JS_FALSE;
    }

    /* Initialize any classes that have not been initialized yet. */
    for (i = 0; standard_class_atoms[i].init; i++) {
        if (!js::IsStandardClassResolved(obj, standard_class_atoms[i].clasp) &&
            !standard_class_atoms[i].init(cx, obj))
        {
                return JS_FALSE;
        }
    }

    return JS_TRUE;
}

namespace js {

JSIdArray *
NewIdArray(JSContext *cx, jsint length)
{
    JSIdArray *ida;

    ida = (JSIdArray *)
        cx->calloc_(offsetof(JSIdArray, vector) + length * sizeof(jsval));
    if (ida)
        ida->length = length;
    return ida;
}

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
    if (!rida) {
        JS_DestroyIdArray(cx, ida);
    } else {
        rida->length = length;
    }
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
    *foundp = obj->nativeContains(cx, ATOM_TO_JSID(atom));
    if (*foundp)
        ida = AddAtomToArray(cx, atom, ida, ip);
    return ida;
}

JS_PUBLIC_API(JSIdArray *)
JS_EnumerateResolvedStandardClasses(JSContext *cx, JSObject *obj, JSIdArray *ida)
{
    JSRuntime *rt;
    jsint i, j, k;
    JSAtom *atom;
    JSBool found;
    JSObjectOp init;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, ida);
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
JS_GetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key, JSObject **objp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_GetClassObject(cx, obj, key, objp);
}

JS_PUBLIC_API(JSObject *)
JS_GetScopeChain(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return GetScopeChain(cx);
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForObject(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);
    return obj->getGlobal();
}

JS_PUBLIC_API(JSObject *)
JS_GetGlobalForScopeChain(JSContext *cx)
{
    CHECK_REQUEST(cx);
    return GetGlobalForScopeChain(cx);
}

JS_PUBLIC_API(jsval)
JS_ComputeThis(JSContext *cx, jsval *vp)
{
    assertSameCompartment(cx, JSValueArray(vp, 2));
    CallReceiver call = CallReceiverFromVp(Valueify(vp));
    if (!BoxNonStrictThis(cx, call))
        return JSVAL_NULL;
    return Jsvalify(call.thisv());
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    return cx->malloc_(nbytes);
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
    return cx->realloc_(p, nbytes);
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
    return cx->free_(p);
}

JS_PUBLIC_API(void)
JS_updateMallocCounter(JSContext *cx, size_t nbytes)
{
    return cx->runtime->updateMallocCounter(nbytes);
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    size_t n;
    void *p;

    n = strlen(s) + 1;
    p = cx->malloc_(n);
    if (!p)
        return NULL;
    return (char *)memcpy(p, s, n);
}

JS_PUBLIC_API(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval)
{
    d = JS_CANONICALIZE_NAN(d);
    Valueify(rval)->setNumber(d);
    return JS_TRUE;
}

#undef JS_AddRoot

JS_PUBLIC_API(JSBool)
JS_AddValueRoot(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    return js_AddRoot(cx, Valueify(vp), NULL);
}

JS_PUBLIC_API(JSBool)
JS_AddStringRoot(JSContext *cx, JSString **rp)
{
    CHECK_REQUEST(cx);
    return js_AddGCThingRoot(cx, (void **)rp, NULL);
}

JS_PUBLIC_API(JSBool)
JS_AddObjectRoot(JSContext *cx, JSObject **rp)
{
    CHECK_REQUEST(cx);
    return js_AddGCThingRoot(cx, (void **)rp, NULL);
}

JS_PUBLIC_API(JSBool)
JS_AddGCThingRoot(JSContext *cx, void **rp)
{
    CHECK_REQUEST(cx);
    return js_AddGCThingRoot(cx, (void **)rp, NULL);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedValueRoot(JSContext *cx, jsval *vp, const char *name)
{
    CHECK_REQUEST(cx);
    return js_AddRoot(cx, Valueify(vp), name);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedStringRoot(JSContext *cx, JSString **rp, const char *name)
{
    CHECK_REQUEST(cx);
    return js_AddGCThingRoot(cx, (void **)rp, name);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedObjectRoot(JSContext *cx, JSObject **rp, const char *name)
{
    CHECK_REQUEST(cx);
    return js_AddGCThingRoot(cx, (void **)rp, name);
}

JS_PUBLIC_API(JSBool)
JS_AddNamedGCThingRoot(JSContext *cx, void **rp, const char *name)
{
    CHECK_REQUEST(cx);
    return js_AddGCThingRoot(cx, (void **)rp, name);
}

JS_PUBLIC_API(JSBool)
JS_RemoveValueRoot(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    return js_RemoveRoot(cx->runtime, (void *)vp);
}

JS_PUBLIC_API(JSBool)
JS_RemoveStringRoot(JSContext *cx, JSString **rp)
{
    CHECK_REQUEST(cx);
    return js_RemoveRoot(cx->runtime, (void *)rp);
}

JS_PUBLIC_API(JSBool)
JS_RemoveObjectRoot(JSContext *cx, JSObject **rp)
{
    CHECK_REQUEST(cx);
    return js_RemoveRoot(cx->runtime, (void *)rp);
}

JS_PUBLIC_API(JSBool)
JS_RemoveGCThingRoot(JSContext *cx, void **rp)
{
    CHECK_REQUEST(cx);
    return js_RemoveRoot(cx->runtime, (void *)rp);
}

JS_NEVER_INLINE JS_PUBLIC_API(void)
JS_AnchorPtr(void *p)
{
}

#ifdef DEBUG

JS_PUBLIC_API(void)
JS_DumpNamedRoots(JSRuntime *rt,
                  void (*dump)(const char *name, void *rp, JSGCRootType type, void *data),
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
    TraceRuntime(trc);
}

JS_PUBLIC_API(void)
JS_CallTracer(JSTracer *trc, void *thing, JSGCTraceKind kind)
{
    JS_ASSERT(thing);
    MarkKind(trc, thing, kind);
}

#ifdef DEBUG

#ifdef HAVE_XPCONNECT
#include "dump_xpc.h"
#endif

JS_PUBLIC_API(void)
JS_PrintTraceThingInfo(char *buf, size_t bufsize, JSTracer *trc, void *thing,
                       JSGCTraceKind kind, JSBool details)
{
    const char *name = NULL; /* silence uninitialized warning */
    size_t n;

    if (bufsize == 0)
        return;

    switch (kind) {
      case JSTRACE_OBJECT:
      {
        JSObject *obj = (JSObject *)thing;
        Class *clasp = obj->getClass();

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

      case JSTRACE_SCRIPT:
        name = "script";
        break;

      case JSTRACE_SHAPE:
        name = "shape";
        break;

      case JSTRACE_TYPE_OBJECT:
        name = "type_object";
        break;

#if JS_HAS_XML_SUPPORT
      case JSTRACE_XML:
        name = "xml";
        break;
#endif
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
            Class *clasp = obj->getClass();
            if (clasp == &FunctionClass) {
                JSFunction *fun = obj->getFunctionPrivate();
                if (!fun) {
                    JS_snprintf(buf, bufsize, "<newborn>");
                } else if (fun != obj) {
                    JS_snprintf(buf, bufsize, "%p", fun);
                } else {
                    if (fun->atom)
                        PutEscapedString(buf, bufsize, fun->atom, 0);
                }
            } else if (clasp->flags & JSCLASS_HAS_PRIVATE) {
                JS_snprintf(buf, bufsize, "%p", obj->getPrivate());
            } else {
                JS_snprintf(buf, bufsize, "<no private>");
            }
            break;
          }

          case JSTRACE_STRING:
          {
            JSString *str = (JSString *)thing;
            if (str->isLinear())
                PutEscapedString(buf, bufsize, &str->asLinear(), 0);
            else
                JS_snprintf(buf, bufsize, "<rope: length %d>", (int)str->length());
            break;
          }

          case JSTRACE_SCRIPT:
          {
            JSScript *script = static_cast<JSScript *>(thing);
            JS_snprintf(buf, bufsize, "%s:%u", script->filename, unsigned(script->lineno));
            break;
          }

          case JSTRACE_SHAPE:
          case JSTRACE_TYPE_OBJECT:
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
        }
    }
    buf[bufsize - 1] = '\0';
}

typedef struct JSHeapDumpNode JSHeapDumpNode;

struct JSHeapDumpNode {
    void            *thing;
    JSGCTraceKind   kind;
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
DumpNotify(JSTracer *trc, void *thing, JSGCTraceKind kind)
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
    size_t bytes = offsetof(JSHeapDumpNode, edgeName) + edgeNameSize;
    node = (JSHeapDumpNode *) OffTheBooks::malloc_(bytes);
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
JS_DumpHeap(JSContext *cx, FILE *fp, void* startThing, JSGCTraceKind startKind,
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
        JS_ASSERT(startKind == JSTRACE_OBJECT);
        TraceRuntime(&dtrc.base);
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
            Foreground::free_(node);
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

extern JS_PUBLIC_API(JSBool)
JS_IsGCMarkingTracer(JSTracer *trc)
{
    return IS_GC_MARKING_TRACER(trc);
}

JS_PUBLIC_API(void)
JS_CompartmentGC(JSContext *cx, JSCompartment *comp)
{
    /* We cannot GC the atoms compartment alone; use a full GC instead. */
    JS_ASSERT(comp != cx->runtime->atomsCompartment);

    LeaveTrace(cx);

    /* Don't nuke active arenas if executing or compiling. */
    if (cx->tempPool.current == &cx->tempPool.first)
        JS_FinishArenaPool(&cx->tempPool);

    GCREASON(PUBLIC_API);
    js_GC(cx, comp, GC_NORMAL);
}

JS_PUBLIC_API(void)
JS_GC(JSContext *cx)
{
    GCREASON(PUBLIC_API);
    JS_CompartmentGC(cx, NULL);
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
    LeaveTrace(cx);

    /* Don't nuke active arenas if executing or compiling. */
    if (cx->tempPool.current == &cx->tempPool.first)
        JS_FinishArenaPool(&cx->tempPool);

    MaybeGC(cx);
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
    JS_ASSERT(!cx->runtime->gcMarkingTracer);
    return IsAboutToBeFinalized(cx, thing);
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
        JS_ASSERT(key == JSGC_MODE);
        rt->gcMode = JSGCMode(value);
        JS_ASSERT(rt->gcMode == JSGC_MODE_GLOBAL ||
                  rt->gcMode == JSGC_MODE_COMPARTMENT);
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
      case JSGC_BYTES:
        return rt->gcBytes;
      case JSGC_MODE:
        return uint32(rt->gcMode);
      case JSGC_UNUSED_CHUNKS:
        return uint32(rt->gcEmptyChunkCount);
      case JSGC_TOTAL_CHUNKS:
        return uint32(rt->gcChunkSet.count() + rt->gcEmptyChunkCount);
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
    return JS_THREAD_DATA(cx)->maxCodeCacheBytes;
#else
    return 0;
#endif
}

JS_PUBLIC_API(void)
JS_FlushCaches(JSContext *cx)
{
#ifdef JS_TRACER
    if (cx->compartment->hasTraceMonitor())
        FlushJITCache(cx, cx->compartment->traceMonitor());
#endif
}

JS_PUBLIC_API(intN)
JS_AddExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    return JSExternalString::changeFinalizer(NULL, finalizer);
}

JS_PUBLIC_API(intN)
JS_RemoveExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    return JSExternalString::changeFinalizer(finalizer, NULL);
}

JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, const jschar *chars, size_t length, intN type)
{
    CHECK_REQUEST(cx);
    JSString *s = JSExternalString::new_(cx, chars, length, type, NULL);
    Probes::createString(cx, s, length);
    return s;
}

extern JS_PUBLIC_API(JSString *)
JS_NewExternalStringWithClosure(JSContext *cx, const jschar *chars, size_t length,
                                intN type, void *closure)
{
    CHECK_REQUEST(cx);
    return JSExternalString::new_(cx, chars, length, type, closure);
}

extern JS_PUBLIC_API(JSBool)
JS_IsExternalString(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    return str->isExternal();
}

extern JS_PUBLIC_API(void *)
JS_GetExternalStringClosure(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    return str->asExternal().externalClosure();
}

JS_PUBLIC_API(void)
JS_SetThreadStackLimit(JSContext *cx, jsuword limitAddr)
{
#if JS_STACK_GROWTH_DIRECTION > 0
    if (limitAddr == 0)
        limitAddr = jsuword(-1);
#endif
    cx->stackLimit = limitAddr;
}

JS_PUBLIC_API(void)
JS_SetNativeStackQuota(JSContext *cx, size_t stackSize)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(cx->thread());
#endif

#if JS_STACK_GROWTH_DIRECTION > 0
    if (stackSize == 0) {
        cx->stackLimit = jsuword(-1);
    } else {
        jsuword stackBase = reinterpret_cast<jsuword>(JS_THREAD_DATA(cx)->nativeStackBase);
        JS_ASSERT(stackBase <= size_t(-1) - stackSize);
        cx->stackLimit = stackBase + stackSize - 1;
    }
#else
    if (stackSize == 0) {
        cx->stackLimit = 0;
    } else {
        jsuword stackBase = reinterpret_cast<jsuword>(JS_THREAD_DATA(cx)->nativeStackBase);
        JS_ASSERT(stackBase >= stackSize);
        cx->stackLimit = stackBase - (stackSize - 1);
    }
#endif
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
    cx->free_(ida);
}

JS_PUBLIC_API(JSBool)
JS_ValueToId(JSContext *cx, jsval v, jsid *idp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    return ValueToId(cx, Valueify(v), idp);
}

JS_PUBLIC_API(JSBool)
JS_IdToValue(JSContext *cx, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    *vp = IdToJsval(id);
    assertSameCompartment(cx, *vp);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_StrictPropertyStub(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsid id)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    JS_ASSERT(type != JSTYPE_OBJECT && type != JSTYPE_FUNCTION);
    JS_ASSERT(obj);
    return DefaultValue(cx, obj, type, Valueify(vp));
}

JS_PUBLIC_API(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj)
{}

JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, parent_proto);
    return js_InitClass(cx, obj, parent_proto, Valueify(clasp),
                        Valueify(constructor), nargs,
                        ps, fs, static_ps, static_fs);
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSContext *cx, JSObject *obj)
{
    return Jsvalify(obj->getClass());
}
#else
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSObject *obj)
{
    return Jsvalify(obj->getClass());
}
#endif

JS_PUBLIC_API(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
{
    CHECK_REQUEST(cx);
#ifdef DEBUG
    if (argv) {
        assertSameCompartment(cx, obj);
        assertSameCompartment(cx, JSValueArray(argv - 2, 2));
    }
#endif
    if (!obj || obj->getJSClass() != clasp) {
        if (argv)
            ReportIncompatibleMethod(cx, Valueify(argv - 2), Valueify(clasp));
        return false;
    }
    return true;
}

JS_PUBLIC_API(JSBool)
JS_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp)
{
    assertSameCompartment(cx, obj, v);
    return HasInstance(cx, obj, Valueify(&v), bp);
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
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
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
    assertSameCompartment(cx, obj);
    proto = obj->getProto();

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return proto && !proto->isNewborn() ? proto : NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, proto);
    return SetProto(cx, obj, proto, JS_FALSE);
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);
    JSObject *parent = obj->getParent();

    /* Beware ref to dead object (we may be called from obj's finalizer). */
    return parent && !parent->isNewborn() ? parent : NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent)
{
    CHECK_REQUEST(cx);
    JS_ASSERT(parent || !obj->getParent());
    assertSameCompartment(cx, obj, parent);
    obj->setParent(parent);
    return true;
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto)
{
    Value cval;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto);
    {
        JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);

        if (!proto->getProperty(cx, ATOM_TO_JSID(cx->runtime->atomState.constructorAtom), &cval))
            return NULL;
    }
    JSObject *funobj;
    if (!IsFunctionObject(cval, &funobj)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NO_CONSTRUCTOR,
                             proto->getClass()->name);
        return NULL;
    }
    return &cval.toObject();
}

JS_PUBLIC_API(JSBool)
JS_GetObjectId(JSContext *cx, JSObject *obj, jsid *idp)
{
    assertSameCompartment(cx, obj);
    *idp = OBJECT_TO_JSID(obj);
    return JS_TRUE;
}

JS_PUBLIC_API(JSObject *)
JS_NewGlobalObject(JSContext *cx, JSClass *clasp)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);

    return GlobalObject::create(cx, Valueify(clasp));
}

class AutoHoldCompartment {
  public:
    explicit AutoHoldCompartment(JSCompartment *compartment JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : holdp(&compartment->hold)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        *holdp = true;
    }

    ~AutoHoldCompartment() {
        *holdp = false;
    }
  private:
    bool *holdp;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

JS_PUBLIC_API(JSObject *)
JS_NewCompartmentAndGlobalObject(JSContext *cx, JSClass *clasp, JSPrincipals *principals)
{
    CHECK_REQUEST(cx);
    JSCompartment *compartment = NewCompartment(cx, principals);
    if (!compartment)
        return NULL;

    AutoHoldCompartment hold(compartment);

    JSCompartment *saved = cx->compartment;
    cx->setCompartment(compartment);
    JSObject *obj = JS_NewGlobalObject(cx, clasp);
    cx->setCompartment(saved);

    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, JSClass *jsclasp, JSObject *proto, JSObject *parent)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);

    Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &ObjectClass;    /* default class is Object */

    JS_ASSERT(clasp != &FunctionClass);
    JS_ASSERT(!(clasp->flags & JSCLASS_IS_GLOBAL));

    if (proto)
        proto->getNewType(cx, NULL, /* markUnknown = */ true);

    JSObject *obj = NewNonFunction<WithProto::Class>(cx, clasp, proto, parent);
    if (obj) {
        if (clasp->ext.equality)
            MarkTypeObjectFlags(cx, obj, OBJECT_FLAG_SPECIAL_EQUALITY);
        obj->syncSpecialEquality();
        MarkTypeObjectUnknownProperties(cx, obj->type());
    }

    JS_ASSERT_IF(obj, obj->getParent());
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectWithGivenProto(JSContext *cx, JSClass *jsclasp, JSObject *proto, JSObject *parent)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);

    Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &ObjectClass;    /* default class is Object */

    JS_ASSERT(clasp != &FunctionClass);
    JS_ASSERT(!(clasp->flags & JSCLASS_IS_GLOBAL));

    JSObject *obj = NewNonFunction<WithProto::Given>(cx, clasp, proto, parent);
    if (obj) {
        obj->syncSpecialEquality();
        MarkTypeObjectUnknownProperties(cx, obj->type());
    }
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewObjectForConstructor(JSContext *cx, const jsval *vp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, *vp);

    return js_CreateThis(cx, JSVAL_TO_OBJECT(*vp));
}

JS_PUBLIC_API(JSBool)
JS_IsExtensible(JSObject *obj)
{
    return obj->isExtensible();
}

JS_PUBLIC_API(JSBool)
JS_IsNative(JSObject *obj)
{
    return obj->isNative();
}

JS_PUBLIC_API(JSBool)
JS_FreezeObject(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    return obj->freeze(cx);
}

JS_PUBLIC_API(JSBool)
JS_DeepFreezeObject(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    /* Assume that non-extensible objects are already deep-frozen, to avoid divergence. */
    if (!obj->isExtensible())
        return true;

    if (!obj->freeze(cx))
        return false;

    /* Walk slots in obj and if any value is a non-null object, seal it. */
    for (uint32 i = 0, n = obj->slotSpan(); i < n; ++i) {
        const Value &v = obj->getSlot(i);
        if (v.isPrimitive())
            continue;
        if (!JS_DeepFreezeObject(cx, &v.toObject()))
            return false;
    }

    return true;
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *jsclasp, JSObject *proto, JSObject *parent)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent);
    Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &ObjectClass;    /* default class is Object */
    return js_ConstructObject(cx, clasp, proto, parent, 0, NULL);
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObjectWithArguments(JSContext *cx, JSClass *jsclasp, JSObject *proto,
                                JSObject *parent, uintN argc, jsval *argv)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, proto, parent, JSValueArray(argv, argc));
    Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &ObjectClass;    /* default class is Object */
    return js_ConstructObject(cx, clasp, proto, parent, argc, Valueify(argv));
}

static JSBool
LookupPropertyById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                   JSObject **objp, JSProperty **propp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    JSAutoResolveFlags rf(cx, flags);
    id = js_CheckForStringIndex(id);
    return obj->lookupProperty(cx, id, objp, propp);
}

#define AUTO_NAMELEN(s,n)   (((n) == (size_t)-1) ? js_strlen(s) : (n))

static JSBool
LookupResult(JSContext *cx, JSObject *obj, JSObject *obj2, jsid id,
             JSProperty *prop, Value *vp)
{
    if (!prop) {
        /* XXX bad API: no way to tell "not defined" from "void value" */
        vp->setUndefined();
        return JS_TRUE;
    }

    if (obj2->isNative()) {
        Shape *shape = (Shape *) prop;

        if (shape->isMethod()) {
            vp->setObject(shape->methodObject());
            return !!obj2->methodReadBarrier(cx, *shape, vp);
        }

        /* Peek at the native property's slot value, without doing a Get. */
        if (obj2->containsSlot(shape->slot)) {
            *vp = obj2->nativeGetSlot(shape->slot);
            return true;
        }
    } else {
        if (obj2->isDenseArray())
            return js_GetDenseArrayElementValue(cx, obj2, id, vp);
        if (obj2->isProxy()) {
            AutoPropertyDescriptorRooter desc(cx);
            if (!JSProxy::getPropertyDescriptor(cx, obj2, id, false, &desc))
                return false;
            if (!(desc.attrs & JSPROP_SHARED)) {
                *vp = desc.value;
                return true;
            }
        }
    }

    /* XXX bad API: no way to return "defined but value unknown" */
    vp->setBoolean(true);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_LookupPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    JSObject *obj2;
    JSProperty *prop;
    return LookupPropertyById(cx, obj, id, JSRESOLVE_QUALIFIED, &obj2, &prop) &&
           LookupResult(cx, obj, obj2, id, prop, Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_LookupPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_LookupPropertyById(cx, obj, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_LookupUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen, jsval *vp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_LookupPropertyById(cx, obj, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_LookupPropertyWithFlagsById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                               JSObject **objp, jsval *vp)
{
    JSBool ok;
    JSProperty *prop;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    ok = obj->isNative()
         ? LookupPropertyWithFlags(cx, obj, id, flags, objp, &prop)
         : obj->lookupProperty(cx, id, objp, &prop);
    return ok && LookupResult(cx, obj, *objp, id, prop, Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, const char *name, uintN flags, jsval *vp)
{
    JSObject *obj2;
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_LookupPropertyWithFlagsById(cx, obj, ATOM_TO_JSID(atom), flags, &obj2, vp);
}

JS_PUBLIC_API(JSBool)
JS_HasPropertyById(JSContext *cx, JSObject *obj, jsid id, JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;
    JSBool ok = LookupPropertyById(cx, obj, id, JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                                   &obj2, &prop);
    *foundp = (prop != NULL);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_HasElement(JSContext *cx, JSObject *obj, uint32 index, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_HasPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(JSBool)
JS_HasProperty(JSContext *cx, JSObject *obj, const char *name, JSBool *foundp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_HasPropertyById(cx, obj, ATOM_TO_JSID(atom), foundp);
}

JS_PUBLIC_API(JSBool)
JS_HasUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen, JSBool *foundp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_HasPropertyById(cx, obj, ATOM_TO_JSID(atom), foundp);
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnPropertyById(JSContext *cx, JSObject *obj, jsid id, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);

    if (!obj->isNative()) {
        JSObject *obj2;
        JSProperty *prop;

        if (!LookupPropertyById(cx, obj, id, JSRESOLVE_QUALIFIED | JSRESOLVE_DETECTING,
                                &obj2, &prop)) {
            return JS_FALSE;
        }
        *foundp = (obj == obj2);
        return JS_TRUE;
    }

    *foundp = obj->nativeContains(cx, id);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnElement(JSContext *cx, JSObject *obj, uint32 index, JSBool *foundp)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_AlreadyHasOwnPropertyById(cx, obj, id, foundp);
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnProperty(JSContext *cx, JSObject *obj, const char *name, JSBool *foundp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_AlreadyHasOwnPropertyById(cx, obj, ATOM_TO_JSID(atom), foundp);
}

JS_PUBLIC_API(JSBool)
JS_AlreadyHasOwnUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen,
                           JSBool *foundp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_AlreadyHasOwnPropertyById(cx, obj, ATOM_TO_JSID(atom), foundp);
}

static JSBool
DefinePropertyById(JSContext *cx, JSObject *obj, jsid id, const Value &value,
                   PropertyOp getter, StrictPropertyOp setter, uintN attrs,
                   uintN flags, intN tinyid)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, value,
                            (attrs & JSPROP_GETTER)
                            ? JS_FUNC_TO_DATA_PTR(JSObject *, getter)
                            : NULL,
                            (attrs & JSPROP_SETTER)
                            ? JS_FUNC_TO_DATA_PTR(JSObject *, setter)
                            : NULL);

    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_DECLARING);
    if (flags != 0 && obj->isNative()) {
        return !!DefineNativeProperty(cx, obj, id, value, getter, setter,
                                      attrs, flags, tinyid);
    }
    return obj->defineProperty(cx, id, value, getter, setter, attrs);
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyById(JSContext *cx, JSObject *obj, jsid id, jsval value,
                      JSPropertyOp getter, JSStrictPropertyOp setter, uintN attrs)
{
    return DefinePropertyById(cx, obj, id, Valueify(value), Valueify(getter),
                              Valueify(setter), attrs, 0, 0);
}

JS_PUBLIC_API(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, uint32 index, jsval value,
                 JSPropertyOp getter, JSStrictPropertyOp setter, uintN attrs)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return DefinePropertyById(cx, obj, id, Valueify(value),
                              Valueify(getter), Valueify(setter), attrs, 0, 0);
}

static JSBool
DefineProperty(JSContext *cx, JSObject *obj, const char *name, const Value &value,
               PropertyOp getter, StrictPropertyOp setter, uintN attrs,
               uintN flags, intN tinyid)
{
    jsid id;
    JSAtom *atom;

    if (attrs & JSPROP_INDEX) {
        id = INT_TO_JSID(intptr_t(name));
        atom = NULL;
        attrs &= ~JSPROP_INDEX;
    } else {
        atom = js_Atomize(cx, name, strlen(name));
        if (!atom)
            return JS_FALSE;
        id = ATOM_TO_JSID(atom);
    }

    if (attrs & JSPROP_NATIVE_ACCESSORS) {
        JS_ASSERT(!(attrs & (JSPROP_GETTER | JSPROP_SETTER)));
        attrs &= ~JSPROP_NATIVE_ACCESSORS;
        if (getter) {
            JSObject *getobj = JS_NewFunction(cx, (JSNative) getter, 0, 0, obj->getGlobal(), NULL);
            if (!getobj)
                return false;
            getter = JS_DATA_TO_FUNC_PTR(PropertyOp, getobj);
            attrs |= JSPROP_GETTER;
        }
        if (setter) {
            JSObject *setobj = JS_NewFunction(cx, (JSNative) setter, 1, 0, obj->getGlobal(), NULL);
            if (!setobj)
                return false;
            setter = JS_DATA_TO_FUNC_PTR(StrictPropertyOp, setobj);
            attrs |= JSPROP_SETTER;
        }
    }
    return DefinePropertyById(cx, obj, id, value, getter, setter, attrs, flags, tinyid);
}

JS_PUBLIC_API(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
                  JSPropertyOp getter, JSStrictPropertyOp setter, uintN attrs)
{
    return DefineProperty(cx, obj, name, Valueify(value), Valueify(getter),
                          Valueify(setter), attrs, 0, 0);
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name, int8 tinyid,
                            jsval value, JSPropertyOp getter, JSStrictPropertyOp setter, uintN attrs)
{
    return DefineProperty(cx, obj, name, Valueify(value), Valueify(getter),
                          Valueify(setter), attrs, Shape::HAS_SHORTID, tinyid);
}

static JSBool
DefineUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen,
                 const Value &value, PropertyOp getter, StrictPropertyOp setter, uintN attrs,
                 uintN flags, intN tinyid)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && DefinePropertyById(cx, obj, ATOM_TO_JSID(atom), value, getter, setter, attrs,
                                      flags, tinyid);
}

JS_PUBLIC_API(JSBool)
JS_DefineUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen,
                    jsval value, JSPropertyOp getter, JSStrictPropertyOp setter, uintN attrs)
{
    return DefineUCProperty(cx, obj, name, namelen, Valueify(value),
                            Valueify(getter), Valueify(setter), attrs, 0, 0);
}

JS_PUBLIC_API(JSBool)
JS_DefineUCPropertyWithTinyId(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen,
                              int8 tinyid, jsval value,
                              JSPropertyOp getter, JSStrictPropertyOp setter, uintN attrs)
{
    return DefineUCProperty(cx, obj, name, namelen, Valueify(value), Valueify(getter),
                            Valueify(setter), attrs, Shape::HAS_SHORTID, tinyid);
}

JS_PUBLIC_API(JSBool)
JS_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id, jsval descriptor, JSBool *bp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id, descriptor);
    return js_DefineOwnProperty(cx, obj, id, Valueify(descriptor), bp);
}

JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *jsclasp,
                JSObject *proto, uintN attrs)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, proto);

    Class *clasp = Valueify(jsclasp);
    if (!clasp)
        clasp = &ObjectClass;    /* default class is Object */

    JSObject *nobj = NewObject<WithProto::Class>(cx, clasp, proto, obj);
    if (!nobj)
        return NULL;

    nobj->syncSpecialEquality();

    if (!DefineProperty(cx, obj, name, ObjectValue(*nobj), NULL, NULL, attrs, 0, 0))
        return NULL;

    return nobj;
}

JS_PUBLIC_API(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds)
{
    JSBool ok;
    uintN attrs;

    CHECK_REQUEST(cx);
    for (ok = JS_TRUE; cds->name; cds++) {
        Value value = DoubleValue(cds->dval);
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

    for (ok = true; ps->name; ps++) {
        ok = DefineProperty(cx, obj, ps->name, UndefinedValue(),
                            Valueify(ps->getter), Valueify(ps->setter),
                            ps->flags, Shape::HAS_SHORTID, ps->tinyid);
        if (!ok)
            break;
    }
    return ok;
}

static JSBool
GetPropertyDescriptorById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                          JSBool own, PropertyDescriptor *desc)
{
    JSObject *obj2;
    JSProperty *prop;

    if (!LookupPropertyById(cx, obj, id, flags, &obj2, &prop))
        return JS_FALSE;

    if (!prop || (own && obj != obj2)) {
        desc->obj = NULL;
        desc->attrs = 0;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->value.setUndefined();
        return JS_TRUE;
    }

    desc->obj = obj2;
    if (obj2->isNative()) {
        Shape *shape = (Shape *) prop;
        desc->attrs = shape->attributes();

        if (shape->isMethod()) {
            desc->getter = PropertyStub;
            desc->setter = StrictPropertyStub;
            desc->value.setObject(shape->methodObject());
        } else {
            desc->getter = shape->getter();
            desc->setter = shape->setter();
            if (obj2->containsSlot(shape->slot))
                desc->value = obj2->nativeGetSlot(shape->slot);
            else
                desc->value.setUndefined();
        }
    } else {
        if (obj2->isProxy()) {
            JSAutoResolveFlags rf(cx, flags);
            return own
                   ? JSProxy::getOwnPropertyDescriptor(cx, obj2, id, false, desc)
                   : JSProxy::getPropertyDescriptor(cx, obj2, id, false, desc);
        }
        if (!obj2->getAttributes(cx, id, &desc->attrs))
            return false;
        desc->getter = NULL;
        desc->setter = NULL;
        desc->value.setUndefined();
    }
    return true;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescriptorById(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                             JSPropertyDescriptor *desc)
{
    return GetPropertyDescriptorById(cx, obj, id, flags, JS_FALSE, Valueify(desc));
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyAttrsGetterAndSetterById(JSContext *cx, JSObject *obj, jsid id,
                                       uintN *attrsp, JSBool *foundp,
                                       JSPropertyOp *getterp, JSStrictPropertyOp *setterp)
{
    PropertyDescriptor desc;
    if (!GetPropertyDescriptorById(cx, obj, id, JSRESOLVE_QUALIFIED, JS_FALSE, &desc))
        return false;

    *attrsp = desc.attrs;
    *foundp = (desc.obj != NULL);
    if (getterp)
        *getterp = Jsvalify(desc.getter);
    if (setterp)
        *setterp = Jsvalify(desc.setter);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN *attrsp, JSBool *foundp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_GetPropertyAttrsGetterAndSetterById(cx, obj, ATOM_TO_JSID(atom),
                                                          attrsp, foundp, NULL, NULL);
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttributes(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen,
                           uintN *attrsp, JSBool *foundp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_GetPropertyAttrsGetterAndSetterById(cx, obj, ATOM_TO_JSID(atom),
                                                          attrsp, foundp, NULL, NULL);
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyAttrsGetterAndSetter(JSContext *cx, JSObject *obj, const char *name,
                                   uintN *attrsp, JSBool *foundp,
                                   JSPropertyOp *getterp, JSStrictPropertyOp *setterp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_GetPropertyAttrsGetterAndSetterById(cx, obj, ATOM_TO_JSID(atom),
                                                          attrsp, foundp, getterp, setterp);
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttrsGetterAndSetter(JSContext *cx, JSObject *obj,
                                     const jschar *name, size_t namelen,
                                     uintN *attrsp, JSBool *foundp,
                                     JSPropertyOp *getterp, JSStrictPropertyOp *setterp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_GetPropertyAttrsGetterAndSetterById(cx, obj, ATOM_TO_JSID(atom),
                                                          attrsp, foundp, getterp, setterp);
}

JS_PUBLIC_API(JSBool)
JS_GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    return GetOwnPropertyDescriptor(cx, obj, id, Valueify(vp));
}

static JSBool
SetPropertyAttributesById(JSContext *cx, JSObject *obj, jsid id, uintN attrs, JSBool *foundp)
{
    JSObject *obj2;
    JSProperty *prop;

    if (!LookupPropertyById(cx, obj, id, JSRESOLVE_QUALIFIED, &obj2, &prop))
        return false;
    if (!prop || obj != obj2) {
        *foundp = false;
        return true;
    }
    JSBool ok = obj->isNative()
                ? js_SetNativeAttributes(cx, obj, (Shape *) prop, attrs)
                : obj->setAttributes(cx, id, &attrs);
    if (ok)
        *foundp = true;
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN attrs, JSBool *foundp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && SetPropertyAttributesById(cx, obj, ATOM_TO_JSID(atom), attrs, foundp);
}

JS_PUBLIC_API(JSBool)
JS_SetUCPropertyAttributes(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen,
                           uintN attrs, JSBool *foundp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && SetPropertyAttributesById(cx, obj, ATOM_TO_JSID(atom), attrs, foundp);
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->getProperty(cx, id, Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyByIdDefault(JSContext *cx, JSObject *obj, jsid id, jsval def, jsval *vp)
{
    return GetPropertyDefault(cx, obj, id, Valueify(def), Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_GetPropertyById(cx, obj, id, vp);
}

JS_PUBLIC_API(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_GetPropertyById(cx, obj, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDefault(JSContext *cx, JSObject *obj, const char *name, jsval def, jsval *vp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_GetPropertyByIdDefault(cx, obj, ATOM_TO_JSID(atom), def, vp);
}

JS_PUBLIC_API(JSBool)
JS_GetUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen, jsval *vp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_GetPropertyById(cx, obj, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_GetMethodById(JSContext *cx, JSObject *obj, jsid id, JSObject **objp, jsval *vp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    if (!js_GetMethod(cx, obj, id, JSGET_METHOD_BARRIER, Valueify(vp)))
        return JS_FALSE;
    if (objp)
        *objp = obj;
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetMethod(JSContext *cx, JSObject *obj, const char *name, JSObject **objp, jsval *vp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_GetMethodById(cx, obj, ATOM_TO_JSID(atom), objp, vp);
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyById(JSContext *cx, JSObject *obj, jsid id, jsval *vp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED | JSRESOLVE_ASSIGNING);
    return obj->setProperty(cx, id, Valueify(vp), false);
}

JS_PUBLIC_API(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_SetPropertyById(cx, obj, INT_TO_JSID(index), vp);
}

JS_PUBLIC_API(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_SetPropertyById(cx, obj, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_SetUCProperty(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen, jsval *vp)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_SetPropertyById(cx, obj, ATOM_TO_JSID(atom), vp);
}

JS_PUBLIC_API(JSBool)
JS_DeletePropertyById2(JSContext *cx, JSObject *obj, jsid id, jsval *rval)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    JSAutoResolveFlags rf(cx, JSRESOLVE_QUALIFIED);
    return obj->deleteProperty(cx, id, Valueify(rval), false);
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement2(JSContext *cx, JSObject *obj, uint32 index, jsval *rval)
{
    CHECK_REQUEST(cx);
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return JS_DeletePropertyById2(cx, obj, id, rval);
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty2(JSContext *cx, JSObject *obj, const char *name, jsval *rval)
{
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom && JS_DeletePropertyById2(cx, obj, ATOM_TO_JSID(atom), rval);
}

JS_PUBLIC_API(JSBool)
JS_DeleteUCProperty2(JSContext *cx, JSObject *obj, const jschar *name, size_t namelen, jsval *rval)
{
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    return atom && JS_DeletePropertyById2(cx, obj, ATOM_TO_JSID(atom), rval);
}

JS_PUBLIC_API(JSBool)
JS_DeletePropertyById(JSContext *cx, JSObject *obj, jsid id)
{
    jsval junk;
    return JS_DeletePropertyById2(cx, obj, id, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, uint32 index)
{
    jsval junk;
    return JS_DeleteElement2(cx, obj, index, &junk);
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name)
{
    jsval junk;
    return JS_DeleteProperty2(cx, obj, name, &junk);
}

JS_PUBLIC_API(void)
JS_ClearScope(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    JSFinalizeOp clearOp = obj->getOps()->clear;
    if (clearOp)
        clearOp(cx, obj);

    if (obj->isNative())
        js_ClearNative(cx, obj);

    /* Clear cached class objects on the global object. */
    if (obj->isGlobal())
        obj->asGlobal()->clear(cx);

    js_InitRandom(cx);
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);

    AutoIdVector props(cx);
    JSIdArray *ida;
    if (!GetPropertyNames(cx, obj, JSITER_OWNONLY, &props) || !VectorToIdArray(cx, props, &ida))
        return NULL;
    for (size_t n = 0; n < size_t(ida->length); ++n)
        JS_ASSERT(js_CheckForStringIndex(ida->vector[n]) == ida->vector[n]);
    return ida;
}

/*
 * XXX reverse iterator for properties, unreverse and meld with jsinterp.c's
 *     prop_iterator_class somehow...
 * + preserve the obj->enumerate API while optimizing the native object case
 * + native case here uses a Shape *, but that iterates in reverse!
 * + so we make non-native match, by reverse-iterating after JS_Enumerating
 */
const uint32 JSSLOT_ITER_INDEX = 0;

static void
prop_iter_finalize(JSContext *cx, JSObject *obj)
{
    void *pdata = obj->getPrivate();
    if (!pdata)
        return;

    if (obj->getSlot(JSSLOT_ITER_INDEX).toInt32() >= 0) {
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

    if (obj->getSlot(JSSLOT_ITER_INDEX).toInt32() < 0) {
        /* Native case: just mark the next property to visit. */
        MarkShape(trc, (Shape *)pdata, "prop iter shape");
    } else {
        /* Non-native case: mark each id in the JSIdArray private. */
        JSIdArray *ida = (JSIdArray *) pdata;
        MarkIdRange(trc, ida->length, ida->vector, "prop iter");
    }
}

static Class prop_iter_class = {
    "PropertyIterator",
    JSCLASS_HAS_PRIVATE | JSCLASS_HAS_RESERVED_SLOTS(1),
    PropertyStub,         /* addProperty */
    PropertyStub,         /* delProperty */
    PropertyStub,         /* getProperty */
    StrictPropertyStub,   /* setProperty */
    EnumerateStub,
    ResolveStub,
    ConvertStub,
    prop_iter_finalize,
    NULL,           /* reserved0   */
    NULL,           /* checkAccess */
    NULL,           /* call        */
    NULL,           /* construct   */
    NULL,           /* xdrObject   */
    NULL,           /* hasInstance */
    prop_iter_trace
};

JS_PUBLIC_API(JSObject *)
JS_NewPropertyIterator(JSContext *cx, JSObject *obj)
{
    JSObject *iterobj;
    const void *pdata;
    jsint index;
    JSIdArray *ida;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    iterobj = NewNonFunction<WithProto::Class>(cx, &prop_iter_class, NULL, obj);
    if (!iterobj)
        return NULL;

    if (obj->isNative()) {
        /* Native case: start with the last property in obj. */
        pdata = obj->lastProperty();
        index = -1;
    } else {
        /*
         * Non-native case: enumerate a JSIdArray and keep it via private.
         *
         * Note: we have to make sure that we root obj around the call to
         * JS_Enumerate to protect against multiple allocations under it.
         */
        AutoObjectRooter tvr(cx, iterobj);
        ida = JS_Enumerate(cx, obj);
        if (!ida)
            return NULL;
        pdata = ida;
        index = ida->length;
    }

    /* iterobj cannot escape to other threads here. */
    iterobj->setPrivate(const_cast<void *>(pdata));
    iterobj->setSlot(JSSLOT_ITER_INDEX, Int32Value(index));
    return iterobj;
}

JS_PUBLIC_API(JSBool)
JS_NextProperty(JSContext *cx, JSObject *iterobj, jsid *idp)
{
    jsint i;
    const Shape *shape;
    JSIdArray *ida;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, iterobj);
    i = iterobj->getSlot(JSSLOT_ITER_INDEX).toInt32();
    if (i < 0) {
        /* Native case: private data is a property tree node pointer. */
        JS_ASSERT(iterobj->getParent()->isNative());
        shape = (Shape *) iterobj->getPrivate();

        while (shape->previous() && !shape->enumerable())
            shape = shape->previous();

        if (!shape->previous()) {
            JS_ASSERT(JSID_IS_EMPTY(shape->propid));
            *idp = JSID_VOID;
        } else {
            iterobj->setPrivate(const_cast<Shape *>(shape->previous()));
            *idp = shape->propid;
        }
    } else {
        /* Non-native case: use the ida enumerated when iterobj was created. */
        ida = (JSIdArray *) iterobj->getPrivate();
        JS_ASSERT(i <= ida->length);
        STATIC_ASSUME(i <= ida->length);
        if (i == 0) {
            *idp = JSID_VOID;
        } else {
            *idp = ida->vector[--i];
            iterobj->setSlot(JSSLOT_ITER_INDEX, Int32Value(i));
        }
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_GetReservedSlot(cx, obj, index, Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, v);
    return js_SetReservedSlot(cx, obj, index, Valueify(v));
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    /* NB: jsuint cast does ToUint32. */
    assertSameCompartment(cx, JSValueArray(vector, vector ? (jsuint)length : 0));
    return NewDenseCopiedArray(cx, (jsuint)length, Valueify(vector));
}

JS_PUBLIC_API(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj)
{
    assertSameCompartment(cx, obj);
    return obj->isArray() ||
           (obj->isWrapper() && obj->unwrap()->isArray());
}

JS_PUBLIC_API(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_GetLengthProperty(cx, obj, lengthp);
}

JS_PUBLIC_API(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsuint length)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_SetLengthProperty(cx, obj, length);
}

JS_PUBLIC_API(JSBool)
JS_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, id);
    return CheckAccess(cx, obj, id, mode, Valueify(vp), attrsp);
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

JS_PUBLIC_API(void)
JS_SetTrustedPrincipals(JSRuntime *rt, JSPrincipals *prin)
{
    rt->setTrustedPrincipals(prin);
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative native, uintN nargs, uintN flags,
               JSObject *parent, const char *name)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    JSAtom *atom;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    if (!name) {
        atom = NULL;
    } else {
        atom = js_Atomize(cx, name, strlen(name));
        if (!atom)
            return NULL;
    }
    return js_NewFunction(cx, NULL, Valueify(native), nargs, flags, parent, atom);
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunctionById(JSContext *cx, JSNative native, uintN nargs, uintN flags, JSObject *parent,
                   jsid id)
{
    JS_ASSERT(JSID_IS_STRING(id));
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);

    return js_NewFunction(cx, NULL, Valueify(native), nargs, flags, parent,
                          JSID_TO_ATOM(id));
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, parent);  // XXX no funobj for now
    if (!parent) {
        if (cx->hasfp())
            parent = GetScopeChain(cx, cx->fp());
        if (!parent)
            parent = cx->globalObject;
        JS_ASSERT(parent);
    }

    if (!funobj->isFunction()) {
        /*
         * We cannot clone this object, so fail (we used to return funobj, bad
         * idea, but we changed incompatibly to teach any abusers a lesson!).
         */
        Value v = ObjectValue(*funobj);
        js_ReportIsNotFunction(cx, &v, 0);
        return NULL;
    }

    JSFunction *fun = funobj->getFunctionPrivate();
    if (!fun->isInterpreted())
        return CloneFunctionObject(cx, fun, parent);

    if (fun->script()->compileAndGo) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_BAD_CLONE_FUNOBJ_SCOPE);
        return NULL;
    }

    if (!fun->isFlatClosure())
        return CloneFunctionObject(cx, fun, parent);

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
    JSObject *clone = js_AllocFlatClosure(cx, fun, parent);
    if (!clone)
        return NULL;

    JSUpvarArray *uva = fun->script()->upvars();
    uint32 i = uva->length;
    JS_ASSERT(i != 0);

    for (Shape::Range r(fun->script()->bindings.lastUpvar()); i-- != 0; r.popFront()) {
        JSObject *obj = parent;
        int skip = uva->vector[i].level();
        while (--skip > 0) {
            if (!obj) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                     JSMSG_BAD_CLONE_FUNOBJ_SCOPE);
                return NULL;
            }
            obj = obj->getParent();
        }

        Value v;
        if (!obj->getProperty(cx, r.front().propid, &v))
            return NULL;
        clone->getFlatClosureUpvars()[i] = v;
    }

    return clone;
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return fun;
}

JS_PUBLIC_API(JSString *)
JS_GetFunctionId(JSFunction *fun)
{
    return fun->atom;
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
    return obj->isFunction();
}

JS_PUBLIC_API(JSBool)
JS_ObjectIsCallable(JSContext *cx, JSObject *obj)
{
    return obj->isCallable();
}

static JSBool
js_generic_native_method_dispatcher(JSContext *cx, uintN argc, Value *vp)
{
    JSFunctionSpec *fs = (JSFunctionSpec *) vp->toObject().getReservedSlot(0).toPrivate();
    JS_ASSERT((fs->flags & JSFUN_GENERIC_NATIVE) != 0);

    if (argc < 1) {
        js_ReportMissingArg(cx, *vp, 0);
        return JS_FALSE;
    }

    /*
     * Copy all actual (argc) arguments down over our |this| parameter, vp[1],
     * which is almost always the class constructor object, e.g. Array.  Then
     * call the corresponding prototype native method with our first argument
     * passed as |this|.
     */
    memmove(vp + 1, vp + 2, argc * sizeof(jsval));

    /* Clear the last parameter in case too few arguments were passed. */
    vp[2 + --argc].setUndefined();

    Native native =
#ifdef JS_TRACER
                    (fs->flags & JSFUN_TRCINFO)
                    ? JS_FUNC_TO_DATA_PTR(JSNativeTraceInfo *, fs->call)->native
                    :
#endif
                      Valueify(fs->call);
    return native(cx, argc, vp);
}

JS_PUBLIC_API(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    uintN flags;
    JSObject *ctor;
    JSFunction *fun;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    ctor = NULL;
    for (; fs->name; fs++) {
        flags = fs->flags;

        JSAtom *atom = js_Atomize(cx, fs->name, strlen(fs->name));
        if (!atom)
            return JS_FALSE;

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
            fun = js_DefineFunction(cx, ctor, ATOM_TO_JSID(atom),
                                    js_generic_native_method_dispatcher,
                                    fs->nargs + 1,
                                    flags & ~JSFUN_TRCINFO);
            if (!fun)
                return JS_FALSE;

            /*
             * As jsapi.h notes, fs must point to storage that lives as long
             * as fun->object lives.
             */
            Value priv = PrivateValue(fs);
            if (!js_SetReservedSlot(cx, fun, 0, priv))
                return JS_FALSE;
        }

        fun = js_DefineFunction(cx, obj, ATOM_TO_JSID(atom), Valueify(fs->call), fs->nargs, flags);
        if (!fun)
            return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
                  uintN nargs, uintN attrs)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    if (!atom)
        return NULL;
    return js_DefineFunction(cx, obj, ATOM_TO_JSID(atom), Valueify(call), nargs, attrs);
}

JS_PUBLIC_API(JSFunction *)
JS_DefineUCFunction(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, JSNative call,
                    uintN nargs, uintN attrs)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    JSAtom *atom = js_AtomizeChars(cx, name, AUTO_NAMELEN(name, namelen));
    if (!atom)
        return NULL;
    return js_DefineFunction(cx, obj, ATOM_TO_JSID(atom), Valueify(call), nargs, attrs);
}

extern JS_PUBLIC_API(JSFunction *)
JS_DefineFunctionById(JSContext *cx, JSObject *obj, jsid id, JSNative call,
                    uintN nargs, uintN attrs)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    return js_DefineFunction(cx, obj, id, Valueify(call), nargs, attrs);
}

struct AutoLastFrameCheck {
    AutoLastFrameCheck(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : cx(cx) {
        JS_ASSERT(cx);
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    ~AutoLastFrameCheck() {
        if (cx->isExceptionPending() &&
            !JS_IsRunning(cx) &&
            !cx->hasRunOption(JSOPTION_DONT_REPORT_UNCAUGHT)) {
            js_ReportUncaughtException(cx);
        }
    }

  private:
    JSContext       *cx;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

inline static uint32
JS_OPTIONS_TO_TCFLAGS(JSContext *cx)
{
    return (cx->hasRunOption(JSOPTION_COMPILE_N_GO) ? TCF_COMPILE_N_GO : 0) |
           (cx->hasRunOption(JSOPTION_NO_SCRIPT_RVAL) ? TCF_NO_SCRIPT_RVAL : 0);
}

static JSObject *
CompileUCScriptForPrincipalsCommon(JSContext *cx, JSObject *obj, JSPrincipals *principals,
                                      const jschar *chars, size_t length,
                                      const char *filename, uintN lineno, JSVersion version)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, principals);
    AutoLastFrameCheck lfc(cx);

    uint32 tcflags = JS_OPTIONS_TO_TCFLAGS(cx) | TCF_NEED_MUTABLE_SCRIPT | TCF_NEED_SCRIPT_OBJECT;
    JSScript *script = Compiler::compileScript(cx, obj, NULL, principals, tcflags,
                                               chars, length, filename, lineno, version);
    if (!script)
        return NULL;
    JS_ASSERT(script->u.object);
    return script->u.object;
}

extern JS_PUBLIC_API(JSObject *)
JS_CompileUCScriptForPrincipalsVersion(JSContext *cx, JSObject *obj,
                                       JSPrincipals *principals,
                                       const jschar *chars, size_t length,
                                       const char *filename, uintN lineno,
                                       JSVersion version)
{
    AutoVersionAPI avi(cx, version);
    return CompileUCScriptForPrincipalsCommon(cx, obj, principals, chars, length, filename, lineno,
                                              avi.version());
}

JS_PUBLIC_API(JSObject *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj, JSPrincipals *principals,
                                const jschar *chars, size_t length,
                                const char *filename, uintN lineno)
{
    return CompileUCScriptForPrincipalsCommon(cx, obj, principals, chars, length, filename, lineno,
                                              cx->findVersion());
}

JS_PUBLIC_API(JSObject *)
JS_CompileUCScript(JSContext *cx, JSObject *obj, const jschar *chars, size_t length,
                   const char *filename, uintN lineno)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_CompileUCScriptForPrincipals(cx, obj, NULL, chars, length, filename, lineno);
}

JS_PUBLIC_API(JSObject *)
JS_CompileScriptForPrincipalsVersion(JSContext *cx, JSObject *obj,
                                     JSPrincipals *principals,
                                     const char *bytes, size_t length,
                                     const char *filename, uintN lineno,
                                     JSVersion version)
{
    AutoVersionAPI ava(cx, version);
    return JS_CompileScriptForPrincipals(cx, obj, principals, bytes, length, filename, lineno);
}

JS_PUBLIC_API(JSObject *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
                              JSPrincipals *principals,
                              const char *bytes, size_t length,
                              const char *filename, uintN lineno)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);

    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    JSObject *scriptObj =
        JS_CompileUCScriptForPrincipals(cx, obj, principals, chars, length, filename, lineno);
    cx->free_(chars);
    return scriptObj;
}

JS_PUBLIC_API(JSObject *)
JS_CompileScript(JSContext *cx, JSObject *obj, const char *bytes, size_t length,
                 const char *filename, uintN lineno)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_CompileScriptForPrincipals(cx, obj, NULL, bytes, length, filename, lineno);
}

JS_PUBLIC_API(JSBool)
JS_BufferIsCompilableUnit(JSContext *cx, JSBool bytes_are_utf8, JSObject *obj, const char *bytes, size_t length)
{
    jschar *chars;
    JSBool result;
    JSExceptionState *exnState;
    JSErrorReporter older;

    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    if (bytes_are_utf8)
        chars = InflateString(cx, bytes, &length, CESU8Encoding);
    else
        chars = InflateString(cx, bytes, &length);
    if (!chars)
        return JS_TRUE;

    /*
     * Return true on any out-of-memory error, so our caller doesn't try to
     * collect more buffered source.
     */
    result = JS_TRUE;
    exnState = JS_SaveExceptionState(cx);
    {
        Parser parser(cx);
        if (parser.init(chars, length, NULL, 1, cx->findVersion())) {
            older = JS_SetErrorReporter(cx, NULL);
            if (!parser.parse(obj) &&
                parser.tokenStream.isUnexpectedEOF()) {
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
    cx->free_(chars);
    JS_RestoreExceptionState(cx, exnState);
    return result;
}

/* Use the fastest available getc. */
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

static JSObject *
CompileFileHelper(JSContext *cx, JSObject *obj, JSPrincipals *principals,
                  const char* filename, FILE *fp)
{
    struct stat st;
    int ok = fstat(fileno(fp), &st);
    if (ok != 0)
        return NULL;

    jschar *buf = NULL;
    size_t len = st.st_size;
    size_t i = 0;
    JSScript *script;

    /* Read in the whole file, then compile it. */
    if (fp == stdin) {
        if (len == 0)
            len = 8;  /* start with a small buffer, expand as necessary */

        int c;
        bool hitEOF = false;
        while (!hitEOF) {
            len *= 2;
            jschar* tmpbuf = (jschar *) cx->realloc_(buf, len * sizeof(jschar));
            if (!tmpbuf) {
                cx->free_(buf);
                return NULL;
            }
            buf = tmpbuf;

            while (i < len) {
                c = fast_getc(fp);
                if (c == EOF) {
                    hitEOF = true;
                    break;
                }
                buf[i++] = (jschar) (unsigned char) c;
            }
        }
    } else {
        buf = (jschar *) cx->malloc_(len * sizeof(jschar));
        if (!buf)
            return NULL;

        int c;
        // The |i < len| is necessary for files that lie about their length,
        // e.g. /dev/zero and /dev/random.  See bug 669434.
        while (i < len && (c = fast_getc(fp)) != EOF)
            buf[i++] = (jschar) (unsigned char) c;
    }

    JS_ASSERT(i <= len);
    len = i;
    uint32 tcflags = JS_OPTIONS_TO_TCFLAGS(cx) | TCF_NEED_MUTABLE_SCRIPT | TCF_NEED_SCRIPT_OBJECT;
    script = Compiler::compileScript(cx, obj, NULL, principals, tcflags, buf, len, filename, 1,
                                     cx->findVersion());
    cx->free_(buf);
    if (!script)
        return NULL;
    JS_ASSERT(script->u.object);
    return script->u.object;
}

JS_PUBLIC_API(JSObject *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj);
    AutoLastFrameCheck lfc(cx);

    FILE *fp;
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

    JSObject *scriptObj = CompileFileHelper(cx, obj, NULL, filename, fp);
    if (fp != stdin)
        fclose(fp);
    return scriptObj;
}

JS_PUBLIC_API(JSObject *)
JS_CompileFileHandleForPrincipals(JSContext *cx, JSObject *obj, const char *filename,
                                  FILE *file, JSPrincipals *principals)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, principals);
    AutoLastFrameCheck lfc(cx);

    return CompileFileHelper(cx, obj, principals, filename, file);
}

JS_PUBLIC_API(JSObject *)
JS_CompileFileHandleForPrincipalsVersion(JSContext *cx, JSObject *obj, const char *filename,
                                         FILE *file, JSPrincipals *principals, JSVersion version)
{
    AutoVersionAPI ava(cx, version);
    return JS_CompileFileHandleForPrincipals(cx, obj, filename, file, principals);
}

JS_PUBLIC_API(JSObject *)
JS_CompileFileHandle(JSContext *cx, JSObject *obj, const char *filename, FILE *file)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_CompileFileHandleForPrincipals(cx, obj, filename, file, NULL);
}

JS_PUBLIC_API(JSScript *)
JS_GetScriptFromObject(JSObject *scriptObj)
{
    JS_ASSERT(scriptObj->isScript());

    return (JSScript *) scriptObj->getPrivate();
}

static JSFunction *
CompileUCFunctionForPrincipalsCommon(JSContext *cx, JSObject *obj,
                                     JSPrincipals *principals, const char *name,
                                     uintN nargs, const char **argnames,
                                     const jschar *chars, size_t length,
                                     const char *filename, uintN lineno, JSVersion version)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, principals);
    AutoLastFrameCheck lfc(cx);

    JSAtom *funAtom;
    if (!name) {
        funAtom = NULL;
    } else {
        funAtom = js_Atomize(cx, name, strlen(name));
        if (!funAtom)
            return NULL;
    }

    Bindings bindings(cx);
    for (uintN i = 0; i < nargs; i++) {
        uint16 dummy;
        JSAtom *argAtom = js_Atomize(cx, argnames[i], strlen(argnames[i]));
        if (!argAtom || !bindings.addArgument(cx, argAtom, &dummy))
            return NULL;
    }

    JSFunction *fun = js_NewFunction(cx, NULL, NULL, 0, JSFUN_INTERPRETED, obj, funAtom);
    if (!fun)
        return NULL;

    if (!Compiler::compileFunctionBody(cx, fun, principals, &bindings,
                                       chars, length, filename, lineno, version)) {
        return NULL;
    }
    
    if (obj && funAtom &&
        !obj->defineProperty(cx, ATOM_TO_JSID(funAtom), ObjectValue(*fun),
                             NULL, NULL, JSPROP_ENUMERATE)) {
        return NULL;
    }

    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipalsVersion(JSContext *cx, JSObject *obj,
                                         JSPrincipals *principals, const char *name,
                                         uintN nargs, const char **argnames,
                                         const jschar *chars, size_t length,
                                         const char *filename, uintN lineno,
                                         JSVersion version)
{
    AutoVersionAPI avi(cx, version);
    return CompileUCFunctionForPrincipalsCommon(cx, obj, principals, name, nargs, argnames, chars,
                                                length, filename, lineno, avi.version());
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                  JSPrincipals *principals, const char *name,
                                  uintN nargs, const char **argnames,
                                  const jschar *chars, size_t length,
                                  const char *filename, uintN lineno)
{
    return CompileUCFunctionForPrincipalsCommon(cx, obj, principals, name, nargs, argnames, chars,
                                                length, filename, lineno, cx->findVersion());
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
                     uintN nargs, const char **argnames,
                     const jschar *chars, size_t length,
                     const char *filename, uintN lineno)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_CompileUCFunctionForPrincipals(cx, obj, NULL, name, nargs, argnames,
                                             chars, length, filename, lineno);
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals, const char *name,
                                uintN nargs, const char **argnames,
                                const char *bytes, size_t length,
                                const char *filename, uintN lineno)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    JSFunction *fun = JS_CompileUCFunctionForPrincipals(cx, obj, principals, name,
                                                        nargs, argnames, chars, length,
                                                        filename, lineno);
    cx->free_(chars);
    return fun;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
                   uintN nargs, const char **argnames,
                   const char *bytes, size_t length,
                   const char *filename, uintN lineno)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_CompileFunctionForPrincipals(cx, obj, NULL, name, nargs, argnames, bytes, length,
                                           filename, lineno);
}

JS_PUBLIC_API(JSString *)
JS_DecompileScript(JSContext *cx, JSScript *script, const char *name, uintN indent)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    JSPrinter *jp;
    JSString *str;

    CHECK_REQUEST(cx);
#ifdef DEBUG
    if (cx->compartment != script->compartment())
        CompartmentChecker::fail(cx->compartment, script->compartment());
#endif
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
JS_DecompileScriptObject(JSContext *cx, JSObject *scriptObj, const char *name, uintN indent)
{
    return JS_DecompileScript(cx, scriptObj->getScript(), name, indent);
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fun);
    return js_DecompileToString(cx, "JS_DecompileFunction", fun,
                                indent & ~JS_DONT_PRETTY_PRINT,
                                !(indent & JS_DONT_PRETTY_PRINT),
                                false, false, js_DecompileFunction);
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, fun);
    return js_DecompileToString(cx, "JS_DecompileFunctionBody", fun,
                                indent & ~JS_DONT_PRETTY_PRINT,
                                !(indent & JS_DONT_PRETTY_PRINT),
                                false, false, js_DecompileFunctionBody);
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSObject *scriptObj, jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, scriptObj);
    AutoLastFrameCheck lfc(cx);

    return Execute(cx, scriptObj->getScript(), *obj, Valueify(rval));
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScriptVersion(JSContext *cx, JSObject *obj, JSObject *scriptObj, jsval *rval,
                        JSVersion version)
{
    AutoVersionAPI ava(cx, version);
    return JS_ExecuteScript(cx, obj, scriptObj, rval);
}

bool
EvaluateUCScriptForPrincipalsCommon(JSContext *cx, JSObject *obj,
                                    JSPrincipals *principals,
                                    const jschar *chars, uintN length,
                                    const char *filename, uintN lineno,
                                    jsval *rval, JSVersion compileVersion)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);

    uint32 flags = TCF_COMPILE_N_GO | TCF_NEED_SCRIPT_OBJECT;
    if (!rval)
        flags |= TCF_NO_SCRIPT_RVAL;

    CHECK_REQUEST(cx);
    AutoLastFrameCheck lfc(cx);
    JSScript *script = Compiler::compileScript(cx, obj, NULL, principals, flags,
                                               chars, length, filename, lineno, compileVersion);
    if (!script)
        return false;

    JS_ASSERT(script->getVersion() == compileVersion);

    return Execute(cx, script, *obj, Valueify(rval));
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipalsVersion(JSContext *cx, JSObject *obj,
                                        JSPrincipals *principals,
                                        const jschar *chars, uintN length,
                                        const char *filename, uintN lineno,
                                        jsval *rval, JSVersion version)
{
    AutoVersionAPI avi(cx, version);
    return EvaluateUCScriptForPrincipalsCommon(cx, obj, principals, chars, length,
                                               filename, lineno, rval, avi.version());
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                 JSPrincipals *principals,
                                 const jschar *chars, uintN length,
                                 const char *filename, uintN lineno,
                                 jsval *rval)
{
    return EvaluateUCScriptForPrincipalsCommon(cx, obj, principals, chars, length,
                                               filename, lineno, rval, cx->findVersion());
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj, const jschar *chars, uintN length,
                    const char *filename, uintN lineno, jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_EvaluateUCScriptForPrincipals(cx, obj, NULL, chars, length, filename, lineno, rval);
}

/* Ancient uintN nbytes is part of API/ABI, so use size_t length local. */
JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj, JSPrincipals *principals,
                               const char *bytes, uintN nbytes,
                               const char *filename, uintN lineno, jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    size_t length = nbytes;
    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return JS_FALSE;
    JSBool ok = JS_EvaluateUCScriptForPrincipals(cx, obj, principals, chars, length,
                                                 filename, lineno, rval);
    cx->free_(chars);
    return ok;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipalsVersion(JSContext *cx, JSObject *obj, JSPrincipals *principals,
                                      const char *bytes, uintN nbytes,
                                      const char *filename, uintN lineno, jsval *rval, JSVersion version)
{
    AutoVersionAPI avi(cx, version);
    return JS_EvaluateScriptForPrincipals(cx, obj, principals, bytes, nbytes, filename, lineno,
                                          rval);
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj, const char *bytes, uintN nbytes,
                  const char *filename, uintN lineno, jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    return JS_EvaluateScriptForPrincipals(cx, obj, NULL, bytes, nbytes, filename, lineno, rval);
}

JS_PUBLIC_API(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc, jsval *argv,
                jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, fun, JSValueArray(argv, argc));
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, ObjectOrNullValue(obj), ObjectValue(*fun), argc, Valueify(argv),
                  Valueify(rval));
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc, jsval *argv,
                    jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, JSValueArray(argv, argc));
    AutoLastFrameCheck lfc(cx);

    Value v;
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    return atom &&
           js_GetMethod(cx, obj, ATOM_TO_JSID(atom), JSGET_NO_METHOD_BARRIER, &v) &&
           Invoke(cx, ObjectOrNullValue(obj), v, argc, Valueify(argv), Valueify(rval));
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc, jsval *argv,
                     jsval *rval)
{
    JS_THREADSAFE_ASSERT(cx->compartment != cx->runtime->atomsCompartment);
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, obj, fval, JSValueArray(argv, argc));
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, ObjectOrNullValue(obj), Valueify(fval), argc, Valueify(argv),
                  Valueify(rval));
}

namespace JS {

JS_PUBLIC_API(bool)
Call(JSContext *cx, jsval thisv, jsval fval, uintN argc, jsval *argv, jsval *rval)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, thisv, fval, JSValueArray(argv, argc));
    AutoLastFrameCheck lfc(cx);

    return Invoke(cx, Valueify(thisv), Valueify(fval), argc, Valueify(argv), Valueify(rval));
}

} // namespace JS

JS_PUBLIC_API(JSObject *)
JS_New(JSContext *cx, JSObject *ctor, uintN argc, jsval *argv)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, ctor, JSValueArray(argv, argc));
    AutoLastFrameCheck lfc(cx);

    // This is not a simple variation of JS_CallFunctionValue because JSOP_NEW
    // is not a simple variation of JSOP_CALL. We have to determine what class
    // of object to create, create it, and clamp the return value to an object,
    // among other details. InvokeConstructor does the hard work.
    InvokeArgsGuard args;
    if (!cx->stack.pushInvokeArgs(cx, argc, &args))
        return NULL;

    args.calleev().setObject(*ctor);
    args.thisv().setNull();
    memcpy(args.argv(), argv, argc * sizeof(jsval));

    if (!InvokeConstructor(cx, args))
        return NULL;

    if (!args.rval().isObject()) {
        /*
         * Although constructors may return primitives (via proxies), this
         * API is asking for an object, so we report an error.
         */
        JSAutoByteString bytes;
        if (js_ValueToPrintable(cx, args.rval(), &bytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_NEW_RESULT,
                                 bytes.ptr());
        }
        return NULL;
    }

    return &args.rval().toObject();
}

JS_PUBLIC_API(JSOperationCallback)
JS_SetOperationCallback(JSContext *cx, JSOperationCallback callback)
{
#ifdef JS_THREADSAFE
    JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread()));
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
#ifdef JS_THREADSAFE
    AutoLockGC lock(cx->runtime);
#endif
    TriggerOperationCallback(cx);
}

JS_PUBLIC_API(void)
JS_TriggerAllOperationCallbacks(JSRuntime *rt)
{
#ifdef JS_THREADSAFE
    AutoLockGC lock(rt);
#endif
    TriggerAllOperationCallbacks(rt);
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

#ifdef JS_THREADSAFE
    if (!cx->thread())
        return false;
#endif

#ifdef JS_TRACER
    JS_ASSERT_IF(JS_ON_TRACE(cx) && JS_TRACE_MONITOR_ON_TRACE(cx)->tracecx == cx, cx->hasfp());
#endif

    StackFrame *fp = cx->maybefp();
    while (fp && fp->isDummyFrame())
        fp = fp->prev();
    return fp != NULL;
}

JS_PUBLIC_API(JSBool)
JS_SaveFrameChain(JSContext *cx)
{
    CHECK_REQUEST(cx);
    LeaveTrace(cx);
    return cx->stack.saveFrameChain();
}

JS_PUBLIC_API(void)
JS_RestoreFrameChain(JSContext *cx)
{
    CHECK_REQUEST(cx);
    JS_ASSERT_NOT_ON_TRACE(cx);
    cx->stack.restoreFrameChain();
}

/************************************************************************/
JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    CHECK_REQUEST(cx);
    return js_NewStringCopyN(cx, s, n);
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
    js = InflateString(cx, s, &n);
    if (!js)
        return NULL;
    str = js_NewString(cx, js, n);
    if (!str)
        cx->free_(js);
    return str;
}

JS_PUBLIC_API(JSBool)
JS_StringHasBeenInterned(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);

    if (!str->isAtom())
        return false;

    return AtomIsInterned(cx, &str->asAtom());
}

JS_PUBLIC_API(JSString *)
JS_InternJSString(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    JSAtom *atom = js_AtomizeString(cx, str, InternAtom);
    JS_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    CHECK_REQUEST(cx);
    JSAtom *atom = js_Atomize(cx, s, strlen(s), InternAtom);
    JS_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
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
    CHECK_REQUEST(cx);
    JSAtom *atom = js_AtomizeChars(cx, s, length, InternAtom);
    JS_ASSERT_IF(atom, JS_StringHasBeenInterned(cx, atom));
    return atom;
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
    return JS_InternUCStringN(cx, s, js_strlen(s));
}

JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str)
{
    return str->length();
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsZ(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    return str->getCharsZ(cx);
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsZAndLength(JSContext *cx, JSString *str, size_t *plength)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    *plength = str->length();
    return str->getCharsZ(cx);
}

JS_PUBLIC_API(const jschar *)
JS_GetStringCharsAndLength(JSContext *cx, JSString *str, size_t *plength)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    *plength = str->length();
    return str->getChars(cx);
}

JS_PUBLIC_API(const jschar *)
JS_GetInternedStringChars(JSString *str)
{
    return str->asAtom().chars();
}

JS_PUBLIC_API(const jschar *)
JS_GetInternedStringCharsAndLength(JSString *str, size_t *plength)
{
    JSAtom &atom = str->asAtom();
    *plength = atom.length();
    return atom.chars();
}

extern JS_PUBLIC_API(JSFlatString *)
JS_FlattenString(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, str);
    return str->getCharsZ(cx) ? (JSFlatString *)str : NULL;
}

extern JS_PUBLIC_API(const jschar *)
JS_GetFlatStringChars(JSFlatString *str)
{
    return str->chars();
}

JS_PUBLIC_API(JSBool)
JS_CompareStrings(JSContext *cx, JSString *str1, JSString *str2, int32 *result)
{
    return CompareStrings(cx, str1, str2, result);
}

JS_PUBLIC_API(JSBool)
JS_StringEqualsAscii(JSContext *cx, JSString *str, const char *asciiBytes, JSBool *match)
{
    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return false;
    *match = StringEqualsAscii(linearStr, asciiBytes);
    return true;
}

JS_PUBLIC_API(JSBool)
JS_FlatStringEqualsAscii(JSFlatString *str, const char *asciiBytes)
{
    return StringEqualsAscii(str, asciiBytes);
}

JS_PUBLIC_API(size_t)
JS_PutEscapedFlatString(char *buffer, size_t size, JSFlatString *str, char quote)
{
    return PutEscapedString(buffer, size, str, quote);
}

JS_PUBLIC_API(size_t)
JS_PutEscapedString(JSContext *cx, char *buffer, size_t size, JSString *str, char quote)
{
    JSLinearString *linearStr = str->ensureLinear(cx);
    if (!linearStr)
        return size_t(-1);
    return PutEscapedString(buffer, size, linearStr, quote);
}

JS_PUBLIC_API(JSBool)
JS_FileEscapedString(FILE *fp, JSString *str, char quote)
{
    JSLinearString *linearStr = str->ensureLinear(NULL);
    return linearStr && FileEscapedString(fp, linearStr, quote);
}

JS_PUBLIC_API(JSString *)
JS_NewGrowableString(JSContext *cx, jschar *chars, size_t length)
{
    CHECK_REQUEST(cx);
    return js_NewString(cx, chars, length);
}

JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, JSString *str, size_t start, size_t length)
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
    return str->getCharsZ(cx);
}

JS_PUBLIC_API(JSBool)
JS_MakeStringImmutable(JSContext *cx, JSString *str)
{
    CHECK_REQUEST(cx);
    return !!str->ensureFixed(cx);
}

JS_PUBLIC_API(JSBool)
JS_EncodeCharacters(JSContext *cx, const jschar *src, size_t srclen, char *dst, size_t *dstlenp)
{
    size_t n;
    if (!dst) {
        n = GetDeflatedStringLength(cx, src, srclen);
        if (n == (size_t)-1) {
            *dstlenp = 0;
            return JS_FALSE;
        }
        *dstlenp = n;
        return JS_TRUE;
    }

    return DeflateStringToBuffer(cx, src, srclen, dst, dstlenp);
}

JS_PUBLIC_API(JSBool)
JS_DecodeBytes(JSContext *cx, const char *src, size_t srclen, jschar *dst, size_t *dstlenp)
{
    return InflateStringToBuffer(cx, src, srclen, dst, dstlenp);
}

JS_PUBLIC_API(JSBool)
JS_DecodeUTF8(JSContext *cx, const char *src, size_t srclen, jschar *dst,
              size_t *dstlenp)
{
    return InflateUTF8StringToBuffer(cx, src, srclen, dst, dstlenp);
}

JS_PUBLIC_API(char *)
JS_EncodeString(JSContext *cx, JSString *str)
{
    const jschar *chars = str->getChars(cx);
    if (!chars)
        return NULL;
    return DeflateString(cx, chars, str->length());
}

JS_PUBLIC_API(size_t)
JS_GetStringEncodingLength(JSContext *cx, JSString *str)
{
    const jschar *chars = str->getChars(cx);
    if (!chars)
        return size_t(-1);
    return GetDeflatedStringLength(cx, chars, str->length());
}

JS_PUBLIC_API(size_t)
JS_EncodeStringToBuffer(JSString *str, char *buffer, size_t length)
{
    /*
     * FIXME bug 612141 - fix DeflateStringToBuffer interface so the result
     * would allow to distinguish between insufficient buffer and encoding
     * error.
     */
    size_t writtenLength = length;
    const jschar *chars = str->getChars(NULL);
    if (!chars)
        return size_t(-1);
    if (DeflateStringToBuffer(NULL, chars, str->length(), buffer, &writtenLength)) {
        JS_ASSERT(writtenLength <= length);
        return writtenLength;
    }
    JS_ASSERT(writtenLength <= length);
    size_t necessaryLength = GetDeflatedStringLength(NULL, chars, str->length());
    if (necessaryLength == size_t(-1))
        return size_t(-1);
    if (writtenLength != length) {
        /* Make sure that the buffer contains only valid UTF-8 sequences. */
        JS_ASSERT(js_CStringsAreUTF8);
        PodZero(buffer + writtenLength, length - writtenLength);
    }
    return necessaryLength;
}

JS_PUBLIC_API(JSBool)
JS_Stringify(JSContext *cx, jsval *vp, JSObject *replacer, jsval space,
             JSONWriteCallback callback, void *data)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, replacer, space);
    StringBuffer sb(cx);
    Value spacev = Valueify(space);
    if (!js_Stringify(cx, Valueify(vp), replacer, spacev, sb))
        return false;
    if (sb.empty()) {
        JSAtom *nullAtom = cx->runtime->atomState.nullAtom;
        return callback(nullAtom->chars(), nullAtom->length(), data);
    }
    return callback(sb.begin(), sb.length(), data);
}

JS_PUBLIC_API(JSBool)
JS_ParseJSON(JSContext *cx, const jschar *chars, uint32 len, jsval *vp)
{
    CHECK_REQUEST(cx);

    return ParseJSONWithReviver(cx, chars, len, NullValue(), Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_ParseJSONWithReviver(JSContext *cx, const jschar *chars, uint32 len, jsval reviver, jsval *vp)
{
    CHECK_REQUEST(cx);

    return ParseJSONWithReviver(cx, chars, len, Valueify(reviver), Valueify(vp));
}

JS_PUBLIC_API(JSBool)
JS_ReadStructuredClone(JSContext *cx, const uint64 *buf, size_t nbytes,
                       uint32 version, jsval *vp,
                       const JSStructuredCloneCallbacks *optionalCallbacks,
                       void *closure)
{
    if (version > JS_STRUCTURED_CLONE_VERSION) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_CLONE_VERSION);
        return false;
    }
    const JSStructuredCloneCallbacks *callbacks =
        optionalCallbacks ?
        optionalCallbacks :
        cx->runtime->structuredCloneCallbacks;
    return ReadStructuredClone(cx, buf, nbytes, Valueify(vp), callbacks, closure);
}

JS_PUBLIC_API(JSBool)
JS_WriteStructuredClone(JSContext *cx, jsval v, uint64 **bufp, size_t *nbytesp,
                        const JSStructuredCloneCallbacks *optionalCallbacks,
                        void *closure)
{
    const JSStructuredCloneCallbacks *callbacks =
        optionalCallbacks ?
        optionalCallbacks :
        cx->runtime->structuredCloneCallbacks;
    return WriteStructuredClone(cx, Valueify(v), (uint64_t **) bufp, nbytesp,
                                callbacks, closure);
}

JS_PUBLIC_API(JSBool)
JS_StructuredClone(JSContext *cx, jsval v, jsval *vp,
                   const JSStructuredCloneCallbacks *optionalCallbacks,
                   void *closure)
{
    const JSStructuredCloneCallbacks *callbacks =
        optionalCallbacks ?
        optionalCallbacks :
        cx->runtime->structuredCloneCallbacks;
    JSAutoStructuredCloneBuffer buf;
    return buf.write(cx, v, callbacks, closure) &&
           buf.read(cx, vp, callbacks, closure);
}

void
JSAutoStructuredCloneBuffer::clear()
{
    if (data_) {
        Foreground::free_(data_);
        data_ = NULL;
        nbytes_ = 0;
        version_ = 0;
    }
}

void
JSAutoStructuredCloneBuffer::adopt(JSUint64 *data, size_t nbytes, JSUint32 version)
{
    clear();
    data_ = data;
    nbytes_ = nbytes;
    version_ = version;
}

bool
JSAutoStructuredCloneBuffer::copy(const JSUint64 *srcData, size_t nbytes, JSUint32 version)
{
    JSUint64 *newData = static_cast<JSUint64 *>(OffTheBooks::malloc_(nbytes));
    if (!newData)
        return false;

    memcpy(newData, srcData, nbytes);

    clear();
    data_ = newData;
    nbytes_ = nbytes;
    version_ = version;
    return true;
}
void
JSAutoStructuredCloneBuffer::steal(JSUint64 **datap, size_t *nbytesp, JSUint32 *versionp)
{
    *datap = data_;
    *nbytesp = nbytes_;
    if (versionp)
        *versionp = version_;

    data_ = NULL;
    nbytes_ = 0;
    version_ = 0;
}

bool
JSAutoStructuredCloneBuffer::read(JSContext *cx, jsval *vp,
                                  const JSStructuredCloneCallbacks *optionalCallbacks,
                                  void *closure) const
{
    JS_ASSERT(cx);
    JS_ASSERT(data_);
    return !!JS_ReadStructuredClone(cx, data_, nbytes_, version_, vp,
                                    optionalCallbacks, closure);
}

bool
JSAutoStructuredCloneBuffer::write(JSContext *cx, jsval v,
                                   const JSStructuredCloneCallbacks *optionalCallbacks,
                                   void *closure)
{
    clear();
    bool ok = !!JS_WriteStructuredClone(cx, v, &data_, &nbytes_,
                                        optionalCallbacks, closure);
    if (!ok) {
        data_ = NULL;
        nbytes_ = 0;
        version_ = JS_STRUCTURED_CLONE_VERSION;
    }
    return ok;
}

void
JSAutoStructuredCloneBuffer::swap(JSAutoStructuredCloneBuffer &other)
{
    JSUint64 *data = other.data_;
    size_t nbytes = other.nbytes_;
    JSUint32 version = other.version_;

    other.data_ = this->data_;
    other.nbytes_ = this->nbytes_;
    other.version_ = this->version_;

    this->data_ = data;
    this->nbytes_ = nbytes;
    this->version_ = version;
}

JS_PUBLIC_API(void)
JS_SetStructuredCloneCallbacks(JSRuntime *rt, const JSStructuredCloneCallbacks *callbacks)
{
    rt->structuredCloneCallbacks = callbacks;
}

JS_PUBLIC_API(JSBool)
JS_ReadUint32Pair(JSStructuredCloneReader *r, uint32 *p1, uint32 *p2)
{
    return r->input().readPair((uint32_t *) p1, (uint32_t *) p2);
}

JS_PUBLIC_API(JSBool)
JS_ReadBytes(JSStructuredCloneReader *r, void *p, size_t len)
{
    return r->input().readBytes(p, len);
}

JS_PUBLIC_API(JSBool)
JS_WriteUint32Pair(JSStructuredCloneWriter *w, uint32 tag, uint32 data)
{
    return w->output().writePair(tag, data);
}

JS_PUBLIC_API(JSBool)
JS_WriteBytes(JSStructuredCloneWriter *w, const void *p, size_t len)
{
    return w->output().writeBytes(p, len);
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
 * Dates.
 */
JS_PUBLIC_API(JSObject *)
JS_NewDateObject(JSContext *cx, int year, int mon, int mday, int hour, int min, int sec)
{
    CHECK_REQUEST(cx);
    return js_NewDateObject(cx, year, mon, mday, hour, min, sec);
}

JS_PUBLIC_API(JSObject *)
JS_NewDateObjectMsec(JSContext *cx, jsdouble msec)
{
    CHECK_REQUEST(cx);
    return js_NewDateObjectMsec(cx, msec);
}

JS_PUBLIC_API(JSBool)
JS_ObjectIsDate(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj);
    return obj->isDate();
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, JSObject *obj, char *bytes, size_t length, uintN flags)
{
    CHECK_REQUEST(cx);
    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    RegExpStatics *res = RegExpStatics::extractFrom(obj->asGlobal());
    JSObject *reobj = RegExp::createObject(cx, res, chars, length, flags, NULL);
    cx->free_(chars);
    return reobj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, JSObject *obj, jschar *chars, size_t length, uintN flags)
{
    CHECK_REQUEST(cx);
    RegExpStatics *res = RegExpStatics::extractFrom(obj->asGlobal());
    return RegExp::createObject(cx, res, chars, length, flags, NULL);
}

JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, JSObject *obj, JSString *input, JSBool multiline)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, input);

    RegExpStatics::extractFrom(obj->asGlobal())->reset(input, !!multiline);
}

JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);
    JS_ASSERT(obj);

    RegExpStatics::extractFrom(obj->asGlobal())->clear();
}

JS_PUBLIC_API(JSBool)
JS_ExecuteRegExp(JSContext *cx, JSObject *obj, JSObject *reobj, jschar *chars, size_t length,
                 size_t *indexp, JSBool test, jsval *rval)
{
    CHECK_REQUEST(cx);

    RegExp *re = RegExp::extractFrom(reobj);
    if (!re)
        return false;

    JSString *str = js_NewStringCopyN(cx, chars, length);
    if (!str)
        return false;

    return re->execute(cx, RegExpStatics::extractFrom(obj->asGlobal()), str, indexp, test,
                       Valueify(rval));
}

JS_PUBLIC_API(JSObject *)
JS_NewRegExpObjectNoStatics(JSContext *cx, char *bytes, size_t length, uintN flags)
{
    CHECK_REQUEST(cx);
    jschar *chars = InflateString(cx, bytes, &length);
    if (!chars)
        return NULL;
    JSObject *obj = RegExp::createObjectNoStatics(cx, chars, length, flags, NULL);
    cx->free_(chars);
    return obj;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObjectNoStatics(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
    CHECK_REQUEST(cx);
    return RegExp::createObjectNoStatics(cx, chars, length, flags, NULL);
}

JS_PUBLIC_API(JSBool)
JS_ExecuteRegExpNoStatics(JSContext *cx, JSObject *obj, jschar *chars, size_t length,
                          size_t *indexp, JSBool test, jsval *rval)
{
    CHECK_REQUEST(cx);

    RegExp *re = RegExp::extractFrom(obj);
    if (!re)
        return false;

    JSString *str = js_NewStringCopyN(cx, chars, length);
    if (!str)
        return false;

    return re->executeNoStatics(cx, str, indexp, test, Valueify(rval));
}

JS_PUBLIC_API(JSBool)
JS_ObjectIsRegExp(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj);
    return obj->isRegExp();
}

JS_PUBLIC_API(uintN)
JS_GetRegExpFlags(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);

    RegExp *re = RegExp::extractFrom(obj);
    return re->getFlags();
}

JS_PUBLIC_API(JSString *)
JS_GetRegExpSource(JSContext *cx, JSObject *obj)
{
    CHECK_REQUEST(cx);

    RegExp *re = RegExp::extractFrom(obj);
    return re->getSource();
}

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
    return (JSBool) cx->isExceptionPending();
}

JS_PUBLIC_API(JSBool)
JS_GetPendingException(JSContext *cx, jsval *vp)
{
    CHECK_REQUEST(cx);
    if (!cx->isExceptionPending())
        return JS_FALSE;
    Valueify(*vp) = cx->getPendingException();
    assertSameCompartment(cx, *vp);
    return JS_TRUE;
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
    cx->setPendingException(Valueify(v));
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
    cx->clearPendingException();
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
    state = (JSExceptionState *) cx->malloc_(sizeof(JSExceptionState));
    if (state) {
        state->throwing = JS_GetPendingException(cx, &state->exception);
        if (state->throwing && JSVAL_IS_GCTHING(state->exception))
            js_AddRoot(cx, Valueify(&state->exception), "JSExceptionState.exception");
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
        if (state->throwing && JSVAL_IS_GCTHING(state->exception)) {
            assertSameCompartment(cx, state->exception);
            JS_RemoveValueRoot(cx, &state->exception);
        }
        cx->free_(state);
    }
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    assertSameCompartment(cx, v);
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
    return reinterpret_cast<jsword>(JS_THREAD_ID(cx));
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
    JS_ASSERT(!cx->outstandingRequests);
    if (cx->thread()) {
        JS_ASSERT(CURRENT_THREAD_IS_ME(cx->thread()));
        return reinterpret_cast<jsword>(cx->thread()->id);
    }

    if (!js_InitContextThreadAndLockGC(cx)) {
        js_ReportOutOfMemory(cx);
        return -1;
    }

    JS_UNLOCK_GC(cx->runtime);
#endif
    return 0;
}

JS_PUBLIC_API(jsword)
JS_ClearContextThread(JSContext *cx)
{
#ifdef JS_THREADSAFE
    /*
     * cx must have exited all requests it entered and, if cx is associated
     * with a thread, this must be called only from that thread.  If not, this
     * is a harmless no-op.
     */
    JS_ASSERT(cx->outstandingRequests == 0);
    JSThread *t = cx->thread();
    if (!t)
        return 0;
    JS_ASSERT(CURRENT_THREAD_IS_ME(t));

    /*
     * We must not race with a GC that accesses cx->thread for all threads,
     * see bug 476934.
     */
    JSRuntime *rt = cx->runtime;
    AutoLockGC lock(rt);
    js_WaitForGC(rt);
    js_ClearContextThread(cx);
    JS_ASSERT_IF(JS_CLIST_IS_EMPTY(&t->contextList), !t->data.requestDepth);

    /*
     * We can access t->id as long as the GC lock is held and we cannot race
     * with the GC that may delete t.
     */
    return reinterpret_cast<jsword>(t->id);
#else
    return 0;
#endif
}

#ifdef JS_GC_ZEAL
JS_PUBLIC_API(void)
JS_SetGCZeal(JSContext *cx, uint8 zeal, uint32 frequency, JSBool compartment)
{
    cx->runtime->gcZeal_ = zeal;
    cx->runtime->gcZealFrequency = frequency;
    cx->runtime->gcNextScheduled = zeal >= 2 ? frequency : 0;
    cx->runtime->gcDebugCompartmentGC = !!compartment;
}

JS_PUBLIC_API(void)
JS_ScheduleGC(JSContext *cx, uint32 count, JSBool compartment)
{
    cx->runtime->gcNextScheduled = count;
    cx->runtime->gcDebugCompartmentGC = !!compartment;
}
#endif

/************************************************************************/

#if !defined(STATIC_EXPORTABLE_JS_API) && !defined(STATIC_JS_API) && defined(XP_WIN)

#include "jswin.h"

/*
 * Initialization routine for the JS DLL.
 */
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#endif
