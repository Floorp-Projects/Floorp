/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerUnregisterJob.h"

namespace mozilla {
namespace dom {
namespace workers {


ServiceWorkerUnregisterJob::ServiceWorkerUnregisterJob(nsIPrincipal* aPrincipal,
                                                       const nsACString& aScope,
                                                       bool aSendToParent)
  : ServiceWorkerJob(Type::Unregister, aPrincipal, aScope, EmptyCString())
  , mResult(false)
  , mSendToParent(aSendToParent)
{
}

bool
ServiceWorkerUnregisterJob::GetResult() const
{
  AssertIsOnMainThread();
  return mResult;
}

ServiceWorkerUnregisterJob::~ServiceWorkerUnregisterJob()
{
}

void
ServiceWorkerUnregisterJob::AsyncExecute()
{
  AssertIsOnMainThread();

  if (Canceled()) {
    Finish(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  // Step 1 of the Unregister algorithm requires checking that the
  // client origin matches the scope's origin.  We perform this in
  // registration->update() method directly since we don't have that
  // client information available here.

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();

  // "Let registration be the result of running [[Get Registration]]
  // algorithm passing scope as the argument."
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    swm->GetRegistration(mPrincipal, mScope);
  if (!registration) {
    // "If registration is null, then, resolve promise with false."
    Finish(NS_OK);
    return;
  }

  // Note, we send the message to remove the registration from disk now even
  // though we may only set the mPendingUninstall flag below.  This is
  // necessary to ensure the registration is removed if the controlled
  // clients are closed by shutting down the browser.  If the registration
  // is resurrected by clearing mPendingUninstall then it should be saved
  // to disk again.
  if (mSendToParent && !registration->mPendingUninstall) {
    swm->MaybeSendUnregister(mPrincipal, mScope);
  }

  // "Set registration's uninstalling flag."
  registration->mPendingUninstall = true;

  // "Resolve promise with true"
  mResult = true;
  InvokeResultCallbacks(NS_OK);

  // "If no service worker client is using registration..."
  if (!registration->IsControllingDocuments()) {
    // "Invoke [[Clear Registration]]..."
    swm->RemoveRegistration(registration);
  }

  Finish(NS_OK);
}

} // namespace workers
} // namespace dom
} // namespace mozilla
