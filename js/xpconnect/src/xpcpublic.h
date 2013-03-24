/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcpublic_h
#define xpcpublic_h

#include "jsapi.h"
#include "js/MemoryMetrics.h"
#include "jsclass.h"
#include "jsfriendapi.h"
#include "jspubtd.h"
#include "jsproxy.h"
#include "js/HeapAPI.h"
#include "js/GCAPI.h"

#include "nsISupports.h"
#include "nsIPrincipal.h"
#include "nsWrapperCache.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "mozilla/dom/DOMJSClass.h"
#include "nsMathUtils.h"
#include "nsStringBuffer.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsIPrincipal;
class nsIXPConnectWrappedJS;
class nsScriptNameSpaceManager;

#ifndef BAD_TLS_INDEX
#define BAD_TLS_INDEX ((uint32_t) -1)
#endif

namespace xpc {
JSObject *
TransplantObject(JSContext *cx, JSObject *origobj, JSObject *target);

JSObject *
TransplantObjectWithWrapper(JSContext *cx,
                            JSObject *origobj, JSObject *origwrapper,
                            JSObject *targetobj, JSObject *targetwrapper);

// Return a raw XBL scope object corresponding to contentScope, which must
// be an object whose global is a DOM window.
//
// The return value is not wrapped into cx->compartment, so be sure to enter
// its compartment before doing anything meaningful.
JSObject *
GetXBLScope(JSContext *cx, JSObject *contentScope);

// Returns whether XBL scopes have been explicitly disabled for code running
// in this compartment. See the comment around mAllowXBLScope.
bool
AllowXBLScope(JSCompartment *c);

bool
IsSandboxPrototypeProxy(JSObject *obj);

} /* namespace xpc */

#define XPCONNECT_GLOBAL_FLAGS                                                \
    JSCLASS_DOM_GLOBAL | JSCLASS_HAS_PRIVATE |                                \
    JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_IMPLEMENTS_BARRIERS |            \
    JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(2)

void
TraceXPCGlobal(JSTracer *trc, JSObject *obj);

// XXX These should be moved into XPCJSRuntime!
NS_EXPORT_(bool)
xpc_LocalizeRuntime(JSRuntime *rt);
NS_EXPORT_(void)
xpc_DelocalizeRuntime(JSRuntime *rt);

nsresult
xpc_MorphSlimWrapper(JSContext *cx, nsISupports *tomorph);

static inline bool IS_WRAPPER_CLASS(js::Class* clazz)
{
    return clazz->ext.isWrappedNative;
}

// If IS_WRAPPER_CLASS for the JSClass of an object is true, the object can be
// a slim wrapper, holding a native in its private slot, or a wrappednative
// wrapper, holding the XPCWrappedNative in its private slot. A slim wrapper
// also holds a pointer to its XPCWrappedNativeProto in a reserved slot, we can
// check that slot for a private value (i.e. a double) to distinguish between
// the two. This allows us to store a JSObject in that slot for non-slim wrappers
// while still being able to distinguish the two cases.

// NB: This slot isn't actually reserved for us on globals, because SpiderMonkey
// uses the first N slots on globals internally. The fact that we use it for
// wrapped global objects is totally broken. But due to a happy coincidence, the
// JS engine never uses that slot. This still needs fixing though. See bug 760095.
#define WRAPPER_MULTISLOT 0

static inline bool IS_WN_WRAPPER_OBJECT(JSObject *obj)
{
    MOZ_ASSERT(IS_WRAPPER_CLASS(js::GetObjectClass(obj)));
    return !js::GetReservedSlot(obj, WRAPPER_MULTISLOT).isDouble();
}
static inline bool IS_SLIM_WRAPPER_OBJECT(JSObject *obj)
{
    return !IS_WN_WRAPPER_OBJECT(obj);
}

// Use these functions if IS_WRAPPER_CLASS(GetObjectClass(obj)) might be false.
// Avoid calling them if IS_WRAPPER_CLASS(GetObjectClass(obj)) can only be
// true, as we'd do a redundant call to IS_WRAPPER_CLASS.
static inline bool IS_WN_WRAPPER(JSObject *obj)
{
    return IS_WRAPPER_CLASS(js::GetObjectClass(obj)) && IS_WN_WRAPPER_OBJECT(obj);
}
static inline bool IS_SLIM_WRAPPER(JSObject *obj)
{
    return IS_WRAPPER_CLASS(js::GetObjectClass(obj)) && IS_SLIM_WRAPPER_OBJECT(obj);
}

extern bool
xpc_OkToHandOutWrapper(nsWrapperCache *cache);

inline JSObject*
xpc_FastGetCachedWrapper(nsWrapperCache *cache, JSObject *scope, jsval *vp)
{
    if (cache) {
        JSObject* wrapper = cache->GetWrapper();
        NS_ASSERTION(!wrapper ||
                     !cache->IsDOMBinding() ||
                     !IS_SLIM_WRAPPER(wrapper),
                     "Should never have a slim wrapper when IsDOMBinding()");
        if (wrapper &&
            js::GetObjectCompartment(wrapper) == js::GetObjectCompartment(scope) &&
            (cache->IsDOMBinding() ? !cache->HasSystemOnlyWrapper() :
             (IS_SLIM_WRAPPER(wrapper) || xpc_OkToHandOutWrapper(cache)))) {
            *vp = OBJECT_TO_JSVAL(wrapper);
            return wrapper;
        }
    }

    return nullptr;
}

// The JS GC marks objects gray that are held alive directly or
// indirectly by an XPConnect root. The cycle collector explores only
// this subset of the JS heap.
inline JSBool
xpc_IsGrayGCThing(void *thing)
{
    return JS::GCThingIsMarkedGray(thing);
}

// The cycle collector only cares about some kinds of GCthings that are
// reachable from an XPConnect root. Implemented in nsXPConnect.cpp.
extern JSBool
xpc_GCThingIsGrayCCThing(void *thing);

// Unmark gray for known-nonnull cases
MOZ_ALWAYS_INLINE void
xpc_UnmarkNonNullGrayObject(JSObject *obj)
{
    JS::ExposeGCThingToActiveJS(obj, JSTRACE_OBJECT);
}

// Remove the gray color from the given JSObject and any other objects that can
// be reached through it.
MOZ_ALWAYS_INLINE JSObject *
xpc_UnmarkGrayObject(JSObject *obj)
{
    if (obj)
        xpc_UnmarkNonNullGrayObject(obj);
    return obj;
}

inline JSScript *
xpc_UnmarkGrayScript(JSScript *script)
{
    if (script)
        JS::ExposeGCThingToActiveJS(script, JSTRACE_SCRIPT);

    return script;
}

inline JSContext *
xpc_UnmarkGrayContext(JSContext *cx)
{
    if (cx) {
        JSObject *global = JS_GetGlobalObject(cx);
        xpc_UnmarkGrayObject(global);
        if (global && JS_IsInRequest(JS_GetRuntime(cx))) {
            JSObject *scope = JS_GetGlobalForScopeChain(cx);
            if (scope != global)
                xpc_UnmarkGrayObject(scope);
        }
    }
    return cx;
}

#ifdef __cplusplus
class XPCAutoRequest : public JSAutoRequest {
public:
    XPCAutoRequest(JSContext *cx) : JSAutoRequest(cx) {
        xpc_UnmarkGrayContext(cx);
    }
};
#endif

// If aVariant is an XPCVariant, this marks the object to be in aGeneration.
// This also unmarks the gray JSObject.
extern void
xpc_MarkInCCGeneration(nsISupports* aVariant, uint32_t aGeneration);

// If aWrappedJS is a JS wrapper, unmark its JSObject.
extern void
xpc_TryUnmarkWrappedGrayObject(nsISupports* aWrappedJS);

extern void
xpc_UnmarkSkippableJSHolders();

// No JS can be on the stack when this is called. Probably only useful from
// xpcshell.
NS_EXPORT_(void)
xpc_ActivateDebugMode();

class nsIMemoryMultiReporterCallback;

// readable string conversions, static methods and members only
class XPCStringConvert
{
public:

    // If the string shares the readable's buffer, that buffer will
    // get assigned to *sharedBuffer.  Otherwise null will be
    // assigned.
    static jsval ReadableToJSVal(JSContext *cx, const nsAString &readable,
                                 nsStringBuffer** sharedBuffer);

    // Convert the given stringbuffer/length pair to a jsval
    static MOZ_ALWAYS_INLINE bool
    StringBufferToJSVal(JSContext* cx, nsStringBuffer* buf, uint32_t length,
                        JS::Value* rval, bool* sharedBuffer)
    {
        if (buf == sCachedBuffer &&
            JS::GetGCThingZone(sCachedString) == js::GetContextZone(cx))
        {
            *rval = JS::StringValue(sCachedString);
            *sharedBuffer = false;
            return true;
        }

        JSString *str = JS_NewExternalString(cx,
                                             static_cast<jschar*>(buf->Data()),
                                             length, &sDOMStringFinalizer);
        if (!str) {
            return false;
        }
        *rval = JS::StringValue(str);
        sCachedString = str;
        sCachedBuffer = buf;
        *sharedBuffer = true;
        return true;
    }

    static void ClearCache();

private:
    static nsStringBuffer* sCachedBuffer;
    static JSString* sCachedString;
    static const JSStringFinalizer sDOMStringFinalizer;

    static void FinalizeDOMString(const JSStringFinalizer *fin, jschar *chars);

    XPCStringConvert();         // not implemented
};

namespace xpc {

bool DeferredRelease(nsISupports *obj);

// If these functions return false, then an exception will be set on cx.
NS_EXPORT_(bool) Base64Encode(JSContext *cx, JS::Value val, JS::Value *out);
NS_EXPORT_(bool) Base64Decode(JSContext *cx, JS::Value val, JS::Value *out);

/**
 * Convert an nsString to jsval, returning true on success.
 * Note, the ownership of the string buffer may be moved from str to rval.
 * If that happens, str will point to an empty string after this call.
 */
bool NonVoidStringToJsval(JSContext *cx, nsAString &str, JS::Value *rval);
inline bool StringToJsval(JSContext *cx, nsAString &str, JS::Value *rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if (str.IsVoid()) {
        *rval = JSVAL_NULL;
        return true;
    }
    return NonVoidStringToJsval(cx, str, rval);
}

inline bool
NonVoidStringToJsval(JSContext* cx, const nsAString& str, JS::Value *rval)
{
    nsString mutableCopy(str);
    return NonVoidStringToJsval(cx, mutableCopy, rval);
}

inline bool
StringToJsval(JSContext* cx, const nsAString& str, JS::Value *rval)
{
    nsString mutableCopy(str);
    return StringToJsval(cx, mutableCopy, rval);
}

/**
 * As above, but for mozilla::dom::DOMString.
 */
MOZ_ALWAYS_INLINE
bool NonVoidStringToJsval(JSContext* cx, mozilla::dom::DOMString& str,
                          JS::Value *rval)
{
    if (!str.HasStringBuffer()) {
        // It's an actual XPCOM string
        return NonVoidStringToJsval(cx, str.AsAString(), rval);
    }

    uint32_t length = str.StringBufferLength();
    if (length == 0) {
        *rval = JS_GetEmptyStringValue(cx);
        return true;
    }

    nsStringBuffer* buf = str.StringBuffer();
    bool shared;
    if (!XPCStringConvert::StringBufferToJSVal(cx, buf, length, rval,
                                               &shared)) {
        return false;
    }
    if (shared) {
        // JS now needs to hold a reference to the buffer
        buf->AddRef();
    }
    return true;
}

MOZ_ALWAYS_INLINE
bool StringToJsval(JSContext* cx, mozilla::dom::DOMString& str,
                   JS::Value *rval)
{
    if (str.IsNull()) {
        *rval = JS::NullValue();
        return true;
    }
    return NonVoidStringToJsval(cx, str, rval);
}

nsIPrincipal *GetCompartmentPrincipal(JSCompartment *compartment);

bool IsXBLScope(JSCompartment *compartment);

void DumpJSHeap(FILE* file);

void SetLocationForGlobal(JSObject *global, const nsACString& location);
void SetLocationForGlobal(JSObject *global, nsIURI *locationURI);

/**
 * Define quick stubs on the given object, @a proto.
 *
 * @param cx
 *     A context.  Requires request.
 * @param proto
 *     The (newly created) prototype object for a DOM class.  The JS half
 *     of an XPCWrappedNativeProto.
 * @param flags
 *     Property flags for the quick stub properties--should be either
 *     JSPROP_ENUMERATE or 0.
 * @param interfaceCount
 *     The number of interfaces the class implements.
 * @param interfaceArray
 *     The interfaces the class implements; interfaceArray and
 *     interfaceCount are like what nsIClassInfo.getInterfaces returns.
 */
bool
DOM_DefineQuickStubs(JSContext *cx, JSObject *proto, uint32_t flags,
                     uint32_t interfaceCount, const nsIID **interfaceArray);

// This reports all the stats in |rtStats| that belong in the "explicit" tree,
// (which isn't all of them).
nsresult
ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats &rtStats,
                                 const nsACString &rtPath,
                                 nsIMemoryMultiReporterCallback *cb,
                                 nsISupports *closure, size_t *rtTotal = NULL);

/**
 * Given an arbitrary object, Unwrap will return the wrapped object if the
 * passed-in object is a wrapper that Unwrap knows about *and* the
 * currently running code has permission to access both the wrapper and
 * wrapped object.
 *
 * Since this is meant to be called from functions like
 * XPCWrappedNative::GetWrappedNativeOfJSObject, it does not set an
 * exception on |cx|.
 */
JSObject *
Unwrap(JSContext *cx, JSObject *wrapper, bool stopAtOuter = true);

/**
 * Throws an exception on cx and returns false.
 */
bool
Throw(JSContext *cx, nsresult rv);

} // namespace xpc

nsCycleCollectionParticipant *
xpc_JSZoneParticipant();

namespace mozilla {
namespace dom {

extern int HandlerFamily;
inline void* ProxyFamily() { return &HandlerFamily; }

inline bool IsDOMProxy(JSObject *obj, const js::Class* clasp)
{
    MOZ_ASSERT(js::GetObjectClass(obj) == clasp);
    return (js::IsObjectProxyClass(clasp) || js::IsFunctionProxyClass(clasp)) &&
           js::GetProxyHandler(obj)->family() == ProxyFamily();
}

inline bool IsDOMProxy(JSObject *obj)
{
    return IsDOMProxy(obj, js::GetObjectClass(obj));
}

typedef JSObject*
(*DefineInterface)(JSContext *cx, JSObject *global, jsid id, bool *enabled);

typedef bool
(*PrefEnabled)();

extern bool
DefineStaticJSVals(JSContext *cx);
void
Register(nsScriptNameSpaceManager* aNameSpaceManager);

} // namespace dom
} // namespace mozilla

#endif
