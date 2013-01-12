/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CallbackFunction.h"
#include "jsfriendapi.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXPConnect.h"
#include "nsIScriptContext.h"
#include "nsPIDOMWindow.h"
#include "nsJSUtils.h"
#include "nsIScriptSecurityManager.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CallbackFunction)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(CallbackFunction)
NS_IMPL_CYCLE_COLLECTING_RELEASE(CallbackFunction)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(CallbackFunction)
  tmp->DropCallback();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(CallbackFunction)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(CallbackFunction)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCallable)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

CallbackFunction::CallSetup::CallSetup(JSObject* const aCallable)
  : mCx(nullptr)
{
  xpc_UnmarkGrayObject(aCallable);

  // We need to produce a useful JSContext here.  Ideally one that the callable
  // is in some sense associated with, so that we can sort of treat it as a
  // "script entry point".

  // First, find the real underlying callable.
  JSObject* realCallable = js::UnwrapObject(aCallable);

  // Now get the nsIScriptGlobalObject for this callable.
  JSContext* cx = nullptr;
  nsIScriptContext* ctx = nullptr;
  nsIScriptGlobalObject* sgo = nsJSUtils::GetStaticScriptGlobal(realCallable);
  if (sgo) {
    // Make sure that if this is a window it's the current inner, since the
    // nsIScriptContext and hence JSContext are associated with the outer
    // window.  Which means that if someone holds on to a function from a
    // now-unloaded document we'd have the new document as the script entry
    // point...
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(sgo);
    if (win) {
      MOZ_ASSERT(win->IsInnerWindow());
      nsPIDOMWindow* outer = win->GetOuterWindow();
      if (!outer || win != outer->GetCurrentInnerWindow()) {
        // Just bail out from here
        return;
      }
    }
    // if not a window at all, just press on

    ctx = sgo->GetContext();
    if (ctx) {
      // We don't check whether scripts are enabled on ctx, because
      // CheckFunctionAccess will do that anyway... and because we ignore them
      // being disabled if the callee is system.
      cx = ctx->GetNativeContext();
    }
  }

  if (!cx) {
    // We didn't manage to hunt down a script global to work with.  Just fall
    // back on using the safe context.
    cx = nsContentUtils::GetSafeJSContext();
  }

  // Victory!  We have a JSContext.  Now do the things we need a JSContext for.
  mAr.construct(cx);

  // Make sure our JSContext is pushed on the stack.
  if (!mCxPusher.Push(cx, false)) {
    return;
  }

  // After this point we guarantee calling ScriptEvaluated() if we
  // have an nsIScriptContext.
  // XXXbz Why, if, say CheckFunctionAccess fails?  I know that's how
  // nsJSContext::CallEventHandler works, but is it required?
  // FIXME: Bug 807369.
  mCtx = ctx;

  // Check that it's ok to run this callback at all.
  // FIXME: Bug 807371: we want a less silly check here.
  // Make sure to unwrap aCallable before passing it in, because
  // getting principals from wrappers is silly.
  nsresult rv = nsContentUtils::GetSecurityManager()->
    CheckFunctionAccess(cx, js::UnwrapObject(aCallable), nullptr);

  // Construct a termination func holder even if we're not planning to
  // run any script.  We need this because we're going to call
  // ScriptEvaluated even if we don't run the script...  See XXX
  // comment above.
  if (ctx) {
    mTerminationFuncHolder.construct(static_cast<nsJSContext*>(ctx));
  }

  if (NS_FAILED(rv)) {
    // Security check failed.  We're done here.
    return;
  }

  // Enter the compartment of our callable, so we can actually call it.
  mAc.construct(cx, aCallable);

  // And now we're ready to go.
  mCx = cx;
}

CallbackFunction::CallSetup::~CallSetup()
{
  // First things first: if we have a JSContext, report any pending
  // errors on it.
  if (mCx) {
    nsJSUtils::ReportPendingException(mCx);
  }

  // If we have an mCtx, we need to call ScriptEvaluated() on it.  But we have
  // to do that after we pop the JSContext stack (see bug 295983).  And to get
  // our nesting right we have to destroy our JSAutoCompartment first.  But be
  // careful: it might not have been constructed at all!
  mAc.destroyIfConstructed();

  // XXXbz For that matter why do we need to manually call ScriptEvaluated at
  // all?  nsCxPusher::Pop will do that nowadays if !mScriptIsRunning, so the
  // concerns from bug 295983 don't seem relevant anymore.  Do we want to make
  // sure it's still called when !mScriptIsRunning?  I guess play it safe for
  // now and do what CallEventHandler did, which is call always.

  // Popping an nsCxPusher is safe even if it never got pushed.
  mCxPusher.Pop();

  if (mCtx) {
    mCtx->ScriptEvaluated(true);
  }
}

} // namespace dom
} // namespace mozilla
