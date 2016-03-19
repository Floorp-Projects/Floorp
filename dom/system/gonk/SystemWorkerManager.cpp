/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SystemWorkerManager.h"

#include "nsINetworkService.h"
#include "nsIWifi.h"
#include "nsIWorkerHolder.h"
#include "nsIXPConnect.h"

#include "jsfriendapi.h"
#include "mozilla/dom/workers/Workers.h"
#include "AutoMounter.h"
#include "TimeZoneSettingObserver.h"
#include "AudioManager.h"
#include "mozilla/dom/ScriptSettings.h"
#ifdef MOZ_B2G_RIL
#include "mozilla/ipc/Ril.h"
#endif
#include "mozilla/ipc/KeyStore.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "WifiWorker.h"
#include "mozilla/Services.h"

USING_WORKERS_NAMESPACE

using namespace mozilla::dom::gonk;
using namespace mozilla::ipc;
using namespace mozilla::system;

namespace {

NS_DEFINE_CID(kWifiWorkerCID, NS_WIFIWORKER_CID);

// Doesn't carry a reference, we're owned by services.
SystemWorkerManager *gInstance = nullptr;

} // namespace

SystemWorkerManager::SystemWorkerManager()
  : mShutdown(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance, "There should only be one instance!");
}

SystemWorkerManager::~SystemWorkerManager()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!gInstance || gInstance == this,
               "There should only be one instance!");
  gInstance = nullptr;
}

nsresult
SystemWorkerManager::Init()
{
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_ASSERTION(NS_IsMainThread(), "We can only initialize on the main thread");
  NS_ASSERTION(!mShutdown, "Already shutdown!");

  nsresult rv = InitWifi();
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to initialize WiFi Networking!");
    return rv;
  }

  InitKeyStore();

  InitAutoMounter();
  InitializeTimeZoneSettingObserver();
  nsCOMPtr<nsIAudioManager> audioManager =
    do_GetService(NS_AUDIOMANAGER_CONTRACTID);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    NS_WARNING("Failed to get observer service!");
    return NS_ERROR_FAILURE;
  }

  rv = obs->AddObserver(this, WORKERS_SHUTDOWN_TOPIC, false);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to initialize worker shutdown event!");
    return rv;
  }

  return NS_OK;
}

void
SystemWorkerManager::Shutdown()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  mShutdown = true;

  ShutdownAutoMounter();

#ifdef MOZ_B2G_RIL
  RilWorker::Shutdown();
#endif

  nsCOMPtr<nsIWifi> wifi(do_QueryInterface(mWifiWorker));
  if (wifi) {
    wifi->Shutdown();
    wifi = nullptr;
  }
  mWifiWorker = nullptr;

  if (mKeyStore) {
    mKeyStore->Shutdown();
    mKeyStore = nullptr;
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, WORKERS_SHUTDOWN_TOPIC);
  }
}

// static
already_AddRefed<SystemWorkerManager>
SystemWorkerManager::FactoryCreate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  RefPtr<SystemWorkerManager> instance(gInstance);

  if (!instance) {
    instance = new SystemWorkerManager();
    if (NS_FAILED(instance->Init())) {
      instance->Shutdown();
      return nullptr;
    }

    gInstance = instance;
  }

  return instance.forget();
}

// static
nsIInterfaceRequestor*
SystemWorkerManager::GetInterfaceRequestor()
{
  return gInstance;
}

NS_IMETHODIMP
SystemWorkerManager::GetInterface(const nsIID &aIID, void **aResult)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aIID.Equals(NS_GET_IID(nsIWifi))) {
    return CallQueryInterface(mWifiWorker,
                              reinterpret_cast<nsIWifi**>(aResult));
  }

  NS_WARNING("Got nothing for the requested IID!");
  return NS_ERROR_NO_INTERFACE;
}

nsresult
SystemWorkerManager::RegisterRilWorker(unsigned int aClientId,
                                       JS::Handle<JS::Value> aWorker,
                                       JSContext *aCx)
{
#ifndef MOZ_B2G_RIL
  return NS_ERROR_NOT_IMPLEMENTED;
#else
  NS_ENSURE_TRUE(aWorker.isObject(), NS_ERROR_UNEXPECTED);

  JSAutoCompartment ac(aCx, &aWorker.toObject());

  WorkerCrossThreadDispatcher *wctd =
    GetWorkerCrossThreadDispatcher(aCx, aWorker);
  if (!wctd) {
    NS_WARNING("Failed to GetWorkerCrossThreadDispatcher for ril");
    return NS_ERROR_FAILURE;
  }

  return RilWorker::Register(aClientId, wctd);
#endif // MOZ_B2G_RIL
}

nsresult
SystemWorkerManager::InitWifi()
{
  nsCOMPtr<nsIWorkerHolder> worker = do_CreateInstance(kWifiWorkerCID);
  NS_ENSURE_TRUE(worker, NS_ERROR_FAILURE);

  mWifiWorker = worker;
  return NS_OK;
}

nsresult
SystemWorkerManager::InitKeyStore()
{
  mKeyStore = new KeyStore();
  return NS_OK;
}

NS_IMPL_ISUPPORTS(SystemWorkerManager,
                  nsIObserver,
                  nsIInterfaceRequestor,
                  nsISystemWorkerManager)

NS_IMETHODIMP
SystemWorkerManager::Observe(nsISupports *aSubject, const char *aTopic,
                             const char16_t *aData)
{
  if (!strcmp(aTopic, WORKERS_SHUTDOWN_TOPIC)) {
    Shutdown();
  }

  return NS_OK;
}
