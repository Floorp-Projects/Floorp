/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AbortController.h"
#include "AbortSignal.h"
#include "mozilla/dom/AbortControllerBinding.h"
#include "mozilla/dom/WorkerPrivate.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(AbortController, mGlobal, mSignal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(AbortController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AbortController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AbortController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<AbortController>
AbortController::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<AbortController> abortController = new AbortController(global);
  return abortController.forget();
}

AbortController::AbortController(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mAborted(false)
{}

JSObject*
AbortController::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return AbortController_Binding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject*
AbortController::GetParentObject() const
{
  return mGlobal;
}

AbortSignal*
AbortController::Signal()
{
  if (!mSignal) {
    mSignal = new AbortSignal(this, mAborted);
  }

  return mSignal;
}

void
AbortController::Abort()
{
  if (mAborted) {
    return;
  }

  mAborted = true;

  if (mSignal) {
    mSignal->Abort();
  }
}

} // dom namespace
} // mozilla namespace
