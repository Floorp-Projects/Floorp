/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for managing the script settings object stack defined in webapps */

#ifndef mozilla_dom_ScriptSettings_h
#define mozilla_dom_ScriptSettings_h

#include "MainThreadUtils.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "xpcpublic.h"

#include "mozilla/Maybe.h"

#include "jsapi.h"
#include "js/Debug.h"
#include "js/Warnings.h"  // JS::WarningReporter

class nsPIDOMWindowInner;
class nsGlobalWindowInner;
class nsIScriptContext;
class nsIDocShell;

namespace mozilla {
namespace dom {

class Document;

/*
 * System-wide setup/teardown routines. Init and Destroy should be invoked
 * once each, at startup and shutdown (respectively).
 */
void InitScriptSettings();
void DestroyScriptSettings();
bool ScriptSettingsInitialized();

/*
 * Static helpers in ScriptSettings which track the number of listeners
 * of Javascript RunToCompletion events.  These should be used by the code in
 * nsDocShell::SetRecordProfileTimelineMarkers to indicate to script
 * settings that script run-to-completion needs to be monitored.
 * SHOULD BE CALLED ONLY BY MAIN THREAD.
 */
void UseEntryScriptProfiling();
void UnuseEntryScriptProfiling();

// To implement a web-compatible browser, it is often necessary to obtain the
// global object that is "associated" with the currently-running code. This
// process is made more complicated by the fact that, historically, different
// algorithms have operated with different definitions of the "associated"
// global.
//
// HTML5 formalizes this into two concepts: the "incumbent global" and the
// "entry global". The incumbent global corresponds to the global of the
// current script being executed, whereas the entry global corresponds to the
// global of the script where the current JS execution began.
//
// There is also a potentially-distinct third global that is determined by the
// current compartment. This roughly corresponds with the notion of Realms in
// ECMAScript.
//
// Suppose some event triggers an event listener in window |A|, which invokes a
// scripted function in window |B|, which invokes the |window.location.href|
// setter in window |C|. The entry global would be |A|, the incumbent global
// would be |B|, and the current compartment would be that of |C|.
//
// In general, it's best to use to use the most-closely-associated global
// unless the spec says to do otherwise. In 95% of the cases, the global of
// the current compartment (GetCurrentGlobal()) is the right thing. For
// example, WebIDL constructors (new C.XMLHttpRequest()) are initialized with
// the global of the current compartment (i.e. |C|).
//
// The incumbent global is very similar, but differs in a few edge cases. For
// example, if window |B| does |C.location.href = "..."|, the incumbent global
// used for the navigation algorithm is B, because no script from |C| was ever
// run.
//
// The entry global is used for various things like computing base URIs, mostly
// for historical reasons.
//
// Note that all of these functions return bonafide global objects. This means
// that, for Windows, they always return the inner.

// Returns the global associated with the top-most Candidate Entry Point on
// the Script Settings Stack. See the HTML spec. This may be null.
nsIGlobalObject* GetEntryGlobal();

// If the entry global is a window, returns its extant document. Otherwise,
// returns null.
Document* GetEntryDocument();

// Returns the global associated with the top-most entry of the the Script
// Settings Stack. See the HTML spec. This may be null.
nsIGlobalObject* GetIncumbentGlobal();

// Returns the global associated with the current compartment. This may be null.
nsIGlobalObject* GetCurrentGlobal();

// JS-implemented WebIDL presents an interesting situation with respect to the
// subject principal. A regular C++-implemented API can simply examine the
// compartment of the most-recently-executed script, and use that to infer the
// responsible party. However, JS-implemented APIs are run with system
// principal, and thus clobber the subject principal of the script that
// invoked the API. So we have to do some extra work to keep track of this
// information.
//
// We therefore implement the following behavior:
// * Each Script Settings Object has an optional WebIDL Caller Principal field.
//   This defaults to null.
// * When we push an Entry Point in preparation to run a JS-implemented WebIDL
//   callback, we grab the subject principal at the time of invocation, and
//   store that as the WebIDL Caller Principal.
// * When non-null, callers can query this principal from script via an API on
//   Components.utils.
nsIPrincipal* GetWebIDLCallerPrincipal();

// Returns whether JSAPI is active right now.  If it is not, working with a
// JSContext you grab from somewhere random is not OK and you should be doing
// AutoJSAPI or AutoEntryScript to get yourself a properly set up JSContext.
bool IsJSAPIActive();

namespace danger {

// Get the JSContext for this thread.  This is in the "danger" namespace because
// we generally want people using AutoJSAPI instead, unless they really know
// what they're doing.
JSContext* GetJSContext();

}  // namespace danger

JS::RootingContext* RootingCx();

class ScriptSettingsStack;
class ScriptSettingsStackEntry {
  friend class ScriptSettingsStack;

 public:
  ~ScriptSettingsStackEntry();

  bool NoJSAPI() const { return mType == eNoJSAPI; }
  bool IsEntryCandidate() const {
    return mType == eEntryScript || mType == eNoJSAPI;
  }
  bool IsIncumbentCandidate() { return mType != eJSAPI; }
  bool IsIncumbentScript() { return mType == eIncumbentScript; }

 protected:
  enum Type { eEntryScript, eIncumbentScript, eJSAPI, eNoJSAPI };

  ScriptSettingsStackEntry(nsIGlobalObject* aGlobal, Type aEntryType);

  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  Type mType;

 private:
  ScriptSettingsStackEntry* mOlder;
};

/*
 * For any interaction with JSAPI, an AutoJSAPI (or one of its subclasses)
 * must be on the stack.
 *
 * This base class should be instantiated as-is when the caller wants to use
 * JSAPI but doesn't expect to run script. The caller must then call one of its
 * Init functions before being able to access the JSContext through cx().
 * Its current duties are as-follows (see individual Init comments for details):
 *
 * * Grabbing an appropriate JSContext, and, on the main thread, pushing it onto
 *   the JSContext stack.
 * * Entering an initial (possibly null) compartment, to ensure that the
 *   previously entered compartment for that JSContext is not used by mistake.
 * * Reporting any exceptions left on the JSRuntime, unless the caller steals
 *   or silences them.
 *
 * Additionally, the following duties are planned, but not yet implemented:
 *
 * * De-poisoning the JSRuntime to allow manipulation of JSAPI. This requires
 *   implementing the poisoning first.  For now, this de-poisoning
 *   effectively corresponds to having a non-null cx on the stack.
 *
 * In situations where the consumer expects to run script, AutoEntryScript
 * should be used, which does additional manipulation of the script settings
 * stack. In bug 991758, we'll add hard invariants to SpiderMonkey, such that
 * any attempt to run script without an AutoEntryScript on the stack will
 * fail. This prevents system code from accidentally triggering script
 * execution at inopportune moments via surreptitious getters and proxies.
 */
class MOZ_STACK_CLASS AutoJSAPI : protected ScriptSettingsStackEntry {
 public:
  // Trivial constructor. One of the Init functions must be called before
  // accessing the JSContext through cx().
  AutoJSAPI();

  ~AutoJSAPI();

  // This uses the SafeJSContext (or worker equivalent), and enters a null
  // compartment, so that the consumer is forced to select a compartment to
  // enter before manipulating objects.
  //
  // This variant will ensure that any errors reported by this AutoJSAPI as it
  // comes off the stack will not fire error events or be associated with any
  // particular web-visible global.
  void Init();

  // This uses the SafeJSContext (or worker equivalent), and enters the
  // compartment of aGlobalObject.
  // If aGlobalObject or its associated JS global are null then it returns
  // false and use of cx() will cause an assertion.
  //
  // If aGlobalObject represents a web-visible global, errors reported by this
  // AutoJSAPI as it comes off the stack will fire the relevant error events and
  // show up in the corresponding web console.
  //
  // Successfully initializing the AutoJSAPI will ensure that it enters the
  // Realm of aGlobalObject's JSObject and exposes that JSObject to active JS.
  MOZ_MUST_USE bool Init(nsIGlobalObject* aGlobalObject);

  // This is a helper that grabs the native global associated with aObject and
  // invokes the above Init() with that. aObject must not be a cross-compartment
  // wrapper: CCWs are not associated with a single global.
  MOZ_MUST_USE bool Init(JSObject* aObject);

  // Unsurprisingly, this uses aCx and enters the compartment of aGlobalObject.
  // If aGlobalObject or its associated JS global are null then it returns
  // false and use of cx() will cause an assertion.
  // If aCx is null it will cause an assertion.
  //
  // If aGlobalObject represents a web-visible global, errors reported by this
  // AutoJSAPI as it comes off the stack will fire the relevant error events and
  // show up in the corresponding web console.
  MOZ_MUST_USE bool Init(nsIGlobalObject* aGlobalObject, JSContext* aCx);

  // Convenience functions to take an nsPIDOMWindowInner or nsGlobalWindowInner,
  // when it is more easily available than an nsIGlobalObject.
  MOZ_MUST_USE bool Init(nsPIDOMWindowInner* aWindow);
  MOZ_MUST_USE bool Init(nsPIDOMWindowInner* aWindow, JSContext* aCx);

  MOZ_MUST_USE bool Init(nsGlobalWindowInner* aWindow);
  MOZ_MUST_USE bool Init(nsGlobalWindowInner* aWindow, JSContext* aCx);

  JSContext* cx() const {
    MOZ_ASSERT(mCx, "Must call Init before using an AutoJSAPI");
    MOZ_ASSERT(IsStackTop());
    return mCx;
  }

#ifdef DEBUG
  bool IsStackTop() const;
#endif

  // If HasException, report it.  Otherwise, a no-op.
  void ReportException();

  bool HasException() const {
    MOZ_ASSERT(IsStackTop());
    return JS_IsExceptionPending(cx());
  };

  // Transfers ownership of the current exception from the JS engine to the
  // caller. Callers must ensure that HasException() is true, and that cx()
  // is in a non-null compartment.
  //
  // Note that this fails if and only if we OOM while wrapping the exception
  // into the current compartment.
  MOZ_MUST_USE bool StealException(JS::MutableHandle<JS::Value> aVal);

  // As for StealException(), but put the saved frames for any stack trace
  // associated with the point the exception was thrown into aStack.
  // aVal will be in the current compartment, but aStack might not be.
  MOZ_MUST_USE bool StealExceptionAndStack(JS::MutableHandle<JS::Value> aVal,
                                           JS::MutableHandle<JSObject*> aStack);

  // Peek the current exception from the JS engine, without stealing it.
  // Callers must ensure that HasException() is true, and that cx() is in a
  // non-null compartment.
  //
  // Note that this fails if and only if we OOM while wrapping the exception
  // into the current compartment.
  MOZ_MUST_USE bool PeekException(JS::MutableHandle<JS::Value> aVal);

  void ClearException() {
    MOZ_ASSERT(IsStackTop());
    JS_ClearPendingException(cx());
  }

 protected:
  // Protected constructor for subclasses.  This constructor initialises the
  // AutoJSAPI, so Init must NOT be called on subclasses that use this.
  AutoJSAPI(nsIGlobalObject* aGlobalObject, bool aIsMainThread, Type aType);

  mozilla::Maybe<JSAutoNullableRealm> mAutoNullableRealm;
  JSContext* mCx;

  // Whether we're mainthread or not; set when we're initialized.
  bool mIsMainThread;
  Maybe<JS::WarningReporter> mOldWarningReporter;

 private:
  void InitInternal(nsIGlobalObject* aGlobalObject, JSObject* aGlobal,
                    JSContext* aCx, bool aIsMainThread);

  AutoJSAPI(const AutoJSAPI&) = delete;
  AutoJSAPI& operator=(const AutoJSAPI&) = delete;
};

/*
 * A class that represents a new script entry point.
 *
 * |aReason| should be a statically-allocated C string naming the reason we're
 * invoking JavaScript code: "setTimeout", "event", and so on. The devtools use
 * these strings to label JS execution in timeline and profiling displays.
 *
 */
class MOZ_STACK_CLASS AutoEntryScript : public AutoJSAPI {
 public:
  // Constructing the AutoEntryScript will ensure that it enters the
  // Realm of aGlobalObject's JSObject and exposes that JSObject to active JS.
  AutoEntryScript(nsIGlobalObject* aGlobalObject, const char* aReason,
                  bool aIsMainThread = NS_IsMainThread());

  // aObject can be any object from the relevant global. It must not be a
  // cross-compartment wrapper because CCWs are not associated with a single
  // global.
  //
  // Constructing the AutoEntryScript will ensure that it enters the
  // Realm of aObject JSObject and exposes aObject's global to active JS.
  AutoEntryScript(JSObject* aObject, const char* aReason,
                  bool aIsMainThread = NS_IsMainThread());

  ~AutoEntryScript();

  void SetWebIDLCallerPrincipal(nsIPrincipal* aPrincipal) {
    mWebIDLCallerPrincipal = aPrincipal;
  }

 private:
  // A subclass of AutoEntryMonitor that notifies the docshell.
  class DocshellEntryMonitor final : public JS::dbg::AutoEntryMonitor {
   public:
    DocshellEntryMonitor(JSContext* aCx, const char* aReason);

    // Please note that |aAsyncCause| here is owned by the caller, and its
    // lifetime must outlive the lifetime of the DocshellEntryMonitor object.
    // In practice, |aAsyncCause| is identical to |aReason| passed into
    // the AutoEntryScript constructor, so the lifetime requirements are
    // trivially satisfied by |aReason| being a statically allocated string.
    void Entry(JSContext* aCx, JSFunction* aFunction,
               JS::Handle<JS::Value> aAsyncStack,
               const char* aAsyncCause) override {
      Entry(aCx, aFunction, nullptr, aAsyncStack, aAsyncCause);
    }

    void Entry(JSContext* aCx, JSScript* aScript,
               JS::Handle<JS::Value> aAsyncStack,
               const char* aAsyncCause) override {
      Entry(aCx, nullptr, aScript, aAsyncStack, aAsyncCause);
    }

    void Exit(JSContext* aCx) override;

   private:
    void Entry(JSContext* aCx, JSFunction* aFunction, JSScript* aScript,
               JS::Handle<JS::Value> aAsyncStack, const char* aAsyncCause);

    const char* mReason;
  };

  // It's safe to make this a weak pointer, since it's the subject principal
  // when we go on the stack, so can't go away until after we're gone.  In
  // particular, this is only used from the CallSetup constructor, and only in
  // the aIsJSImplementedWebIDL case.  And in that case, the subject principal
  // is the principal of the callee function that is part of the CallArgs just a
  // bit up the stack, and which will outlive us.  So we know the principal
  // can't go away until then either.
  nsIPrincipal* MOZ_NON_OWNING_REF mWebIDLCallerPrincipal;
  friend nsIPrincipal* GetWebIDLCallerPrincipal();

  Maybe<DocshellEntryMonitor> mDocShellEntryMonitor;
  Maybe<xpc::AutoScriptActivity> mScriptActivity;
  JS::AutoHideScriptedCaller mCallerOverride;
#ifdef MOZ_GECKO_PROFILER
  AutoProfilerLabel mAutoProfilerLabel;
#endif
};

/*
 * A class that can be used to force a particular incumbent script on the stack.
 */
class AutoIncumbentScript : protected ScriptSettingsStackEntry {
 public:
  explicit AutoIncumbentScript(nsIGlobalObject* aGlobalObject);
  ~AutoIncumbentScript();

 private:
  JS::AutoHideScriptedCaller mCallerOverride;
};

/*
 * A class to put the JS engine in an unusable state. The subject principal
 * will become System, the information on the script settings stack is
 * rendered inaccessible, and JSAPI may not be manipulated until the class is
 * either popped or an AutoJSAPI instance is subsequently pushed.
 *
 * This class may not be instantiated if an exception is pending.
 */
class AutoNoJSAPI : protected ScriptSettingsStackEntry {
 public:
  explicit AutoNoJSAPI();
  ~AutoNoJSAPI();
};

}  // namespace dom

/**
 * Use AutoJSContext when you need a JS context on the stack but don't have one
 * passed as a parameter. AutoJSContext will take care of finding the most
 * appropriate JS context and release it when leaving the stack.
 */
class MOZ_RAII AutoJSContext {
 public:
  explicit AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

 protected:
  JSContext* mCx;
  dom::AutoJSAPI mJSAPI;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * AutoSafeJSContext is similar to AutoJSContext but will only return the safe
 * JS context. That means it will never call
 * nsContentUtils::GetCurrentJSContext().
 *
 * Note - This is deprecated. Please use AutoJSAPI instead.
 */
class MOZ_RAII AutoSafeJSContext : public dom::AutoJSAPI {
 public:
  explicit AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const { return cx(); }

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * Use AutoSlowOperation when native side calls many JS callbacks in a row
 * and slow script dialog should be activated if too much time is spent going
 * through those callbacks.
 * AutoSlowOperation puts an AutoScriptActivity on the stack so that we don't
 * continue to reset the watchdog. CheckForInterrupt can then be used to check
 * whether JS execution should be interrupted.
 * This class (including CheckForInterrupt) is a no-op when used off the main
 * thread.
 */
class MOZ_RAII AutoSlowOperation {
 public:
  explicit AutoSlowOperation(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  void CheckForInterrupt();

 private:
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
  bool mIsMainThread;
  Maybe<xpc::AutoScriptActivity> mScriptActivity;
};

/**
 * A class to disable interrupt callback temporary.
 */
class MOZ_RAII AutoDisableJSInterruptCallback {
 public:
  explicit AutoDisableJSInterruptCallback(JSContext* aCx)
      : mCx(aCx), mOld(JS_DisableInterruptCallback(aCx)) {}

  ~AutoDisableJSInterruptCallback() { JS_ResetInterruptCallback(mCx, mOld); }

 private:
  JSContext* mCx;
  bool mOld;
};

}  // namespace mozilla

#endif  // mozilla_dom_ScriptSettings_h
