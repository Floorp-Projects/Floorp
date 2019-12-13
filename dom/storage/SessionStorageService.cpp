/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageService.h"

#include "mozilla/Services.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "nsXULAppAPI.h"

#include <cstring>

namespace mozilla {
namespace dom {

namespace {

const char* const kContentProcessShutdownTopic = "content-child-will-shutdown";

}

RefPtr<SessionStorageService> SessionStorageService::sService = nullptr;
bool SessionStorageService::sShutdown = false;

NS_IMPL_ISUPPORTS(SessionStorageService, nsIObserver)

SessionStorageService::SessionStorageService() {
  if (const nsCOMPtr<nsIObserverService> observerService =
          services::GetObserverService()) {
    Unused << observerService->AddObserver(this, kContentProcessShutdownTopic,
                                           false);
  }
}

SessionStorageService::~SessionStorageService() {
  if (const nsCOMPtr<nsIObserverService> observerService =
          services::GetObserverService()) {
    Unused << observerService->RemoveObserver(this,
                                              kContentProcessShutdownTopic);
  }
}

SessionStorageService* SessionStorageService::Get() {
  if (sShutdown) {
    return nullptr;
  }

  if (XRE_IsParentProcess()) {
    ShutDown();
    return nullptr;
  }

  if (!sService) {
    sService = new SessionStorageService();
  }

  return sService;
}

NS_IMETHODIMP SessionStorageService::Observe(nsISupports* const aSubject,
                                             const char* const aTopic,
                                             const char16_t* const aData) {
  if (std::strcmp(aTopic, kContentProcessShutdownTopic) == 0) {
    SendSessionStorageDataToParentProcess();
    ShutDown();
  }
  return NS_OK;
}

void SessionStorageService::RegisterSessionStorageManager(
    SessionStorageManager* aManager) {
  mManagers.PutEntry(aManager);
}

void SessionStorageService::UnregisterSessionStorageManager(
    SessionStorageManager* aManager) {
  if (const auto entry = mManagers.GetEntry(aManager)) {
    mManagers.RemoveEntry(entry);
  }
}

void SessionStorageService::SendSessionStorageDataToParentProcess() {
  for (auto iter = mManagers.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->SendSessionStorageDataToParentProcess();
  }
}

void SessionStorageService::ShutDown() {
  sShutdown = true;
  sService = nullptr;
}

}  // namespace dom
}  // namespace mozilla
