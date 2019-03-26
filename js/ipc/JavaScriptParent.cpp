/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=4 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "JavaScriptParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsJSUtils.h"
#include "nsIScriptError.h"
#include "jsfriendapi.h"
#include "js/Proxy.h"
#include "js/HeapAPI.h"
#include "js/Wrapper.h"
#include "xpcprivate.h"
#include "mozilla/Casting.h"
#include "mozilla/Telemetry.h"
#include "nsAutoPtr.h"

using namespace js;
using namespace JS;
using namespace mozilla;
using namespace mozilla::jsipc;
using namespace mozilla::dom;

static void TraceParent(JSTracer* trc, void* data) {
  static_cast<JavaScriptParent*>(data)->trace(trc);
}

JavaScriptParent::JavaScriptParent() : savedNextCPOWNumber_(1) {
  JS_AddExtraGCRootsTracer(danger::GetJSContext(), TraceParent, this);
}

JavaScriptParent::~JavaScriptParent() {
  JS_RemoveExtraGCRootsTracer(danger::GetJSContext(), TraceParent, this);
}

static bool ForbidUnsafeBrowserCPOWs() {
  static bool result;
  static bool cached = false;
  if (!cached) {
    cached = true;
    Preferences::AddBoolVarCache(
        &result, "dom.ipc.cpows.forbid-unsafe-from-browser", false);
  }
  return result;
}

bool JavaScriptParent::allowMessage(JSContext* cx) {
  MOZ_ASSERT(cx);

  // If we're running browser code while running tests (in automation),
  // then we allow all safe CPOWs and forbid unsafe CPOWs
  // based on a pref (which defaults to forbidden).
  // We also allow CPOWs unconditionally in selected globals (based on
  // Cu.permitCPOWsInScope).
  // A normal (release) browser build will never allow CPOWs,
  // excecpt as a token to pass round.

  if (!xpc::IsInAutomation()) {
    JS_ReportErrorASCII(cx, "CPOW usage forbidden");
    return false;
  }

  MessageChannel* channel = GetIPCChannel();
  bool isSafe = channel->IsInTransaction();

  if (isSafe) {
    return true;
  }

  nsIGlobalObject* global = dom::GetIncumbentGlobal();
  JS::Rooted<JSObject*> jsGlobal(
      cx, global ? global->GetGlobalJSObject() : nullptr);
  if (jsGlobal) {
    JSAutoRealm ar(cx, jsGlobal);

    if (!xpc::CompartmentPrivate::Get(jsGlobal)->allowCPOWs &&
        ForbidUnsafeBrowserCPOWs()) {
      Telemetry::Accumulate(Telemetry::BROWSER_SHIM_USAGE_BLOCKED, 1);
      JS_ReportErrorASCII(cx, "unsafe CPOW usage forbidden");
      return false;
    }
  }

  static bool disableUnsafeCPOWWarnings =
      PR_GetEnv("DISABLE_UNSAFE_CPOW_WARNINGS");
  if (!disableUnsafeCPOWWarnings) {
    nsCOMPtr<nsIConsoleService> console(
        do_GetService(NS_CONSOLESERVICE_CONTRACTID));
    if (console) {
      nsAutoString filename;
      uint32_t lineno = 0, column = 0;
      nsJSUtils::GetCallingLocation(cx, filename, &lineno, &column);
      nsCOMPtr<nsIScriptError> error(
          do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
      error->Init(NS_LITERAL_STRING("unsafe/forbidden CPOW usage"), filename,
                  EmptyString(), lineno, column, nsIScriptError::warningFlag,
                  "chrome javascript", false /* from private window */,
                  true /* from chrome context */);
      console->LogMessage(error);
    } else {
      NS_WARNING("Unsafe synchronous IPC message");
    }
  }

  return true;
}

void JavaScriptParent::trace(JSTracer* trc) {
  objects_.trace(trc);
  unwaivedObjectIds_.trace(trc);
  waivedObjectIds_.trace(trc);
}

JSObject* JavaScriptParent::scopeForTargetObjects() {
  // CPOWs from the child need to point into the parent's unprivileged junk
  // scope so that a compromised child cannot compromise the parent. In
  // practice, this means that a child process can only (a) hold parent
  // objects alive and (b) invoke them if they are callable.
  return xpc::UnprivilegedJunkScope();
}

void JavaScriptParent::afterProcessTask() {
  if (savedNextCPOWNumber_ == nextCPOWNumber_) {
    return;
  }

  savedNextCPOWNumber_ = nextCPOWNumber_;

  MOZ_ASSERT(nextCPOWNumber_ > 0);
  if (active()) {
    Unused << SendDropTemporaryStrongReferences(nextCPOWNumber_ - 1);
  }
}

PJavaScriptParent* mozilla::jsipc::NewJavaScriptParent() {
  return new JavaScriptParent();
}

void mozilla::jsipc::ReleaseJavaScriptParent(PJavaScriptParent* parent) {
  static_cast<JavaScriptParent*>(parent)->decref();
}

void mozilla::jsipc::AfterProcessTask() {
  for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
    if (PJavaScriptParent* p =
            LoneManagedOrNullAsserts(cp->ManagedPJavaScriptParent())) {
      static_cast<JavaScriptParent*>(p)->afterProcessTask();
    }
  }
}
