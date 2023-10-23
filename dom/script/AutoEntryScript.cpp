/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AutoEntryScript.h"

#include <stdint.h>
#include <utility>
#include "js/ProfilingCategory.h"
#include "js/ProfilingStack.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Span.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsIDocShell.h"
#include "nsJSUtils.h"
#include "nsPIDOMWindow.h"
#include "nsPIDOMWindowInlines.h"
#include "nsString.h"
#include "xpcpublic.h"

namespace mozilla::dom {

namespace {
// Assert if it's not safe to run script. The helper class
// AutoAllowLegacyScriptExecution allows to allow-list
// legacy cases where it's actually not safe to run script.
#ifdef DEBUG
void AssertIfNotSafeToRunScript() {
  // if it's safe to run script, then there is nothing to do here.
  if (nsContentUtils::IsSafeToRunScript()) {
    return;
  }

  // auto allowing legacy script execution is fine for now.
  if (AutoAllowLegacyScriptExecution::IsAllowed()) {
    return;
  }

  MOZ_ASSERT(false, "is it safe to run script?");
}
#endif

}  // namespace

AutoEntryScript::AutoEntryScript(nsIGlobalObject* aGlobalObject,
                                 const char* aReason, bool aIsMainThread)
    : AutoJSAPI(aGlobalObject, aIsMainThread, eEntryScript),
      mWebIDLCallerPrincipal(nullptr)
      // This relies on us having a cx() because the AutoJSAPI constructor
      // already ran.
      ,
      mCallerOverride(cx()),
      mAutoProfilerLabel(
          "", aReason, JS::ProfilingCategoryPair::JS,
          uint32_t(js::ProfilingStackFrame::Flags::RELEVANT_FOR_JS)),
      mJSThreadExecution(aGlobalObject, aIsMainThread) {
  MOZ_ASSERT(aGlobalObject);

  if (aIsMainThread) {
#ifdef DEBUG
    AssertIfNotSafeToRunScript();
#endif
    mScriptActivity.emplace(true);
  }
}

AutoEntryScript::AutoEntryScript(JSObject* aObject, const char* aReason,
                                 bool aIsMainThread)
    : AutoEntryScript(xpc::NativeGlobal(aObject), aReason, aIsMainThread) {
  // xpc::NativeGlobal uses JS::GetNonCCWObjectGlobal, which asserts that
  // aObject is not a CCW.
}

AutoEntryScript::~AutoEntryScript() = default;

}  // namespace mozilla::dom
