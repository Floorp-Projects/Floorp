/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattManager.h"

#include "BluetoothInterface.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

#define ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(runnable)                       \
  do {                                                                        \
    if (!sBluetoothGattInterface) {                                           \
      DispatchReplyError(runnable,                                            \
        NS_LITERAL_STRING("BluetoothGattClientInterface is not ready"));      \
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

static StaticAutoPtr<nsTArray<nsRefPtr<BluetoothGattClient> > > sClients;

struct BluetoothGattClientReadCharState
{
  bool mAuthRetry;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(bool aAuthRetry,
              BluetoothReplyRunnable* aRunnable)
  {
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset()
  {
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

struct BluetoothGattClientWriteCharState
{
  BluetoothGattWriteType mWriteType;
  nsTArray<uint8_t> mWriteValue;
  bool mAuthRetry;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(BluetoothGattWriteType aWriteType,
              const nsTArray<uint8_t>& aWriteValue,
              bool aAuthRetry,
              BluetoothReplyRunnable* aRunnable)
  {
    mWriteType = aWriteType;
    mWriteValue = aWriteValue;
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset()
  {
    mWriteType = GATT_WRITE_TYPE_NORMAL;
    mWriteValue.Clear();
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

struct BluetoothGattClientReadDescState
{
  bool mAuthRetry;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(bool aAuthRetry,
              BluetoothReplyRunnable* aRunnable)
  {
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset()
  {
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

struct BluetoothGattClientWriteDescState
{
  nsTArray<uint8_t> mWriteValue;
  bool mAuthRetry;
  nsRefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(const nsTArray<uint8_t>& aWriteValue,
              bool aAuthRetry,
              BluetoothReplyRunnable* aRunnable)
  {
    mWriteValue = aWriteValue;
    mAuthRetry = aAuthRetry;
    mRunnable = aRunnable;
  }

  void Reset()
  {
    mWriteValue.Clear();
    mAuthRetry = false;
    mRunnable = nullptr;
  }
};

class mozilla::dom::bluetooth::BluetoothGattClient final : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  BluetoothGattClient(const nsAString& aAppUuid, const nsAString& aDeviceAddr)
  : mAppUuid(aAppUuid)
  , mDeviceAddr(aDeviceAddr)
  , mClientIf(0)
  , mConnId(0)
  { }

  void NotifyDiscoverCompleted(bool aSuccess)
  {
    MOZ_ASSERT(!mAppUuid.IsEmpty());
    MOZ_ASSERT(mDiscoverRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify application to clear the cache values of
    // service/characteristic/descriptor.
    bs->DistributeSignal(NS_LITERAL_STRING("DiscoverCompleted"),
                         mAppUuid,
                         BluetoothValue(aSuccess));

    // Resolve/Reject the Promise.
    if (aSuccess) {
      DispatchReplySuccess(mDiscoverRunnable);
    } else {
      DispatchReplyError(mDiscoverRunnable,
                         NS_LITERAL_STRING("Discover failed"));
    }

    // Cleanup
    mServices.Clear();
    mIncludedServices.Clear();
    mCharacteristics.Clear();
    mDescriptors.Clear();
    mDiscoverRunnable = nullptr;
  }

  nsString mAppUuid;
  nsString mDeviceAddr;
  int mClientIf;
  int mConnId;
  nsRefPtr<BluetoothReplyRunnable> mStartLeScanRunnable;
  nsRefPtr<BluetoothReplyRunnable> mConnectRunnable;
  nsRefPtr<BluetoothReplyRunnable> mDisconnectRunnable;
  nsRefPtr<BluetoothReplyRunnable> mDiscoverRunnable;
  nsRefPtr<BluetoothReplyRunnable> mReadRemoteRssiRunnable;
  nsRefPtr<BluetoothReplyRunnable> mRegisterNotificationsRunnable;
  nsRefPtr<BluetoothReplyRunnable> mDeregisterNotificationsRunnable;
  nsRefPtr<BluetoothReplyRunnable> mUnregisterClientRunnable;

  BluetoothGattClientReadCharState mReadCharacteristicState;
  BluetoothGattClientWriteCharState mWriteCharacteristicState;
  BluetoothGattClientReadDescState mReadDescriptorState;
  BluetoothGattClientWriteDescState mWriteDescriptorState;

  /**
   * These temporary arrays are used only during discover operations.
   * All of them are empty if there are no ongoing discover operations.
   */
  nsTArray<BluetoothGattServiceId> mServices;
  nsTArray<BluetoothGattServiceId> mIncludedServices;
  nsTArray<BluetoothGattCharAttribute> mCharacteristics;
  nsTArray<BluetoothGattId> mDescriptors;

private:
  ~BluetoothGattClient()
  { }
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

class ConnIdComparator
{
public:
  bool Equals(const nsRefPtr<BluetoothGattClient>& aClient,
              int aConnId) const
  {
    return aClient->mConnId == aConnId;
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

class BluetoothGattManager::InitGattResultHandler final
  : public BluetoothGattResultHandler
{
public:
  InitGattResultHandler(BluetoothProfileResultHandler* aRes)
  : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::Init failed: %d",
               (int)aStatus);
    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void Init() override
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

class BluetoothGattManager::CleanupResultHandler final
  : public BluetoothGattResultHandler
{
public:
  CleanupResultHandler(BluetoothProfileResultHandler* aRes)
  : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::Cleanup failed: %d",
               (int)aStatus);
    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void Cleanup() override
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

class BluetoothGattManager::CleanupResultHandlerRunnable final
  : public nsRunnable
{
public:
  CleanupResultHandlerRunnable(BluetoothProfileResultHandler* aRes)
  : mRes(aRes)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
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

class BluetoothGattManager::RegisterClientResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  RegisterClientResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::RegisterClient failed: %d",
               (int)aStatus);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      mClient->mAppUuid,
      BluetoothValue(false)); // Disconnected

    // Reject the connect request
    if (mClient->mConnectRunnable) {
      DispatchReplyError(mClient->mConnectRunnable,
                         NS_LITERAL_STRING("Register GATT client failed"));
      mClient->mConnectRunnable = nullptr;
    }

    sClients->RemoveElement(mClient);
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::UnregisterClientResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  UnregisterClientResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void UnregisterClient() override
  {
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt to clear the clientIf
    bs->DistributeSignal(
      NS_LITERAL_STRING("ClientUnregistered"),
      mClient->mAppUuid,
      BluetoothValue(true));

    // Resolve the unregister request
    DispatchReplySuccess(mClient->mUnregisterClientRunnable);
    mClient->mUnregisterClientRunnable = nullptr;

    sClients->RemoveElement(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::UnregisterClient failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);

    // Reject the unregister request
    DispatchReplyError(mClient->mUnregisterClientRunnable,
                       NS_LITERAL_STRING("Unregister GATT client failed"));
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
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mUnregisterClientRunnable = aRunnable;

  sBluetoothGattClientInterface->UnregisterClient(
    aClientIf,
    new UnregisterClientResultHandler(client));
}

class BluetoothGattManager::StartLeScanResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  StartLeScanResultHandler(BluetoothGattClient* aClient)
    : mClient(aClient)
  { }

  void Scan() override
  {
    MOZ_ASSERT(mClient > 0);

    DispatchReplySuccess(mClient->mStartLeScanRunnable,
                         BluetoothValue(mClient->mAppUuid));
    mClient->mStartLeScanRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::StartLeScan failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mStartLeScanRunnable);

    // Unregister client if startLeScan failed
    if (mClient->mClientIf > 0) {
      BluetoothGattManager* gattManager = BluetoothGattManager::Get();
      NS_ENSURE_TRUE_VOID(gattManager);

      nsRefPtr<BluetoothVoidReplyRunnable> result =
        new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterClient(mClient->mClientIf, result);
    }

    DispatchReplyError(mClient->mStartLeScanRunnable, aStatus);
    mClient->mStartLeScanRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::StopLeScanResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
   StopLeScanResultHandler(BluetoothReplyRunnable* aRunnable, int aClientIf)
     : mRunnable(aRunnable), mClientIf(aClientIf)
  { }

  void Scan() override
  {
    DispatchReplySuccess(mRunnable);

    // Unregister client when stopLeScan succeeded
    if (mClientIf > 0) {
      BluetoothGattManager* gattManager = BluetoothGattManager::Get();
      NS_ENSURE_TRUE_VOID(gattManager);

      nsRefPtr<BluetoothVoidReplyRunnable> result =
        new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterClient(mClientIf, result);
    }
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::StopLeScan failed: %d",
                (int)aStatus);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  nsRefPtr<BluetoothReplyRunnable> mRunnable;
  int mClientIf;
};

void
BluetoothGattManager::StartLeScan(const nsTArray<nsString>& aServiceUuids,
                                  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  nsString appUuidStr;
  GenerateUuid(appUuidStr);

  size_t index = sClients->IndexOf(appUuidStr, 0 /* Start */, UuidComparator());

  // Reject the startLeScan request if the clientIf is being used.
  if (NS_WARN_IF(index != sClients->NoIndex)) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("start LE scan failed"));
    return;
  }

  index = sClients->Length();
  sClients->AppendElement(new BluetoothGattClient(appUuidStr, EmptyString()));
  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mStartLeScanRunnable = aRunnable;

  BluetoothUuid appUuid;
  StringToUuid(NS_ConvertUTF16toUTF8(appUuidStr).get(), appUuid);

  // 'startLeScan' will be proceeded after client registered
  sBluetoothGattClientInterface->RegisterClient(
    appUuid, new RegisterClientResultHandler(client));
}

void
BluetoothGattManager::StopLeScan(const nsAString& aScanUuid,
                                 BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aScanUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    // Reject the stop LE scan request
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("StopLeScan failed"));
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  sBluetoothGattClientInterface->Scan(
    client->mClientIf,
    false /* Stop */,
    new StopLeScanResultHandler(aRunnable, client->mClientIf));
}

class BluetoothGattManager::ConnectResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  ConnectResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::Connect failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mConnectRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      mClient->mAppUuid,
      BluetoothValue(false)); // Disconnected

    // Reject the connect request
    DispatchReplyError(mClient->mConnectRunnable,
                       NS_LITERAL_STRING("Connect failed"));
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
                                           TRANSPORT_AUTO,
                                           new ConnectResultHandler(client));
  } else {
    BluetoothUuid uuid;
    StringToUuid(NS_ConvertUTF16toUTF8(aAppUuid).get(), uuid);

    // connect will be proceeded after client registered
    sBluetoothGattClientInterface->RegisterClient(
      uuid, new RegisterClientResultHandler(client));
  }
}

class BluetoothGattManager::DisconnectResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  DisconnectResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::Disconnect failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mDisconnectRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGatt that the client remains connected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      mClient->mAppUuid,
      BluetoothValue(true)); // Connected

    // Reject the disconnect request
    DispatchReplyError(mClient->mDisconnectRunnable,
                       NS_LITERAL_STRING("Disconnect failed"));
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
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
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

class BluetoothGattManager::DiscoverResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  DiscoverResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::Discover failed: %d",
               (int)aStatus);

    mClient->NotifyDiscoverCompleted(false);
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::Discover(const nsAString& aAppUuid,
                               BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mConnId > 0);
  MOZ_ASSERT(!client->mDiscoverRunnable);

  client->mDiscoverRunnable = aRunnable;

  /**
   * Discover all services/characteristics/descriptors offered by the remote
   * GATT server.
   *
   * The discover procesure includes following steps.
   * 1) Discover all services.
   * 2) After all services are discovered, for each service S, we will do
   *    following actions.
   *    2-1) Discover all included services of service S.
   *    2-2) Discover all characteristics of service S.
   *    2-3) Discover all descriptors of those characteristics discovered in
   *         2-2).
   */
  sBluetoothGattClientInterface->SearchService(
    client->mConnId,
    true, // search all services
    BluetoothUuid(),
    new DiscoverResultHandler(client));
}

class BluetoothGattManager::ReadRemoteRssiResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  ReadRemoteRssiResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::ReadRemoteRssi failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mReadRemoteRssiRunnable);

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Reject the read remote rssi request
    DispatchReplyError(mClient->mReadRemoteRssiRunnable,
                       NS_LITERAL_STRING("ReadRemoteRssi failed"));
    mClient->mReadRemoteRssiRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::ReadRemoteRssi(int aClientIf,
                                     const nsAString& aDeviceAddr,
                                     BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mReadRemoteRssiRunnable = aRunnable;

  sBluetoothGattClientInterface->ReadRemoteRssi(
    aClientIf, aDeviceAddr,
    new ReadRemoteRssiResultHandler(client));
}

class BluetoothGattManager::RegisterNotificationsResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  RegisterNotificationsResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void RegisterNotification() override
  {
    MOZ_ASSERT(mClient->mRegisterNotificationsRunnable);

    /**
     * Resolve the promise directly if we successfully issued this request to
     * stack.
     *
     * We resolve the promise here since bluedroid stack always returns
     * incorrect connId in |RegisterNotificationNotification| and we cannot map
     * back to the target client because of it.
     * Please see Bug 1149043 for more information.
     */
    DispatchReplySuccess(mClient->mRegisterNotificationsRunnable);
    mClient->mRegisterNotificationsRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING(
      "BluetoothGattClientInterface::RegisterNotifications failed: %d",
      (int)aStatus);
    MOZ_ASSERT(mClient->mRegisterNotificationsRunnable);

    DispatchReplyError(mClient->mRegisterNotificationsRunnable,
                       NS_LITERAL_STRING("RegisterNotifications failed"));
    mClient->mRegisterNotificationsRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::RegisterNotifications(
  const nsAString& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Reject the request if there is an ongoing request or client is already
  // disconnected
  if (client->mRegisterNotificationsRunnable || client->mConnId <= 0) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("RegisterNotifications failed"));
    return;
  }

  client->mRegisterNotificationsRunnable = aRunnable;

  sBluetoothGattClientInterface->RegisterNotification(
    client->mClientIf, client->mDeviceAddr, aServId, aCharId,
    new RegisterNotificationsResultHandler(client));
}

class BluetoothGattManager::DeregisterNotificationsResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  DeregisterNotificationsResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void DeregisterNotification() override
  {
    MOZ_ASSERT(mClient->mDeregisterNotificationsRunnable);

    /**
     * Resolve the promise directly if we successfully issued this request to
     * stack.
     *
     * We resolve the promise here since bluedroid stack always returns
     * incorrect connId in |RegisterNotificationNotification| and we cannot map
     * back to the target client because of it.
     * Please see Bug 1149043 for more information.
     */
    DispatchReplySuccess(mClient->mDeregisterNotificationsRunnable);
    mClient->mDeregisterNotificationsRunnable = nullptr;
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING(
      "BluetoothGattClientInterface::DeregisterNotifications failed: %d",
      (int)aStatus);
    MOZ_ASSERT(mClient->mDeregisterNotificationsRunnable);

    DispatchReplyError(mClient->mDeregisterNotificationsRunnable,
                       NS_LITERAL_STRING("DeregisterNotifications failed"));
    mClient->mDeregisterNotificationsRunnable = nullptr;
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::DeregisterNotifications(
  const nsAString& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Reject the request if there is an ongoing request
  if (client->mDeregisterNotificationsRunnable) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("DeregisterNotifications failed"));
    return;
  }

  client->mDeregisterNotificationsRunnable = aRunnable;

  sBluetoothGattClientInterface->DeregisterNotification(
    client->mClientIf, client->mDeviceAddr, aServId, aCharId,
    new DeregisterNotificationsResultHandler(client));
}

class BluetoothGattManager::ReadCharacteristicValueResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  ReadCharacteristicValueResultHandler(BluetoothGattClient* aClient)
    : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::ReadCharacteristicValue failed" \
               ": %d", (int)aStatus);
    MOZ_ASSERT(mClient->mReadCharacteristicState.mRunnable);

    nsRefPtr<BluetoothReplyRunnable> runnable =
      mClient->mReadCharacteristicState.mRunnable;
    mClient->mReadCharacteristicState.Reset();

    // Reject the read characteristic value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("ReadCharacteristicValue failed"));
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::ReadCharacteristicValue(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent reading requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mReadCharacteristicState.mRunnable) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("ReadCharacteristicValue failed"));
    return;
  }

  client->mReadCharacteristicState.Assign(false, aRunnable);

  /**
   * First, read the characteristic value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to read through an authenticated physical link.
   */
  sBluetoothGattClientInterface->ReadCharacteristic(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    GATT_AUTH_REQ_NONE,
    new ReadCharacteristicValueResultHandler(client));
}

class BluetoothGattManager::WriteCharacteristicValueResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  WriteCharacteristicValueResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::WriteCharacteristicValue failed" \
               ": %d", (int)aStatus);
    MOZ_ASSERT(mClient->mWriteCharacteristicState.mRunnable);

    nsRefPtr<BluetoothReplyRunnable> runnable =
      mClient->mWriteCharacteristicState.mRunnable;
    mClient->mWriteCharacteristicState.Reset();

    // Reject the write characteristic value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("WriteCharacteristicValue failed"));
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::WriteCharacteristicValue(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattWriteType& aWriteType,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent writing requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mWriteCharacteristicState.mRunnable) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("WriteCharacteristicValue failed"));
    return;
  }

  client->mWriteCharacteristicState.Assign(aWriteType, aValue, false, aRunnable);

  /**
   * First, write the characteristic value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to write through an authenticated physical link.
   */
  sBluetoothGattClientInterface->WriteCharacteristic(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    aWriteType,
    GATT_AUTH_REQ_NONE,
    aValue,
    new WriteCharacteristicValueResultHandler(client));
}

class BluetoothGattManager::ReadDescriptorValueResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  ReadDescriptorValueResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::ReadDescriptorValue failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mReadDescriptorState.mRunnable);

    nsRefPtr<BluetoothReplyRunnable> runnable =
      mClient->mReadDescriptorState.mRunnable;
    mClient->mReadDescriptorState.Reset();

    // Reject the read descriptor value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("ReadDescriptorValue failed"));
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::ReadDescriptorValue(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent reading requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mReadDescriptorState.mRunnable) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("ReadDescriptorValue failed"));
    return;
  }

  client->mReadDescriptorState.Assign(false, aRunnable);

  /**
   * First, read the descriptor value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to read through an authenticated physical link.
   */
  sBluetoothGattClientInterface->ReadDescriptor(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    aDescriptorId,
    GATT_AUTH_REQ_NONE,
    new ReadDescriptorValueResultHandler(client));
}

class BluetoothGattManager::WriteDescriptorValueResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  WriteDescriptorValueResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattClientInterface::WriteDescriptorValue failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mWriteDescriptorState.mRunnable);

    nsRefPtr<BluetoothReplyRunnable> runnable =
      mClient->mWriteDescriptorState.mRunnable;
    mClient->mWriteDescriptorState.Reset();

    // Reject the write descriptor value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("WriteDescriptorValue failed"));
  }

private:
  nsRefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::WriteDescriptorValue(
  const nsAString& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_CLIENT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  /**
   * Reject subsequent writing requests to follow ATT sequential protocol that
   * handles one request at a time. Otherwise underlying layers would drop the
   * subsequent requests silently.
   *
   * Bug 1147776 intends to solve a larger problem that other kind of requests
   * may still interfere the ongoing request.
   */
  if (client->mWriteDescriptorState.mRunnable) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("WriteDescriptorValue failed"));
    return;
  }

  /**
   * First, write the descriptor value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to write through an authenticated physical link.
   */
  client->mWriteDescriptorState.Assign(aValue, false, aRunnable);

  sBluetoothGattClientInterface->WriteDescriptor(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    aDescriptorId,
    GATT_WRITE_TYPE_NORMAL,
    GATT_AUTH_REQ_NONE,
    aValue,
    new WriteDescriptorValueResultHandler(client));
}

//
// Notification Handlers
//
void
BluetoothGattManager::RegisterClientNotification(BluetoothGattStatus aStatus,
                                                 int aClientIf,
                                                 const BluetoothUuid& aAppUuid)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString uuid;
  UuidToString(aAppUuid, uuid);

  size_t index = sClients->IndexOf(uuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aStatus != GATT_STATUS_SUCCESS) {
    BT_LOGD("RegisterClient failed: clientIf = %d, status = %d, appUuid = %s",
            aClientIf, aStatus, NS_ConvertUTF16toUTF8(uuid).get());

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      uuid, BluetoothValue(false)); // Disconnected

    if (client->mStartLeScanRunnable) {
      // Reject the LE scan request
      DispatchReplyError(client->mStartLeScanRunnable,
                         NS_LITERAL_STRING(
                           "StartLeScan failed due to registration failed"));
      client->mStartLeScanRunnable = nullptr;
    } else if (client->mConnectRunnable) {
      // Reject the connect request
      DispatchReplyError(client->mConnectRunnable,
                         NS_LITERAL_STRING(
                           "Connect failed due to registration failed"));
      client->mConnectRunnable = nullptr;
    }

    sClients->RemoveElement(client);
    return;
  }

  client->mClientIf = aClientIf;

  // Notify BluetoothGatt to update the clientIf
  bs->DistributeSignal(
    NS_LITERAL_STRING("ClientRegistered"),
    uuid, BluetoothValue(uint32_t(aClientIf)));

  if (client->mStartLeScanRunnable) {
    // Client just registered, proceed remaining startLeScan request.
    sBluetoothGattClientInterface->Scan(
      aClientIf, true /* start */,
      new StartLeScanResultHandler(client));
  } else if (client->mConnectRunnable) {
    // Client just registered, proceed remaining connect request.
    sBluetoothGattClientInterface->Connect(
      aClientIf, client->mDeviceAddr, true /* direct connect */,
      TRANSPORT_AUTO,
      new ConnectResultHandler(client));
  }
}

class BluetoothGattManager::ScanDeviceTypeResultHandler final
  : public BluetoothGattClientResultHandler
{
public:
  ScanDeviceTypeResultHandler(const nsAString& aBdAddr, int aRssi,
                              const BluetoothGattAdvData& aAdvData)
  : mBdAddr(aBdAddr)
  , mRssi(static_cast<int32_t>(aRssi))
  {
    mAdvData.AppendElements(aAdvData.mAdvData, sizeof(aAdvData.mAdvData));
  }

  void GetDeviceType(BluetoothTypeOfDevice type)
  {
    DistributeSignalDeviceFound(type);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    DistributeSignalDeviceFound(TYPE_OF_DEVICE_BLE);
  }

private:
  void DistributeSignalDeviceFound(BluetoothTypeOfDevice type)
  {
    MOZ_ASSERT(NS_IsMainThread());

    InfallibleTArray<BluetoothNamedValue> properties;

    BT_APPEND_NAMED_VALUE(properties, "Address", mBdAddr);
    BT_APPEND_NAMED_VALUE(properties, "Rssi", mRssi);
    BT_APPEND_NAMED_VALUE(properties, "GattAdv", mAdvData);
    BT_APPEND_NAMED_VALUE(properties, "Type", static_cast<uint32_t>(type));

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    bs->DistributeSignal(NS_LITERAL_STRING("LeDeviceFound"),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         BluetoothValue(properties));
  }

  nsString mBdAddr;
  int32_t mRssi;
  nsTArray<uint8_t> mAdvData;
};

void
BluetoothGattManager::ScanResultNotification(
  const nsAString& aBdAddr, int aRssi,
  const BluetoothGattAdvData& aAdvData)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothGattClientInterface);

  // Distribute "LeDeviceFound" signal after we know the corresponding
  // BluetoothTypeOfDevice of the device
  sBluetoothGattClientInterface->GetDeviceType(
    aBdAddr,
    new ScanDeviceTypeResultHandler(aBdAddr, aRssi, aAdvData));
}

void
BluetoothGattManager::ConnectNotification(int aConnId,
                                          BluetoothGattStatus aStatus,
                                          int aClientIf,
                                          const nsAString& aDeviceAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    BT_LOGD("Connect failed: clientIf = %d, connId = %d, status = %d",
            aClientIf, aConnId, aStatus);

    // Notify BluetoothGatt that the client remains disconnected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      client->mAppUuid,
      BluetoothValue(false)); // Disconnected

    // Reject the connect request
    if (client->mConnectRunnable) {
      DispatchReplyError(client->mConnectRunnable,
                         NS_LITERAL_STRING("Connect failed"));
      client->mConnectRunnable = nullptr;
    }

    return;
  }

  client->mConnId = aConnId;

  // Notify BluetoothGatt for client connected
  bs->DistributeSignal(
    NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
    client->mAppUuid,
    BluetoothValue(true)); // Connected

  // Resolve the connect request
  if (client->mConnectRunnable) {
    DispatchReplySuccess(client->mConnectRunnable);
    client->mConnectRunnable = nullptr;
  }
}

void
BluetoothGattManager::DisconnectNotification(int aConnId,
                                             BluetoothGattStatus aStatus,
                                             int aClientIf,
                                             const nsAString& aDeviceAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    // Notify BluetoothGatt that the client remains connected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      client->mAppUuid,
      BluetoothValue(true)); // Connected

    // Reject the disconnect request
    if (client->mDisconnectRunnable) {
      DispatchReplyError(client->mDisconnectRunnable,
                         NS_LITERAL_STRING("Disconnect failed"));
      client->mDisconnectRunnable = nullptr;
    }

    return;
  }

  client->mConnId = 0;

  // Notify BluetoothGatt for client disconnected
  bs->DistributeSignal(
    NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
    client->mAppUuid,
    BluetoothValue(false)); // Disconnected

  // Resolve the disconnect request
  if (client->mDisconnectRunnable) {
    DispatchReplySuccess(client->mDisconnectRunnable);
    client->mDisconnectRunnable = nullptr;
  }
}

void
BluetoothGattManager::SearchCompleteNotification(int aConnId,
                                                 BluetoothGattStatus aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */,
                                   ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  if (aStatus != GATT_STATUS_SUCCESS) {
    client->NotifyDiscoverCompleted(false);
    return;
  }

  // Notify BluetoothGatt to create all services
  bs->DistributeSignal(NS_LITERAL_STRING("ServicesDiscovered"),
                       client->mAppUuid,
                       BluetoothValue(client->mServices));

  // All services are discovered, continue to search included services of each
  // service if existed, otherwise, notify application that discover completed
  if (!client->mServices.IsEmpty()) {
    sBluetoothGattClientInterface->GetIncludedService(
      aConnId,
      client->mServices[0], // start from first service
      true, // first included service
      BluetoothGattServiceId(),
      new DiscoverResultHandler(client));
  } else {
    client->NotifyDiscoverCompleted(true);
  }
}

void
BluetoothGattManager::SearchResultNotification(
  int aConnId, const BluetoothGattServiceId& aServiceId)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */,
                                   ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  // Save to mServices for distributing to application and discovering
  // included services, characteristics of this service later
  sClients->ElementAt(index)->mServices.AppendElement(aServiceId);
}

void
BluetoothGattManager::GetCharacteristicNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId,
  const BluetoothGattCharProp& aCharProperty)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */,
                                   ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  if (aStatus == GATT_STATUS_SUCCESS) {
    BluetoothGattCharAttribute attribute;
    attribute.mId = aCharId;
    attribute.mProperties = aCharProperty;
    attribute.mWriteType =
      aCharProperty & GATT_CHAR_PROP_BIT_WRITE_NO_RESPONSE
        ? GATT_WRITE_TYPE_NO_RESPONSE
        : GATT_WRITE_TYPE_NORMAL;

    // Save to mCharacteristics for distributing to applications and
    // discovering descriptors of this characteristic later
    client->mCharacteristics.AppendElement(attribute);

    // Get next characteristic of this service
    sBluetoothGattClientInterface->GetCharacteristic(
      aConnId,
      aServiceId,
      false,
      aCharId,
      new DiscoverResultHandler(client));
  } else { // all characteristics of this service are discovered
    // Notify BluetoothGatt to make BluetoothGattService create characteristics
    // then proceed
    nsTArray<BluetoothNamedValue> values;
    BT_APPEND_NAMED_VALUE(values, "serviceId", aServiceId);
    BT_APPEND_NAMED_VALUE(values, "characteristics", client->mCharacteristics);

    bs->DistributeSignal(NS_LITERAL_STRING("CharacteristicsDiscovered"),
                         client->mAppUuid,
                         BluetoothValue(values));

    ProceedDiscoverProcess(client, aServiceId);
  }
}

void
BluetoothGattManager::GetDescriptorNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId,
  const BluetoothGattId& aDescriptorId)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */,
                                   ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  if (aStatus == GATT_STATUS_SUCCESS) {
    // Save to mDescriptors for distributing to applications
    client->mDescriptors.AppendElement(aDescriptorId);

    // Get next descriptor of this characteristic
    sBluetoothGattClientInterface->GetDescriptor(
      aConnId,
      aServiceId,
      aCharId,
      false,
      aDescriptorId,
      new DiscoverResultHandler(client));
  } else { // all descriptors of this characteristic are discovered
    // Notify BluetoothGatt to make BluetoothGattCharacteristic create
    // descriptors then proceed
    nsTArray<BluetoothNamedValue> values;
    BT_APPEND_NAMED_VALUE(values, "serviceId", aServiceId);
    BT_APPEND_NAMED_VALUE(values, "characteristicId", aCharId);
    BT_APPEND_NAMED_VALUE(values, "descriptors", client->mDescriptors);

    bs->DistributeSignal(NS_LITERAL_STRING("DescriptorsDiscovered"),
                         client->mAppUuid,
                         BluetoothValue(values));
    client->mDescriptors.Clear();

    ProceedDiscoverProcess(client, aServiceId);
  }
}

void
BluetoothGattManager::GetIncludedServiceNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattServiceId& aIncludedServId)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */,
                                   ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  if (aStatus == GATT_STATUS_SUCCESS) {
    // Save to mIncludedServices for distributing to applications
    client->mIncludedServices.AppendElement(aIncludedServId);

    // Get next included service of this service
    sBluetoothGattClientInterface->GetIncludedService(
      aConnId,
      aServiceId,
      false,
      aIncludedServId,
      new DiscoverResultHandler(client));
  } else { // all included services of this service are discovered
    // Notify BluetoothGatt to make BluetoothGattService create included
    // services
    nsTArray<BluetoothNamedValue> values;
    BT_APPEND_NAMED_VALUE(values, "serviceId", aServiceId);
    BT_APPEND_NAMED_VALUE(values, "includedServices",
                          client->mIncludedServices);

    bs->DistributeSignal(NS_LITERAL_STRING("IncludedServicesDiscovered"),
                         client->mAppUuid,
                         BluetoothValue(values));
    client->mIncludedServices.Clear();

    // Start to discover characteristics of this service
    sBluetoothGattClientInterface->GetCharacteristic(
      aConnId,
      aServiceId,
      true, // first characteristic
      BluetoothGattId(),
      new DiscoverResultHandler(client));
  }
}

void
BluetoothGattManager::RegisterNotificationNotification(
  int aConnId, int aIsRegister, BluetoothGattStatus aStatus,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharId)
{
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGD("aStatus = %d, aConnId = %d, aIsRegister = %d",
          aStatus, aConnId, aIsRegister);

  /**
   * FIXME: Bug 1149043
   *
   * aConnId reported by bluedroid stack is wrong, with these limited
   * information we have, we currently cannot map back to the client from this
   * callback. Therefore, we resolve/reject the Promise for registering or
   * deregistering notifications in their result handlers instead of this
   * callback.
   * We should resolve/reject the Promise for registering or deregistering
   * notifications here if this bluedroid stack bug is fixed.
   *
   * Please see Bug 1149043 for more information.
   */
}

void
BluetoothGattManager::NotifyNotification(
  int aConnId, const BluetoothGattNotifyParam& aNotifyParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Notify BluetoothGattCharacteristic to update characteristic value
  nsString path;
  GeneratePathFromGattId(aNotifyParam.mCharId, path);

  nsTArray<uint8_t> value;
  value.AppendElements(aNotifyParam.mValue, aNotifyParam.mLength);

  bs->DistributeSignal(NS_LITERAL_STRING("CharacteristicValueUpdated"),
                       path,
                       BluetoothValue(value));

  // Notify BluetoothGatt for characteristic changed
  nsTArray<BluetoothNamedValue> ids;
  ids.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("serviceId"),
                                        aNotifyParam.mServiceId));
  ids.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("charId"),
                                        aNotifyParam.mCharId));

  bs->DistributeSignal(NS_LITERAL_STRING(GATT_CHARACTERISTIC_CHANGED_ID),
                       client->mAppUuid,
                       BluetoothValue(ids));
}

void
BluetoothGattManager::ReadCharacteristicNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattReadParam& aReadParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mReadCharacteristicState.mRunnable);
  nsRefPtr<BluetoothReplyRunnable> runnable =
    client->mReadCharacteristicState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mReadCharacteristicState.Reset();
    // Notify BluetoothGattCharacteristic to update characteristic value
    nsString path;
    GeneratePathFromGattId(aReadParam.mCharId, path);

    nsTArray<uint8_t> value;
    value.AppendElements(aReadParam.mValue, aReadParam.mValueLength);

    bs->DistributeSignal(NS_LITERAL_STRING("CharacteristicValueUpdated"),
                         path,
                         BluetoothValue(value));

    // Notify BluetoothGatt for characteristic changed
    nsTArray<BluetoothNamedValue> ids;
    ids.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("serviceId"),
                                          aReadParam.mServiceId));
    ids.AppendElement(BluetoothNamedValue(NS_LITERAL_STRING("charId"),
                                          aReadParam.mCharId));

    bs->DistributeSignal(NS_LITERAL_STRING(GATT_CHARACTERISTIC_CHANGED_ID),
                         client->mAppUuid,
                         BluetoothValue(ids));

    // Resolve the promise
    DispatchReplySuccess(runnable, BluetoothValue(value));
  } else if (!client->mReadCharacteristicState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    client->mReadCharacteristicState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattClientInterface->ReadCharacteristic(
      aConnId,
      aReadParam.mServiceId,
      aReadParam.mCharId,
      GATT_AUTH_REQ_MITM,
      new ReadCharacteristicValueResultHandler(client));
  } else {
    client->mReadCharacteristicState.Reset();
    // Reject the promise
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("ReadCharacteristicValue failed"));
  }
}

void
BluetoothGattManager::WriteCharacteristicNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattWriteParam& aWriteParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mWriteCharacteristicState.mRunnable);
  nsRefPtr<BluetoothReplyRunnable> runnable =
    client->mWriteCharacteristicState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mWriteCharacteristicState.Reset();
    // Resolve the promise
    DispatchReplySuccess(runnable);
  } else if (!client->mWriteCharacteristicState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    client->mWriteCharacteristicState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattClientInterface->WriteCharacteristic(
      aConnId,
      aWriteParam.mServiceId,
      aWriteParam.mCharId,
      client->mWriteCharacteristicState.mWriteType,
      GATT_AUTH_REQ_MITM,
      client->mWriteCharacteristicState.mWriteValue,
      new WriteCharacteristicValueResultHandler(client));
  } else {
    client->mWriteCharacteristicState.Reset();
    // Reject the promise
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("WriteCharacteristicValue failed"));
  }
}

void
BluetoothGattManager::ReadDescriptorNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattReadParam& aReadParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mReadDescriptorState.mRunnable);
  nsRefPtr<BluetoothReplyRunnable> runnable =
    client->mReadDescriptorState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mReadDescriptorState.Reset();
    // Notify BluetoothGattDescriptor to update descriptor value
    nsString path;
    GeneratePathFromGattId(aReadParam.mDescriptorId, path);

    nsTArray<uint8_t> value;
    value.AppendElements(aReadParam.mValue, aReadParam.mValueLength);

    bs->DistributeSignal(NS_LITERAL_STRING("DescriptorValueUpdated"),
                         path,
                         BluetoothValue(value));

    // Resolve the promise
    DispatchReplySuccess(runnable, BluetoothValue(value));
  } else if (!client->mReadDescriptorState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    client->mReadDescriptorState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattClientInterface->ReadDescriptor(
      aConnId,
      aReadParam.mServiceId,
      aReadParam.mCharId,
      aReadParam.mDescriptorId,
      GATT_AUTH_REQ_MITM,
      new ReadDescriptorValueResultHandler(client));
  } else {
    client->mReadDescriptorState.Reset();
    // Reject the promise
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("ReadDescriptorValue failed"));
  }
}

void
BluetoothGattManager::WriteDescriptorNotification(
  int aConnId, BluetoothGattStatus aStatus,
  const BluetoothGattWriteParam& aWriteParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mWriteDescriptorState.mRunnable);
  nsRefPtr<BluetoothReplyRunnable> runnable =
    client->mWriteDescriptorState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mWriteDescriptorState.Reset();
    // Resolve the promise
    DispatchReplySuccess(runnable);
  } else if (!client->mWriteDescriptorState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    client->mWriteDescriptorState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattClientInterface->WriteDescriptor(
      aConnId,
      aWriteParam.mServiceId,
      aWriteParam.mCharId,
      aWriteParam.mDescriptorId,
      GATT_WRITE_TYPE_NORMAL,
      GATT_AUTH_REQ_MITM,
      client->mWriteDescriptorState.mWriteValue,
      new WriteDescriptorValueResultHandler(client));
  } else {
    client->mWriteDescriptorState.Reset();
    // Reject the promise
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("WriteDescriptorValue failed"));
  }
}

void
BluetoothGattManager::ExecuteWriteNotification(int aConnId,
                                               BluetoothGattStatus aStatus)
{ }

void
BluetoothGattManager::ReadRemoteRssiNotification(int aClientIf,
                                                 const nsAString& aBdAddr,
                                                 int aRssi,
                                                 BluetoothGattStatus aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   ClientIfComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  nsRefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) { // operation failed
    BT_LOGD("ReadRemoteRssi failed: clientIf = %d, bdAddr = %s, rssi = %d, " \
            "status = %d", aClientIf, NS_ConvertUTF16toUTF8(aBdAddr).get(),
            aRssi, (int)aStatus);

    // Reject the read remote rssi request
    if (client->mReadRemoteRssiRunnable) {
      DispatchReplyError(client->mReadRemoteRssiRunnable,
                         NS_LITERAL_STRING("ReadRemoteRssi failed"));
      client->mReadRemoteRssiRunnable = nullptr;
    }

    return;
  }

  // Resolve the read remote rssi request
  if (client->mReadRemoteRssiRunnable) {
    DispatchReplySuccess(client->mReadRemoteRssiRunnable,
                         BluetoothValue(static_cast<int32_t>(aRssi)));
    client->mReadRemoteRssiRunnable = nullptr;
  }
}

void
BluetoothGattManager::ListenNotification(BluetoothGattStatus aStatus,
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
  sClients = nullptr;
}

void
BluetoothGattManager::ProceedDiscoverProcess(
  BluetoothGattClient* aClient,
  const BluetoothGattServiceId& aServiceId)
{
  /**
   * This function will be called to decide how to proceed the discover process
   * after discovering all characteristics of a given service, or after
   * discovering all descriptors of a given characteristic.
   *
   * There are three cases here,
   * 1) mCharacteristics is not empty:
   *      Proceed to discover descriptors of the first saved characteristic.
   * 2) mCharacteristics is empty but mServices is not empty:
   *      This service does not have any saved characteristics left, proceed to
   *      discover included services of the next service.
   * 3) Both arrays are already empty:
   *      Discover is done, notify application.
   */
  if (!aClient->mCharacteristics.IsEmpty()) {
    sBluetoothGattClientInterface->GetDescriptor(
      aClient->mConnId,
      aServiceId,
      aClient->mCharacteristics[0].mId,
      true, // first descriptor
      BluetoothGattId(),
      new DiscoverResultHandler(aClient));
    aClient->mCharacteristics.RemoveElementAt(0);
  } else if (!aClient->mServices.IsEmpty()) {
    sBluetoothGattClientInterface->GetIncludedService(
      aClient->mConnId,
      aClient->mServices[0],
      true, // first included service
      BluetoothGattServiceId(),
      new DiscoverResultHandler(aClient));
    aClient->mServices.RemoveElementAt(0);
  } else {
    aClient->NotifyDiscoverCompleted(true);
  }
}

NS_IMPL_ISUPPORTS(BluetoothGattManager, nsIObserver)
