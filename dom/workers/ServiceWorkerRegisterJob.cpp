/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegisterJob.h"

#include "Workers.h"

namespace mozilla {
namespace dom {
namespace workers {

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(nsIPrincipal* aPrincipal,
                                                   const nsACString& aScope,
                                                   const nsACString& aScriptSpec,
                                                   nsILoadGroup* aLoadGroup)
  : ServiceWorkerUpdateJob(Type::Register, aPrincipal, aScope, aScriptSpec,
                           aLoadGroup)
{
}

void
ServiceWorkerRegisterJob::AsyncExecute()
{
  AssertIsOnMainThread();

  if (Canceled()) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  RefPtr<ServiceWorkerRegistrationInfo> registration =
    swm->GetRegistration(mPrincipal, mScope);

  if (registration) {
    // If we are resurrecting an uninstalling registration, then persist
    // it to disk again.  We preemptively removed it earlier during
    // unregister so that closing the window by shutting down the browser
    // results in the registration being gone on restart.
    if (registration->mPendingUninstall) {
      swm->StoreRegistration(mPrincipal, registration);
    }
    registration->mPendingUninstall = false;
    RefPtr<ServiceWorkerInfo> newest = registration->Newest();
    if (newest && mScriptSpec.Equals(newest->ScriptSpec())) {
      SetRegistration(registration);
      Finish(NS_OK);
      return;
    }
  } else {
    registration = swm->CreateNewRegistration(mScope, mPrincipal,
                                              nsIRequest::LOAD_NORMAL);
  }

  SetRegistration(registration);
  Update();
}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob()
{
}

} // namespace workers
} // namespace dom
} // namespace mozilla
