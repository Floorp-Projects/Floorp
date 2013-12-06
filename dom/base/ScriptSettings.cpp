/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ScriptSettings.h"

#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 bool aIsMainThread,
                                 JSContext* aCx)
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
}

AutoIncumbentScript::AutoIncumbentScript(nsIGlobalObject* aGlobalObject)
{
  MOZ_ASSERT(aGlobalObject);
}

AutoSystemCaller::AutoSystemCaller(bool aIsMainThread)
{
  if (aIsMainThread) {
    mCxPusher.PushNull();
  }
}

} // namespace dom
} // namespace mozilla
