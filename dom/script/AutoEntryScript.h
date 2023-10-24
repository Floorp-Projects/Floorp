/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SCRIPT_AUTOENTRYSCRIPT_H_
#define DOM_SCRIPT_AUTOENTRYSCRIPT_H_

#include "MainThreadUtils.h"
#include "js/Debug.h"
#include "js/TypeDecls.h"
#include "jsapi.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/ScriptSettings.h"

class nsIGlobalObject;
class nsIPrincipal;

namespace xpc {
class AutoScriptActivity;
}

namespace mozilla::dom {

/*
 * Static helpers in ScriptSettings which track the number of listeners
 * of Javascript RunToCompletion events.  These should be used by the code in
 * nsDocShell::SetRecordProfileTimelineMarkers to indicate to script
 * settings that script run-to-completion needs to be monitored.
 * SHOULD BE CALLED ONLY BY MAIN THREAD.
 */
void UseEntryScriptProfiling();
void UnuseEntryScriptProfiling();

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
  AutoProfilerLabel mAutoProfilerLabel;
  AutoRequestJSThreadExecution mJSThreadExecution;
};

}  // namespace mozilla::dom

#endif  // DOM_SCRIPT_AUTOENTRYSCRIPT_H_
