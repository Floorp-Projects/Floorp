/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcpublic_h
#define xpcpublic_h

#include "jsapi.h"
#include "jsproxy.h"
#include "js/HeapAPI.h"
#include "js/GCAPI.h"

#include "nsISupports.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsWrapperCache.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "mozilla/dom/JSSlots.h"
#include "nsMathUtils.h"
#include "nsStringBuffer.h"
#include "mozilla/dom/BindingDeclarations.h"

class nsIPrincipal;
class nsScriptNameSpaceManager;
class nsIGlobalObject;
class nsIMemoryReporterCallback;

#ifndef BAD_TLS_INDEX
#define BAD_TLS_INDEX ((uint32_t) -1)
#endif

namespace xpc {

class Scriptability {
public:
    Scriptability(JSCompartment *c);
    bool Allowed();
    bool IsImmuneToScriptPolicy();

    void Block();
    void Unblock();
    void SetDocShellAllowsScript(bool aAllowed);

    static Scriptability& Get(JSObject *aScope);

private:
    // Whenever a consumer wishes to prevent script from running on a global,
    // it increments this value with a call to Block(). When it wishes to
    // re-enable it (if ever), it decrements this value with a call to Unblock().
    // Script may not run if this value is non-zero.
    uint32_t mScriptBlocks;

    // Whether the docshell allows javascript in this scope. If this scope
    // doesn't have a docshell, this value is always true.
    bool mDocShellAllowsScript;

    // Whether this scope is immune to user-defined or addon-defined script
    // policy.
    bool mImmuneToScriptPolicy;

    // Whether the new-style domain policy when this compartment was created
    // forbids script execution.
    bool mScriptBlockedByPolicy;
};

JSObject *
TransplantObject(JSContext *cx, JS::HandleObject origobj, JS::HandleObject target);

bool IsXBLScope(JSCompartment *compartment);
bool IsInXBLScope(JSObject *obj);

// Return a raw XBL scope object corresponding to contentScope, which must
// be an object whose global is a DOM window.
//
// The return value is not wrapped into cx->compartment, so be sure to enter
// its compartment before doing anything meaningful.
//
// Also note that XBL scopes are lazily created, so the return-value should be
// null-checked unless the caller can ensure that the scope must already
// exist.
//
// This function asserts if |contentScope| is itself in an XBL scope to catch
// sloppy consumers. Conversely, GetXBLScopeOrGlobal will handle objects that
// are in XBL scope (by just returning the global).
JSObject *
GetXBLScope(JSContext *cx, JSObject *contentScope);

inline JSObject *
GetXBLScopeOrGlobal(JSContext *cx, JSObject *obj) {
    if (IsInXBLScope(obj))
        return js::GetGlobalForObjectCrossCompartment(obj);
    return GetXBLScope(cx, obj);
}

// Returns whether XBL scopes have been explicitly disabled for code running
// in this compartment. See the comment around mAllowXBLScope.
bool
AllowXBLScope(JSCompartment *c);

// Returns whether we will use an XBL scope for this compartment. This is
// semantically equivalent to comparing global != GetXBLScope(global), but it
// does not have the side-effect of eagerly creating the XBL scope if it does
// not already exist.
bool
UseXBLScope(JSCompartment *c);

bool
IsSandboxPrototypeProxy(JSObject *obj);

bool
IsReflector(JSObject *obj);

bool
IsXrayWrapper(JSObject *obj);

// If this function was created for a given XrayWrapper, returns the global of
// the Xrayed object. Otherwise, returns the global of the function.
//
// To emphasize the obvious: the return value here is not necessarily same-
// compartment with the argument.
JSObject *
XrayAwareCalleeGlobal(JSObject *fun);

} /* namespace xpc */

namespace JS {

struct RuntimeStats;

}

#define XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(n)                            \
    JSCLASS_DOM_GLOBAL | JSCLASS_HAS_PRIVATE |                                \
    JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_IMPLEMENTS_BARRIERS |            \
    JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(DOM_GLOBAL_SLOTS + n)

#define XPCONNECT_GLOBAL_EXTRA_SLOT_OFFSET (JSCLASS_GLOBAL_SLOT_COUNT + DOM_GLOBAL_SLOTS)

#define XPCONNECT_GLOBAL_FLAGS XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(0)

inline JSObject*
xpc_FastGetCachedWrapper(nsWrapperCache *cache, JSObject *scope, JS::MutableHandleValue vp)
{
    if (cache) {
        JSObject* wrapper = cache->GetWrapper();
        if (wrapper &&
            js::GetObjectCompartment(wrapper) == js::GetObjectCompartment(scope))
        {
            vp.setObject(*wrapper);
            return wrapper;
        }
    }

    return nullptr;
}

// The JS GC marks objects gray that are held alive directly or
// indirectly by an XPConnect root. The cycle collector explores only
// this subset of the JS heap.
inline bool
xpc_IsGrayGCThing(void *thing)
{
    return JS::GCThingIsMarkedGray(thing);
}

// The cycle collector only cares about some kinds of GCthings that are
// reachable from an XPConnect root. Implemented in nsXPConnect.cpp.
extern bool
xpc_GCThingIsGrayCCThing(void *thing);

inline JSScript *
xpc_UnmarkGrayScript(JSScript *script)
{
    if (script)
        JS::ExposeGCThingToActiveJS(script, JSTRACE_SCRIPT);

    return script;
}

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
void
xpc_ActivateDebugMode();

// readable string conversions, static methods and members only
class XPCStringConvert
{
    // One-slot cache, because it turns out it's common for web pages to
    // get the same string a few times in a row.  We get about a 40% cache
    // hit rate on this cache last it was measured.  We'd get about 70%
    // hit rate with a hashtable with removal on finalization, but that
    // would take a lot more machinery.
    struct ZoneStringCache
    {
        nsStringBuffer* mBuffer;
        JSString* mString;
    };

public:

    // If the string shares the readable's buffer, that buffer will
    // get assigned to *sharedBuffer.  Otherwise null will be
    // assigned.
    static bool ReadableToJSVal(JSContext *cx, const nsAString &readable,
                                nsStringBuffer** sharedBuffer,
                                JS::MutableHandleValue vp);

    // Convert the given stringbuffer/length pair to a jsval
    static MOZ_ALWAYS_INLINE bool
    StringBufferToJSVal(JSContext* cx, nsStringBuffer* buf, uint32_t length,
                        JS::MutableHandleValue rval, bool* sharedBuffer)
    {
        JS::Zone *zone = js::GetContextZone(cx);
        ZoneStringCache *cache = static_cast<ZoneStringCache*>(JS_GetZoneUserData(zone));
        if (cache && buf == cache->mBuffer) {
            MOZ_ASSERT(JS::GetGCThingZone(cache->mString) == zone);
            JS::MarkStringAsLive(zone, cache->mString);
            rval.setString(cache->mString);
            *sharedBuffer = false;
            return true;
        }

        JSString *str = JS_NewExternalString(cx,
                                             static_cast<jschar*>(buf->Data()),
                                             length, &sDOMStringFinalizer);
        if (!str) {
            return false;
        }
        rval.setString(str);
        if (!cache) {
            cache = new ZoneStringCache();
            JS_SetZoneUserData(zone, cache);
        }
        cache->mBuffer = buf;
        cache->mString = str;
        *sharedBuffer = true;
        return true;
    }

    static void FreeZoneCache(JS::Zone *zone);
    static void ClearZoneCache(JS::Zone *zone);

    static MOZ_ALWAYS_INLINE bool IsLiteral(JSString *str)
    {
        return JS_IsExternalString(str) &&
               JS_GetExternalStringFinalizer(str) == &sLiteralFinalizer;
    }

    static MOZ_ALWAYS_INLINE bool IsDOMString(JSString *str)
    {
        return JS_IsExternalString(str) &&
               JS_GetExternalStringFinalizer(str) == &sDOMStringFinalizer;
    }

private:
    static const JSStringFinalizer sLiteralFinalizer, sDOMStringFinalizer;

    static void FinalizeLiteral(const JSStringFinalizer *fin, jschar *chars);

    static void FinalizeDOMString(const JSStringFinalizer *fin, jschar *chars);

    XPCStringConvert();         // not implemented
};

namespace xpc {

// If these functions return false, then an exception will be set on cx.
bool Base64Encode(JSContext *cx, JS::HandleValue val, JS::MutableHandleValue out);
bool Base64Decode(JSContext *cx, JS::HandleValue val, JS::MutableHandleValue out);

/**
 * Convert an nsString to jsval, returning true on success.
 * Note, the ownership of the string buffer may be moved from str to rval.
 * If that happens, str will point to an empty string after this call.
 */
bool NonVoidStringToJsval(JSContext *cx, nsAString &str, JS::MutableHandleValue rval);
inline bool StringToJsval(JSContext *cx, nsAString &str, JS::MutableHandleValue rval)
{
    // From the T_DOMSTRING case in XPCConvert::NativeData2JS.
    if (str.IsVoid()) {
        rval.setNull();
        return true;
    }
    return NonVoidStringToJsval(cx, str, rval);
}

inline bool
NonVoidStringToJsval(JSContext* cx, const nsAString& str, JS::MutableHandleValue rval)
{
    nsString mutableCopy(str);
    return NonVoidStringToJsval(cx, mutableCopy, rval);
}

inline bool
StringToJsval(JSContext* cx, const nsAString& str, JS::MutableHandleValue rval)
{
    nsString mutableCopy(str);
    return StringToJsval(cx, mutableCopy, rval);
}

/**
 * As above, but for mozilla::dom::DOMString.
 */
MOZ_ALWAYS_INLINE
bool NonVoidStringToJsval(JSContext* cx, mozilla::dom::DOMString& str,
                          JS::MutableHandleValue rval)
{
    if (!str.HasStringBuffer()) {
        // It's an actual XPCOM string
        return NonVoidStringToJsval(cx, str.AsAString(), rval);
    }

    uint32_t length = str.StringBufferLength();
    if (length == 0) {
        rval.set(JS_GetEmptyStringValue(cx));
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
                   JS::MutableHandleValue rval)
{
    if (str.IsNull()) {
        rval.setNull();
        return true;
    }
    return NonVoidStringToJsval(cx, str, rval);
}

nsIPrincipal *GetCompartmentPrincipal(JSCompartment *compartment);

void SetLocationForGlobal(JSObject *global, const nsACString& location);
void SetLocationForGlobal(JSObject *global, nsIURI *locationURI);

// ReportJSRuntimeExplicitTreeStats will expect this in the |extra| member
// of JS::ZoneStats.
class ZoneStatsExtras {
public:
    ZoneStatsExtras()
    {}

    nsAutoCString pathPrefix;

private:
    ZoneStatsExtras(const ZoneStatsExtras &other) MOZ_DELETE;
    ZoneStatsExtras& operator=(const ZoneStatsExtras &other) MOZ_DELETE;
};

// ReportJSRuntimeExplicitTreeStats will expect this in the |extra| member
// of JS::CompartmentStats.
class CompartmentStatsExtras {
public:
    CompartmentStatsExtras()
    {}

    nsAutoCString jsPathPrefix;
    nsAutoCString domPathPrefix;
    nsCOMPtr<nsIURI> location;

private:
    CompartmentStatsExtras(const CompartmentStatsExtras &other) MOZ_DELETE;
    CompartmentStatsExtras& operator=(const CompartmentStatsExtras &other) MOZ_DELETE;
};

// This reports all the stats in |rtStats| that belong in the "explicit" tree,
// (which isn't all of them).
// @see ZoneStatsExtras
// @see CompartmentStatsExtras
nsresult
ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats &rtStats,
                                 const nsACString &rtPath,
                                 nsIMemoryReporterCallback *cb,
                                 nsISupports *closure, size_t *rtTotal = nullptr);

/**
 * Throws an exception on cx and returns false.
 */
bool
Throw(JSContext *cx, nsresult rv);

/**
 * Every global should hold a native that implements the nsIGlobalObject interface.
 */
nsIGlobalObject *
GetNativeForGlobal(JSObject *global);

/**
 * In some cases a native object does not really belong to any compartment (XBL,
 * document created from by XHR of a worker, etc.). But when for some reason we
 * have to wrap these natives (because of an event for example) instead of just
 * wrapping them into some random compartment we find on the context stack (like
 * we did previously) a default compartment is used. This function returns that
 * compartment's global. It is a singleton on the runtime.
 * If you find yourself wanting to use this compartment, you're probably doing
 * something wrong. Callers MUST consult with the XPConnect module owner before
 * using this compartment. If you don't, bholley will hunt you down.
 */
JSObject *
GetJunkScope();

/**
 * Returns the native global of the junk scope. See comment of GetJunkScope
 * about the conditions of using it.
 */
nsIGlobalObject *
GetJunkScopeGlobal();

/**
 * If |aObj| is a window, returns the associated nsGlobalWindow.
 * Otherwise, returns null.
 */
nsGlobalWindow*
WindowOrNull(JSObject *aObj);

/**
 * If |aObj| has a window for a global, returns the associated nsGlobalWindow.
 * Otherwise, returns null.
 */
nsGlobalWindow*
WindowGlobalOrNull(JSObject *aObj);

// Error reporter used when there is no associated DOM window on to which to
// report errors and warnings.
void
SystemErrorReporter(JSContext *cx, const char *message, JSErrorReport *rep);

void
SimulateActivityCallback(bool aActive);

void
RecordAdoptedNode(JSCompartment *c);

void
RecordDonatedNode(JSCompartment *c);

} // namespace xpc

namespace mozilla {
namespace dom {

typedef JSObject*
(*DefineInterface)(JSContext *cx, JS::Handle<JSObject*> global,
                   JS::Handle<jsid> id, bool defineOnGlobal);

typedef JSObject*
(*ConstructNavigatorProperty)(JSContext *cx, JS::Handle<JSObject*> naviObj);

// Check whether a constructor should be enabled for the given object.
// Note that the object should NOT be an Xray, since Xrays will end up
// defining constructors on the underlying object.
// This is a typedef for the function type itself, not the function
// pointer, so it's more obvious that pointers to a ConstructorEnabled
// can be null.
typedef bool
(ConstructorEnabled)(JSContext* cx, JS::Handle<JSObject*> obj);

void
Register(nsScriptNameSpaceManager* aNameSpaceManager);

/**
 * A test for whether WebIDL methods that should only be visible to
 * chrome or XBL scopes should be exposed.
 */
bool IsChromeOrXBL(JSContext* cx, JSObject* /* unused */);

} // namespace dom
} // namespace mozilla

#endif
