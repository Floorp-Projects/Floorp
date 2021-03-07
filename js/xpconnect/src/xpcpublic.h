/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xpcpublic_h
#define xpcpublic_h

#include <cstddef>
#include <cstdint>
#include "ErrorList.h"
#include "js/BuildId.h"
#include "js/ErrorReport.h"
#include "js/GCAPI.h"
#include "js/Object.h"
#include "js/RootingAPI.h"
#include "js/String.h"
#include "js/TypeDecls.h"
#include "js/Utility.h"
#include "js/Value.h"
#include "jsapi.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/fallible.h"
#include "nsAtom.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIURI.h"
#include "nsStringBuffer.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

// XXX only for NukeAllWrappersForRealm, which is only used in
// dom/base/WindowDestroyedEvent.cpp outside of js
#include "jsfriendapi.h"

class JSObject;
class JSString;
class JSTracer;
class nsGlobalWindowInner;
class nsIAddonInterposition;
class nsIGlobalObject;
class nsIHandleReportCallback;
class nsIPrincipal;
class nsPIDOMWindowInner;
struct JSContext;
struct nsID;
struct nsXPTInterfaceInfo;

namespace JS {
class Compartment;
class Realm;
class RealmOptions;
class Value;
struct RuntimeStats;
}  // namespace JS

namespace mozilla {
class BasePrincipal;

namespace dom {
class Exception;
}  // namespace dom
}  // namespace mozilla

using xpcGCCallback = void (*)(JSGCStatus);

namespace xpc {

class Scriptability {
 public:
  explicit Scriptability(JS::Realm* realm);
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

JSObject* TransplantObject(JSContext* cx, JS::HandleObject origobj,
                           JS::HandleObject target);

JSObject* TransplantObjectRetainingXrayExpandos(JSContext* cx,
                                                JS::HandleObject origobj,
                                                JS::HandleObject target);

// If origObj has an xray waiver, nuke it before transplant.
JSObject* TransplantObjectNukingXrayWaiver(JSContext* cx,
                                           JS::HandleObject origObj,
                                           JS::HandleObject target);

bool IsUAWidgetCompartment(JS::Compartment* compartment);
bool IsUAWidgetScope(JS::Realm* realm);
bool IsInUAWidgetScope(JSObject* obj);

bool MightBeWebContentCompartment(JS::Compartment* compartment);

void SetCompartmentChangedDocumentDomain(JS::Compartment* compartment);

JSObject* GetUAWidgetScope(JSContext* cx, nsIPrincipal* principal);

JSObject* GetUAWidgetScope(JSContext* cx, JSObject* contentScope);

// Returns whether XBL scopes have been explicitly disabled for code running
// in this compartment. See the comment around mAllowContentXBLScope.
bool AllowContentXBLScope(JS::Realm* realm);

// Get the scope for creating reflectors for native anonymous content
// whose normal global would be the given global.
JSObject* NACScope(JSObject* global);

bool IsSandboxPrototypeProxy(JSObject* obj);

// The JSContext argument represents the Realm that's asking the question.  This
// is needed to properly answer without exposing information unnecessarily
// from behind security wrappers.  There will be no exceptions thrown on this
// JSContext.
bool IsReflector(JSObject* obj, JSContext* cx);

bool IsXrayWrapper(JSObject* obj);

// If this function was created for a given XrayWrapper, returns the global of
// the Xrayed object. Otherwise, returns the global of the function.
//
// To emphasize the obvious: the return value here is not necessarily same-
// compartment with the argument.
JSObject* XrayAwareCalleeGlobal(JSObject* fun);

void TraceXPCGlobal(JSTracer* trc, JSObject* obj);

/**
 * Creates a new global object using the given aCOMObj as the global
 * object. The object will be set up according to the flags (defined
 * below). If you do not pass INIT_JS_STANDARD_CLASSES, then aCOMObj
 * must implement nsIXPCScriptable so it can resolve the standard
 * classes when asked by the JS engine.
 *
 * @param aJSContext the context to use while creating the global object.
 * @param aCOMObj the native object that represents the global object.
 * @param aPrincipal the principal of the code that will run in this
 *                   compartment. Can be null if not on the main thread.
 * @param aFlags one of the flags below specifying what options this
 *               global object wants.
 * @param aOptions JSAPI-specific options for the new compartment.
 */
nsresult InitClassesWithNewWrappedGlobal(JSContext* aJSContext,
                                         nsISupports* aCOMObj,
                                         nsIPrincipal* aPrincipal,
                                         uint32_t aFlags,
                                         JS::RealmOptions& aOptions,
                                         JS::MutableHandleObject aNewGlobal);

enum InitClassesFlag {
  INIT_JS_STANDARD_CLASSES = 1 << 0,
  DONT_FIRE_ONNEWGLOBALHOOK = 1 << 1,
  OMIT_COMPONENTS_OBJECT = 1 << 2,
};

} /* namespace xpc */

namespace JS {

struct RuntimeStats;

}  // namespace JS

#define XPC_WRAPPER_FLAGS (JSCLASS_HAS_PRIVATE | JSCLASS_FOREGROUND_FINALIZE)

#define XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(n)                            \
  JSCLASS_DOM_GLOBAL | JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS | \
      JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(DOM_GLOBAL_SLOTS + n)

#define XPCONNECT_GLOBAL_EXTRA_SLOT_OFFSET \
  (JSCLASS_GLOBAL_SLOT_COUNT + DOM_GLOBAL_SLOTS)

#define XPCONNECT_GLOBAL_FLAGS XPCONNECT_GLOBAL_FLAGS_WITH_EXTRA_SLOTS(0)

inline JSObject* xpc_FastGetCachedWrapper(JSContext* cx, nsWrapperCache* cache,
                                          JS::MutableHandleValue vp) {
  if (cache) {
    JSObject* wrapper = cache->GetWrapper();
    if (wrapper &&
        JS::GetCompartment(wrapper) == js::GetContextCompartment(cx)) {
      vp.setObject(*wrapper);
      return wrapper;
    }
  }

  return nullptr;
}

// If aWrappedJS is a JS wrapper, unmark its JSObject.
extern void xpc_TryUnmarkWrappedGrayObject(nsISupports* aWrappedJS);

extern void xpc_UnmarkSkippableJSHolders();

// Defined in XPCDebug.cpp.
extern bool xpc_DumpJSStack(bool showArgs, bool showLocals, bool showThisProps);

// Return a newly-allocated string containing a representation of the
// current JS stack. Defined in XPCDebug.cpp.
extern JS::UniqueChars xpc_PrintJSStack(JSContext* cx, bool showArgs,
                                        bool showLocals, bool showThisProps);

// readable string conversions, static methods and members only
class XPCStringConvert {
 public:
  // If the string shares the readable's buffer, that buffer will
  // get assigned to *sharedBuffer.  Otherwise null will be
  // assigned.
  static bool ReadableToJSVal(JSContext* cx, const nsAString& readable,
                              nsStringBuffer** sharedBuffer,
                              JS::MutableHandleValue vp);

  // Convert the given stringbuffer/length pair to a jsval
  static MOZ_ALWAYS_INLINE bool StringBufferToJSVal(JSContext* cx,
                                                    nsStringBuffer* buf,
                                                    uint32_t length,
                                                    JS::MutableHandleValue rval,
                                                    bool* sharedBuffer) {
    JSString* str = JS_NewMaybeExternalString(
        cx, static_cast<char16_t*>(buf->Data()), length,
        &sDOMStringExternalString, sharedBuffer);
    if (!str) {
      return false;
    }
    rval.setString(str);
    return true;
  }

  static inline bool StringLiteralToJSVal(JSContext* cx,
                                          const char16_t* literal,
                                          uint32_t length,
                                          JS::MutableHandleValue rval) {
    bool ignored;
    JSString* str = JS_NewMaybeExternalString(
        cx, literal, length, &sLiteralExternalString, &ignored);
    if (!str) {
      return false;
    }
    rval.setString(str);
    return true;
  }

  static inline bool DynamicAtomToJSVal(JSContext* cx, nsDynamicAtom* atom,
                                        JS::MutableHandleValue rval) {
    bool sharedAtom;
    JSString* str =
        JS_NewMaybeExternalString(cx, atom->GetUTF16String(), atom->GetLength(),
                                  &sDynamicAtomExternalString, &sharedAtom);
    if (!str) {
      return false;
    }
    if (sharedAtom) {
      // We only have non-owning atoms in DOMString for now.
      // nsDynamicAtom::AddRef is always-inline and defined in a
      // translation unit we can't get to here.  So we need to go through
      // nsAtom::AddRef to call it.
      static_cast<nsAtom*>(atom)->AddRef();
    }
    rval.setString(str);
    return true;
  }

  static MOZ_ALWAYS_INLINE bool MaybeGetExternalStringChars(
      JSString* str, const JSExternalStringCallbacks* desiredCallbacks,
      const char16_t** chars) {
    const JSExternalStringCallbacks* callbacks;
    return JS::IsExternalString(str, &callbacks, chars) &&
           callbacks == desiredCallbacks;
  }

  // Returns non-null chars if the given string is a literal external string.
  static MOZ_ALWAYS_INLINE bool MaybeGetLiteralStringChars(
      JSString* str, const char16_t** chars) {
    return MaybeGetExternalStringChars(str, &sLiteralExternalString, chars);
  }

  // Returns non-null chars if the given string is a DOM external string.
  static MOZ_ALWAYS_INLINE bool MaybeGetDOMStringChars(JSString* str,
                                                       const char16_t** chars) {
    return MaybeGetExternalStringChars(str, &sDOMStringExternalString, chars);
  }

 private:
  struct LiteralExternalString : public JSExternalStringCallbacks {
    void finalize(char16_t* aChars) const override;
    size_t sizeOfBuffer(const char16_t* aChars,
                        mozilla::MallocSizeOf aMallocSizeOf) const override;
  };
  struct DOMStringExternalString : public JSExternalStringCallbacks {
    void finalize(char16_t* aChars) const override;
    size_t sizeOfBuffer(const char16_t* aChars,
                        mozilla::MallocSizeOf aMallocSizeOf) const override;
  };
  struct DynamicAtomExternalString : public JSExternalStringCallbacks {
    void finalize(char16_t* aChars) const override;
    size_t sizeOfBuffer(const char16_t* aChars,
                        mozilla::MallocSizeOf aMallocSizeOf) const override;
  };
  static const LiteralExternalString sLiteralExternalString;
  static const DOMStringExternalString sDOMStringExternalString;
  static const DynamicAtomExternalString sDynamicAtomExternalString;

  XPCStringConvert() = delete;
};

class nsIAddonInterposition;

namespace xpc {

// If these functions return false, then an exception will be set on cx.
bool Base64Encode(JSContext* cx, JS::HandleValue val,
                  JS::MutableHandleValue out);
bool Base64Decode(JSContext* cx, JS::HandleValue val,
                  JS::MutableHandleValue out);

/**
 * Convert an nsString to jsval, returning true on success.
 * Note, the ownership of the string buffer may be moved from str to rval.
 * If that happens, str will point to an empty string after this call.
 */
bool NonVoidStringToJsval(JSContext* cx, nsAString& str,
                          JS::MutableHandleValue rval);
inline bool StringToJsval(JSContext* cx, nsAString& str,
                          JS::MutableHandleValue rval) {
  // From the T_ASTRING case in XPCConvert::NativeData2JS.
  if (str.IsVoid()) {
    rval.setNull();
    return true;
  }
  return NonVoidStringToJsval(cx, str, rval);
}

inline bool NonVoidStringToJsval(JSContext* cx, const nsAString& str,
                                 JS::MutableHandleValue rval) {
  nsString mutableCopy;
  if (!mutableCopy.Assign(str, mozilla::fallible)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  return NonVoidStringToJsval(cx, mutableCopy, rval);
}

inline bool StringToJsval(JSContext* cx, const nsAString& str,
                          JS::MutableHandleValue rval) {
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
inline bool NonVoidStringToJsval(JSContext* cx, mozilla::dom::DOMString& str,
                                 JS::MutableHandleValue rval) {
  if (str.IsEmpty()) {
    rval.set(JS_GetEmptyStringValue(cx));
    return true;
  }

  if (str.HasStringBuffer()) {
    uint32_t length = str.StringBufferLength();
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

  if (str.HasLiteral()) {
    return XPCStringConvert::StringLiteralToJSVal(cx, str.Literal(),
                                                  str.LiteralLength(), rval);
  }

  if (str.HasAtom()) {
    return XPCStringConvert::DynamicAtomToJSVal(cx, str.Atom(), rval);
  }

  // It's an actual XPCOM string
  return NonVoidStringToJsval(cx, str.AsAString(), rval);
}

MOZ_ALWAYS_INLINE
bool StringToJsval(JSContext* cx, mozilla::dom::DOMString& str,
                   JS::MutableHandleValue rval) {
  if (str.IsNull()) {
    rval.setNull();
    return true;
  }
  return NonVoidStringToJsval(cx, str, rval);
}

mozilla::BasePrincipal* GetRealmPrincipal(JS::Realm* realm);

void NukeAllWrappersForRealm(JSContext* cx, JS::Realm* realm,
                             js::NukeReferencesToWindow nukeReferencesToWindow =
                                 js::NukeWindowReferences);

void SetLocationForGlobal(JSObject* global, const nsACString& location);
void SetLocationForGlobal(JSObject* global, nsIURI* locationURI);

// ReportJSRuntimeExplicitTreeStats will expect this in the |extra| member
// of JS::ZoneStats.
class ZoneStatsExtras {
 public:
  ZoneStatsExtras() = default;

  nsCString pathPrefix;

 private:
  ZoneStatsExtras(const ZoneStatsExtras& other) = delete;
  ZoneStatsExtras& operator=(const ZoneStatsExtras& other) = delete;
};

// ReportJSRuntimeExplicitTreeStats will expect this in the |extra| member
// of JS::RealmStats.
class RealmStatsExtras {
 public:
  RealmStatsExtras() = default;

  nsCString jsPathPrefix;
  nsCString domPathPrefix;
  nsCOMPtr<nsIURI> location;

 private:
  RealmStatsExtras(const RealmStatsExtras& other) = delete;
  RealmStatsExtras& operator=(const RealmStatsExtras& other) = delete;
};

// This reports all the stats in |rtStats| that belong in the "explicit" tree,
// (which isn't all of them).
// @see ZoneStatsExtras
// @see RealmStatsExtras
void ReportJSRuntimeExplicitTreeStats(const JS::RuntimeStats& rtStats,
                                      const nsACString& rtPath,
                                      nsIHandleReportCallback* handleReport,
                                      nsISupports* data, bool anonymize,
                                      size_t* rtTotal = nullptr);

/**
 * Throws an exception on cx and returns false.
 */
bool Throw(JSContext* cx, nsresult rv);

/**
 * Returns the nsISupports native behind a given reflector (either DOM or
 * XPCWN).  If a non-reflector object is passed in, null will be returned.
 *
 * This function will not correctly handle Window or Location objects behind
 * cross-compartment wrappers: it will return null.  If you care about getting
 * non-null for Window or Location, use ReflectorToISupportsDynamic.
 */
already_AddRefed<nsISupports> ReflectorToISupportsStatic(JSObject* reflector);

/**
 * Returns the nsISupports native behind a given reflector (either DOM or
 * XPCWN).  If a non-reflector object is passed in, null will be returned.
 *
 * The JSContext argument represents the Realm that's asking for the
 * nsISupports.  This is needed to properly handle Window and Location objects,
 * which do dynamic security checks.
 */
already_AddRefed<nsISupports> ReflectorToISupportsDynamic(JSObject* reflector,
                                                          JSContext* cx);

/**
 * Singleton scopes for stuff that really doesn't fit anywhere else.
 *
 * If you find yourself wanting to use these compartments, you're probably doing
 * something wrong. Callers MUST consult with the XPConnect module owner before
 * using this compartment. If you don't, bholley will hunt you down.
 */
JSObject* UnprivilegedJunkScope();

JSObject* UnprivilegedJunkScope(const mozilla::fallible_t&);

bool IsUnprivilegedJunkScope(JSObject*);

/**
 * This will generally be the shared JSM global, but callers should not depend
 * on that fact.
 */
JSObject* PrivilegedJunkScope();

/**
 * Shared compilation scope for XUL prototype documents and XBL
 * precompilation.
 */
JSObject* CompilationScope();

/**
 * Returns the nsIGlobalObject corresponding to |obj|'s JS global. |obj| must
 * not be a cross-compartment wrapper: CCWs are not associated with a single
 * global.
 */
nsIGlobalObject* NativeGlobal(JSObject* obj);

/**
 * Returns the nsIGlobalObject corresponding to |cx|'s JS global. Must not be
 * called when |cx| is not in a Realm.
 */
nsIGlobalObject* CurrentNativeGlobal(JSContext* cx);

/**
 * If |aObj| is a window, returns the associated nsGlobalWindow.
 * Otherwise, returns null.
 */
nsGlobalWindowInner* WindowOrNull(JSObject* aObj);

/**
 * If |aObj| has a window for a global, returns the associated nsGlobalWindow.
 * Otherwise, returns null. Note: aObj must not be a cross-compartment wrapper
 * because CCWs are not associated with a single global/realm.
 */
nsGlobalWindowInner* WindowGlobalOrNull(JSObject* aObj);

/**
 * If |aObj| is a Sandbox object associated with a DOMWindow via a
 * sandboxPrototype, then return that DOMWindow.
 * |aCx| is used for checked unwrapping of the Window.
 */
nsGlobalWindowInner* SandboxWindowOrNull(JSObject* aObj, JSContext* aCx);

/**
 * If |cx| is in a realm whose global is a window, returns the associated
 * nsGlobalWindow. Otherwise, returns null.
 */
nsGlobalWindowInner* CurrentWindowOrNull(JSContext* cx);

class MOZ_RAII AutoScriptActivity {
  bool mActive;
  bool mOldValue;

 public:
  explicit AutoScriptActivity(bool aActive);
  ~AutoScriptActivity();
};

// This function may be used off-main-thread, in which case it is benignly
// racey.
bool ShouldDiscardSystemSource();

void SetPrefableRealmOptions(JS::RealmOptions& options);

class ErrorBase {
 public:
  nsString mErrorMsg;
  nsString mFileName;
  uint32_t mSourceId;
  uint32_t mLineNumber;
  uint32_t mColumn;

  ErrorBase() : mSourceId(0), mLineNumber(0), mColumn(0) {}

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
  bool mIsWarning;
  bool mIsMuted;
  bool mIsPromiseRejection;

  ErrorReport()
      : mWindowID(0),
        mIsWarning(false),
        mIsMuted(false),
        mIsPromiseRejection(false) {}

  void Init(JSErrorReport* aReport, const char* aToStringResult, bool aIsChrome,
            uint64_t aWindowID);
  void Init(JSContext* aCx, mozilla::dom::Exception* aException, bool aIsChrome,
            uint64_t aWindowID);

  // Log the error report to the console.  Which console will depend on the
  // window id it was initialized with.
  void LogToConsole();
  // Log to console, using the given stack object (which should be a stack of
  // the sort that JS::CaptureCurrentStack produces).  aStack is allowed to be
  // null. If aStack is non-null, aStackGlobal must be a non-null global
  // object that's same-compartment with aStack. Note that aStack might be a
  // CCW.
  void LogToConsoleWithStack(nsGlobalWindowInner* aWin,
                             JS::Handle<mozilla::Maybe<JS::Value>> aException,
                             JS::HandleObject aStack,
                             JS::HandleObject aStackGlobal);

  // Produce an error event message string from the given JSErrorReport.  Note
  // that this may produce an empty string if aReport doesn't have a
  // message attached.
  static void ErrorReportToMessageString(JSErrorReport* aReport,
                                         nsAString& aString);

  // Log the error report to the stderr.
  void LogToStderr();

  bool IsWarning() const { return mIsWarning; };

 private:
  ~ErrorReport() = default;
};

void DispatchScriptErrorEvent(nsPIDOMWindowInner* win,
                              JS::RootingContext* rootingCx,
                              xpc::ErrorReport* xpcReport,
                              JS::Handle<JS::Value> exception,
                              JS::Handle<JSObject*> exceptionStack);

// Get a stack (as stackObj outparam) of the sort that can be passed to
// xpc::ErrorReport::LogToConsoleWithStack from the given exception value.  Can
// be nullptr if the exception value doesn't have an associated stack, and if
// there is no stack supplied by the JS engine in exceptionStack.  The
// returned stack, if any, may also not be in the same compartment as
// exceptionValue.
//
// The "win" argument passed in here should be the same as the window whose
// WindowID() is used to initialize the xpc::ErrorReport.  This may be null, of
// course.  If it's not null, this function may return a null stack object if
// the window is far enough gone, because in those cases we don't want to have
// the stack in the console message keeping the window alive.
//
// If this function sets stackObj to a non-null value, stackGlobal is set to
// either the JS exception object's global or the global of the SavedFrame we
// got from a DOM or XPConnect exception. In all cases, stackGlobal is an
// unwrapped global object and is same-compartment with stackObj.
void FindExceptionStackForConsoleReport(nsPIDOMWindowInner* win,
                                        JS::HandleValue exceptionValue,
                                        JS::HandleObject exceptionStack,
                                        JS::MutableHandleObject stackObj,
                                        JS::MutableHandleObject stackGlobal);

// Return a name for the realm.
// This function makes reasonable efforts to make this name both mostly
// human-readable and unique. However, there are no guarantees of either
// property.
extern void GetCurrentRealmName(JSContext*, nsCString& name);

void AddGCCallback(xpcGCCallback cb);
void RemoveGCCallback(xpcGCCallback cb);

// We need an exact page size only if we run the binary in automation.
#if defined(XP_DARWIN) && defined(__aarch64__)
const size_t kAutomationPageSize = 16384;
#else
const size_t kAutomationPageSize = 4096;
#endif

struct alignas(kAutomationPageSize) ReadOnlyPage final {
  bool mNonLocalConnectionsDisabled = false;
  bool mTurnOffAllSecurityPref = false;

  static void Init();

#ifdef MOZ_TSAN
  // TSan is confused by write access to read-only section.
  static ReadOnlyPage sInstance;
#else
  static const volatile ReadOnlyPage sInstance;
#endif

 private:
  constexpr ReadOnlyPage() = default;
  ReadOnlyPage(const ReadOnlyPage&) = delete;
  void operator=(const ReadOnlyPage&) = delete;

  static void Write(const volatile bool* aPtr, bool aValue);
};

inline bool AreNonLocalConnectionsDisabled() {
  return ReadOnlyPage::sInstance.mNonLocalConnectionsDisabled;
}

inline bool IsInAutomation() {
  if (!ReadOnlyPage::sInstance.mTurnOffAllSecurityPref) {
    return false;
  }
  MOZ_RELEASE_ASSERT(AreNonLocalConnectionsDisabled());
  return true;
}

void InitializeJSContext();

/**
 * Extract the native nsID object from a JS ID, IfaceID, ClassID, or ContractID
 * value.
 *
 * Returns 'Nothing()' if 'aVal' does is not one of the supported ID types.
 */
mozilla::Maybe<nsID> JSValue2ID(JSContext* aCx, JS::HandleValue aVal);

/**
 * Reflect an ID into JS
 */
bool ID2JSValue(JSContext* aCx, const nsID& aId, JS::MutableHandleValue aVal);

/**
 * Reflect an IfaceID into JS
 *
 * This object will expose constants from the selected interface, and support
 * 'instanceof', in addition to the other methods available on JS ID objects.
 *
 * Use 'xpc::JSValue2ID' to unwrap JS::Values created with this function.
 */
bool IfaceID2JSValue(JSContext* aCx, const nsXPTInterfaceInfo& aInfo,
                     JS::MutableHandleValue aVal);

/**
 * Reflect a ContractID into JS
 *
 * This object will expose 'getService' and 'createInstance' methods in addition
 * to the other methods available on nsID objects.
 *
 * Use 'xpc::JSValue2ID' to unwrap JS::Values created with this function.
 */
bool ContractID2JSValue(JSContext* aCx, JSString* aContract,
                        JS::MutableHandleValue aVal);

class JSStackFrameBase {
 public:
  virtual void Clear() = 0;
};

void RegisterJSStackFrame(JS::Realm* aRealm, JSStackFrameBase* aStackFrame);
void UnregisterJSStackFrame(JS::Realm* aRealm, JSStackFrameBase* aStackFrame);
void NukeJSStackFrames(JS::Realm* aRealm);

// Check whether the given jsid is a property name (string or symbol) whose
// value can be gotten cross-origin.  Cross-origin gets always return undefined
// as the value, unless the Xray actually provides a different value.
bool IsCrossOriginWhitelistedProp(JSContext* cx, JS::HandleId id);

// Appends to props the jsids for property names (strings or symbols) whose
// value can be gotten cross-origin.
bool AppendCrossOriginWhitelistedPropNames(JSContext* cx,
                                           JS::MutableHandleIdVector props);
}  // namespace xpc

namespace mozilla {
namespace dom {

/**
 * This is used to prevent UA widget code from directly creating and adopting
 * nodes via the content document, since they should use the special
 * create-and-insert apis instead.
 */
bool IsNotUAWidget(JSContext* cx, JSObject* /* unused */);

/**
 * A test for whether WebIDL methods that should only be visible to
 * chrome, XBL scopes, or UA Widget scopes.
 */
bool IsChromeOrUAWidget(JSContext* cx, JSObject* /* unused */);

/**
 * Same as IsChromeOrUAWidget but can be used in worker threads as well.
 */
bool ThreadSafeIsChromeOrUAWidget(JSContext* cx, JSObject* obj);

}  // namespace dom

/**
 * Fill the given vector with the buildid.
 */
bool GetBuildId(JS::BuildIdCharVector* aBuildID);

}  // namespace mozilla

#endif
