/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FetchController.h"
#include "FetchSignal.h"
#include "mozilla/dom/FetchControllerBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(FetchController, mGlobal, mSignal,
                                      mFollowingSignal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(FetchController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(FetchController)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(FetchController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* static */ bool
FetchController::IsEnabled(JSContext* aCx, JSObject* aGlobal)
{
  if (NS_IsMainThread()) {
    return Preferences::GetBool("dom.fetchController.enabled", false);
  }

  using namespace workers;

  // Otherwise, check the pref via the WorkerPrivate
  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aCx);
  if (!workerPrivate) {
    return false;
  }

  return workerPrivate->FetchControllerEnabled();
}

/* static */ already_AddRefed<FetchController>
FetchController::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<FetchController> fetchController = new FetchController(global);
  return fetchController.forget();
}

FetchController::FetchController(nsIGlobalObject* aGlobal)
  : mGlobal(aGlobal)
  , mAborted(false)
{}

JSObject*
FetchController::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FetchControllerBinding::Wrap(aCx, this, aGivenProto);
}

nsIGlobalObject*
FetchController::GetParentObject() const
{
  return mGlobal;
}

FetchSignal*
FetchController::Signal()
{
  if (!mSignal) {
    mSignal = new FetchSignal(this, mAborted);
  }

  return mSignal;
}

void
FetchController::Abort()
{
  if (mAborted) {
    return;
  }

  mAborted = true;

  if (mSignal) {
    mSignal->Abort();
  }
}

void
FetchController::Follow(FetchSignal& aSignal)
{
  FetchSignal::Follower::Follow(&aSignal);
}

void
FetchController::Unfollow(FetchSignal& aSignal)
{
  if (mFollowingSignal != &aSignal) {
    return;
  }

  FetchSignal::Follower::Unfollow();
}

FetchSignal*
FetchController::Following() const
{
  return mFollowingSignal;
}

void
FetchController::Aborted()
{
  Abort();
}

} // dom namespace
} // mozilla namespace
