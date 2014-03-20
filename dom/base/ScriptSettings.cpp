/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/ThreadLocal.h"
#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "xpcpublic.h"
#include "nsIGlobalObject.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"
#include "nsTArray.h"
#include "nsJSUtils.h"

namespace mozilla {
namespace dom {

class ScriptSettingsStack;
static mozilla::ThreadLocal<ScriptSettingsStack*> sScriptSettingsTLS;

ScriptSettingsStackEntry ScriptSettingsStackEntry::SystemSingleton;

class ScriptSettingsStack {
public:
  static ScriptSettingsStack& Ref() {
    return *sScriptSettingsTLS.get();
  }
  ScriptSettingsStack() {};

  void Push(ScriptSettingsStackEntry* aSettings) {
    // The bottom-most entry must always be a candidate entry point.
    MOZ_ASSERT_IF(mStack.Length() == 0 || mStack.LastElement()->IsSystemSingleton(),
                  aSettings->mIsCandidateEntryPoint);
    mStack.AppendElement(aSettings);
  }

  void PushSystem() {
    mStack.AppendElement(&ScriptSettingsStackEntry::SystemSingleton);
  }

  void Pop() {
    MOZ_ASSERT(mStack.Length() > 0);
    mStack.RemoveElementAt(mStack.Length() - 1);
  }

  ScriptSettingsStackEntry* Incumbent() {
    if (!mStack.Length()) {
      return nullptr;
    }
    return mStack.LastElement();
  }

  nsIGlobalObject* IncumbentGlobal() {
    ScriptSettingsStackEntry *entry = Incumbent();
    return entry ? entry->mGlobalObject : nullptr;
  }

  ScriptSettingsStackEntry* EntryPoint() {
    if (!mStack.Length())
      return nullptr;
    for (int i = mStack.Length() - 1; i >= 0; --i) {
      if (mStack[i]->mIsCandidateEntryPoint) {
        return mStack[i];
      }
    }
    MOZ_ASSUME_UNREACHABLE("Non-empty stack should always have an entry point");
  }

  nsIGlobalObject* EntryGlobal() {
    ScriptSettingsStackEntry *entry = EntryPoint();
    return entry ? entry->mGlobalObject : nullptr;
  }

private:
  // These pointers are caller-owned.
  nsTArray<ScriptSettingsStackEntry*> mStack;
};

void
InitScriptSettings()
{
  if (!sScriptSettingsTLS.initialized()) {
    bool success = sScriptSettingsTLS.init();
    if (!success) {
      MOZ_CRASH();
    }
  }

  ScriptSettingsStack* ptr = new ScriptSettingsStack();
  sScriptSettingsTLS.set(ptr);
}

void DestroyScriptSettings()
{
  ScriptSettingsStack* ptr = sScriptSettingsTLS.get();
  MOZ_ASSERT(ptr);
  sScriptSettingsTLS.set(nullptr);
  delete ptr;
}

// This mostly gets the entry global, but doesn't entirely match the spec in
// certain edge cases. It's good enough for some purposes, but not others. If
// you want to call this function, ping bholley and describe your use-case.
nsIGlobalObject*
BrokenGetEntryGlobal()
{
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry global on the stack.
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::Ref().EntryGlobal() == nullptr);
    return nullptr;
  }

  return nsJSUtils::GetDynamicScriptGlobal(cx);
}

// Note: When we're ready to expose it, GetEntryGlobal will look similar to
// GetIncumbentGlobal below.

nsIGlobalObject*
GetIncumbentGlobal()
{
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry global on the stack, and therefore no incumbent
  // global either.
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::Ref().EntryGlobal() == nullptr);
    return nullptr;
  }

  // See what the JS engine has to say. If we've got a scripted caller
  // override in place, the JS engine will lie to us and pretend that
  // there's nothing on the JS stack, which will cause us to check the
  // incumbent script stack below.
  if (JSObject *global = JS::GetScriptedCallerGlobal(cx)) {
    return xpc::GetNativeForGlobal(global);
  }

  // Ok, nothing from the JS engine. Let's use whatever's on the
  // explicit stack.
  return ScriptSettingsStack::Ref().IncumbentGlobal();
}

nsIPrincipal*
GetWebIDLCallerPrincipal()
{
  MOZ_ASSERT(NS_IsMainThread());
  ScriptSettingsStackEntry *entry = ScriptSettingsStack::Ref().EntryPoint();

  // If we have an entry point that is not the system singleton, we know it
  // must be an AutoEntryScript.
  if (!entry || entry->IsSystemSingleton()) {
    return nullptr;
  }
  AutoEntryScript* aes = static_cast<AutoEntryScript*>(entry);

  // We can't yet rely on the Script Settings Stack to properly determine the
  // entry script, because there are still lots of places in the tree where we
  // don't yet use an AutoEntryScript (bug 951991 tracks this work). In the
  // mean time though, we can make some observations to hack around the
  // problem:
  //
  // (1) All calls into JS-implemented WebIDL go through CallSetup, which goes
  //     through AutoEntryScript.
  // (2) The top candidate entry point in the Script Settings Stack is the
  //     entry point if and only if no other JSContexts have been pushed on
  //     top of the push made by that entry's AutoEntryScript.
  //
  // Because of (1), all of the cases where we might return a non-null
  // WebIDL Caller are guaranteed to have put an entry on the Script Settings
  // Stack, so we can restrict our search to that. Moreover, (2) gives us a
  // criterion to determine whether an entry in the Script Setting Stack means
  // that we should return a non-null WebIDL Caller.
  //
  // Once we fix bug 951991, this can all be simplified.
  if (!aes->mCxPusher.ref().IsStackTop()) {
    return nullptr;
  }

  return aes->mWebIDLCallerPrincipal;
}

static JSContext*
FindJSContext(nsIGlobalObject* aGlobalObject)
{
  MOZ_ASSERT(NS_IsMainThread());
  JSContext *cx = nullptr;
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobalObject);
  if (sgo && sgo->GetScriptContext()) {
    cx = sgo->GetScriptContext()->GetNativeContext();
  }
  if (!cx) {
    cx = nsContentUtils::GetSafeJSContext();
  }
  return cx;
}

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 bool aIsMainThread,
                                 JSContext* aCx)
  : ScriptSettingsStackEntry(aGlobalObject, /* aCandidate = */ true)
  , mStack(ScriptSettingsStack::Ref())
  , mCx(aCx)
{
  MOZ_ASSERT(aGlobalObject);
  MOZ_ASSERT_IF(!mCx, aIsMainThread); // cx is mandatory off-main-thread.
  MOZ_ASSERT_IF(mCx && aIsMainThread, mCx == FindJSContext(aGlobalObject));
  if (!mCx) {
    // If the caller didn't provide a cx, hunt one down. This isn't exactly
    // fast, but the callers that care about performance can pass an explicit
    // cx for now. Eventually, the whole cx pushing thing will go away
    // entirely.
    mCx = FindJSContext(aGlobalObject);
    MOZ_ASSERT(mCx);
  }
  if (aIsMainThread) {
    mCxPusher.construct(mCx);
  }
  mAc.construct(mCx, aGlobalObject->GetGlobalJSObject());
  mStack.Push(this);
}

AutoEntryScript::~AutoEntryScript()
{
  MOZ_ASSERT(mStack.Incumbent() == this);
  mStack.Pop();
}

AutoIncumbentScript::AutoIncumbentScript(nsIGlobalObject* aGlobalObject)
  : ScriptSettingsStackEntry(aGlobalObject, /* aCandidate = */ false)
  , mStack(ScriptSettingsStack::Ref())
  , mCallerOverride(nsContentUtils::GetCurrentJSContextForThread())
{
  mStack.Push(this);
}

AutoIncumbentScript::~AutoIncumbentScript()
{
  MOZ_ASSERT(mStack.Incumbent() == this);
  mStack.Pop();
}

AutoSystemCaller::AutoSystemCaller(bool aIsMainThread)
  : mStack(ScriptSettingsStack::Ref())
{
  if (aIsMainThread) {
    mCxPusher.construct(static_cast<JSContext*>(nullptr),
                        /* aAllowNull = */ true);
  }
  mStack.PushSystem();
}

AutoSystemCaller::~AutoSystemCaller()
{
  mStack.Pop();
}

} // namespace dom
} // namespace mozilla
