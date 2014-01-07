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

  nsIGlobalObject* Incumbent() {
    if (!mStack.Length()) {
      return nullptr;
    }
    return mStack.LastElement()->mGlobalObject;
  }

  nsIGlobalObject* EntryPoint() {
    if (!mStack.Length())
      return nullptr;
    for (int i = mStack.Length() - 1; i >= 0; --i) {
      if (mStack[i]->mIsCandidateEntryPoint) {
        return mStack[i]->mGlobalObject;
      }
    }
    MOZ_ASSUME_UNREACHABLE("Non-empty stack should always have an entry point");
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

// Note: When we're ready to expose it, GetEntryGlobal will look similar to
// GetIncumbentGlobal below.

nsIGlobalObject*
GetIncumbentGlobal()
{
  // We need the current JSContext in order to check the JS for
  // scripted frames that may have appeared since anyone last
  // manipulated the stack. If it's null, that means that there
  // must be no entry point on the stack, and therefore no incumbent
  // global either.
  JSContext *cx = nsContentUtils::GetCurrentJSContextForThread();
  if (!cx) {
    MOZ_ASSERT(ScriptSettingsStack::Ref().EntryPoint() == nullptr);
    return nullptr;
  }

  // See what the JS engine has to say. If we've got a scripted caller
  // override in place, the JS engine will lie to us and pretend that
  // there's nothing on the JS stack, which will cause us to check the
  // incumbent script stack below.
  JS::RootedScript script(cx);
  if (JS_DescribeScriptedCaller(cx, &script, nullptr)) {
    JS::RootedObject global(cx, JS_GetGlobalFromScript(script));
    MOZ_ASSERT(global);
    return xpc::GetNativeForGlobal(global);
  }

  // Ok, nothing from the JS engine. Let's use whatever's on the
  // explicit stack.
  return ScriptSettingsStack::Ref().Incumbent();
}

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 bool aIsMainThread,
                                 JSContext* aCx)
  : mStack(ScriptSettingsStack::Ref())
  , mEntry(aGlobalObject, /* aCandidate = */ true)
{
  MOZ_ASSERT(aGlobalObject);
  if (!aCx) {
    // If the caller didn't provide a cx, hunt one down. This isn't exactly
    // fast, but the callers that care about performance can pass an explicit
    // cx for now. Eventually, the whole cx pushing thing will go away
    // entirely.
    MOZ_ASSERT(aIsMainThread, "cx is mandatory off-main-thread");
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aGlobalObject);
    if (sgo && sgo->GetScriptContext()) {
      aCx = sgo->GetScriptContext()->GetNativeContext();
    }
    if (!aCx) {
      aCx = nsContentUtils::GetSafeJSContext();
    }
  }
  if (aIsMainThread) {
    mCxPusher.Push(aCx);
  }
  mAc.construct(aCx, aGlobalObject->GetGlobalJSObject());
  mStack.Push(&mEntry);
}

AutoEntryScript::~AutoEntryScript()
{
  MOZ_ASSERT(mStack.Incumbent() == mEntry.mGlobalObject);
  mStack.Pop();
}

AutoIncumbentScript::AutoIncumbentScript(nsIGlobalObject* aGlobalObject)
  : mStack(ScriptSettingsStack::Ref())
  , mEntry(aGlobalObject, /* aCandidate = */ false)
  , mCallerOverride(nsContentUtils::GetCurrentJSContextForThread())
{
  mStack.Push(&mEntry);
}

AutoIncumbentScript::~AutoIncumbentScript()
{
  MOZ_ASSERT(mStack.Incumbent() == mEntry.mGlobalObject);
  mStack.Pop();
}

AutoSystemCaller::AutoSystemCaller(bool aIsMainThread)
  : mStack(ScriptSettingsStack::Ref())
{
  if (aIsMainThread) {
    mCxPusher.PushNull();
  }
  mStack.PushSystem();
}

AutoSystemCaller::~AutoSystemCaller()
{
  mStack.Pop();
}

} // namespace dom
} // namespace mozilla
