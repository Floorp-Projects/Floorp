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

  bool IsSystemSingleton() { return this == &SystemSingleton; }
  static ScriptSettingsStackEntry SystemSingleton;

private:
  ScriptSettingsStackEntry() : mGlobalObject(nullptr)
                             , mIsCandidateEntryPoint(true)
  {}
};

/*
 * A class that represents a new script entry point.
 */
class AutoEntryScript : protected ScriptSettingsStackEntry {
public:
  AutoEntryScript(nsIGlobalObject* aGlobalObject,
                  bool aIsMainThread = NS_IsMainThread(),
                  // Note: aCx is mandatory off-main-thread.
                  JSContext* aCx = nullptr);
  ~AutoEntryScript();

  void SetWebIDLCallerPrincipal(nsIPrincipal *aPrincipal) {
    mWebIDLCallerPrincipal = aPrincipal;
  }

  JSContext* cx() const { return mCx; }

private:
  dom::ScriptSettingsStack& mStack;
  nsCOMPtr<nsIPrincipal> mWebIDLCallerPrincipal;
  JSContext *mCx;
  mozilla::Maybe<AutoCxPusher> mCxPusher;
  mozilla::Maybe<JSAutoCompartment> mAc; // This can de-Maybe-fy when mCxPusher
                                         // goes away.
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
 * A class used for C++ to indicate that existing entry and incumbent scripts
 * should not apply to anything in scope, and that callees should act as if
 * they were invoked "from C++".
 */
class AutoSystemCaller {
public:
  AutoSystemCaller(bool aIsMainThread = NS_IsMainThread());
  ~AutoSystemCaller();
private:
  dom::ScriptSettingsStack& mStack;
  mozilla::Maybe<AutoCxPusher> mCxPusher;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ScriptSettings_h
