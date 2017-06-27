/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcpublic_h
#define xpcpublic_h

#include "jsapi.h"
#include "js/HeapAPI.h"
#include "js/GCAPI.h"
#include "js/Proxy.h"

#include "nsISupports.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsIGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "nsStringGlue.h"
#include "nsTArray.h"
#include "mozilla/dom/JSSlots.h"
#include "mozilla/fallible.h"
#include "nsMathUtils.h"
#include "nsStringBuffer.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/Preferences.h"

class nsGlobalWindow;
class nsIPrincipal;
class nsScriptNameSpaceManager;
class nsIMemoryReporterCallback;

namespace mozilla {
namespace dom {
class Exception;
}
}

typedef void (* xpcGCCallback)(JSGCStatus status);

namespace xpc {

class Scriptability {
public:
    explicit Scriptability(JSCompartment* c);
    bool Allowed();
    bool IsImmuneToScriptPolicy();

    void Block();
    void Unblock();
    void SetDocShellAllowsScript(bool aAllowed);

    static Scriptability& Get(JSObject* aScope);

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

JSObject*
TransplantObject(JSContext* cx, JS::HandleObject origobj, JS::HandleObject target);

bool IsContentXBLScope(JSCompartment* compartment);
bool IsInContentXBLScope(JSObject* obj);

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
JSObject*
GetXBLScope(JSContext* cx, JSObject* contentScope);

inline JSObject*
GetXBLScopeOrGlobal(JSContext* cx, JSObject* obj)
{
    if (IsInContentXBLScope(obj))
        return js::GetGlobalForObjectCrossCompartment(obj);
    return GetXBLScope(cx, obj);
}

// This function is similar to GetXBLScopeOrGlobal. However, if |obj| is a
// chrome scope, then it will return an add-on scope if addonId is non-null.
// Like GetXBLScopeOrGlobal, it returns the scope of |obj| if it's already a
// content XBL scope. But it asserts that |obj| is not an add-on scope.
JSObject*
GetScopeForXBLExecution(JSContext* cx, JS::HandleObject obj, JSAddonId* addonId);

// Returns whether XBL scopes have been explicitly disabled for code running
// in this compartment. See the comment around mAllowContentXBLScope.
bool
AllowContentXBLScope(JSCompartment* c);

// Returns whether we will use an XBL scope for this compartment. This is
// semantically equivalent to comparing global != GetXBLScope(global), but it
// does not have the side-effect of eagerly creating the XBL scope if it does
// not already exist.
bool
UseContentXBLScope(JSCompartment* c);

// Clear out the content XBL scope (if any) on the given global.  This will
// force creation of a new one if one is needed again.
void
ClearContentXBLScope(JSObject* global);

bool
IsInAddonScope(JSObject* obj);

JSObject*
GetAddonScope(JSContext* cx, JS::HandleObject contentScope, JSAddonId* addonId);

bool
IsSandboxPrototypeProxy(JSObject* obj);

bool
IsReflector(JSObject* obj);

bool
IsXrayWrapper(JSObject* obj);

// If this function was created for a given XrayWrapper, returns the global of
// the Xrayed object. Otherwise, returns the global of the function.
//
// To emphasize the obvious: the return value here is not necessarily same-
// compartment with the argument.
JSObject*
XrayAwareCalleeGlobal(JSObject* fun);

// A version of XrayAwareCalleeGlobal that can be used from a binding
// specialized getter.  We need this function because in a specialized getter we
// don't have a callee JSFunction, so can't use xpc::XrayAwareCalleeGlobal.
// Instead we do something a bit hacky using our current compartment and "this"
// value.  Note that for the Xray case thisObj will NOT be in the compartment of
// "cx".
//
// As expected, the outparam "global" need not be same-compartment with either
// thisObj or cx, though it _will_ be same-compartment with one of them.
//
// This function can fail; the return value indicates success or failure.
bool
XrayAwareCalleeGlobalForSpecializedGetters(JSContext* cx,
                                           JS::Handle<JSObject*> thisObj,
                                           JS::MutableHandle<JSObject*> global);


void
TraceXPCGlobal(JSTracer* trc, JSObject* obj);

} /* namespace xpc */

namespace JS {

struct RuntimeStats;

} // namespace JS

#define XPC_WRAPPER_FLAGS (JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE)

#define XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(n)                            \
    JSCLASS_DOM_GLOBAL | JSCLASS_HAS_PRIVATE |                                \
    JSCLASS_PRIVATE_IS_NSISUPPORTS |                                          \
    JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(DOM_GLOBAL_SLOTS + n)

#define XPCONNECT_GLOBAL_EXTRA_SLOT_OFFSET (JSCLASS_GLOBAL_SLOT_COUNT + DOM_GLOBAL_SLOTS)

#define XPCONNECT_GLOBAL_FLAGS XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(0)

inline JSObject*
xpc_FastGetCachedWrapper(JSContext* cx, nsWrapperCache* cache, JS::MutableHandleValue vp)
{
    if (cache) {
        JSObject* wrapper = cache->GetWrapper();
        if (wrapper &&
            js::GetObjectCompartment(wrapper) == js::GetContextCompartment(cx))
        {
            vp.setObject(*wrapper);
            return wrapper;
        }
    }

    return nullptr;
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

// readable string conversions, static methods and members only
class XPCStringConvert
{
public:

    // If the string shares the readable's buffer, that buffer will
    // get assigned to *sharedBuffer.  Otherwise null will be
    // assigned.
    static bool ReadableToJSVal(JSContext* cx, const nsAString& readable,
                                nsStringBuffer** sharedBuffer,
                                JS::MutableHandleValue vp);

    // Convert the given stringbuffer/length pair to a jsval
    static MOZ_ALWAYS_INLINE bool
    StringBufferToJSVal(JSContext* cx, nsStringBuffer* buf, uint32_t length,
                        JS::MutableHandleValue rval, bool* sharedBuffer)
    {
        JSString* str = JS_NewMaybeExternalString(cx,
                                                  static_cast<char16_t*>(buf->Data()),
                                                  length, &sDOMStringFinalizer, sharedBuffer);
        if (!str) {
            return false;
        }
        rval.setString(str);
        return true;
    }

    static MOZ_ALWAYS_INLINE bool IsLiteral(JSString* str)
    {
        return JS_IsExternalString(str) &&
               JS_GetExternalStringFinalizer(str) == &sLiteralFinalizer;
    }

    static MOZ_ALWAYS_INLINE bool IsDOMString(JSString* str)
    {
        return JS_IsExternalString(str) &&
               JS_GetExternalStringFinalizer(str) == &sDOMStringFinalizer;
    }

private:
    static const JSStringFinalizer sLiteralFinalizer, sDOMStringFinalizer;

    static void FinalizeLiteral(const JSStringFinalizer* fin, char16_t* chars);

    static void FinalizeDOMString(const JSStringFinalizer* fin, char16_t* chars);

    XPCStringConvert() = delete;
};

class nsIAddonInterposition;

namespace xpc {

// If these functions return false, then an exception will be set on cx.
bool Base64Encode(JSContext* cx, JS::HandleValue val, JS::MutableHandleValue out);
bool Base64Decode(JSContext* cx, JS::HandleValue val, JS::MutableHandleValue out);

/**
 * Convert an nsString to jsval, returning true on success.
 * Note, the ownership of the string buffer may be moved from str to rval.
 * If that happens, str will point to an empty string after this call.
 */
bool NonVoidStringToJsval(JSContext* cx, nsAString& str, JS::MutableHandleValue rval);
inline bool StringToJsval(JSContext* cx, nsAString& str, JS::MutableHandleValue rval)
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
    nsString mutableCopy;
    if (!mutableCopy.Assign(str, mozilla::fallible)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return NonVoidStringToJsval(cx, mutableCopy, rval);
}

inline bool
StringToJsval(JSContext* cx, const nsAString& str, JS::MutableHandleValue rval)
{
    nsString mutableCopy;
    if (!mutableCopy.Assign(str, mozilla::fallible)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return StringToJsval(cx, mutableCopy, rval);
}

/**
 * As above, but for mozilla::dom::DOMString.
 */
inline
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
        str.RelinquishBufferOwnership();
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

nsIPrincipal* GetCompartmentPrincipal(JSCompartment* compartment);

void NukeAllWrappersForCompartment(JSContext* cx, JSCompartment* compartment,
                                   js::NukeReferencesToWindow nukeReferencesToWindow = js::NukeWindowReferences);

void SetLocationForGlobal(JSObject* global, const nsACString& location);
void SetLocationForGlobal(JSObject* global, nsIURI* locationURI);

// ReportJSRuntimeExplicitTreeStats will expect this in the |extra| member
// of JS::ZoneStats.
class ZoneStatsExtras {
public:
    ZoneStatsExtras() {}

    nsCString pathPrefix;

private:
    ZoneStatsExtras(const ZoneStatsExtras& other) = delete;
    ZoneStatsExtras& operator=(const ZoneStatsExtras& other) = delete;
};

// ReportJSRuntimeExplicitTreeStats will expect this in the |extra| member
// of JS::CompartmentStats.
class CompartmentStatsExtras {
public:
    CompartmentStatsExtras() {}

    nsCString jsPathPrefix;
    nsCString domPathPrefix;
    nsCOMPtr<nsIURI> location;

private:
    CompartmentStatsExtras(const CompartmentStatsExtras& other) = delete;
    CompartmentStatsExtras& operator=(const CompartmentStatsExtras& other) = delete;
};

// This reports all the stats in |rtStats| that belong in the "explicit" tree,
// (which isn't all of them).
// @see ZoneStatsExtras
// @see CompartmentStatsExtras
void
ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats& rtStats,
                                 const nsACString& rtPath,
                                 nsIMemoryReporterCallback* handleReport,
                                 nsISupports* data,
                                 bool anonymize,
                                 size_t* rtTotal = nullptr);

/**
 * Throws an exception on cx and returns false.
 */
bool
Throw(JSContext* cx, nsresult rv);

/**
 * Returns the nsISupports native behind a given reflector (either DOM or
 * XPCWN).
 */
nsISupports*
UnwrapReflectorToISupports(JSObject* reflector);

/**
 * Singleton scopes for stuff that really doesn't fit anywhere else.
 *
 * If you find yourself wanting to use these compartments, you're probably doing
 * something wrong. Callers MUST consult with the XPConnect module owner before
 * using this compartment. If you don't, bholley will hunt you down.
 */
JSObject*
UnprivilegedJunkScope();

JSObject*
PrivilegedJunkScope();

/**
 * Shared compilation scope for XUL prototype documents and XBL
 * precompilation. This compartment has a null principal. No code may run, and
 * it is invisible to the debugger.
 */
JSObject*
CompilationScope();

/**
 * Returns the nsIGlobalObject corresponding to |aObj|'s JS global.
 */
nsIGlobalObject*
NativeGlobal(JSObject* aObj);

/**
 * If |aObj| is a window, returns the associated nsGlobalWindow.
 * Otherwise, returns null.
 */
nsGlobalWindow*
WindowOrNull(JSObject* aObj);

/**
 * If |aObj| has a window for a global, returns the associated nsGlobalWindow.
 * Otherwise, returns null.
 */
nsGlobalWindow*
WindowGlobalOrNull(JSObject* aObj);

/**
 * If |aObj| is in an addon scope and that addon scope is associated with a
 * live DOM Window, returns the associated nsGlobalWindow. Otherwise, returns
 * null.
 */
nsGlobalWindow*
AddonWindowOrNull(JSObject* aObj);

/**
 * If |cx| is in a compartment whose global is a window, returns the associated
 * nsGlobalWindow. Otherwise, returns null.
 */
nsGlobalWindow*
CurrentWindowOrNull(JSContext* cx);

void
SimulateActivityCallback(bool aActive);

// This function may be used off-main-thread, in which case it is benignly
// racey.
bool
ShouldDiscardSystemSource();

bool
SharedMemoryEnabled();

bool
SetAddonInterposition(const nsACString& addonId, nsIAddonInterposition* interposition);

bool
AllowCPOWsInAddon(const nsACString& addonId, bool allow);

bool
ExtraWarningsForSystemJS();

class ErrorBase {
  public:
    nsString mErrorMsg;
    nsString mFileName;
    uint32_t mLineNumber;
    uint32_t mColumn;

    ErrorBase() : mLineNumber(0)
                , mColumn(0)
    {}

    void Init(JSErrorBase* aReport);

    void AppendErrorDetailsTo(nsCString& error);
};

class ErrorNote : public ErrorBase {
  public:
    void Init(JSErrorNotes::Note* aNote);

    // Produce an error event message string from the given JSErrorNotes::Note.
    // This may produce an empty string if aNote doesn't have a message
    // attached.
    static void ErrorNoteToMessageString(JSErrorNotes::Note* aNote,
                                         nsAString& aString);

    // Log the error note to the stderr.
    void LogToStderr();
};

class ErrorReport : public ErrorBase {
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ErrorReport);

    nsTArray<ErrorNote> mNotes;

    nsCString mCategory;
    nsString mSourceLine;
    nsString mErrorMsgName;
    uint64_t mWindowID;
    uint32_t mFlags;
    bool mIsMuted;

    ErrorReport() : mWindowID(0)
                  , mFlags(0)
                  , mIsMuted(false)
    {}

    void Init(JSErrorReport* aReport, const char* aToStringResult,
              bool aIsChrome, uint64_t aWindowID);
    void Init(JSContext* aCx, mozilla::dom::Exception* aException,
              bool aIsChrome, uint64_t aWindowID);

    // Log the error report to the console.  Which console will depend on the
    // window id it was initialized with.
    void LogToConsole();
    // Log to console, using the given stack object (which should be a stack of
    // the sort that JS::CaptureCurrentStack produces).  aStack is allowed to be
    // null.
    void LogToConsoleWithStack(JS::HandleObject aStack);

    // Produce an error event message string from the given JSErrorReport.  Note
    // that this may produce an empty string if aReport doesn't have a
    // message attached.
    static void ErrorReportToMessageString(JSErrorReport* aReport,
                                           nsAString& aString);

    // Log the error report to the stderr.
    void LogToStderr();

  private:
    ~ErrorReport() {}
};

void
DispatchScriptErrorEvent(nsPIDOMWindowInner* win, JS::RootingContext* rootingCx,
                         xpc::ErrorReport* xpcReport, JS::Handle<JS::Value> exception);

// Get a stack of the sort that can be passed to
// xpc::ErrorReport::LogToConsoleWithStack from the given exception value.  Can
// return null if the exception value doesn't have an associated stack.  The
// returned stack, if any, may also not be in the same compartment as
// exceptionValue.
//
// The "win" argument passed in here should be the same as the window whose
// WindowID() is used to initialize the xpc::ErrorReport.  This may be null, of
// course.  If it's not null, this function may return a null stack object if
// the window is far enough gone, because in those cases we don't want to have
// the stack in the console message keeping the window alive.
JSObject*
FindExceptionStackForConsoleReport(nsPIDOMWindowInner* win,
                                   JS::HandleValue exceptionValue);

// Return a name for the compartment.
// This function makes reasonable efforts to make this name both mostly human-readable
// and unique. However, there are no guarantees of either property.
extern void
GetCurrentCompartmentName(JSContext*, nsCString& name);

void AddGCCallback(xpcGCCallback cb);
void RemoveGCCallback(xpcGCCallback cb);

inline bool
AreNonLocalConnectionsDisabled()
{
    static int disabledForTest = -1;
    if (disabledForTest == -1) {
        char *s = getenv("MOZ_DISABLE_NONLOCAL_CONNECTIONS");
        if (s) {
            disabledForTest = *s != '0';
        } else {
            disabledForTest = 0;
        }
    }
    return disabledForTest;
}

inline bool
IsInAutomation()
{
    const char* prefName =
      "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer";
    return mozilla::Preferences::GetBool(prefName) &&
        AreNonLocalConnectionsDisabled();
}

void
CreateCooperativeContext();

void
DestroyCooperativeContext();

// Please see JS_YieldCooperativeContext in jsapi.h.
void
YieldCooperativeContext();

// Please see JS_ResumeCooperativeContext in jsapi.h.
void
ResumeCooperativeContext();

} // namespace xpc

namespace mozilla {
namespace dom {

/**
 * A test for whether WebIDL methods that should only be visible to
 * chrome or XBL scopes should be exposed.
 */
bool IsChromeOrXBL(JSContext* cx, JSObject* /* unused */);

/**
 * Same as IsChromeOrXBL but can be used in worker threads as well.
 */
bool ThreadSafeIsChromeOrXBL(JSContext* cx, JSObject* obj);

} // namespace dom
} // namespace mozilla

#endif
