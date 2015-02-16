/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattManager.h"

#include "BluetoothCommon.h"
#include "BluetoothInterface.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

#define ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(runnable)                       \
  do {                                                                        \
    if (!sBluetoothGattInterface) {                                           \
      NS_NAMED_LITERAL_STRING(errorStr,                                       \
                              "BluetoothGattClientInterface is not ready");   \
      DispatchBluetoothReply(runnable, BluetoothValue(), errorStr);           \
      return;                                                                 \
    }                                                                         \
  } while(0)

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothGattManager> sBluetoothGattManager;
  static BluetoothGattInterface* sBluetoothGattInterface;
  static BluetoothGattClientInterface* sBluetoothGattClientInterface;
} // anonymous namespace

bool BluetoothGattManager::mInShutdown = false;

class BluetoothGattClient;
static StaticAutoPtr<nsTArray<nsRefPtr<BluetoothGattClient> > > sClients;

class BluetoothGattClient MOZ_FINAL : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  BluetoothGattClient(const nsAString& aAppUuid, const nsAString& aDeviceAddr)
  : mAppUuid(aAppUuid)
  , mDeviceAddr(aDeviceAddr)
  , mClientIf(0)
  , mConnId(0)
  { }

  ~BluetoothGattClient()
  {
    mConnectRunnable = nullptr;
    mDisconnectRunnable = nullptr;
    mUnregisterClientRunnable = nullptr;
  }

  nsString mAppUuid;
  nsString mDeviceAddr;
  int mClientIf;
  int mConnId;
  nsRefPtr<BluetoothReplyRunnable> mConnectRunnable;
  nsRefPtr<BluetoothReplyRunnable> mDisconnectRunnable;
  nsRefPtr<BluetoothReplyRunnable> mUnregisterClientRunnable;
};

NS_IMPL_ISUPPORTS0(BluetoothGattClient)

class UuidComparator
{
public:
  bool Equals(const nsRefPtr<BluetoothGattClient>& aClient,
              const nsAString& aAppUuid) const
  {
    return aClient->mAppUuid.Equals(aAppUuid);
  }
};

class ClientIfComparator
{
public:
  bool Equals(const nsRefPtr<BluetoothGattClient>& aClient,
              int aClientIf) const
  {
    return aClient->mClientIf == aClientIf;
  }
};

BluetoothGattManager*
BluetoothGattManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothGattManager already exists, exit early
  if (sBluetoothGattManager) {
    return sBluetoothGattManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(mInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothGattManager* manager = new BluetoothGattManager();
  sBluetoothGattManager = manager;
  return sBluetoothGattManager;
}

class BluetoothGattManager::InitGattResultHandler MOZ_FINAL
  : public BluetoothGattResultHandler
{
public:
  InitGattResultHandler(BluetoothProfileResultHandler* aRes)
  : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    BT_WARNING("BluetoothGattInterface::Init failed: %d",
               (int)aStatus);
    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void Init() MOZ_OVERRIDE
  {
    if (mRes) {
      mRes->Init();
    }
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

// static
void
BluetoothGattManager::InitGattInterface(BluetoothProfileResultHandler* aRes)
{
  BluetoothInterface* btInf = BluetoothInterface::GetInstance();
  if (!btInf) {
    BT_LOGR("Error: Bluetooth interface not available");
    if (aRes) {
      aRes->OnError(NS_ERROR_FAILURE);
    }
    return;
  }

  sBluetoothGattInterface = btInf->GetBluetoothGattInterface();
  if (!sBluetoothGattInterface) {
    BT_LOGR("Error: Bluetooth GATT interface not available");
    if (aRes) {
      aRes->OnError(NS_ERROR_FAILURE);
    }
    return;
  }

  sBluetoothGattClientInterface =
    sBluetoothGattInterface->GetBluetoothGattClientInterface();
  NS_ENSURE_TRUE_VOID(sBluetoothGattClientInterface);

  if (!sClients) {
    sClients = new nsTArray<nsRefPtr<BluetoothGattClient> >;
  }

  BluetoothGattManager* gattManager = BluetoothGattManager::Get();
  sBluetoothGattInterface->Init(gattManager,
                                new InitGattResultHandler(aRes));
}

class BluetoothGattManager::CleanupResultHandler MOZ_FINAL
  : public BluetoothGattResultHandler
{
public:
  CleanupResultHandler(BluetoothProfileResultHandler* aRes)
  : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    BT_WARNING("BluetoothGattInterface::Cleanup failed: %d",
               (int)aStatus);
    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void Cleanup() MOZ_OVERRIDE
  {
    sBluetoothGattClientInterface = nullptr;
    sBluetoothGattInterface = nullptr;
    sClients = nullptr;

    if (mRes) {
      mRes->Deinit();
    }
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothGattManager::CleanupResultHandlerRunnable MOZ_FINAL
  : public nsRunnable
{
public:
  CleanupResultHandlerRunnable(BluetoothProfileResultHandler* aRes)
  : mRes(aRes)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    mRes->Deinit();
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

// static
void
BluetoothGattManager::DeinitGattInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sBluetoothGattInterface) {
    sBluetoothGattInterface->Cleanup(new CleanupResultHandler(aRes));
  } else if (aRes) {
    // We dispatch a runnable here to make the profile resource handler
    // behave as if GATT was initialized.
    nsRefPtr<nsRunnable> r = new CleanupResultHandlerRunnable(aRes);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch cleanup-result-handler runnable");
    }
  }
}

class BluetoothGattManager::RegisterClientResultHandler MOZ_FINAL
  : public BluetoothGattClientResultHandler
{
public:
  RegisterClientResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    BT_WARNING("BluetoothGattClientInterface::RegisterClient failed: %d",
               (int)aStatus);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt for client disconnected
    BluetoothSignal signal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      mClient->mAppUuid,
      BluetoothValue(false)); // Disconnected
    bs->DistributeSignal(signal);

    // Reject the connect request
    if (mClient->mConnectRunnable) {
      NS_NAMED_LITERAL_STRING(errorStr, "Register GATT client failed");
      DispatchBluetoothReply(mClient->mConnectRunnable,
                             BluetoothValue(),
                             errorStr);
      mClient->mConnectRunnable = nullptr;
    }

    sClients->RemoveElement(mClient);
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::UnregisterClientResultHandler MOZ_FINAL
  : public BluetoothGattClientResultHandler
{
public:
  UnregisterClientResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void UnregisterClient() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt to clear the clientIf
    BluetoothSignal signal(
      NS_LITERAL_STRING("ClientUnregistered"),
      mClient->mAppUuid,
      BluetoothValue(true));
    bs->DistributeSignal(signal);

    // Resolve the unregister request
    DispatchBluetoothReply(mClient->mUnregisterClientRunnable,
                           BluetoothValue(true),
                           EmptyString());
    mClient->mUnregisterClientRunnable = nullptr;

    sClients->RemoveElement(mClient);
  }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    BT_WARNING("BluetoothGattClientInterface::UnregisterClient failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);

    // Reject the unregister request
    NS_NAMED_LITERAL_STRING(errorStr, "Unregister GATT client failed");
    DispatchBluetoothReply(mClient->mUnregisterClientRunnable,
                           BluetoothValue(),
                           errorStr);
    mClient->mUnregisterClientRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::UnregisterClient(int aClientIf,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());

  // Reject the unregister request if the client is not found
  if (index == sClients->NoIndex) {
    NS_NAMED_LITERAL_STRING(errorStr, "Unregister GATT client failed");
    DispatchBluetoothReply(aRunnable,
                           BluetoothValue(),
                           errorStr);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mUnregisterClientRunnable = aRunnable;

  sBluetoothGattClientInterface->UnregisterClient(
    aClientIf,
    new UnregisterClientResultHandler(client));
}

class BluetoothGattManager::ConnectResultHandler MOZ_FINAL
  : public BluetoothGattClientResultHandler
{
public:
  ConnectResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    BT_WARNING("BluetoothGattClientInterface::Connect failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mConnectRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt for client disconnected
    BluetoothSignal signal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      mClient->mAppUuid,
      BluetoothValue(false)); // Disconnected
    bs->DistributeSignal(signal);

    // Reject the connect request
    NS_NAMED_LITERAL_STRING(errorStr, "Connect failed");
    DispatchBluetoothReply(mClient->mConnectRunnable,
                           BluetoothValue(),
                           errorStr);
    mClient->mConnectRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::Connect(const nsAString& aAppUuid,
                              const nsAString& aDeviceAddr,
                              BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sClients->NoIndex) {
    index = sClients->Length();
    sClients->AppendElement(new BluetoothGattClient(aAppUuid, aDeviceAddr));
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mConnectRunnable = aRunnable;

  if (client->mClientIf > 0) {
    sBluetoothGattClientInterface->Connect(client->mClientIf,
                                           aDeviceAddr,
                                           true, // direct connect
                                           new ConnectResultHandler(client));
  } else {
    BluetoothUuid uuid;
    StringToUuid(NS_ConvertUTF16toUTF8(aAppUuid).get(), uuid);

    // connect will be proceeded after client registered
    sBluetoothGattClientInterface->RegisterClient(
      uuid, new RegisterClientResultHandler(client));
  }
}

class BluetoothGattManager::DisconnectResultHandler MOZ_FINAL
  : public BluetoothGattClientResultHandler
{
public:
  DisconnectResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) MOZ_OVERRIDE
  {
    BT_WARNING("BluetoothGattClientInterface::Disconnect failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mDisconnectRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt that the client remains connected
    BluetoothSignal signal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      mClient->mAppUuid,
      BluetoothValue(true)); // Connected
    bs->DistributeSignal(signal);

    // Reject the disconnect request
    NS_NAMED_LITERAL_STRING(errorStr, "Disconnect failed");
    DispatchBluetoothReply(mClient->mDisconnectRunnable,
                           BluetoothValue(),
                           errorStr);
    mClient->mDisconnectRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::Disconnect(const nsAString& aAppUuid,
                                 const nsAString& aDeviceAddr,
                                 BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());

  // Reject the disconnect request if the client is not found
  if (index == sClients->NoIndex) {
    NS_NAMED_LITERAL_STRING(errorStr, "Disconnect failed");
    DispatchBluetoothReply(aRunnable, BluetoothValue(), errorStr);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mDisconnectRunnable = aRunnable;

  sBluetoothGattClientInterface->Disconnect(
    client->mClientIf,
    aDeviceAddr,
    client->mConnId,
    new DisconnectResultHandler(client));
}

//
// Notification Handlers
//
void
BluetoothGattManager::RegisterClientNotification(int aStatus,
                                                 int aClientIf,
                                                 const BluetoothUuid& aAppUuid)
{
  BT_API2_LOGR("Client Registered, clientIf = %d", aClientIf);
  MOZ_ASSERT(NS_IsMainThread());

  nsString uuid;
  UuidToString(aAppUuid, uuid);

  size_t index = sClients->IndexOf(uuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);
  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aStatus) { // operation failed
    BT_API2_LOGR(
      "RegisterClient failed, clientIf = %d, status = %d, appUuid = %s",
      aClientIf, aStatus, NS_ConvertUTF16toUTF8(uuid).get());

    // Notify BluetoothGatt for client disconnected
    BluetoothSignal signal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      uuid, BluetoothValue(false)); // Disconnected
    bs->DistributeSignal(signal);

    // Reject the connect request
    if (client->mConnectRunnable) {
      NS_NAMED_LITERAL_STRING(errorStr,
                              "Connect failed due to registration failed");
      DispatchBluetoothReply(client->mConnectRunnable,
                             BluetoothValue(),
                             errorStr);
      client->mConnectRunnable = nullptr;
    }

    sClients->RemoveElement(client);
    return;
  }

  client->mClientIf = aClientIf;

  // Notify BluetoothGatt to update the clientIf
  BluetoothSignal signal(
    NS_LITERAL_STRING("ClientRegistered"),
    uuid, BluetoothValue(uint32_t(aClientIf)));
  bs->DistributeSignal(signal);

  // Client just registered, proceed remaining connect request.
  if (client->mConnectRunnable) {
    sBluetoothGattClientInterface->Connect(
      aClientIf, client->mDeviceAddr, true /* direct connect */,
      new ConnectResultHandler(client));
  }
}

void
BluetoothGattManager::ScanResultNotification(
  const nsAString& aBdAddr, int aRssi,
  const BluetoothGattAdvData& aAdvData)
{ }

void
BluetoothGattManager::ConnectNotification(int aConnId,
                                          int aStatus,
                                          int aClientIf,
                                          const nsAString& aDeviceAddr)
{
  BT_API2_LOGR();
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);
  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus) { // operation failed
    BT_API2_LOGR("Connect failed, clientIf = %d, connId = %d, status = %d",
                 aClientIf, aConnId, aStatus);

    // Notify BluetoothGatt that the client remains disconnected
    BluetoothSignal signal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      client->mAppUuid,
      BluetoothValue(false)); // Disconnected
    bs->DistributeSignal(signal);

    // Reject the connect request
    if (client->mConnectRunnable) {
      NS_NAMED_LITERAL_STRING(errorStr, "Connect failed");
      DispatchBluetoothReply(client->mConnectRunnable,
                             BluetoothValue(),
                             errorStr);
      client->mConnectRunnable = nullptr;
    }

    return;
  }

  client->mConnId = aConnId;

  // Notify BluetoothGatt for client connected
  BluetoothSignal signal(
    NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
    client->mAppUuid,
    BluetoothValue(true)); // Connected
  bs->DistributeSignal(signal);

  // Resolve the connect request
  if (client->mConnectRunnable) {
    DispatchBluetoothReply(client->mConnectRunnable,
                           BluetoothValue(true),
                           EmptyString());
    client->mConnectRunnable = nullptr;
  }
}

void
BluetoothGattManager::DisconnectNotification(int aConnId,
                                             int aStatus,
                                             int aClientIf,
                                             const nsAString& aDeviceAddr)
{
  BT_API2_LOGR();
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);
  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus) { // operation failed
    // Notify BluetoothGatt that the client remains connected
    BluetoothSignal signal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      client->mAppUuid,
      BluetoothValue(true)); // Connected
    bs->DistributeSignal(signal);

    // Reject the disconnect request
    if (client->mDisconnectRunnable) {
      NS_NAMED_LITERAL_STRING(errorStr, "Disconnect failed");
      DispatchBluetoothReply(client->mDisconnectRunnable,
                             BluetoothValue(),
                             errorStr);
      client->mDisconnectRunnable = nullptr;
    }

    return;
  }

  client->mConnId = 0;

  // Notify BluetoothGatt for client disconnected
  BluetoothSignal signal(
    NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
    client->mAppUuid,
    BluetoothValue(false)); // Disconnected
  bs->DistributeSignal(signal);

  // Resolve the disconnect request
  if (client->mDisconnectRunnable) {
    DispatchBluetoothReply(client->mDisconnectRunnable,
                           BluetoothValue(true),
                           EmptyString());
    client->mDisconnectRunnable = nullptr;
  }
}

void
BluetoothGattManager::SearchCompleteNotification(int aConnId, int aStatus)
{ }

void
BluetoothGattManager::SearchResultNotification(
  int aConnId, const BluetoothGattServiceId& aServiceId)
{ }

void
BluetoothGattManager::GetCharacteristicNotification(
  int aConnId, int aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId,
  int aCharProperty)
{ }

void
BluetoothGattManager::GetDescriptorNotification(
  int aConnId, int aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId,
  const BluetoothGattId& aDescriptorId)
{ }

void
BluetoothGattManager::GetIncludedServiceNotification(
  int aConnId, int aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattServiceId& aIncludedServId)
{ }

void
BluetoothGattManager::RegisterNotificationNotification(
  int aConnId, int aIsRegister, int aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId)
{ }

void
BluetoothGattManager::NotifyNotification(
  int aConnId, const BluetoothGattNotifyParam& aNotifyParam)
{ }

void
BluetoothGattManager::ReadCharacteristicNotification(
  int aConnId, int aStatus, const BluetoothGattReadParam& aReadParam)
{ }

void
BluetoothGattManager::WriteCharacteristicNotification(
  int aConnId, int aStatus, const BluetoothGattWriteParam& aWriteParam)
{ }

void
BluetoothGattManager::ReadDescriptorNotification(
  int aConnId, int aStatus, const BluetoothGattReadParam& aReadParam)
{ }

void
BluetoothGattManager::WriteDescriptorNotification(
  int aConnId, int aStatus, const BluetoothGattWriteParam& aWriteParam)
{ }

void
BluetoothGattManager::ExecuteWriteNotification(int aConnId, int aStatus)
{ }

void
BluetoothGattManager::ReadRemoteRssiNotification(int aClientIf,
                                                 const nsAString& aBdAddr,
                                                 int aRssi,
                                                 int aStatus)
{ }

void
BluetoothGattManager::ListenNotification(int aStatus,
                                         int aServerIf)
{ }

BluetoothGattManager::BluetoothGattManager()
{ }

BluetoothGattManager::~BluetoothGattManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

NS_IMETHODIMP
BluetoothGattManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothGattManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothGattManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

void
BluetoothGattManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  mInShutdown = true;
  sBluetoothGattManager = nullptr;
}

NS_IMPL_ISUPPORTS(BluetoothGattManager, nsIObserver)
