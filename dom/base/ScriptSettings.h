/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for managing the script settings object stack defined in webapps */

#ifndef mozilla_dom_ScriptSettings_h
#define mozilla_dom_ScriptSettings_h

#include "MainThreadUtils.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"

#include "mozilla/Maybe.h"

#include "jsapi.h"

class nsPIDOMWindow;
class nsGlobalWindow;
class nsIScriptContext;

namespace mozilla {
namespace dom {

// For internal use only - use AutoJSAPI instead.
namespace danger {

/**
 * Fundamental cx pushing class. All other cx pushing classes are implemented
 * in terms of this class.
 */
class MOZ_STACK_CLASS AutoCxPusher
{
public:
  explicit AutoCxPusher(JSContext *aCx, bool aAllowNull = false);
  ~AutoCxPusher();

  nsIScriptContext* GetScriptContext() { return mScx; }

  // Returns true if this AutoCxPusher performed the push that is currently at
  // the top of the cx stack.
  bool IsStackTop() const;

private:
  mozilla::Maybe<JSAutoRequest> mAutoRequest;
  mozilla::Maybe<JSAutoCompartment> mAutoCompartment;
  nsCOMPtr<nsIScriptContext> mScx;
  uint32_t mStackDepthAfterPush;
#ifdef DEBUG
  JSContext* mPushedContext;
  unsigned mCompartmentDepthOnEntry;
#endif
};

} /* namespace danger */

/*
 * System-wide setup/teardown routines. Init and Destroy should be invoked
 * once each, at startup and shutdown (respectively).
 */
void InitScriptSettings();
void DestroyScriptSettings();

// This mostly gets the entry global, but doesn't entirely match the spec in
// certain edge cases. It's good enough for some purposes, but not others. If
// you want to call this function, ping bholley and describe your use-case.
nsIGlobalObject* BrokenGetEntryGlobal();

// Note: We don't yet expose GetEntryGlobal, because in order for it to be
// correct, we first need to replace a bunch of explicit cx pushing in the
// browser with AutoEntryScript. But GetIncumbentGlobal is simpler, because it
// can mostly be inferred from the JS stack.
nsIGlobalObject* GetIncumbentGlobal();

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

// This may be used by callers that know that their incumbent global is non-
// null (i.e. they know there have been no System Caller pushes since the
// inner-most script execution).
inline JSObject& IncumbentJSGlobal()
{
  return *GetIncumbentGlobal()->GetGlobalJSObject();
}

class ScriptSettingsStack;
class ScriptSettingsStackEntry {
  friend class ScriptSettingsStack;

public:
  ~ScriptSettingsStackEntry();

  bool NoJSAPI() { return !mGlobalObject; }

protected:
  ScriptSettingsStackEntry(nsIGlobalObject *aGlobal, bool aCandidate);

  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  bool mIsCandidateEntryPoint;

private:
  // This constructor is only for use by AutoNoJSAPI.
  friend class AutoNoJSAPI;
  ScriptSettingsStackEntry();

  ScriptSettingsStackEntry *mOlder;
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
 *
 * Additionally, the following duties are planned, but not yet implemented:
 *
 * * De-poisoning the JSRuntime to allow manipulation of JSAPI. We can't
 *   actually implement this poisoning until all the JSContext pushing in the
 *   system goes through AutoJSAPI (see bug 951991). For now, this de-poisoning
 *   effectively corresponds to having a non-null cx on the stack.
 * * Reporting any exceptions left on the JSRuntime, unless the caller steals
 *   or silences them.
 * * Entering a JSAutoRequest. At present, this is handled by the cx pushing
 *   on the main thread, and by other code on workers. Depending on the order
 *   in which various cleanup lands, this may never be necessary, because
 *   JSAutoRequests may go away.
 *
 * In situations where the consumer expects to run script, AutoEntryScript
 * should be used, which does additional manipulation of the script settings
 * stack. In bug 991758, we'll add hard invariants to SpiderMonkey, such that
 * any attempt to run script without an AutoEntryScript on the stack will
 * fail. This prevents system code from accidentally triggering script
 * execution at inopportune moments via surreptitious getters and proxies.
 */
class MOZ_STACK_CLASS AutoJSAPI {
public:
  // Trivial constructor. One of the Init functions must be called before
  // accessing the JSContext through cx().
  AutoJSAPI();

  // This uses the SafeJSContext (or worker equivalent), and enters a null
  // compartment, so that the consumer is forced to select a compartment to
  // enter before manipulating objects.
  void Init();

  // This uses the SafeJSContext (or worker equivalent), and enters the
  // compartment of aGlobalObject.
  // If aGlobalObject or its associated JS global are null then it returns
  // false and use of cx() will cause an assertion.
  bool Init(nsIGlobalObject* aGlobalObject);

  // Unsurprisingly, this uses aCx and enters the compartment of aGlobalObject.
  // If aGlobalObject or its associated JS global are null then it returns
  // false and use of cx() will cause an assertion.
  // If aCx is null it will cause an assertion.
  bool Init(nsIGlobalObject* aGlobalObject, JSContext* aCx);

  // This may only be used on the main thread.
  // This attempts to use the JSContext associated with aGlobalObject, otherwise
  // it uses the SafeJSContext. It then enters the compartment of aGlobalObject.
  // This means that existing error reporting mechanisms that use the JSContext
  // to find the JSErrorReporter should still work as before.
  // We should be able to remove this around bug 981198.
  // If aGlobalObject or its associated JS global are null then it returns
  // false and use of cx() will cause an assertion.
  bool InitWithLegacyErrorReporting(nsIGlobalObject* aGlobalObject);

  // Convenience functions to take an nsPIDOMWindow* or nsGlobalWindow*,
  // when it is more easily available than an nsIGlobalObject.
  bool Init(nsPIDOMWindow* aWindow);
  bool Init(nsPIDOMWindow* aWindow, JSContext* aCx);

  bool Init(nsGlobalWindow* aWindow);
  bool Init(nsGlobalWindow* aWindow, JSContext* aCx);

  bool InitWithLegacyErrorReporting(nsPIDOMWindow* aWindow);
  bool InitWithLegacyErrorReporting(nsGlobalWindow* aWindow);

  JSContext* cx() const {
    MOZ_ASSERT(mCx, "Must call Init before using an AutoJSAPI");
    MOZ_ASSERT_IF(NS_IsMainThread(), CxPusherIsStackTop());
    return mCx;
  }

  bool CxPusherIsStackTop() const { return mCxPusher->IsStackTop(); }

protected:
  // Protected constructor, allowing subclasses to specify a particular cx to
  // be used. This constructor initialises the AutoJSAPI, so Init must NOT be
  // called on subclasses that use this.
  // If aGlobalObject, its associated JS global or aCx are null this will cause
  // an assertion, as will setting aIsMainThread incorrectly.
  AutoJSAPI(nsIGlobalObject* aGlobalObject, bool aIsMainThread, JSContext* aCx);

private:
  mozilla::Maybe<danger::AutoCxPusher> mCxPusher;
  mozilla::Maybe<JSAutoNullableCompartment> mAutoNullableCompartment;
  JSContext *mCx;

  void InitInternal(JSObject* aGlobal, JSContext* aCx, bool aIsMainThread);
};

/*
 * A class that represents a new script entry point.
 */
class AutoEntryScript : public AutoJSAPI,
                        protected ScriptSettingsStackEntry {
public:
  explicit AutoEntryScript(nsIGlobalObject* aGlobalObject,
                  bool aIsMainThread = NS_IsMainThread(),
                  // Note: aCx is mandatory off-main-thread.
                  JSContext* aCx = nullptr);

  ~AutoEntryScript();

  void SetWebIDLCallerPrincipal(nsIPrincipal *aPrincipal) {
    mWebIDLCallerPrincipal = aPrincipal;
  }

private:
  // It's safe to make this a weak pointer, since it's the subject principal
  // when we go on the stack, so can't go away until after we're gone.  In
  // particular, this is only used from the CallSetup constructor, and only in
  // the aIsJSImplementedWebIDL case.  And in that case, the subject principal
  // is the principal of the callee function that is part of the CallArgs just a
  // bit up the stack, and which will outlive us.  So we know the principal
  // can't go away until then either.
  nsIPrincipal* mWebIDLCallerPrincipal;
  friend nsIPrincipal* GetWebIDLCallerPrincipal();
};

/*
 * A class that can be used to force a particular incumbent script on the stack.
 */
class AutoIncumbentScript : protected ScriptSettingsStackEntry {
public:
  explicit AutoIncumbentScript(nsIGlobalObject* aGlobalObject);
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
  explicit AutoNoJSAPI(bool aIsMainThread = NS_IsMainThread());
private:
  mozilla::Maybe<danger::AutoCxPusher> mCxPusher;
};

} // namespace dom

/**
 * Use AutoJSContext when you need a JS context on the stack but don't have one
 * passed as a parameter. AutoJSContext will take care of finding the most
 * appropriate JS context and release it when leaving the stack.
 */
class MOZ_STACK_CLASS AutoJSContext {
public:
  explicit AutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

protected:
  explicit AutoJSContext(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  // We need this Init() method because we can't use delegating constructor for
  // the moment. It is a C++11 feature and we do not require C++11 to be
  // supported to be able to compile Gecko.
  void Init(bool aSafe MOZ_GUARD_OBJECT_NOTIFIER_PARAM);

  JSContext* mCx;
  dom::AutoJSAPI mJSAPI;
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * Use ThreadsafeAutoJSContext when you want an AutoJSContext but might be
 * running on a worker thread.
 */
class MOZ_STACK_CLASS ThreadsafeAutoJSContext {
public:
  explicit ThreadsafeAutoJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

private:
  JSContext* mCx; // Used on workers.  Null means mainthread.
  Maybe<JSAutoRequest> mRequest; // Used on workers.
  Maybe<AutoJSContext> mAutoJSContext; // Used on main thread.
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

/**
 * AutoSafeJSContext is similar to AutoJSContext but will only return the safe
 * JS context. That means it will never call nsContentUtils::GetCurrentJSContext().
 *
 * Note - This is deprecated. Please use AutoJSAPI instead.
 */
class MOZ_STACK_CLASS AutoSafeJSContext : public AutoJSContext {
public:
  explicit AutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
private:
  JSAutoCompartment mAc;
};

/**
 * Like AutoSafeJSContext but can be used safely on worker threads.
 */
class MOZ_STACK_CLASS ThreadsafeAutoSafeJSContext {
public:
  explicit ThreadsafeAutoSafeJSContext(MOZ_GUARD_OBJECT_NOTIFIER_ONLY_PARAM);
  operator JSContext*() const;

private:
  JSContext* mCx; // Used on workers.  Null means mainthread.
  Maybe<JSAutoRequest> mRequest; // Used on workers.
  Maybe<AutoSafeJSContext> mAutoSafeJSContext; // Used on main thread.
  MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};


} // namespace mozilla

#endif // mozilla_dom_ScriptSettings_h
