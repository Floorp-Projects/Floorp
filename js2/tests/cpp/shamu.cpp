
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

#include "shamu.h"

jsval convertJS2ValueToJSValue(JSContext *cx, JavaScript::JS2Runtime::js2val v)
{
    jsval result = JSVAL_VOID;
    switch (JavaScript::JS2Runtime::JSValue::tag(v)) {
    case JS2VAL_DOUBLE: 
        JS_NewDoubleValue(cx, JavaScript::JS2Runtime::JSValue::f64(v), &result);
        break;
    case JS2VAL_OBJECT: 
        result = OBJECT_TO_JSVAL(JavaScript::JS2Runtime::JSValue::object(v));
        break;
    case JS2VAL_STRING:
        result = STRING_TO_JSVAL(JavaScript::JS2Runtime::JSValue::string(v));
        break;
    }
    return result;
}

JavaScript::JS2Runtime::js2val convertJSValueToJS2Value(JSContext *cx, jsval v)
{
    if (JSVAL_IS_OBJECT(v))
        return JavaScript::JS2Runtime::JSValue::newObject((JavaScript::JS2Runtime::JSObject *)(JSVAL_TO_OBJECT(v)));
    else
    if (JSVAL_IS_DOUBLE(v))
        return JavaScript::JS2Runtime::JSValue::newNumber(*JSVAL_TO_DOUBLE(v));
    else
    if (JSVAL_IS_STRING(v))
        return JavaScript::JS2Runtime::JSValue::newString((JavaScript::String *)JSVAL_TO_STRING(v));

    return JavaScript::JS2Runtime::kUndefinedValue;
}

JavaScript::JS2Runtime::js2val callSpiderMonkeyNative(JavaScript::JS2Runtime::JSFunction::NativeCode *js2target, 
                        JavaScript::JS2Runtime::Context *js2cx, 
                        const JavaScript::JS2Runtime::js2val thisValue, 
                        JavaScript::JS2Runtime::js2val argv[], uint32 argc)
{
    JSNative target = (JSNative)js2target;
    jsval result;
    jsval *args = new jsval[argc];
    for (uint32 i = 0; i < argc; i++) {
        args[i] = convertJS2ValueToJSValue((JSContext *)js2cx, argv[i]);
    }

    target( (JSContext *)js2cx, (JSObject *)(JavaScript::JS2Runtime::JSValue::object(thisValue)), argc, args, &result);

    return convertJSValueToJS2Value((JSContext *)js2cx, result);
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

JS_PUBLIC_API(jsval)
JS_GetNaNValue(JSContext *cx)
{
    return DOUBLE_TO_JSVAL(cx->runtime->jsNaN);
}

JS_PUBLIC_API(jsval)
JS_GetNegativeInfinityValue(JSContext *cx)
{
    return DOUBLE_TO_JSVAL(cx->runtime->jsNegativeInfinity);
}

JS_PUBLIC_API(jsval)
JS_GetPositiveInfinityValue(JSContext *cx)
{
    return DOUBLE_TO_JSVAL(cx->runtime->jsPositiveInfinity);
}

JS_PUBLIC_API(jsval)
JS_GetEmptyStringValue(JSContext *cx)
{
    return STRING_TO_JSVAL(cx->runtime->emptyString);
}

#ifdef JS_ARGUMENT_FORMATTER_DEFINED
static JSBool
TryArgumentFormatter(JSContext *cx, const char **formatp, JSBool fromJS,
                     jsval **vpp, va_list *app)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArguments(JSContext *cx, uintN argc, jsval *argv, const char *format,
                    ...)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ConvertArgumentsVA(JSContext *cx, uintN argc, jsval *argv,
                      const char *format, va_list ap)
{
    return JS_FALSE;
}

JS_PUBLIC_API(jsval *)
JS_PushArguments(JSContext *cx, void **markp, const char *format, ...)
{
    return NULL;
}

JS_PUBLIC_API(jsval *)
JS_PushArgumentsVA(JSContext *cx, void **markp, const char *format, va_list ap)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_PopArguments(JSContext *cx, void *mark)
{
}

JS_PUBLIC_API(JSBool)
JS_AddArgumentFormatter(JSContext *cx, const char *format,
                        JSArgumentFormatter formatter)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_RemoveArgumentFormatter(JSContext *cx, const char *format)
{
}

#endif


JS_PUBLIC_API(JSBool)
JS_ConvertValue(JSContext *cx, jsval v, JSType type, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToObject(JSContext *cx, jsval v, JSObject **objp)
{
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToFunction(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_ValueToConstructor(JSContext *cx, jsval v)
{
    CHECK_REQUEST(cx);
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_ValueToString(JSContext *cx, jsval v)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::js2val js2value = convertJSValueToJS2Value(cx, v);
    
    return (JSString *)JavaScript::JS2Runtime::JSValue::string(JavaScript::JS2Runtime::JSValue::toString(js2cx, js2value));
}

JS_PUBLIC_API(JSBool)
JS_ValueToNumber(JSContext *cx, jsval v, jsdouble *dp)
{
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToECMAUint32(JSContext *cx, jsval v, uint32 *ip)
{
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToInt32(JSContext *cx, jsval v, int32 *ip)
{
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToUint16(JSContext *cx, jsval v, uint16 *ip)
{
    CHECK_REQUEST(cx);
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ValueToBoolean(JSContext *cx, jsval v, JSBool *bp)
{
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
}

#ifdef JS_THREADSAFE

JS_PUBLIC_API(void)
JS_BeginRequest(JSContext *cx)
{
    JSRuntime *rt;

    JS_ASSERT(cx->thread);
    if (!cx->requestDepth) {
        /* Wait until the GC is finished. */
        rt = cx->runtime;
        JS_LOCK_GC(rt);

        /* NB: we use cx->thread here, not js_CurrentThreadId(). */
        if (rt->gcThread != cx->thread) {
            while (rt->gcLevel > 0)
                JS_AWAIT_GC_DONE(rt);
        }

        /* Indicate that a request is running. */
        rt->requestCount++;
        cx->requestDepth = 1;
        JS_UNLOCK_GC(rt);
        return;
    }
    cx->requestDepth++;
}

JS_PUBLIC_API(void)
JS_EndRequest(JSContext *cx)
{
    JSRuntime *rt;
    JSScope *scope, **todop;
    uintN nshares;

    CHECK_REQUEST(cx);
    JS_ASSERT(cx->requestDepth > 0);
    if (cx->requestDepth == 1) {
        /* Lock before clearing to interlock with ClaimScope, in jslock.c. */
        rt = cx->runtime;
        JS_LOCK_GC(rt);
        cx->requestDepth = 0;

        /* See whether cx has any single-threaded scopes to start sharing. */
        todop = &rt->scopeSharingTodo;
        nshares = 0;
        while ((scope = *todop) != NO_SCOPE_SHARING_TODO) {
            if (scope->ownercx != cx) {
                todop = &scope->u.link;
                continue;
            }
            *todop = scope->u.link;
            scope->u.link = NULL;       /* null u.link for sanity ASAP */

            /*
             * If js_DropObjectMap returns null, we held the last ref to scope.
             * The waiting thread(s) must have been killed, after which the GC
             * collected the object that held this scope.  Unlikely, because it
             * requires that the GC ran (e.g., from a branch callback) during
             * this request, but possible.
             */
            if (js_DropObjectMap(cx, &scope->map, NULL)) {
                js_InitLock(&scope->lock);
                scope->u.count = 0;                 /* NULL may not pun as 0 */
                js_FinishSharingScope(rt, scope);   /* set ownercx = NULL */
                nshares++;
            }
        }
        if (nshares)
            JS_NOTIFY_ALL_CONDVAR(rt->scopeSharingDone);

        /* Give the GC a chance to run if this was the last request running. */
        JS_ASSERT(rt->requestCount > 0);
        rt->requestCount--;
        if (rt->requestCount == 0)
            JS_NOTIFY_REQUEST_DONE(rt);

        JS_UNLOCK_GC(rt);
        return;
    }

    cx->requestDepth--;
}

/* Yield to pending GC operations, regardless of request depth */
JS_PUBLIC_API(void)
JS_YieldRequest(JSContext *cx)
{
    JSRuntime *rt;

    JS_ASSERT(cx->thread);
    CHECK_REQUEST(cx);

    rt = cx->runtime;
    JS_LOCK_GC(rt);
    JS_ASSERT(rt->requestCount > 0);
    rt->requestCount--;
    if (rt->requestCount == 0)
        JS_NOTIFY_REQUEST_DONE(rt);
    JS_UNLOCK_GC(rt);
    /* XXXbe give the GC or another request calling it a chance to run here?
             Assumes FIFO scheduling */
    JS_LOCK_GC(rt);
    rt->requestCount++;
    JS_UNLOCK_GC(rt);
}

JS_PUBLIC_API(jsrefcount)
JS_SuspendRequest(JSContext *cx)
{
    jsrefcount saveDepth = cx->requestDepth;

    while (cx->requestDepth)
        JS_EndRequest(cx);
    return saveDepth;
}

JS_PUBLIC_API(void)
JS_ResumeRequest(JSContext *cx, jsrefcount saveDepth)
{
    JS_ASSERT(!cx->requestDepth);
    while (--saveDepth >= 0)
        JS_BeginRequest(cx);
}

#endif /* JS_THREADSAFE */

JS_PUBLIC_API(void)
JS_Lock(JSRuntime *rt)
{
}

JS_PUBLIC_API(void)
JS_Unlock(JSRuntime *rt)
{
}

JS_PUBLIC_API(JSContext *)
JS_NewContext(JSRuntime *rt, size_t stackChunkSize)
{
    return (JSContext *)(new JavaScript::JS2Runtime::Context(&globalObject, *((JavaScript::World *)rt), a, JavaScript::Pragma::js2));
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
}

JS_PUBLIC_API(JSRuntime *)
JS_GetRuntime(JSContext *cx)
{
    return NULL;
}

JS_PUBLIC_API(JSContext *)
JS_ContextIterator(JSRuntime *rt, JSContext **iterp)
{
    return NULL;
}

JS_PUBLIC_API(JSVersion)
JS_GetVersion(JSContext *cx)
{
    return JSVERSION_UNKNOWN;
}

JS_PUBLIC_API(JSVersion)
JS_SetVersion(JSContext *cx, JSVersion version)
{
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
    return 0;
}

JS_PUBLIC_API(uint32)
JS_SetOptions(JSContext *cx, uint32 options)
{
    return 0;
}

JS_PUBLIC_API(uint32)
JS_ToggleOptions(JSContext *cx, uint32 options)
{
    return 0;
}

JS_PUBLIC_API(const char *)
JS_GetImplementationVersion(void)
{
    return "JavaScript DikDik Shim";
}


JS_PUBLIC_API(JSObject *)
JS_GetGlobalObject(JSContext *cx)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetGlobalObject(JSContext *cx, JSObject *obj)
{
}

static JSObject *
InitFunctionAndObjectClasses(JSContext *cx, JSObject *obj)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_InitStandardClasses(JSContext *cx, JSObject *obj)
{
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ResolveStandardClass(JSContext *cx, JSObject *obj, jsval id,
                        JSBool *resolved)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EnumerateStandardClasses(JSContext *cx, JSObject *obj)
{
    return JS_FALSE;
}


JS_PUBLIC_API(JSObject *)
JS_GetScopeChain(JSContext *cx)
{
    return NULL;
}

JS_PUBLIC_API(void *)
JS_malloc(JSContext *cx, size_t nbytes)
{
    return NULL;
}

JS_PUBLIC_API(void *)
JS_realloc(JSContext *cx, void *p, size_t nbytes)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_free(JSContext *cx, void *p)
{
}

JS_PUBLIC_API(char *)
JS_strdup(JSContext *cx, const char *s)
{
    return NULL;
}

JS_PUBLIC_API(jsdouble *)
JS_NewDouble(JSContext *cx, jsdouble d)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    return (jsdouble *)js2cx->mArena.allocate(sizeof(jsdouble));
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
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRootRT(JSRuntime *rt, void *rp, const char *name)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_RemoveRoot(JSContext *cx, void *rp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_RemoveRootRT(JSRuntime *rt, void *rp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AddNamedRoot(JSContext *cx, void *rp, const char *name)
{
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
    return JSDHashOperator(0);
}

JS_PUBLIC_API(uint32)
JS_MapGCRoots(JSRuntime *rt, JSGCRootMapFun map, void *data)
{
    return 0;
}

JS_PUBLIC_API(JSBool)
JS_LockGCThing(JSContext *cx, void *thing)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_UnlockGCThing(JSContext *cx, void *thing)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_MarkGCThing(JSContext *cx, void *thing, const char *name, void *arg)
{
}

JS_PUBLIC_API(void)
JS_GC(JSContext *cx)
{
}

JS_PUBLIC_API(void)
JS_MaybeGC(JSContext *cx)
{
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallback(JSContext *cx, JSGCCallback cb)
{
    return cb;
}

JS_PUBLIC_API(JSGCCallback)
JS_SetGCCallbackRT(JSRuntime *rt, JSGCCallback cb)
{
    return cb;
}

JS_PUBLIC_API(JSBool)
JS_IsAboutToBeFinalized(JSContext *cx, void *thing)
{
    return JS_FALSE;
}

JS_PUBLIC_API(intN)
JS_AddExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    return 0;
}

JS_PUBLIC_API(intN)
JS_RemoveExternalStringFinalizer(JSStringFinalizeOp finalizer)
{
    return 0;
}

JS_PUBLIC_API(JSString *)
JS_NewExternalString(JSContext *cx, jschar *chars, size_t length, intN type)
{
    return NULL;
}

JS_PUBLIC_API(intN)
JS_GetExternalStringGCType(JSRuntime *rt, JSString *str)
{
    return 0;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_DestroyIdArray(JSContext *cx, JSIdArray *ida)
{
}

JS_PUBLIC_API(JSBool)
JS_ValueToId(JSContext *cx, jsval v, jsid *idp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_IdToValue(JSContext *cx, jsid id, jsval *vp)
{
    return JS_FALSE;
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
    return JS_FALSE;
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
    return NULL;
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSContext *cx, JSObject *obj)
{
    return (JSClass *)
        JSVAL_TO_PRIVATE(GC_AWARE_GET_SLOT(cx, obj, JSSLOT_CLASS));
}
#else
JS_PUBLIC_API(JSClass *)
JS_GetClass(JSObject *obj)
{
    return NULL;
}
#endif

JS_PUBLIC_API(JSBool)
JS_InstanceOf(JSContext *cx, JSObject *obj, JSClass *clasp, jsval *argv)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void *)
JS_GetPrivate(JSContext *cx, JSObject *obj)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrivate(JSContext *cx, JSObject *obj, void *data)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void *)
JS_GetInstancePrivate(JSContext *cx, JSObject *obj, JSClass *clasp,
                      jsval *argv)
{
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_GetPrototype(JSContext *cx, JSObject *obj)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetPrototype(JSContext *cx, JSObject *obj, JSObject *proto)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSObject *)
JS_GetParent(JSContext *cx, JSObject *obj)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_SetParent(JSContext *cx, JSObject *obj, JSObject *parent)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSObject *)
JS_GetConstructor(JSContext *cx, JSObject *proto)
{
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
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_ConstructObjectWithArguments(JSContext *cx, JSClass *clasp, JSObject *proto,
                                JSObject *parent, uintN argc, jsval *argv)
{
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
    js2obj->setProperty(js2cx, JavaScript::widenCString(name), NULL, convertJSValueToJS2Value(cx, value));
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_DefinePropertyWithTinyId(JSContext *cx, JSObject *obj, const char *name,
                            int8 tinyid, jsval value,
                            JSPropertyOp getter, JSPropertyOp setter,
                            uintN attrs)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AliasProperty(JSContext *cx, JSObject *obj, const char *name,
                 const char *alias)
{
    return JS_FALSE;
}


JS_PUBLIC_API(JSBool)
JS_GetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN *attrsp, JSBool *foundp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetPropertyAttributes(JSContext *cx, JSObject *obj, const char *name,
                         uintN attrs, JSBool *foundp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_LookupProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetProperty(JSContext *cx, JSObject *obj, const char *name, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty(JSContext *cx, JSObject *obj, const char *name)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteProperty2(JSContext *cx, JSObject *obj, const char *name,
                   jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen, jsval value,
                    JSPropertyOp getter, JSPropertyOp setter,
                    uintN attrs)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN *attrsp, JSBool *foundp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetUCPropertyAttributes(JSContext *cx, JSObject *obj,
                           const jschar *name, size_t namelen,
                           uintN attrs, JSBool *foundp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineUCPropertyWithTinyId(JSContext *cx, JSObject *obj,
                              const jschar *name, size_t namelen,
                              int8 tinyid, jsval value,
                              JSPropertyOp getter, JSPropertyOp setter,
                              uintN attrs)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_LookupUCProperty(JSContext *cx, JSObject *obj,
                    const jschar *name, size_t namelen,
                    jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetUCProperty(JSContext *cx, JSObject *obj,
                 const jschar *name, size_t namelen,
                 jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteUCProperty2(JSContext *cx, JSObject *obj,
                     const jschar *name, size_t namelen,
                     jsval *rval)
{
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
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetArrayLength(JSContext *cx, JSObject *obj, jsuint length)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_HasArrayLength(JSContext *cx, JSObject *obj, jsuint *lengthp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DefineElement(JSContext *cx, JSObject *obj, jsint index, jsval value,
                 JSPropertyOp getter, JSPropertyOp setter, uintN attrs)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_AliasElement(JSContext *cx, JSObject *obj, const char *name, jsint alias)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_LookupElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetElement(JSContext *cx, JSObject *obj, jsint index, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement(JSContext *cx, JSObject *obj, jsint index)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_DeleteElement2(JSContext *cx, JSObject *obj, jsint index, jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_ClearScope(JSContext *cx, JSObject *obj)
{
}

JS_PUBLIC_API(JSIdArray *)
JS_Enumerate(JSContext *cx, JSObject *obj)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSCheckAccessOp)
JS_SetCheckObjectAccessCallback(JSRuntime *rt, JSCheckAccessOp acb)
{
    return acb;
}

JS_PUBLIC_API(JSBool)
JS_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSPrincipalsTranscoder)
JS_SetPrincipalsTranscoder(JSRuntime *rt, JSPrincipalsTranscoder px)
{
    return px;
}

JS_PUBLIC_API(JSFunction *)
JS_NewFunction(JSContext *cx, JSNative native, uintN nargs, uintN flags,
               JSObject *parent, const char *name)
{
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_CloneFunctionObject(JSContext *cx, JSObject *funobj, JSObject *parent)
{
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_GetFunctionObject(JSFunction *fun)
{
    return NULL;
}

JS_PUBLIC_API(const char *)
JS_GetFunctionName(JSFunction *fun)
{
    return NULL;
}


/*
        typedef JSValue (NativeCode)(Context *cx, const JSValue &thisValue, JSValue argv[], uint32 argc);
        typedef JSBool
        (* JS_DLL_CALLBACK JSNative)(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
*/


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
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScript(JSContext *cx, JSObject *obj,
                   const jschar *chars, size_t length,
                   const char *filename, uintN lineno)
{
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals,
                                const jschar *chars, size_t length,
                                const char *filename, uintN lineno)
{
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
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandle(JSContext *cx, JSObject *obj, const char *filename,
                     FILE *file)
{
    return NULL;
}

JS_PUBLIC_API(JSScript *)
JS_CompileFileHandleForPrincipals(JSContext *cx, JSObject *obj,
                                  const char *filename, FILE *file,
                                  JSPrincipals *principals)
{
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_NewScriptObject(JSContext *cx, JSScript *script)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_DestroyScript(JSContext *cx, JSScript *script)
{
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunction(JSContext *cx, JSObject *obj, const char *name,
                   uintN nargs, const char **argnames,
                   const char *bytes, size_t length,
                   const char *filename, uintN lineno)
{
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                JSPrincipals *principals, const char *name,
                                uintN nargs, const char **argnames,
                                const char *bytes, size_t length,
                                const char *filename, uintN lineno)
{
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunction(JSContext *cx, JSObject *obj, const char *name,
                     uintN nargs, const char **argnames,
                     const jschar *chars, size_t length,
                     const char *filename, uintN lineno)
{
    return NULL;
}

JS_PUBLIC_API(JSFunction *)
JS_CompileUCFunctionForPrincipals(JSContext *cx, JSObject *obj,
                                  JSPrincipals *principals, const char *name,
                                  uintN nargs, const char **argnames,
                                  const jschar *chars, size_t length,
                                  const char *filename, uintN lineno)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunction(JSContext *cx, JSFunction *fun, uintN indent)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_DecompileFunctionBody(JSContext *cx, JSFunction *fun, uintN indent)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScript(JSContext *cx, JSObject *obj, JSScript *script, jsval *rval)
{
    JavaScript::JS2Runtime::Context *js2cx = (JavaScript::JS2Runtime::Context *)cx;
    JavaScript::JS2Runtime::ByteCodeModule *bcm = (JavaScript::JS2Runtime::ByteCodeModule *)script;

    JavaScript::JS2Runtime::js2val result = js2cx->interpret(bcm, 0, NULL, JavaScript::JS2Runtime::JSValue::newObject(js2cx->getGlobalObject()), NULL, 0);
    *rval = convertJS2ValueToJSValue(cx, result);
    return JS_TRUE;
}

JS_PUBLIC_API(JSBool)
JS_ExecuteScriptPart(JSContext *cx, JSObject *obj, JSScript *script,
                     JSExecPart part, jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScript(JSContext *cx, JSObject *obj,
                  const char *bytes, uintN length,
                  const char *filename, uintN lineno,
                  jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateScriptForPrincipals(JSContext *cx, JSObject *obj,
                               JSPrincipals *principals,
                               const char *bytes, uintN length,
                               const char *filename, uintN lineno,
                               jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScript(JSContext *cx, JSObject *obj,
                    const jschar *chars, uintN length,
                    const char *filename, uintN lineno,
                    jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_EvaluateUCScriptForPrincipals(JSContext *cx, JSObject *obj,
                                 JSPrincipals *principals,
                                 const jschar *chars, uintN length,
                                 const char *filename, uintN lineno,
                                 jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunction(JSContext *cx, JSObject *obj, JSFunction *fun, uintN argc,
                jsval *argv, jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionName(JSContext *cx, JSObject *obj, const char *name, uintN argc,
                    jsval *argv, jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_CallFunctionValue(JSContext *cx, JSObject *obj, jsval fval, uintN argc,
                     jsval *argv, jsval *rval)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBranchCallback)
JS_SetBranchCallback(JSContext *cx, JSBranchCallback cb)
{
    return cb;
}

JS_PUBLIC_API(JSBool)
JS_IsRunning(JSContext *cx)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_IsConstructing(JSContext *cx)
{
    return JS_FALSE;
}

JS_FRIEND_API(JSBool)
JS_IsAssigning(JSContext *cx)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_SetCallReturnValue2(JSContext *cx, jsval v)
{
}

/************************************************************************/

JS_PUBLIC_API(JSString *)
JS_NewString(JSContext *cx, char *bytes, size_t length)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyN(JSContext *cx, const char *s, size_t n)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewStringCopyZ(JSContext *cx, const char *s)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_InternString(JSContext *cx, const char *s)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewUCString(JSContext *cx, jschar *chars, size_t length)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyN(JSContext *cx, const jschar *s, size_t n)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewUCStringCopyZ(JSContext *cx, const jschar *s)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_InternUCStringN(JSContext *cx, const jschar *s, size_t length)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_InternUCString(JSContext *cx, const jschar *s)
{
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
    return 0;
}

JS_PUBLIC_API(intN)
JS_CompareStrings(JSString *str1, JSString *str2)
{
    return 0;
}

JS_PUBLIC_API(JSString *)
JS_NewGrowableString(JSContext *cx, jschar *chars, size_t length)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_NewDependentString(JSContext *cx, JSString *str, size_t start,
                      size_t length)
{
    return NULL;
}

JS_PUBLIC_API(JSString *)
JS_ConcatStrings(JSContext *cx, JSString *left, JSString *right)
{
    return NULL;
}

JS_PUBLIC_API(const jschar *)
JS_UndependString(JSContext *cx, JSString *str)
{
    return NULL;
}

JS_PUBLIC_API(JSBool)
JS_MakeStringImmutable(JSContext *cx, JSString *str)
{
    return JS_FALSE;
}

/************************************************************************/

JS_PUBLIC_API(void)
JS_ReportError(JSContext *cx, const char *format, ...)
{
}

JS_PUBLIC_API(void)
JS_ReportErrorNumber(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...)
{
}

JS_PUBLIC_API(void)
JS_ReportErrorNumberUC(JSContext *cx, JSErrorCallback errorCallback,
                     void *userRef, const uintN errorNumber, ...)
{
}

JS_PUBLIC_API(JSBool)
JS_ReportWarning(JSContext *cx, const char *format, ...)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumber(JSContext *cx, uintN flags,
                             JSErrorCallback errorCallback, void *userRef,
                             const uintN errorNumber, ...)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_ReportErrorFlagsAndNumberUC(JSContext *cx, uintN flags,
                               JSErrorCallback errorCallback, void *userRef,
                               const uintN errorNumber, ...)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_ReportOutOfMemory(JSContext *cx)
{
}

JS_PUBLIC_API(JSErrorReporter)
JS_SetErrorReporter(JSContext *cx, JSErrorReporter er)
{
    return er;
}

/************************************************************************/

/*
 * Regular Expressions.
 */
JS_PUBLIC_API(JSObject *)
JS_NewRegExpObject(JSContext *cx, char *bytes, size_t length, uintN flags)
{
    return NULL;
}

JS_PUBLIC_API(JSObject *)
JS_NewUCRegExpObject(JSContext *cx, jschar *chars, size_t length, uintN flags)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_SetRegExpInput(JSContext *cx, JSString *input, JSBool multiline)
{
}

JS_PUBLIC_API(void)
JS_ClearRegExpStatics(JSContext *cx)
{
}

JS_PUBLIC_API(void)
JS_ClearRegExpRoots(JSContext *cx)
{
}

/* TODO: compile, execute, get/set other statics... */

/************************************************************************/

JS_PUBLIC_API(void)
JS_SetLocaleCallbacks(JSContext *cx, JSLocaleCallbacks *callbacks)
{
}

JS_PUBLIC_API(JSLocaleCallbacks *)
JS_GetLocaleCallbacks(JSContext *cx)
{
    return NULL;
}

/************************************************************************/

JS_PUBLIC_API(JSBool)
JS_IsExceptionPending(JSContext *cx)
{
    return JS_FALSE;
}

JS_PUBLIC_API(JSBool)
JS_GetPendingException(JSContext *cx, jsval *vp)
{
    return JS_FALSE;
}

JS_PUBLIC_API(void)
JS_SetPendingException(JSContext *cx, jsval v)
{
}

JS_PUBLIC_API(void)
JS_ClearPendingException(JSContext *cx)
{
}

JS_PUBLIC_API(JSExceptionState *)
JS_SaveExceptionState(JSContext *cx)
{
    return NULL;
}

JS_PUBLIC_API(void)
JS_RestoreExceptionState(JSContext *cx, JSExceptionState *state)
{
}

JS_PUBLIC_API(void)
JS_DropExceptionState(JSContext *cx, JSExceptionState *state)
{
}

JS_PUBLIC_API(JSErrorReport *)
JS_ErrorFromException(JSContext *cx, jsval v)
{
    return NULL;
}

#ifdef JS_THREADSAFE
JS_PUBLIC_API(intN)
JS_GetContextThread(JSContext *cx)
{
    return cx->thread;
}

JS_PUBLIC_API(intN)
JS_SetContextThread(JSContext *cx)
{
    intN old = cx->thread;
    cx->thread = js_CurrentThreadId();
    return old;
}

JS_PUBLIC_API(intN)
JS_ClearContextThread(JSContext *cx)
{
    intN old = cx->thread;
    cx->thread = 0;
    return old;
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
    return 0;
}

JS_FRIEND_API(uintN)
js_Disassemble1(JSContext *cx, JSScript *script, jsbytecode *pc, uintN loc,
		JSBool lines, FILE *fp)
{
    return 0;
}

JS_FRIEND_API(void)
js_Disassemble(JSContext *cx, JSScript *script, JSBool lines, FILE *fp)
{
}

JS_FRIEND_API(JSAtom *)
js_Atomize(JSContext *cx, const char *bytes, size_t length, uintN flags)
{
    return NULL;
}

JS_FRIEND_DATA(JSScopeOps) js_list_scope_ops;
JS_FRIEND_DATA(JSObjectOps) js_ObjectOps;

JS_PUBLIC_API(char *)JS_smprintf(const char *fmt, ...)
{
    return NULL;
}

JS_PUBLIC_API(char *) JS_sprintf_append(char *last, const char *fmt, ...)
{
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
    return JS_FALSE;
}

JS_FRIEND_API(JSAtom *)
js_GetAtom(JSContext *cx, JSAtomMap *map, jsatomid i)
{
    return NULL;
}

JS_FRIEND_DATA(JSSrcNoteSpec) js_SrcNoteSpec[] = { 0 };

JS_FRIEND_DATA(JSClass) js_FunctionClass = { 0 };
JS_FRIEND_DATA(JSClass) js_ScriptClass = { 0 };

JS_FRIEND_API(uintN)
js_SrcNoteLength(jssrcnote *sn)
{
    return 0;
}

JS_FRIEND_API(ptrdiff_t)
js_GetSrcNoteOffset(jssrcnote *sn, uintN which)
{
    return 0;
}

JS_PUBLIC_API(jsbytecode *)
JS_LineNumberToPC(JSContext *cx, JSScript *script, uintN lineno)
{
    return NULL;
}

JS_PUBLIC_API(uintN)
JS_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    return 0;
}

JS_PUBLIC_API(void)
JS_ClearTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
             JSTrapHandler *handlerp, void **closurep)
{
}

JS_FRIEND_API(void)
js_ForceGC(JSContext *cx)
{
}

JS_PUBLIC_API(JSBool)
JS_SetTrap(JSContext *cx, JSScript *script, jsbytecode *pc,
           JSTrapHandler handler, void *closure)
{
    return JS_FALSE;
}

JS_FRIEND_API(JSScopeProperty **)
js_SearchScope(JSScope *scope, jsid id, JSBool adding)
{
    return NULL;
}

}   // extern "C"

