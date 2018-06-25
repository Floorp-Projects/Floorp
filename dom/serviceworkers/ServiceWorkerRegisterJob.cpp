/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerRegisterJob.h"

#include "mozilla/dom/WorkerCommon.h"
#include "ServiceWorkerManager.h"

namespace mozilla {
namespace dom {

ServiceWorkerRegisterJob::ServiceWorkerRegisterJob(
    nsIPrincipal* aPrincipal,
    const nsACString& aScope,
    const nsACString& aScriptSpec,
    nsILoadGroup* aLoadGroup,
    ServiceWorkerUpdateViaCache aUpdateViaCache)
  : ServiceWorkerUpdateJob(Type::Register, aPrincipal, aScope, aScriptSpec,
                           aLoadGroup, aUpdateViaCache)
{
}

void
ServiceWorkerRegisterJob::AsyncExecute()
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
  if (Canceled() || !swm) {
    FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
    return;
  }

  RefPtr<ServiceWorkerRegistrationInfo> registration =
    swm->GetRegistration(mPrincipal, mScope);

  if (registration) {
    bool sameUVC = GetUpdateViaCache() == registration->GetUpdateViaCache();
    registration->SetUpdateViaCache(GetUpdateViaCache());

    if (registration->IsPendingUninstall()) {
      registration->ClearPendingUninstall();
      // Its possible that a ready promise is created between when the
      // uninstalling flag is set and when we resurrect the registration
      // here.  In that case we might need to fire the ready promise
      // now.
      swm->CheckPendingReadyPromises();
    }
    RefPtr<ServiceWorkerInfo> newest = registration->Newest();
    if (newest && mScriptSpec.Equals(newest->ScriptSpec()) && sameUVC) {
      SetRegistration(registration);
      Finish(NS_OK);
      return;
    }
  } else {
    registration = swm->CreateNewRegistration(mScope, mPrincipal,
                                              GetUpdateViaCache());
    if (!registration) {
      FailUpdateJob(NS_ERROR_DOM_ABORT_ERR);
      return;
    }
  }

  SetRegistration(registration);
  Update();
}

ServiceWorkerRegisterJob::~ServiceWorkerRegisterJob()
{
}

} // namespace dom
} // namespace mozilla
