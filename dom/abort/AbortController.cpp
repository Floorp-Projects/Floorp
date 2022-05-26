/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortController.h"
#include "AbortSignal.h"
#include "js/Value.h"
#include "mozilla/dom/AbortControllerBinding.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/HoldDropJSObjects.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(AbortController,
                                                      (mGlobal, mSignal),
                                                      (mReason))

NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */
already_AddRefed<AbortController> AbortController::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<AbortController> abortController = new AbortController(global);
  return abortController.forget();
}

AbortController::AbortController(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal), mAborted(false), mReason(JS::UndefinedHandleValue) {
  mozilla::HoldJSObjects(this);
}

JSObject* AbortController::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return AbortController_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject* AbortController::GetParentObject() const { return mGlobal; }

AbortSignal* AbortController::Signal() {
  if (!mSignal) {
    JS::Rooted<JS::Value> reason(RootingCx(), mReason);
    mSignal = new AbortSignal(mGlobal, mAborted, reason);
  }

  return mSignal;
}

void AbortController::Abort(JSContext* aCx, JS::Handle<JS::Value> aReason) {
  if (mAborted) {
    return;
  }

  mAborted = true;

  if (mSignal) {
    mSignal->SignalAbort(aReason);
  } else {
    mReason = aReason;
  }
}

AbortController::~AbortController() { mozilla::DropJSObjects(this); }

}  // namespace mozilla::dom
