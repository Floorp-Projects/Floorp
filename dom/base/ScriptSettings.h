/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for managing the script settings object stack defined in webapps */

#ifndef mozilla_dom_ScriptSettings_h
#define mozilla_dom_ScriptSettings_h

#include "nsCxPusher.h"
#include "MainThreadUtils.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"

#include "mozilla/Maybe.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

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
struct ScriptSettingsStackEntry {
  nsCOMPtr<nsIGlobalObject> mGlobalObject;
  bool mIsCandidateEntryPoint;

  ScriptSettingsStackEntry(nsIGlobalObject *aGlobal, bool aCandidate)
    : mGlobalObject(aGlobal)
    , mIsCandidateEntryPoint(aCandidate)
  {
    MOZ_ASSERT(mGlobalObject);
    MOZ_ASSERT(mGlobalObject->GetGlobalJSObject(),
               "Must have an actual JS global for the duration on the stack");
    MOZ_ASSERT(JS_IsGlobalObject(mGlobalObject->GetGlobalJSObject()),
               "No outer windows allowed");
  }

  ~ScriptSettingsStackEntry() {
    // We must have an actual JS global for the entire time this is on the stack.
    MOZ_ASSERT_IF(mGlobalObject, mGlobalObject->GetGlobalJSObject());
  }

  bool NoJSAPI() { return this == &NoJSAPISingleton; }
  static ScriptSettingsStackEntry NoJSAPISingleton;

private:
  ScriptSettingsStackEntry() : mGlobalObject(nullptr)
                             , mIsCandidateEntryPoint(true)
  {}
};

/*
 * For any interaction with JSAPI, an AutoJSAPI (or one of its subclasses)
 * must be on the stack.
 *
 * This base class should be instantiated as-is when the caller wants to use
 * JSAPI but doesn't expect to run script. Its current duties are as-follows:
 *
 * * Grabbing an appropriate JSContext, and, on the main thread, pushing it onto
 *   the JSContext stack.
 * * Entering a null compartment, so that the consumer is forced to select a
 *   compartment to enter before manipulating objects.
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
class AutoJSAPI {
public:
  // Public constructor for use when the base class is constructed as-is. It
  // uses the SafeJSContext (or worker equivalent), and enters a null
  // compartment.
  AutoJSAPI();
  JSContext* cx() const { return mCx; }

  bool CxPusherIsStackTop() { return mCxPusher.ref().IsStackTop(); }

protected:
  // Protected constructor, allowing subclasses to specify a particular cx to
  // be used.
  AutoJSAPI(JSContext *aCx, bool aIsMainThread, bool aSkipNullAC = false);

private:
  mozilla::Maybe<AutoCxPusher> mCxPusher;
  mozilla::Maybe<JSAutoNullCompartment> mNullAc;
  JSContext *mCx;
};

// Note - the ideal way to implement this is with an accessor on AutoJSAPI
// that lets us select the error reporting target. But at present,
// implementing it that way would require us to destroy and reconstruct
// mCxPusher, which is pretty wasteful. So we do this for now, since it should
// be pretty easy to switch things over later.
//
// This should only be used on the main thread.
class AutoJSAPIWithErrorsReportedToWindow : public AutoJSAPI {
  public:
    AutoJSAPIWithErrorsReportedToWindow(nsIScriptContext* aScx);
    // Equivalent to AutoJSAPI if aGlobal is not a Window.
    AutoJSAPIWithErrorsReportedToWindow(nsIGlobalObject* aGlobalObject);
};

/*
 * A class that represents a new script entry point.
 */
class AutoEntryScript : public AutoJSAPI,
                        protected ScriptSettingsStackEntry {
public:
  AutoEntryScript(nsIGlobalObject* aGlobalObject,
                  bool aIsMainThread = NS_IsMainThread(),
                  // Note: aCx is mandatory off-main-thread.
                  JSContext* aCx = nullptr);
  ~AutoEntryScript();

  void SetWebIDLCallerPrincipal(nsIPrincipal *aPrincipal) {
    mWebIDLCallerPrincipal = aPrincipal;
  }

private:
  JSAutoCompartment mAc;
  dom::ScriptSettingsStack& mStack;
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
  AutoIncumbentScript(nsIGlobalObject* aGlobalObject);
  ~AutoIncumbentScript();
private:
  dom::ScriptSettingsStack& mStack;
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
class AutoNoJSAPI {
public:
  AutoNoJSAPI(bool aIsMainThread = NS_IsMainThread());
  ~AutoNoJSAPI();
private:
  dom::ScriptSettingsStack& mStack;
  mozilla::Maybe<AutoCxPusher> mCxPusher;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScriptSettings_h
