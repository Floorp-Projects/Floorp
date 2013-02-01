/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothScoManager.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "nsContentUtils.h"
#include "nsIAudioManager.h"
#include "nsIObserverService.h"

#define BLUETOOTH_SCO_STATUS_CHANGED "bluetooth-sco-status-changed"

using namespace mozilla;
using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

class mozilla::dom::bluetooth::BluetoothScoManagerObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  BluetoothScoManagerObserver()
  {
  }

  bool Init()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    MOZ_ASSERT(obs);

    if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
      NS_WARNING("Failed to add shutdown observer!");
      return false;
    }
    return true;
  }

  bool Shutdown()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs ||
        (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID)))) {
      NS_WARNING("Can't unregister observers!");
      return false;
    }
    return true;
  }

  ~BluetoothScoManagerObserver()
  {
    Shutdown();
  }
};

void
BluetoothScoManager::NotifyAudioManager(const nsAString& aAddress) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ENSURE_TRUE_VOID(obs);

  if (aAddress.IsEmpty()) {
    if (NS_FAILED(obs->NotifyObservers(nullptr, BLUETOOTH_SCO_STATUS_CHANGED, nullptr))) {
      NS_WARNING("Failed to notify bluetooth-sco-status-changed observsers!");
      return;
    }
  } else {
    if (NS_FAILED(obs->NotifyObservers(nullptr, BLUETOOTH_SCO_STATUS_CHANGED, aAddress.BeginReading()))) {
      NS_WARNING("Failed to notify bluetooth-sco-status-changed observsers!");
      return;
    }
  }

  nsCOMPtr<nsIAudioManager> am =
    do_GetService("@mozilla.org/telephony/audiomanager;1");
  NS_ENSURE_TRUE_VOID(am);
  am->SetForceForUse(am->USE_COMMUNICATION, am->FORCE_BT_SCO);
}

NS_IMPL_ISUPPORTS1(BluetoothScoManagerObserver, nsIObserver)

namespace {
StaticRefPtr<BluetoothScoManager> gBluetoothScoManager;
StaticRefPtr<BluetoothScoManagerObserver> sScoObserver;
bool gInShutdown = false;
} // anonymous namespace

NS_IMETHODIMP
BluetoothScoManagerObserver::Observe(nsISupports* aSubject,
                                     const char* aTopic,
                                     const PRUnichar* aData)
{
  MOZ_ASSERT(gBluetoothScoManager);
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {    
    return gBluetoothScoManager->HandleShutdown();
  }

  MOZ_ASSERT(false, "BluetoothScoManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

BluetoothScoManager::BluetoothScoManager()
{
}

bool
BluetoothScoManager::Init()
{
  mSocketStatus = GetConnectionStatus();

  sScoObserver = new BluetoothScoManagerObserver();
  if (sScoObserver->Init()) {
    NS_WARNING("Cannot set up SCO observers!");
  }
  return true;
}

BluetoothScoManager::~BluetoothScoManager()
{
  Cleanup();
}

void
BluetoothScoManager::Cleanup()
{
  sScoObserver->Shutdown();
  sScoObserver = nullptr;
}

//static
BluetoothScoManager*
BluetoothScoManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gBluetoothScoManager) {
    return gBluetoothScoManager;
  }

  // If we're in shutdown, don't create a new instance
  if (gInShutdown) {
    NS_WARNING("BluetoothScoManager can't be created during shutdown");
    return nullptr;
  }

  // Create new instance, register, return
  nsRefPtr<BluetoothScoManager> manager = new BluetoothScoManager();
  NS_ENSURE_TRUE(manager, nullptr);

  if (!manager->Init()) {
    return nullptr;
  }

  gBluetoothScoManager = manager;
  return gBluetoothScoManager;
}

// Virtual function of class SocketConsumer
void
BluetoothScoManager::ReceiveSocketData(nsAutoPtr<UnixSocketRawData>& aMessage)
{
  // SCO socket do nothing here
  MOZ_NOT_REACHED("This should never be called!");
}

nsresult
BluetoothScoManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  gInShutdown = true;
  CloseSocket();
  gBluetoothScoManager = nullptr;
  return NS_OK;
}

bool
BluetoothScoManager::Connect(const nsAString& aDeviceAddress)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    MOZ_ASSERT(false, "Connect called while in shutdown!");
    return false;
  }

  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_CONNECTED) {
    NS_WARNING("Sco socket has been connected");
    return false;
  }

  CloseSocket();

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsresult rv = bs->GetScoSocket(aDeviceAddress,
                                 true,
                                 false,
                                 this);

  return NS_FAILED(rv) ? false : true;
}

bool
BluetoothScoManager::Listen()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (gInShutdown) {
    MOZ_ASSERT(false, "Listen called while in shutdown!");
    return false;
  }

  CloseSocket();

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsresult rv = bs->ListenSocketViaService(-1,
                                           BluetoothSocketType::SCO,
                                           true,
                                           true,
                                           this);

  mSocketStatus = GetConnectionStatus();

  return NS_FAILED(rv) ? false : true;
}

void
BluetoothScoManager::Disconnect()
{
  if (GetConnectionStatus() == SocketConnectionStatus::SOCKET_DISCONNECTED) {
    return;
  }

  CloseSocket();
}

void
BluetoothScoManager::OnConnectSuccess()
{
  nsString address;
  GetSocketAddr(address);
  NotifyAudioManager(address);

  mSocketStatus = GetConnectionStatus();
}

void
BluetoothScoManager::OnConnectError()
{
  CloseSocket();
  mSocketStatus = GetConnectionStatus();
  Listen();
}

void
BluetoothScoManager::OnDisconnect()
{
  if (mSocketStatus == SocketConnectionStatus::SOCKET_CONNECTED) {
    Listen();

    nsString address = NS_LITERAL_STRING("");
    NotifyAudioManager(address);
  }
}
