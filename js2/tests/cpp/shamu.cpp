

#ifdef _WIN32
 // Turn off warnings about identifiers too long in browser information
#pragma warning(disable: 4786)
#endif


#include <string.h>
#include <stdarg.h>

#include <algorithm>

#include "jspubtd.h"
#include "jsapi.h"

#include "jshash.h"
#include "jsdhash.h"

#include "world.h"
#include "reader.h"
#include "parser.h"
#include "js2runtime.h"
#include "bytecodegen.h"

#include "shamu.h"

void nyi()
{
    ASSERT("Not Yet Implemented");
    throw "Not Yet Implemented";
}

#define JS_ASSERT(x) ASSERT(x)

//
// wrapper function to call SpiderMonkey API functions from the DikDik interpreter.
//  This function is supplied as the native method dispatch routine whenever a JSFunction
//  object is constructed and, when invoked, is passed the target function.
//
//
JavaScript::JS2Runtime::js2val callSpiderMonkeyNative(JavaScript::JS2Runtime::JSFunction::NativeCode *js2target, 
                        JavaScript::JS2Runtime::Context *js2cx, 
                        const JavaScript::JS2Runtime::js2val thisValue, 
                        JavaScript::JS2Runtime::js2val argv[], uint32 argc)
{
    JSNative target = (JSNative)js2target;
    jsval result;
    jsval *args = new jsval[argc];          // XXX unnecessary to make copy of these ?
    for (uint32 i = 0; i < argc; i++) {
        args[i] = argv[i];
    }
    target( (JSContext *)js2cx, (JSObject *)(JavaScript::JS2Runtime::JSValue::object(thisValue)), argc, args, &result);
    return result;
}



extern "C" {

#ifdef HAVE_VA_LIST_AS_ARRAY
#define JS_ADDRESSOF_VA_LIST(ap) (ap)
#else
#define JS_ADDRESSOF_VA_LIST(ap) (&(ap))
#endif

#if defined(JS_PARANOID_REQUEST) && defined(JS_THREADSAFE)
#define CHECK_REQUEST(cx)       JS_ASSERT(cx->requestDepth)
#else
#define CHECK_REQUEST(cx)       ((void)0)
#endif


/*
 * Report an exception, which is currently realized as a printf-style format
 * string and its arguments.
 */
typedef enum JSErrNum {
#define MSG_DEF(name, number, count, exception, format) \
    name = number,
#include "js.msg"
#undef MSG_DEF
    JSErr_Limit
} JSErrNum;

JSErrorFormatString js_ErrorFormatString[JSErr_Limit] = {
#if JS_HAS_DFLT_MSG_STRINGS
#define MSG_DEF(name, number, count, exception, format) \
    { format, count } ,
#else
#define MSG_DEF(name, number, count, exception, format) \
    { NULL, count } ,
#endif
#include "js.msg"
#undef MSG_DEF
};


const JSErrorFormatString *
js2_GetErrorMessage(void *userRef, const char *locale, const uintN errorNumber)
{
    if ((errorNumber > 0) && (errorNumber < JSErr_Limit))
        return &js_ErrorFormatString[errorNumber];
    return NULL;
}


JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return JavaScript::JS2Runtime::kNaNValue;
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return JavaScript::JS2Runtime::kNegativeInfinity;
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return JavaScript::JS2Runtime::kPositiveInfinity;
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return STRING_TO_JSVAL(&js2cx->Empty_StringAtom);
}

JS_PUBLIC_API(JSUint32) JS_snprintf(char *out, JSUint32 outlen, const char *fmt, ...)
{
    nyi();
    return 0;
}

JS_FRIEND_API(jsval *)
js_AllocStack(JSContext *cx, uintN nslots, void **markp)
{
    nyi();
    return NULL;
}


JS_FRIEND_API(void)
js_FreeStack(JSContext *cx, void *mark)
{
    nyi();
}



#ifdef JS_ARGUMENT_FORMATTER_DEFINED
static JSBool
TryArgumentFormatter(JSContext *cx, const char **formatp, JSBool fromJS,
                     jsval **vpp, va_list *app)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;

    const char *format;
    JSArgumentFormatMap *map;

    format = *formatp;
    for (map = (JSArgumentFormatMap *)(js2cx->argumentFormatMap); map; map = map->next) {
        if (!strncmp(format, map->format, map->length)) {
            *formatp = format + map->length;
            return map->formatter(cx, format, fromJS, vpp, app);
        }
    }
    JS_ReportErrorNumber(cx, js2_GetErrorMessage, NULL, JSMSG_BAD_CHAR, format);
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

JSFunction *
js_ValueToFunction(JSContext *cx, jsval *vp, JSBool constructing)
{
    JSFunction *fun = NULL;
    // the SpiderMonkey implementation of this invokes DefaultValue
    // as well as potentially returning the private field of a
    // JSObject. For now we'll just error if the value is not
    // specifically a function object.
    if (JavaScript::JS2Runtime::JSValue::isFunction(*vp))
        fun = (JSFunction *)JavaScript::JS2Runtime::JSValue::function(*vp);
    else
        JS_ReportErrorNumber(cx, js2_GetErrorMessage, NULL, JSMSG_NOT_FUNCTION, "");
    return fun;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArgumentsVA(JSContext *cx, uintN argc, jsval *argv,
                      const char *format, va_list ap)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;

    jsval *sp;
    JSBool required;
    char c;
    JSFunction *fun;
    jsdouble d;
    const JavaScript::String *str;
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
                fun = js_ValueToFunction(cx, &argv[-2], JS_FALSE);
                if (fun) {
                    char numBuf[12];
                    JS_snprintf(numBuf, sizeof numBuf, "%u", argc);
                    JS_ReportErrorNumber(cx, js2_GetErrorMessage, NULL,
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
            *va_arg(ap, JSBool *) = JavaScript::JS2Runtime::JSValue::boolean(JavaScript::JS2Runtime::JSValue::toBoolean(js2cx, *sp));
            break;
          case 'c':
            *va_arg(ap, uint16 *) = JavaScript::JS2Runtime::JSValue::f64(JavaScript::JS2Runtime::JSValue::toUInt16(js2cx, *sp));
            break;
          case 'i':
            *va_arg(ap, int32 *) = JavaScript::JS2Runtime::JSValue::f64(JavaScript::JS2Runtime::JSValue::toInt32(js2cx, *sp));
            break;
          case 'u':
            *va_arg(ap, uint32 *) = JavaScript::JS2Runtime::JSValue::f64(JavaScript::JS2Runtime::JSValue::toUInt32(js2cx, *sp));
            break;
          case 'j':
            *va_arg(ap, int32 *) = JavaScript::JS2Runtime::JSValue::f64(JavaScript::JS2Runtime::JSValue::toInt32(js2cx, *sp));
            break;
          case 'd':
            *va_arg(ap, jsdouble *) = JavaScript::JS2Runtime::JSValue::f64(JavaScript::JS2Runtime::JSValue::toNumber(js2cx, *sp));
            break;
          case 'I':
            d = JavaScript::JS2Runtime::JSValue::f64(JavaScript::JS2Runtime::JSValue::toNumber(js2cx, *sp));
            *va_arg(ap, jsdouble *) = JavaScript::JS2Runtime::JSValue::float64ToInteger(d);
            break;
          case 's':
          case 'S':
          case 'W':
            str = JavaScript::JS2Runtime::JSValue::string(JavaScript::JS2Runtime::JSValue::toString(js2cx, *sp));
            *sp = STRING_TO_JSVAL(str);
            if (c == 's')
                *va_arg(ap, char **) = JS_GetStringBytes((JSString *)str);
            else if (c == 'W')
                *va_arg(ap, jschar **) = JS_GetStringChars((JSString *)str);
            else
                *va_arg(ap, JSString **) = (JSString *)str;
            break;
          case 'o':
            obj = (JSObject *)JavaScript::JS2Runtime::JSValue::object(JavaScript::JS2Runtime::JSValue::toObject(js2cx, *sp));
            *sp = OBJECT_TO_JSVAL(obj);
            *va_arg(ap, JSObject **) = obj;
            break;
          case 'f':
            fun = js_ValueToFunction(cx, sp, JS_FALSE);
            if (!fun)
                return JS_FALSE;
            *sp = OBJECT_TO_JSVAL(fun); // DikDik isn't making a distinction here, OBJECT_TO_JSVAL(fun->object);
            *va_arg(ap, JSFunction **) = fun;
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

JS_PUBLIC_API(jsval *)
JS_PushArguments(JSContext *cx, void **markp, const char *format, ...)
{
    va_list ap;
    jsval *argv;

    va_start(ap, format);
    argv = JS_PushArgumentsVA(cx, markp, format, ap);
    va_end(ap);
    return argv;
}

JS_PUBLIC_API(jsval *)
JS_PushArgumentsVA(JSContext *cx, void **markp, const char *format, va_list ap)
{
    uintN argc;
    jsval *argv, *sp;
    char c;
    const char *cp;
    JSString *str;
    JSFunction *fun;
#if 0
    JSStackHeader *sh;
#endif

    CHECK_REQUEST(cx);
    *markp = NULL;
    argc = 0;
    for (cp = format; (c = *cp) != '\0'; cp++) {
        /*
         * Count non-space non-star characters as individual jsval arguments.
         * This may over-allocate stack, but we'll fix below.
         */
        if (isspace(c) || c == '*')
            continue;
        argc++;
    }
    sp = js_AllocStack(cx, argc, markp);
    if (!sp)
        return NULL;
    argv = sp;
    while ((c = *format++) != '\0') {
        if (isspace(c) || c == '*')
            continue;
        switch (c) {
          case 'b':
            *sp = BOOLEAN_TO_JSVAL((JSBool) va_arg(ap, int));
            break;
          case 'c':
            *sp = INT_TO_JSVAL((uint16) va_arg(ap, unsigned int));
            break;
          case 'i':
          case 'j':
            *sp = DOUBLE_TO_JSVAL(JS_NewDouble(cx, (jsdouble) va_arg(ap, int32)));
            break;
          case 'u':
            *sp = DOUBLE_TO_JSVAL(JS_NewDouble(cx, (jsdouble) va_arg(ap, uint32)));
            break;
          case 'd':
          case 'I':
            *sp = DOUBLE_TO_JSVAL(JS_NewDouble(cx, va_arg(ap, jsdouble)));
            break;
          case 's':
            str = JS_NewStringCopyZ(cx, va_arg(ap, char *));
            if (!str)
                goto bad;
            *sp = STRING_TO_JSVAL(str);
            break;
          case 'W':
            str = JS_NewUCStringCopyZ(cx, va_arg(ap, jschar *));
            if (!str)
                goto bad;
            *sp = STRING_TO_JSVAL(str);
            break;
          case 'S':
            str = va_arg(ap, JSString *);
            *sp = STRING_TO_JSVAL(str);
            break;
          case 'o':
            *sp = OBJECT_TO_JSVAL(va_arg(ap, JSObject *));
            break;
          case 'f':
            fun = va_arg(ap, JSFunction *);
//            *sp = fun ? OBJECT_TO_JSVAL(fun->object) : JSVAL_NULL;
            *sp = fun ? OBJECT_TO_JSVAL(fun) : JSVAL_NULL;
            break;
          case 'v':
            *sp = va_arg(ap, jsval);
            break;
          default:
            format--;
            if (!TryArgumentFormatter(cx, &format, JS_FALSE, &sp,
                                      JS_ADDRESSOF_VA_LIST(ap))) {
                goto bad;
            }
            /* NB: the formatter already updated sp, so we continue here. */
            continue;
        }
        sp++;
    }

    /*
     * We may have overallocated stack due to a multi-character format code
     * handled by a JSArgumentFormatter.  Give back that stack space!
     */
    JS_ASSERT(sp <= argv + argc);
#if 0
    if (sp < argv + argc) {
        /* Return slots not pushed to the current stack arena. */
        cx->stackPool.current->avail = (jsuword)sp;

        /* Reduce the count of slots the GC will scan in this stack segment. */
        sh = cx->stackHeaders;
        JS_ASSERT(JS_STACK_SEGMENT(sh) + sh->nslots == argv + argc);
        sh->nslots -= argc - (sp - argv);
    }
#endif
    return argv;

bad:
    js_FreeStack(cx, *markp);
    return NULL;
}

JS_PUBLIC_API(void)
JS_PopArguments(JSContext *cx, void *mark)
{
    CHECK_REQUEST(cx);
    js_FreeStack(cx, mark);
}

JS_PUBLIC_API(JSBool)
JS_AddArgumentFormatter(JSContext *cx, const char *format,
                        JSArgumentFormatter formatter)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;

    size_t length;
    JSArgumentFormatMap **mpp, *map;

    length = strlen(format);
    mpp = (JSArgumentFormatMap **)(&js2cx->argumentFormatMap);
    while ((map = *mpp) != NULL) {
        /* Insert before any shorter string to match before prefixes. */
        if (map->length < length)
            break;
        if (map->length == length && !strcmp(map->format, format))
            goto out;
        mpp = &map->next;
    }
    map = (JSArgumentFormatMap *) JS_malloc(cx, sizeof *map);
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
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;

    size_t length;
    JSArgumentFormatMap **mpp, *map;

    length = strlen(format);
    mpp = (JSArgumentFormatMap **)(&js2cx->argumentFormatMap);
    while ((map = *mpp) != NULL) {
        if (map->length == length && !strcmp(map->format, format)) {
            *mpp = map->next;
            JS_free(cx, map);
            return;
        }
        mpp = &map->next;
    }
}

#endif


JS_PUBLIC_API(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v)
{
    nyi();
    CHECK_REQUEST(cx);
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, jsval v)
{
    nyi();
    CHECK_REQUEST(cx);
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_ValueToString(JSContext *cx, jsval v)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::js2val js2value = v;//convertJSValueToJS2Value(cx, v);
    
    return (JSString *)JavaScript::JS2Runtime::JSValue::string(JavaScript::JS2Runtime::JSValue::toString(js2cx, js2value));
}

JS_PUBLIC_API(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
    nyi();
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSType)
JS_TypeOfValue(JSContext *cx, jsval v)
{
    JSType type;
    JSObject *obj;
//    JSObjectOps *ops;
//    JSClass *clasp;

    CHECK_REQUEST(cx);
    if (JSVAL_IS_OBJECT(v)) {
        /* XXX JSVAL_IS_OBJECT(v) is true for null too! Can we change ECMA? */
        obj = JSVAL_TO_OBJECT(v);
        if (obj 
/*            &&
            (ops = obj->map->ops,
             ops == &js_ObjectOps
             ? (clasp = OBJ_GET_CLASS(cx, obj),
                clasp->call || clasp == &js_FunctionClass)
             : ops->call != 0) */) {
            type = JSTYPE_FUNCTION;
        } else {
            type = JSTYPE_OBJECT;
        }
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

//--------- from jsatom.c --------------
/*
 * Keep this in sync with jspubtd.h -- an assertion below will insist that
 * its length match the JSType enum's JSTYPE_LIMIT limit value.
 */
const char *js_type_str[] = {
    "undefined",
    "object",
    "function",
    "string",
    "number",
    "boolean",
};
//-------------------------------------


JS_PUBLIC_API(const char *)
JS_GetTypeName(JSContext *cx, JSType type)
{
    if ((uintN)type >= (uintN)JSTYPE_LIMIT)
        return NULL;
    return js_type_str[type];
}

/************************************************************************/



JavaScript::World world;
JavaScript::Arena a;
JavaScript::JS2Runtime::JSObject *globalObject;


JS_PUBLIC_API(JSRuntime *)
JS_NewRuntime(uint32 maxbytes)
{
    return (JSRuntime *)&world;
}

JS_PUBLIC_API(void)
JS_DestroyRuntime(JSRuntime *rt)
{
}

JS_PUBLIC_API(void)
JS_ShutDown(void)
{
}

JS_PUBLIC_API(void *)
JS_GetRuntimePrivate(JSRuntime *rt)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetRuntimePrivate(JSRuntime *rt, void *data)
{
    nyi();
}

#ifdef JS_THREADSAFE

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
    // nyi();       XXX get working
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
    // nyi();       XXX get working
}

/* Yield to pending GC operations, regardless of request depth */
JS_PUBLIC_API(void)
JS_YieldRequest(JSContext *cx)
{
    // nyi();       XXX get working
}

JS_PUBLIC_API(jsrefcount)
JS_SuspendRequest(JSContext *cx)
{
    return 0;
}

JS_PUBLIC_API(void)
JS_ResumeRequest(JSContext *cx, jsrefcount saveDepth)
{
    // nyi();       XXX get working
}

#endif /* JS_THREADSAFE */

JS_PUBLIC_API(void)
JS_Lock(JSRuntime *rt)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_Unlock(JSRuntime *rt)
{
    nyi();
}

JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    JavaScript::World *world = (JavaScript::World *)rt;
    JavaScript::JS2Runtime::Context *js2cx = new JavaScript::JS2Runtime::Context(&globalObject, *((JavaScript::World *)rt), a, JavaScript::Pragma::js2);
    world->contextList.push_back(js2cx);
    return (JSContext *)(js2cx);
}

JS_PUBLIC_API(void)
JS_DestroyContext(JSContext *cx)
{
}

JS_PUBLIC_API(void)
JS_DestroyContextNoGC(JSContext *cx)
{
}

JS_PUBLIC_API(void)
JS_DestroyContextMaybeGC(JSContext *cx)
{
}

JS_PUBLIC_API(void *)
JS_GetContextPrivate(JSContext *cx)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetContextPrivate(JSContext *cx, void *data)
{
    nyi();
}

JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return (JSRuntime *)(&js2cx->mWorld);
}

JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    JavaScript::World *world = (JavaScript::World *)rt;
    if (*iterp == NULL) {
        std::vector<JavaScript::JS2Runtime::Context *>::reverse_iterator *it = new std::vector<JavaScript::JS2Runtime::Context *>::reverse_iterator(world->contextList.rbegin());
        *iterp = (JSContext *)it;
        return (JSContext *)(**it);
    }
    else {
        std::vector<JavaScript::JS2Runtime::Context *>::reverse_iterator *it = (std::vector<JavaScript::JS2Runtime::Context *>::reverse_iterator *)(*iterp);
        (*it)++;
        if (*it == world->contextList.rend())
            return NULL;
        else {
            *iterp = (JSContext *)it;
            return (JSContext *)(**it);
        }
    }
}

JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx)
{
    nyi();
    return JSVERSION_UNKNOWN;
}

JS_PUBLIC_API(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version)
{
    nyi();
    return JSVERSION_UNKNOWN;
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
    {JSVERSION_1_5,     "1.5"},
    {JSVERSION_DEFAULT, "default"},
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
    nyi();
    return 0;
}

JS_PUBLIC_API(uint32)
JS_SetOptions(JSContext *cx, uint32 options)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(uint32)
JS_ToggleOptions(JSContext *cx, uint32 options)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "'Shamu' The Incredible JavaScript DikDik Shim";
}


JS_PUBLIC_API(JSObject *)
JS_GetGlobalObject(JSContext *cx)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj)
{
//
//  suppressing the nyi for now, the global object is built into
//  the context initialization
//    nyi();

}

static JSObject *
InitFunctionAndObjectClasses(JSContext *cx, JSObject *obj)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj)
{
    nyi();
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ResolveStandardClass(JSContext *cx, JSObject *obj, jsval id,
                        JSBool *resolved)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStandardClasses(JSContext *cx, JSObject *obj)
{
    nyi();
    return JS_FALSE;
}


JS_PUBLIC_API(JSObject *)
JS_GetScopeChain(JSContext *cx)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    return malloc(nbytes);
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
    return realloc(p, nbytes);
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
    free(p);
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(jsdouble *)
JS_NewDouble(JSContext *cx, jsdouble d)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    jsdouble *dptr = (jsdouble *)js2cx->mArena.allocate(sizeof(jsdouble));
    *dptr = d;
    return dptr;
}

JS_PUBLIC_API(JSBool)
JS_NewDoubleValue(JSContext *cx, jsdouble d, jsval *rval)
{
    *rval = DOUBLE_TO_JSVAL(JS_NewDouble(cx, d));
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_NewNumberValue(JSContext *cx, jsdouble d, jsval *rval)
{
    *rval = DOUBLE_TO_JSVAL(JS_NewDouble(cx, d));
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_AddRoot(JSContext *cx, void *rp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRootRT(JSRuntime *rt, void *rp, const char *name)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_RemoveRoot(JSContext *cx, void *rp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_RemoveRootRT(JSRuntime *rt, void *rp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRoot(JSContext *cx, void *rp, const char *name)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_ClearNewbornRoots(JSContext *cx)
{
}

typedef struct GCRootMapArgs {
    JSGCRootMapFun map;
    void *data;
} GCRootMapArgs;

JS_STATIC_DLL_CALLBACK(JSDHashOperator)
js_gcroot_mapper(JSDHashTable *table, JSDHashEntryHdr *hdr, uint32 number,
                 void *arg)
{
    nyi();
    return JSDHashOperator(0);
}

JS_PUBLIC_API(uint32)
JS_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(JSBool)
JS_LockGCThing(JSContext *cx, void *thing)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_MarkGCThing(JSContext *cx, void *thing, const char *name, void *arg)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_GC(JSContext *cx)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
    nyi();
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallback(JSContext *cx, JSGCCallback cb)
{
    nyi();
    return cb;
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallbackRT(JSRuntime *rt, JSGCCallback cb)
{
//    nyi();        not doing GC, so no need to call back :-)
    return cb;
}

JS_PUBLIC_API(JSBool)
JS_IsAboutToBeFinalized(JSContext *cx, void *thing)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(intN)
JS_AddExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(intN)
JS_RemoveExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, jschar *chars, size_t length, intN type)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(intN)
JS_GetExternalStringGCType(JSRuntime *rt, JSString *str)
{
    nyi();
    return 0;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
    nyi();
}

bool breakit = true;

JS_PUBLIC_API(JSBool)
JS_ValueToId(JSContext *cx, jsval v, jsid *idp)
{
    if (breakit) nyi();

    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;

    CHECK_REQUEST(cx);
    if (JSVAL_IS_INT(v)) {
        *idp = v;
    } else {
        const JavaScript::String *str = JavaScript::JS2Runtime::JSValue::string(JavaScript::JS2Runtime::JSValue::toString(js2cx, v));
        *idp = (jsid)(&js2cx->mWorld.identifiers[*str]);
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_IdToValue(JSContext *cx, jsid id, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_PropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
    nyi();
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStub(JSContext *cx, JSObject *obj)
{
    nyi();
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ResolveStub(JSContext *cx, JSObject *obj, jsval id)
{
    nyi();
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertStub(JSContext *cx, JSObject *obj, JSType type, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_FinalizeStub(JSContext *cx, JSObject *obj)
{
    nyi();
}

JS_PUBLIC_API(JSObject *)
JS_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
    nyi();
    return NULL;
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSContext *cx, JSObject *obj)
{
    nyi();
    return NULL;
}
#else
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSObject *obj)
{
    nyi();
    return NULL;
}
#endif

JS_PUBLIC_API(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
                      jsval *argv)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return (JSObject *)JavaScript::JS2Runtime::JSValue::object(JavaScript::JS2Runtime::Object_Type->newInstance(js2cx));
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObjectWithArguments(JSContext *cx, JSClass *clasp, JSObject *proto,
                                JSObject *parent, uintN argc, jsval *argv)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_DefineObject(JSContext *cx, JSObject *obj, const char *name, JSClass *clasp,
                JSObject *proto, uintN attrs)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::JSObject *js2obj = (JavaScript::JS2Runtime::JSObject *)obj;
    JavaScript::JS2Runtime::js2val newObj = JavaScript::JS2Runtime::Object_Type->newInstance(js2cx);
    js2obj->setProperty(js2cx, JavaScript::widenCString(name), NULL, newObj);
    return (JSObject *)(JavaScript::JS2Runtime::JSValue::object(newObj));
}

JS_PUBLIC_API(JSBool)
JS_DefineConstDoubles(JSContext *cx, JSObject *obj, JSConstDoubleSpec *cds)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineProperties(JSContext *cx, JSObject *obj, JSPropertySpec *ps)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::JSObject *js2obj = (JavaScript::JS2Runtime::JSObject *)obj;
    while (ps->name) {
        js2obj->setProperty(js2cx, JavaScript::widenCString(ps->name), NULL, JavaScript::JS2Runtime::kUndefinedValue);
        ps++;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_DefineProperty(JSContext *cx, JSObject *obj, const char *name, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::JSObject *js2obj = (JavaScript::JS2Runtime::JSObject *)obj;
    js2obj->setProperty(js2cx, JavaScript::widenCString(name), NULL, value /*convertJSValueToJS2Value(cx, value) */);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
                            int8 tinyid, jsval value,
                            JSPropertyOp getter, JSPropertyOp setter,
                            uintN attrs)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
                 const char *alias)
{
    nyi();
    return JS_FALSE;
}


JS_PUBLIC_API(JSBool)
JS_GetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN *attrsp, JSBool *foundp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN attrs, JSBool *foundp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty2(JSContext *cx, JSObject *obj, const char *name,
                   jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, jsval value,
                    JSPropertyOp getter, JSPropertyOp setter,
                    uintN attrs)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN *attrsp, JSBool *foundp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN attrs, JSBool *foundp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineUCPropertyWithTinyId(JSContext *cx, JSObject *obj,
                              const jschar *name, size_t namelen,
                              int8 tinyid, jsval value,
                              JSPropertyOp getter, JSPropertyOp setter,
                              uintN attrs)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_LookupUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen,
                    jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteUCProperty2(JSContext *cx, JSObject *obj,
                     const jschar *name, size_t namelen,
                     jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSObject *)
JS_NewArrayObject(JSContext *cx, jsint length, jsval *vector)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return (JSObject *)JavaScript::JS2Runtime::JSValue::instance(JavaScript::JS2Runtime::Array_Type->newInstance(js2cx));
}

JS_PUBLIC_API(JSBool)
JS_IsArrayObject(JSContext *cx, JSObject *obj)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsuint length)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_HasArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
                 JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement2(JSContext *cx, JSObject *obj, jsint index, jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_ClearScope(JSContext *cx, JSObject *obj)
{
    nyi();
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *obj)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSCheckAccessOp)
JS_SetCheckObjectAccessCallback(JSRuntime *rt, JSCheckAccessOp acb)
{
    nyi();
    return acb;
}

JS_PUBLIC_API(JSBool)
JS_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSPrincipalsTranscoder)
JS_SetPrincipalsTranscoder(JSRuntime *rt, JSPrincipalsTranscoder px)
{
    nyi();
    return px;
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative native, uintN nargs, uintN flags,
               JSObject *parent, const char *name)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(const char *)
JS_GetFunctionName(JSFunction *fun)
{
    nyi();
    return NULL;
}



JS_PUBLIC_API(JSBool)
JS_DefineFunctions(JSContext *cx, JSObject *obj, JSFunctionSpec *fs)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::JSObject *js2obj = (JavaScript::JS2Runtime::JSObject *)obj;
    while (fs->name) {
        JavaScript::JS2Runtime::JSFunction *f = new JavaScript::JS2Runtime::JSFunction(
                                                    js2cx, (JavaScript::JS2Runtime::JSFunction::NativeCode *)(fs->call), 
                                                    JavaScript::JS2Runtime::Object_Type, callSpiderMonkeyNative);
        js2obj->setProperty(js2cx, JavaScript::widenCString(fs->name), NULL, JavaScript::JS2Runtime::JSValue::newFunction(f));
        fs++;
    }
    return JS_TRUE;
}


JS_PUBLIC_API(JSFunction *)
JS_DefineFunction(JSContext *cx, JSObject *obj, const char *name, JSNative call,
                  uintN nargs, uintN attrs)
{
    nyi();
    return NULL;
}

const JavaScript::String ConsoleName = JavaScript::widenCString("<console>");

JS_PUBLIC_API(JSScript *)
JS_CompileScript(JSContext *cx, JSObject *obj,
                 const char *bytes, size_t length,
                 const char *filename, uintN lineno)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::String buffer;
    JavaScript::JS2Runtime::ByteCodeModule *bcm = NULL;

    JavaScript::appendChars(buffer, bytes, length);
    try {
        JavaScript::Parser p(world, a, js2cx->mFlags, buffer, filename ? JavaScript::widenCString(filename) : ConsoleName);
        JavaScript::StmtNode *parsedStatements = p.parseProgram();
        ASSERT(p.lexer.peek(true).hasKind(JavaScript::Token::end));
        js2cx->buildRuntime(parsedStatements);
        bcm = js2cx->genCode(parsedStatements, filename ? JavaScript::widenCString(filename) : ConsoleName);
    } catch (JavaScript::Exception &e) {
        JavaScript::stdOut << '\n' << e.fullMessage();
        return NULL;
    }
    return (JSScript *)bcm;
}

JS_PUBLIC_API(JSScript *)
JS_CompileScriptForPrincipals(JSContext *cx, JSObject *obj,
                              JSPrincipals *principals,
                              const char *bytes, size_t length,
                              const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
                   const jschar *chars, size_t length,
                   const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals,
                                const jschar *chars, size_t length,
                                const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

extern JS_PUBLIC_API(JSBool)
JS_BufferIsCompilableUnit(JSContext *cx, JSObject *obj,
                          const char *bytes, size_t length)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::String buffer;

    JavaScript::appendChars(buffer, bytes, length);
    try {
        JavaScript::Parser p(world, a, js2cx->mFlags, buffer, ConsoleName);
        JavaScript::StmtNode *parsedStatements = p.parseProgram();
        ASSERT(p.lexer.peek(true).hasKind(JavaScript::Token::end));
    } catch (JavaScript::Exception &e) {
        // If we got a syntax error on the end of input, then it's not a compilable unit
        if (!(e.hasKind(JavaScript::Exception::syntaxError) && e.lineNum && e.pos == buffer.size() &&
              e.sourceFile == ConsoleName)) {
            JavaScript::stdOut << '\n' << e.fullMessage();
        }
        return JS_FALSE;
    }
    return JS_TRUE;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFile(JSContext *cx, JSObject *obj, const char *filename)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandle(JSContext *cx, JSObject *obj, const char *filename,
                     FILE *file)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandleForPrincipals(JSContext *cx, JSObject *obj,
                                  const char *filename, FILE *file,
                                  JSPrincipals *principals)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_NewScriptObject(JSContext *cx, JSScript *script)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_DestroyScript(JSContext *cx, JSScript *script)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::ByteCodeModule *bcm = (JavaScript::JS2Runtime::ByteCodeModule *)script;
    delete bcm;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
                   uintN nargs, const char **argnames,
                   const char *bytes, size_t length,
                   const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals, const char *name,
                                uintN nargs, const char **argnames,
                                const char *bytes, size_t length,
                                const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
                     uintN nargs, const char **argnames,
                     const jschar *chars, size_t length,
                     const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                  JSPrincipals *principals, const char *name,
                                  uintN nargs, const char **argnames,
                                  const jschar *chars, size_t length,
                                  const char *filename, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::ByteCodeModule *bcm = (JavaScript::JS2Runtime::ByteCodeModule *)script;

    JavaScript::JS2Runtime::js2val result = js2cx->interpret(bcm, 0, NULL, JavaScript::JS2Runtime::JSValue::newObject(js2cx->getGlobalObject()), NULL, 0);
    *rval = result; //convertJS2ValueToJSValue(cx, result);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScriptPart(JSContext *cx, JSObject *obj, JSScript *script,
                     JSExecPart part, jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
                  const char *bytes, uintN length,
                  const char *filename, uintN lineno,
                  jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
                               JSPrincipals *principals,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno,
                               jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
                    const jschar *chars, uintN length,
                    const char *filename, uintN lineno,
                    jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                 JSPrincipals *principals,
                                 const jschar *chars, uintN length,
                                 const char *filename, uintN lineno,
                                 jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
                jsval *argv, jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
                    jsval *argv, jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
                     jsval *argv, jsval *rval)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBranchCallback)
JS_SetBranchCallback(JSContext *cx, JSBranchCallback cb)
{
    nyi();
    return cb;
}

JS_PUBLIC_API(JSBool)
JS_IsRunning(JSContext *cx)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_IsConstructing(JSContext *cx)
{
    nyi();
    return JS_FALSE;
}

JS_FRIEND_API(JSBool)
JS_IsAssigning(JSContext *cx)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_SetCallReturnValue2(JSContext *cx, jsval v)
{
    nyi();
}

/************************************************************************/

JS_PUBLIC_API(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t length)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return (JSString *)(&js2cx->mWorld.identifiers[s]);
}

JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const jschar *s, size_t length)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
    nyi();
    return NULL;
}

inline char narrow(char16 ch) { return char(ch); }

JS_PUBLIC_API(char *)
JS_GetStringBytes(JSString *str)
{
    JavaScript::String *js2str = (JavaScript::String *)str;
    std::string cstr(js2str->length(), char());
    std::transform(js2str->begin(), js2str->end(), cstr.begin(), narrow);
    char *result = (char *)malloc(cstr.length() + 1);
    strcpy(result, cstr.c_str());
    return result;
}

JS_PUBLIC_API(jschar *)
JS_GetStringChars(JSString *str)
{
    JavaScript::String *js2str = (JavaScript::String *)str;
    return js2str->begin();
}

JS_PUBLIC_API(size_t)
JS_GetStringLength(JSString *str)
{
    JavaScript::String *js2str = (JavaScript::String *)str;
    return js2str->length();
}

JS_PUBLIC_API(intN)
JS_CompareStrings(JSString *str1, JSString *str2)
{
    JavaScript::String *js2str1 = (JavaScript::String *)str1;
    JavaScript::String *js2str2 = (JavaScript::String *)str2;
    return js2str1->compare(*js2str2);
}

JS_PUBLIC_API(JSString *)
JS_NewGrowableString(JSContext *cx, jschar *chars, size_t length)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, JSString *str, size_t start,
                      size_t length)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_ConcatStrings(JSContext *cx, JSString *left, JSString *right)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(const jschar *)
JS_UndependString(JSContext *cx, JSString *str)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_MakeStringImmutable(JSContext *cx, JSString *str)
{
    nyi();
    return JS_FALSE;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...)
{
    nyi();
}

JS_PUBLIC_API(JSBool)
JS_ReportWarning(JSContext *cx, const char *format, ...)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, uintN flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const uintN errorNumber, ...)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, uintN flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const uintN errorNumber, ...)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx)
{
    nyi();
}

JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JSErrorReporter oldEr = (JSErrorReporter)(js2cx->mErrorReporter);
    js2cx->mErrorReporter = er;
    return oldEr;
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_ClearRegExpRoots(JSContext *cx)
{
    nyi();
}

/* TODO: compile, execute, get/set other statics... */

/************************************************************************/

JS_PUBLIC_API(void)
JS_SetLocaleCallbacks(JSContext *cx, JSLocaleCallbacks *callbacks)
{
    nyi();
}

JS_PUBLIC_API(JSLocaleCallbacks *)
JS_GetLocaleCallbacks(JSContext *cx)
{
    nyi();
    return NULL;
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_IsExceptionPending(JSContext *cx)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetPendingException(JSContext *cx, jsval *vp)
{
    nyi();
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, jsval v)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
//  DikDik uses C++ exceptions internally.
//    nyi();
}

JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state)
{
    nyi();
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, jsval v)
{
    nyi();
    return NULL;
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(intN)
JS_GetContextThread(JSContext *cx)
{
//    nyi();            XXX get working
    return 0;
}

JS_PUBLIC_API(intN)
JS_SetContextThread(JSContext *cx)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(intN)
JS_ClearContextThread(JSContext *cx)
{
    nyi();
    return 0;
}
#endif

/************************************************************************/

#ifdef XP_PC
#if defined(XP_OS2)
/*DSR031297 - the OS/2 equiv is dll_InitTerm, but I don't see the need for it*/
#else
#include <windows.h>
/*
 * Initialization routine for the JS DLL...
 */

/*
 * Global Instance handle...
 * In Win32 this is the module handle of the DLL.
 *
 * In Win16 this is the instance handle of the application
 * which loaded the DLL.
 */

#ifdef _WIN32
BOOL WINAPI DllMain (HINSTANCE hDLL, DWORD dwReason, LPVOID lpReserved)
{
    return TRUE;
}

#else  /* !_WIN32 */

int CALLBACK LibMain( HINSTANCE hInst, WORD wDataSeg,
                      WORD cbHeapSize, LPSTR lpszCmdLine )
{
    return TRUE;
}

BOOL CALLBACK __loadds WEP(BOOL fSystemExit)
{
    return TRUE;
}

#endif /* !_WIN32 */
#endif /* XP_OS2 */
#endif /* XP_PC */
 



/*
 *
 *      Needed for jsshell...
 *
 *
 *
 *
 *
 *
 */


typedef uint8 jsbytecode;
typedef int JSAtom, jsatomid;
typedef struct JSScope JSScope;
typedef struct JSScopeProperty JSScopeProperty;
typedef struct JSAtomMap JSAtomMap;
typedef struct jssrcnote jssrcnote;
typedef struct JSTrapHandler JSTrapHandler;

typedef struct JSSymbol {
    JSHashEntry     entry;              /* base class state */
    JSScope         *scope;             /* pointer to owning scope */
    JSSymbol        *next;              /* next in type-specific list */
} JSSymbol;

typedef struct JSScopeOps {
    JSSymbol *      (*lookup)(JSContext *cx, JSScope *scope, jsid id,
			      JSHashNumber hash);
    JSSymbol *      (*add)(JSContext *cx, JSScope *scope, jsid id,
			   JSScopeProperty *sprop);
    JSBool          (*remove)(JSContext *cx, JSScope *scope, jsid id);
    void            (*clear)(JSContext *cx, JSScope *scope);
} JSScopeOps;

typedef struct JSSrcNoteSpec {

} JSSrcNoteSpec;


struct JSTrapHandler { };

extern JS_PUBLIC_API(intN)
JS_HashTableDump(JSHashTable *ht, JSHashEnumerator dump, FILE *fp)
{
    nyi();
    return 0;
}

JS_FRIEND_API(uintN)
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc, uintN loc,
		JSBool lines, FILE *fp)
{
    nyi();
    return 0;
}

JS_FRIEND_API(void)
js_Disassemble(JSContext *cx, JSScript *script, JSBool lines, FILE *fp)
{
    nyi();
}

JS_FRIEND_API(JSAtom *)
js_Atomize(JSContext *cx, const char *bytes, size_t length, uintN flags)
{
    nyi();
    return NULL;
}

JS_FRIEND_DATA(JSScopeOps) js_list_scope_ops;
JS_FRIEND_DATA(JSObjectOps) js_ObjectOps;

JS_PUBLIC_API(char *)JS_smprintf(const char *fmt, ...)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(char *) JS_sprintf_append(char *last, const char *fmt, ...)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void) JS_Assert(const char *s, const char *file, JSIntn ln)
{
#ifdef XP_MAC
    dprintf("Assertion failure: %s, at %s:%d\n", s, file, ln);
#else
    fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
#endif
#if defined(WIN32) || defined(XP_OS2)
    DebugBreak();
#endif
#ifndef XP_MAC
    abort();
#endif
}

JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp)
{
    nyi();
    return JS_FALSE;
}

JS_FRIEND_API(JSAtom *)
js_GetAtom(JSContext *cx, JSAtomMap *map, jsatomid i)
{
    nyi();
    return NULL;
}

JS_FRIEND_DATA(JSSrcNoteSpec) js_SrcNoteSpec[] = { 0 };

JS_FRIEND_DATA(JSClass) js_FunctionClass = { 0 };
JS_FRIEND_DATA(JSClass) js_ScriptClass = { 0 };

JS_FRIEND_API(uintN)
js_SrcNoteLength(jssrcnote *sn)
{
    nyi();
    return 0;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
             JSTrapHandler *handlerp, void **closurep)
{
}

JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
           JSTrapHandler handler, void *closure)
{
    nyi();
    return JS_FALSE;
}

JS_FRIEND_API(JSScopeProperty **)
js_SearchScope(JSScope *scope, jsid id, JSBool adding)
{
    nyi();
    return NULL;
}


JS_PUBLIC_API(JSScript *)
JS_GetFrameScript(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameFunctionObject(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetFrameAnnotation(JSContext *cx, JSStackFrame *fp, void *annotation)
{
    nyi();
}

JS_PUBLIC_API(void *)
JS_GetFrameAnnotation(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameThis(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}


JS_PUBLIC_API(JSStackFrame *)
JS_FrameIterator(JSContext *cx, JSStackFrame **iteratorp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_GetFunctionScript(JSContext *cx, JSFunction *fun)
{
    nyi();
    return NULL;
}

extern JS_PUBLIC_API(JSPrincipals *)
JS_GetScriptPrincipals(JSContext *cx, JSScript *script)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(uint32)
JS_XDRMemDataLeft(JSXDRState *xdr)
{
    return 0;
}

JS_PUBLIC_API(void *)
JS_XDRMemGetData(JSXDRState *xdr, uint32 *lp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_XDRMemResetData(JSXDRState *xdr)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_XDRMemSetData(JSXDRState *xdr, void *data, uint32 len)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_PutPropertyDescArray(JSContext *cx, JSPropertyDescArray *pda)
{
    nyi();
}

JS_PUBLIC_API(JSBool)
JS_GetPropertyDescArray(JSContext *cx, JSObject *obj, JSPropertyDescArray *pda)
{
    nyi();
    return false;
}

JS_PUBLIC_API(JSObject *)
JS_GetFrameCallObject(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(const char *)
JS_GetScriptFilename(JSContext *cx, JSScript *script)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(jsbytecode *)
JS_GetFramePC(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_IsNativeFrame(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return false;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateInStackFrame(JSContext *cx, JSStackFrame *fp,
			const char *bytes, uintN length,
			const char *filename, uintN lineno,
			jsval *rval)
{
    nyi();
    return false;
}

JS_PUBLIC_API(JSBool)
JS_SetDebuggerHandler(JSRuntime *rt, JSTrapHandler handler, void *closure)
{
//    nyi();    - not supporting a debugger
    return false;
}

JS_PUBLIC_API(JSIntn) JS_CeilingLog2(JSUint32 n)
{
    JSIntn log2 = 0;

    if (n & (n-1))
	log2++;
    if (n >> 16)
	log2 += 16, n >>= 16;
    if (n >> 8)
	log2 += 8, n >>= 8;
    if (n >> 4)
	log2 += 4, n >>= 4;
    if (n >> 2)
	log2 += 2, n >>= 2;
    if (n >> 1)
	log2++;
    return log2;
}


/*
JS_PUBLIC_API(JSDHashEntryHdr *)
JS_DHashTableOperate(JSDHashTable *table, const void *key, JSDHashOperator op)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(uint32)
JS_DHashTableEnumerate(JSDHashTable *table, JSDHashEnumerator etor, void *arg)
{
    nyi();
    return 0;
}

JS_PUBLIC_API(JSDHashTable *)
JS_NewDHashTable(JSDHashTableOps *ops, void *data, uint32 entrySize,
                 uint32 capacity)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(JSDHashTableOps *)
JS_DHashGetStubOps(void)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_DHashTableDestroy(JSDHashTable *table)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_DHashFinalizeStub(JSDHashTable *table)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_DHashClearEntryStub(JSDHashTable *table, JSDHashEntryHdr *entry)
{
    nyi();
}

JS_PUBLIC_API(void)
JS_DHashMoveEntryStub(JSDHashTable *table,
                      const JSDHashEntryHdr *from,
                      JSDHashEntryHdr *to)
{
    nyi();
}

JS_PUBLIC_API(const void *)
JS_DHashGetKeyStub(JSDHashTable *table, JSDHashEntryHdr *entry)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_DHashFreeTable(JSDHashTable *table, void *ptr)
{
    nyi();
}

JS_PUBLIC_API(void *)
JS_DHashAllocTable(JSDHashTable *table, uint32 nbytes)
{
    nyi();
    return NULL;
}

JS_PUBLIC_API(void)
JS_DHashTableFinish(JSDHashTable *table)
{
    nyi();
}

JS_PUBLIC_API(JSBool)
JS_DHashTableInit(JSDHashTable *table, JSDHashTableOps *ops, void *data,
                  uint32 entrySize, uint32 capacity)
{
    nyi();
    return false;
}
*/


JS_FRIEND_API(JSBool)
js_Invoke(JSContext *cx, uintN argc, uintN flags)
{
    nyi();
    return false;
}

JS_PUBLIC_API(void) JS_smprintf_free(char *mem)
{
    nyi();
}

JS_PUBLIC_API(JSFunction *)
JS_GetFrameFunction(JSContext *cx, JSStackFrame *fp)
{
    nyi();
    return NULL;
}

}   // extern "C"

