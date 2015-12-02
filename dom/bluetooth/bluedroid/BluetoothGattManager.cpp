/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattManager.h"

#include "BluetoothHashKeys.h"
#include "BluetoothInterface.h"
#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsDataHashtable.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

#define ENSURE_GATT_INTF_IS_READY_VOID(runnable)                              \
  do {                                                                        \
    if (NS_WARN_IF(!sBluetoothGattInterface)) {                               \
      DispatchReplyError(runnable,                                            \
        NS_LITERAL_STRING("BluetoothGattInterface is not ready"));            \
        runnable = nullptr;                                                   \
      return;                                                                 \
    }                                                                         \
  } while(0)

#define ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client)                             \
  do {                                                                        \
    if (NS_WARN_IF(!sBluetoothGattInterface)) {                               \
      client->NotifyDiscoverCompleted(false);                                 \
      return;                                                                 \
    }                                                                         \
  } while(0)

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

class BluetoothGattServer;

namespace {
  StaticRefPtr<BluetoothGattManager> sBluetoothGattManager;
  static BluetoothGattInterface* sBluetoothGattInterface;
} // namespace

const int BluetoothGattManager::MAX_NUM_CLIENTS = 1;

bool BluetoothGattManager::mInShutdown = false;

static StaticAutoPtr<nsTArray<RefPtr<BluetoothGattClient> > > sClients;
static StaticAutoPtr<nsTArray<RefPtr<BluetoothGattServer> > > sServers;

struct BluetoothGattClientReadCharState
{
  bool mAuthRetry;
  RefPtr<BluetoothReplyRunnable> mRunnable;

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
  RefPtr<BluetoothReplyRunnable> mRunnable;

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
  RefPtr<BluetoothReplyRunnable> mRunnable;

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
  RefPtr<BluetoothReplyRunnable> mRunnable;

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

  BluetoothGattClient(const BluetoothUuid& aAppUuid,
                      const BluetoothAddress& aDeviceAddr)
    : mAppUuid(aAppUuid)
    , mDeviceAddr(aDeviceAddr)
    , mClientIf(0)
    , mConnId(0)
  { }

  void NotifyDiscoverCompleted(bool aSuccess)
  {
    MOZ_ASSERT(!mAppUuid.IsCleared());
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

  BluetoothUuid mAppUuid;
  BluetoothAddress mDeviceAddr;
  int mClientIf;
  int mConnId;
  RefPtr<BluetoothReplyRunnable> mStartLeScanRunnable;
  RefPtr<BluetoothReplyRunnable> mConnectRunnable;
  RefPtr<BluetoothReplyRunnable> mDisconnectRunnable;
  RefPtr<BluetoothReplyRunnable> mDiscoverRunnable;
  RefPtr<BluetoothReplyRunnable> mReadRemoteRssiRunnable;
  RefPtr<BluetoothReplyRunnable> mRegisterNotificationsRunnable;
  RefPtr<BluetoothReplyRunnable> mDeregisterNotificationsRunnable;
  RefPtr<BluetoothReplyRunnable> mUnregisterClientRunnable;

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

struct BluetoothGattServerAddServiceState
{
  BluetoothGattServiceId mServiceId;
  uint16_t mHandleCount;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(const BluetoothGattServiceId& aServiceId,
              uint16_t aHandleCount,
              BluetoothReplyRunnable* aRunnable)
  {
    mServiceId = aServiceId;
    mHandleCount = aHandleCount;
    mRunnable = aRunnable;
  }

  void Reset()
  {
    memset(&mServiceId, 0, sizeof(mServiceId));
    mHandleCount = 0;
    mRunnable = nullptr;
  }
};

struct BluetoothGattServerAddDescriptorState
{
  BluetoothAttributeHandle mServiceHandle;
  BluetoothAttributeHandle mCharacteristicHandle;
  BluetoothUuid mDescriptorUuid;
  RefPtr<BluetoothReplyRunnable> mRunnable;

  void Assign(const BluetoothAttributeHandle& aServiceHandle,
              const BluetoothAttributeHandle& aCharacteristicHandle,
              const BluetoothUuid& aDescriptorUuid,
              BluetoothReplyRunnable* aRunnable)
  {
    mServiceHandle = aServiceHandle;
    mCharacteristicHandle = aCharacteristicHandle;
    mDescriptorUuid = aDescriptorUuid;
    mRunnable = aRunnable;
  }

  void Reset()
  {
    memset(&mServiceHandle, 0, sizeof(mServiceHandle));
    memset(&mCharacteristicHandle, 0, sizeof(mCharacteristicHandle));
    mDescriptorUuid.Clear();
    mRunnable = nullptr;
  }
};

class BluetoothGattServer final : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  BluetoothGattServer(const BluetoothUuid& aAppUuid)
    : mAppUuid(aAppUuid)
    , mServerIf(0)
    , mIsRegistering(false)
  { }

  BluetoothUuid mAppUuid;
  int mServerIf;

  /*
   * Some actions will trigger the registration procedure:
   *  - Connect the GATT server to a peripheral client
   *  - Add a service to the GATT server
   * These actions will be taken only after the registration has been done
   * successfully. If the registration fails, all the existing actions above
   * should be rejected.
   */
  bool mIsRegistering;

  RefPtr<BluetoothReplyRunnable> mConnectPeripheralRunnable;
  RefPtr<BluetoothReplyRunnable> mDisconnectPeripheralRunnable;
  RefPtr<BluetoothReplyRunnable> mUnregisterServerRunnable;

  BluetoothGattServerAddServiceState mAddServiceState;
  RefPtr<BluetoothReplyRunnable> mAddIncludedServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mAddCharacteristicRunnable;
  BluetoothGattServerAddDescriptorState mAddDescriptorState;
  RefPtr<BluetoothReplyRunnable> mRemoveServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mStartServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mStopServiceRunnable;
  RefPtr<BluetoothReplyRunnable> mSendResponseRunnable;
  RefPtr<BluetoothReplyRunnable> mSendIndicationRunnable;

  // Map connection id from device address
  nsDataHashtable<BluetoothAddressHashKey, int> mConnectionMap;
private:
  ~BluetoothGattServer()
  { }
};

NS_IMPL_ISUPPORTS0(BluetoothGattServer)

class UuidComparator
{
public:
  bool Equals(const RefPtr<BluetoothGattClient>& aClient,
              const BluetoothUuid& aAppUuid) const
  {
    return aClient->mAppUuid == aAppUuid;
  }

  bool Equals(const RefPtr<BluetoothGattServer>& aServer,
              const BluetoothUuid& aAppUuid) const
  {
    return aServer->mAppUuid == aAppUuid;
  }
};

class InterfaceIdComparator
{
public:
  bool Equals(const RefPtr<BluetoothGattClient>& aClient,
              int aClientIf) const
  {
    return aClient->mClientIf == aClientIf;
  }

  bool Equals(const RefPtr<BluetoothGattServer>& aServer,
              int aServerIf) const
  {
    return aServer->mServerIf == aServerIf;
  }
};

class ConnIdComparator
{
public:
  bool Equals(const RefPtr<BluetoothGattClient>& aClient,
              int aConnId) const
  {
    return aClient->mConnId == aConnId;
  }

  bool Equals(const RefPtr<BluetoothGattServer>& aServer,
              int aConnId) const
  {
    for (
      auto iter = aServer->mConnectionMap.Iter(); !iter.Done(); iter.Next()) {
      if (aConnId == iter.Data()) {
        return true;
      }
    }
    return false;
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

class BluetoothGattManager::RegisterModuleResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  RegisterModuleResultHandler(BluetoothGattInterface* aInterface,
                              BluetoothProfileResultHandler* aRes)
    : mInterface(aInterface)
    , mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::RegisterModule failed for GATT: %d",
               (int)aStatus);

    mInterface->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void RegisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothGattInterface = mInterface;

    if (mRes) {
      mRes->Init();
    }
  }

private:
  BluetoothGattInterface* mInterface;
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothGattManager::InitProfileResultHandlerRunnable final
  : public nsRunnable
{
public:
  InitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                   nsresult aRv)
    : mRes(aRes)
    , mRv(aRv)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_SUCCEEDED(mRv)) {
      mRes->Init();
    } else {
      mRes->OnError(mRv);
    }
    return NS_OK;
  }

private:
  RefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void
BluetoothGattManager::InitGattInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sBluetoothGattInterface) {
    BT_LOGR("Bluetooth GATT interface is already initalized.");
    RefPtr<nsRunnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT Init runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no Bluetooth interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<nsRunnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<nsRunnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  auto gattInterface = btInf->GetBluetoothGattInterface();

  if (NS_WARN_IF(!gattInterface)) {
    // If there's no GATT interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<nsRunnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  if (!sClients) {
    sClients = new nsTArray<RefPtr<BluetoothGattClient> >;
  }

  if (!sServers) {
    sServers = new nsTArray<RefPtr<BluetoothGattServer> >;
  }

  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  gattInterface->SetNotificationHandler(BluetoothGattManager::Get());

  setupInterface->RegisterModule(
    SETUP_SERVICE_ID_GATT, 0, MAX_NUM_CLIENTS,
    new RegisterModuleResultHandler(gattInterface, aRes));
}

class BluetoothGattManager::UnregisterModuleResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  UnregisterModuleResultHandler(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::UnregisterModule failed for GATT: %d",
               (int)aStatus);

    sBluetoothGattInterface->SetNotificationHandler(nullptr);
    sBluetoothGattInterface = nullptr;

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void UnregisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothGattInterface->SetNotificationHandler(nullptr);
    sBluetoothGattInterface = nullptr;
    sClients = nullptr;
    sServers = nullptr;

    if (mRes) {
      mRes->Deinit();
    }
  }

private:
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothGattManager::DeinitProfileResultHandlerRunnable final
  : public nsRunnable
{
public:
  DeinitProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                     nsresult aRv)
    : mRes(aRes)
    , mRv(aRv)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (NS_SUCCEEDED(mRv)) {
      mRes->Deinit();
    } else {
      mRes->OnError(mRv);
    }
    return NS_OK;
  }

private:
  RefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

// static
void
BluetoothGattManager::DeinitGattInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sBluetoothGattInterface) {
    BT_LOGR("Bluetooth GATT interface has not been initalized.");
    RefPtr<nsRunnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT Deinit runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<nsRunnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<nsRunnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch GATT OnError runnable");
    }
    return;
  }

  setupInterface->UnregisterModule(
    SETUP_SERVICE_ID_GATT,
    new UnregisterModuleResultHandler(aRes));
}

class BluetoothGattManager::RegisterClientResultHandler final
  : public BluetoothGattResultHandler
{
public:
  RegisterClientResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::RegisterClient failed: %d",
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
  RefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::UnregisterClientResultHandler final
  : public BluetoothGattResultHandler
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
    bs->DistributeSignal(NS_LITERAL_STRING("ClientUnregistered"),
                         mClient->mAppUuid);

    // Resolve the unregister request
    DispatchReplySuccess(mClient->mUnregisterClientRunnable);
    mClient->mUnregisterClientRunnable = nullptr;

    sClients->RemoveElement(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::UnregisterClient failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mUnregisterClientRunnable);

    // Reject the unregister request
    DispatchReplyError(mClient->mUnregisterClientRunnable,
                       NS_LITERAL_STRING("Unregister GATT client failed"));
    mClient->mUnregisterClientRunnable = nullptr;
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::UnregisterClient(int aClientIf,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   InterfaceIdComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mUnregisterClientRunnable = aRunnable;

  sBluetoothGattInterface->UnregisterClient(
    aClientIf,
    new UnregisterClientResultHandler(client));
}

class BluetoothGattManager::StartLeScanResultHandler final
  : public BluetoothGattResultHandler
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
    BT_WARNING("BluetoothGattInterface::StartLeScan failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mStartLeScanRunnable);

    // Unregister client if startLeScan failed
    if (mClient->mClientIf > 0) {
      BluetoothGattManager* gattManager = BluetoothGattManager::Get();
      NS_ENSURE_TRUE_VOID(gattManager);

      RefPtr<BluetoothVoidReplyRunnable> result =
        new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterClient(mClient->mClientIf, result);
    }

    DispatchReplyError(mClient->mStartLeScanRunnable, aStatus);
    mClient->mStartLeScanRunnable = nullptr;
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

class BluetoothGattManager::StopLeScanResultHandler final
  : public BluetoothGattResultHandler
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

      RefPtr<BluetoothVoidReplyRunnable> result =
        new BluetoothVoidReplyRunnable(nullptr);
      gattManager->UnregisterClient(mClientIf, result);
    }
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::StopLeScan failed: %d",
                (int)aStatus);
    DispatchReplyError(mRunnable, aStatus);
  }

private:
  RefPtr<BluetoothReplyRunnable> mRunnable;
  int mClientIf;
};

void
BluetoothGattManager::StartLeScan(const nsTArray<BluetoothUuid>& aServiceUuids,
                                  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  BluetoothUuid appUuid;
  if (NS_WARN_IF(NS_FAILED(GenerateUuid(appUuid)))) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("start LE scan failed"));
    return;
  }

  size_t index = sClients->IndexOf(appUuid, 0 /* Start */, UuidComparator());

  // Reject the startLeScan request if the clientIf is being used.
  if (NS_WARN_IF(index != sClients->NoIndex)) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("start LE scan failed"));
    return;
  }

  index = sClients->Length();
  sClients->AppendElement(new BluetoothGattClient(appUuid,
                                                  BluetoothAddress::ANY));
  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mStartLeScanRunnable = aRunnable;

  // 'startLeScan' will be proceeded after client registered
  sBluetoothGattInterface->RegisterClient(
    appUuid, new RegisterClientResultHandler(client));
}

void
BluetoothGattManager::StopLeScan(const BluetoothUuid& aScanUuid,
                                 BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aScanUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    // Reject the stop LE scan request
    DispatchReplyError(aRunnable, NS_LITERAL_STRING("StopLeScan failed"));
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  sBluetoothGattInterface->Scan(
    client->mClientIf,
    false /* Stop */,
    new StopLeScanResultHandler(aRunnable, client->mClientIf));
}

class BluetoothGattManager::ConnectResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ConnectResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::Connect failed: %d",
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
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::Connect(const BluetoothUuid& aAppUuid,
                              const BluetoothAddress& aDeviceAddr,
                              BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sClients->NoIndex) {
    index = sClients->Length();
    sClients->AppendElement(new BluetoothGattClient(aAppUuid, aDeviceAddr));
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mConnectRunnable = aRunnable;

  if (client->mClientIf > 0) {
    sBluetoothGattInterface->Connect(client->mClientIf,
                                     aDeviceAddr,
                                     true, // direct connect
                                     TRANSPORT_AUTO,
                                     new ConnectResultHandler(client));
  } else {
    // connect will be proceeded after client registered
    sBluetoothGattInterface->RegisterClient(
      aAppUuid, new RegisterClientResultHandler(client));
  }
}

class BluetoothGattManager::DisconnectResultHandler final
  : public BluetoothGattResultHandler
{
public:
  DisconnectResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::Disconnect failed: %d",
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
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::Disconnect(const BluetoothUuid& aAppUuid,
                                 const BluetoothAddress& aDeviceAddr,
                                 BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mDisconnectRunnable = aRunnable;

  sBluetoothGattInterface->Disconnect(
    client->mClientIf,
    aDeviceAddr,
    client->mConnId,
    new DisconnectResultHandler(client));
}

class BluetoothGattManager::DiscoverResultHandler final
  : public BluetoothGattResultHandler
{
public:
  DiscoverResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::Discover failed: %d",
               (int)aStatus);

    mClient->NotifyDiscoverCompleted(false);
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::Discover(const BluetoothUuid& aAppUuid,
                               BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
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
  sBluetoothGattInterface->SearchService(
    client->mConnId,
    true, // search all services
    BluetoothUuid(),
    new DiscoverResultHandler(client));
}

class BluetoothGattManager::ReadRemoteRssiResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ReadRemoteRssiResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::ReadRemoteRssi failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mReadRemoteRssiRunnable);

    // Reject the read remote rssi request
    DispatchReplyError(mClient->mReadRemoteRssiRunnable,
                       NS_LITERAL_STRING("ReadRemoteRssi failed"));
    mClient->mReadRemoteRssiRunnable = nullptr;
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::ReadRemoteRssi(int aClientIf,
                                     const BluetoothAddress& aDeviceAddr,
                                     BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   InterfaceIdComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  client->mReadRemoteRssiRunnable = aRunnable;

  sBluetoothGattInterface->ReadRemoteRssi(
    aClientIf, aDeviceAddr,
    new ReadRemoteRssiResultHandler(client));
}

class BluetoothGattManager::RegisterNotificationsResultHandler final
  : public BluetoothGattResultHandler
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
      "BluetoothGattInterface::RegisterNotifications failed: %d",
      (int)aStatus);
    MOZ_ASSERT(mClient->mRegisterNotificationsRunnable);

    DispatchReplyError(mClient->mRegisterNotificationsRunnable,
                       NS_LITERAL_STRING("RegisterNotifications failed"));
    mClient->mRegisterNotificationsRunnable = nullptr;
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::RegisterNotifications(
  const BluetoothUuid& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Reject the request if there is an ongoing request or client is already
  // disconnected
  if (client->mRegisterNotificationsRunnable || client->mConnId <= 0) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("RegisterNotifications failed"));
    return;
  }

  client->mRegisterNotificationsRunnable = aRunnable;

  sBluetoothGattInterface->RegisterNotification(
    client->mClientIf, client->mDeviceAddr, aServId, aCharId,
    new RegisterNotificationsResultHandler(client));
}

class BluetoothGattManager::DeregisterNotificationsResultHandler final
  : public BluetoothGattResultHandler
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
      "BluetoothGattInterface::DeregisterNotifications failed: %d",
      (int)aStatus);
    MOZ_ASSERT(mClient->mDeregisterNotificationsRunnable);

    DispatchReplyError(mClient->mDeregisterNotificationsRunnable,
                       NS_LITERAL_STRING("DeregisterNotifications failed"));
    mClient->mDeregisterNotificationsRunnable = nullptr;
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::DeregisterNotifications(
  const BluetoothUuid& aAppUuid, const BluetoothGattServiceId& aServId,
  const BluetoothGattId& aCharId, BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  // Reject the request if there is an ongoing request
  if (client->mDeregisterNotificationsRunnable) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("DeregisterNotifications failed"));
    return;
  }

  client->mDeregisterNotificationsRunnable = aRunnable;

  sBluetoothGattInterface->DeregisterNotification(
    client->mClientIf, client->mDeviceAddr, aServId, aCharId,
    new DeregisterNotificationsResultHandler(client));
}

class BluetoothGattManager::ReadCharacteristicValueResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ReadCharacteristicValueResultHandler(BluetoothGattClient* aClient)
    : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::ReadCharacteristicValue failed" \
               ": %d", (int)aStatus);
    MOZ_ASSERT(mClient->mReadCharacteristicState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
      mClient->mReadCharacteristicState.mRunnable;
    mClient->mReadCharacteristicState.Reset();

    // Reject the read characteristic value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("ReadCharacteristicValue failed"));
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::ReadCharacteristicValue(
  const BluetoothUuid& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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
  sBluetoothGattInterface->ReadCharacteristic(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    GATT_AUTH_REQ_NONE,
    new ReadCharacteristicValueResultHandler(client));
}

class BluetoothGattManager::WriteCharacteristicValueResultHandler final
  : public BluetoothGattResultHandler
{
public:
  WriteCharacteristicValueResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::WriteCharacteristicValue failed" \
               ": %d", (int)aStatus);
    MOZ_ASSERT(mClient->mWriteCharacteristicState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
      mClient->mWriteCharacteristicState.mRunnable;
    mClient->mWriteCharacteristicState.Reset();

    // Reject the write characteristic value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("WriteCharacteristicValue failed"));
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::WriteCharacteristicValue(
  const BluetoothUuid& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattWriteType& aWriteType,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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

  client->mWriteCharacteristicState.Assign(
    aWriteType, aValue, false, aRunnable);

  /**
   * First, write the characteristic value through an unauthenticated physical
   * link. If the operation fails due to insufficient authentication/encryption
   * key size, retry to write through an authenticated physical link.
   */
  sBluetoothGattInterface->WriteCharacteristic(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    aWriteType,
    GATT_AUTH_REQ_NONE,
    aValue,
    new WriteCharacteristicValueResultHandler(client));
}

class BluetoothGattManager::ReadDescriptorValueResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ReadDescriptorValueResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::ReadDescriptorValue failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mReadDescriptorState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
      mClient->mReadDescriptorState.mRunnable;
    mClient->mReadDescriptorState.Reset();

    // Reject the read descriptor value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("ReadDescriptorValue failed"));
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::ReadDescriptorValue(
  const BluetoothUuid& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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
  sBluetoothGattInterface->ReadDescriptor(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    aDescriptorId,
    GATT_AUTH_REQ_NONE,
    new ReadDescriptorValueResultHandler(client));
}

class BluetoothGattManager::WriteDescriptorValueResultHandler final
  : public BluetoothGattResultHandler
{
public:
  WriteDescriptorValueResultHandler(BluetoothGattClient* aClient)
  : mClient(aClient)
  {
    MOZ_ASSERT(mClient);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattInterface::WriteDescriptorValue failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mClient->mWriteDescriptorState.mRunnable);

    RefPtr<BluetoothReplyRunnable> runnable =
      mClient->mWriteDescriptorState.mRunnable;
    mClient->mWriteDescriptorState.Reset();

    // Reject the write descriptor value request
    DispatchReplyError(runnable,
                       NS_LITERAL_STRING("WriteDescriptorValue failed"));
  }

private:
  RefPtr<BluetoothGattClient> mClient;
};

void
BluetoothGattManager::WriteDescriptorValue(
  const BluetoothUuid& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothGattId& aCharacteristicId,
  const BluetoothGattId& aDescriptorId,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sClients->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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

  sBluetoothGattInterface->WriteDescriptor(
    client->mConnId,
    aServiceId,
    aCharacteristicId,
    aDescriptorId,
    GATT_WRITE_TYPE_NORMAL,
    GATT_AUTH_REQ_NONE,
    aValue,
    new WriteDescriptorValueResultHandler(client));
}

class BluetoothGattManager::RegisterServerResultHandler final
  : public BluetoothGattResultHandler
{
public:
  RegisterServerResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
    MOZ_ASSERT(!mServer->mIsRegistering);

    mServer->mIsRegistering = true;
  }

  /*
   * Some actions will trigger the registration procedure. These actions will
   * be taken only after the registration has been done successfully.
   * If the registration fails, all the existing actions above should be
   * rejected.
   */
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::RegisterServer failed: %d",
               (int)aStatus);

    // Reject the connect request
    if (mServer->mConnectPeripheralRunnable) {
      DispatchReplyError(mServer->mConnectPeripheralRunnable,
                         NS_LITERAL_STRING("Register GATT server failed"));
      mServer->mConnectPeripheralRunnable = nullptr;
    }

    // Reject the add service request
    if (mServer->mAddServiceState.mRunnable) {
      DispatchReplyError(mServer->mAddServiceState.mRunnable,
                         NS_LITERAL_STRING("Register GATT server failed"));
      mServer->mAddServiceState.Reset();
    }

    mServer->mIsRegistering = false;
    sServers->RemoveElement(mServer);
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

class BluetoothGattManager::ConnectPeripheralResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ConnectPeripheralResultHandler(BluetoothGattServer* aServer,
                                 const BluetoothAddress& aDeviceAddr)
    : mServer(aServer)
    , mDeviceAddr(aDeviceAddr)
  {
    MOZ_ASSERT(mServer);
    MOZ_ASSERT(!mDeviceAddr.IsCleared());
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::ConnectPeripheral failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mServer->mConnectPeripheralRunnable);

    DispatchReplyError(mServer->mConnectPeripheralRunnable,
                       NS_LITERAL_STRING("ConnectPeripheral failed"));
    mServer->mConnectPeripheralRunnable = nullptr;
    mServer->mConnectionMap.Remove(mDeviceAddr);
  }

private:
  RefPtr<BluetoothGattServer> mServer;
  BluetoothAddress mDeviceAddr;
};

void
BluetoothGattManager::ConnectPeripheral(
  const BluetoothUuid& aAppUuid,
  const BluetoothAddress& aAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    index = sServers->Length();
    sServers->AppendElement(new BluetoothGattServer(aAppUuid));
  }
  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  /**
   * Early resolve or reject the request based on the current status before
   * sending a request to bluetooth stack.
   *
   * case 1) Connecting/Disconnecting: If connect/disconnect peripheral
   *         runnable exists, reject the request since the local GATT server is
   *         busy connecting or disconnecting to a device.
   * case 2) Connected: If there is an entry whose key is |aAddress| in the
   *         connection map, resolve the request. Since disconnected devices
   *         will not be in the map, all entries in the map are connected
   *         devices.
   */
  if (server->mConnectPeripheralRunnable ||
      server->mDisconnectPeripheralRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  int connId = 0;
  if (server->mConnectionMap.Get(aAddress, &connId)) {
    MOZ_ASSERT(connId > 0);
    DispatchReplySuccess(aRunnable);
    return;
  }

  server->mConnectionMap.Put(aAddress, 0);
  server->mConnectPeripheralRunnable = aRunnable;

  if (server->mServerIf > 0) {
    sBluetoothGattInterface->ConnectPeripheral(
      server->mServerIf,
      aAddress,
      true, // direct connect
      TRANSPORT_AUTO,
      new ConnectPeripheralResultHandler(server, aAddress));
  } else if (!server->mIsRegistering) { /* avoid triggering another registration
                                         * procedure if there is an on-going one
                                         * already */
    // connect will be proceeded after server registered
    sBluetoothGattInterface->RegisterServer(
      aAppUuid, new RegisterServerResultHandler(server));
  }
}

class BluetoothGattManager::DisconnectPeripheralResultHandler final
  : public BluetoothGattResultHandler
{
public:
  DisconnectPeripheralResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::DisconnectPeripheral failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mServer->mDisconnectPeripheralRunnable);

    // Reject the disconnect request
    DispatchReplyError(mServer->mDisconnectPeripheralRunnable,
                       NS_LITERAL_STRING("DisconnectPeripheral failed"));
    mServer->mDisconnectPeripheralRunnable = nullptr;
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::DisconnectPeripheral(
  const BluetoothUuid& aAppUuid,
  const BluetoothAddress& aAddress,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  if (NS_WARN_IF(server->mServerIf <= 0)) {
    DispatchReplyError(aRunnable,
                       NS_LITERAL_STRING("Disconnect failed"));
    return;
  }

  // Reject the request if there is an ongoing connect/disconnect request.
  if (server->mConnectPeripheralRunnable ||
      server->mDisconnectPeripheralRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  // Resolve the request if the device is not connected.
  int connId = 0;
  if (!server->mConnectionMap.Get(aAddress, &connId)) {
    DispatchReplySuccess(aRunnable);
    return;
  }

  server->mDisconnectPeripheralRunnable = aRunnable;

  sBluetoothGattInterface->DisconnectPeripheral(
    server->mServerIf,
    aAddress,
    connId,
    new DisconnectPeripheralResultHandler(server));
}

class BluetoothGattManager::UnregisterServerResultHandler final
  : public BluetoothGattResultHandler
{
public:
  UnregisterServerResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void UnregisterServer() override
  {
    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    // Notify BluetoothGattServer to clear the serverIf
    bs->DistributeSignal(NS_LITERAL_STRING("ServerUnregistered"),
                         mServer->mAppUuid);

    // Resolve the unregister request
    if (mServer->mUnregisterServerRunnable) {
      DispatchReplySuccess(mServer->mUnregisterServerRunnable);
      mServer->mUnregisterServerRunnable = nullptr;
    }

    sServers->RemoveElement(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::UnregisterServer failed: %d",
               (int)aStatus);

    // Reject the unregister request
    if (mServer->mUnregisterServerRunnable) {
      DispatchReplyError(mServer->mUnregisterServerRunnable,
                         NS_LITERAL_STRING("Unregister GATT Server failed"));
      mServer->mUnregisterServerRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::UnregisterServer(int aServerIf,
                                       BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  RefPtr<BluetoothGattServer> server = (*sServers)[index];
  server->mUnregisterServerRunnable = aRunnable;

  sBluetoothGattInterface->UnregisterServer(
    aServerIf,
    new UnregisterServerResultHandler(server));
}

class BluetoothGattManager::ServerAddServiceResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerAddServiceResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::ServerAddService failed: %d",
               (int)aStatus);
    MOZ_ASSERT(mServer->mAddServiceState.mRunnable);

    DispatchReplyError(mServer->mAddServiceState.mRunnable,
                       NS_LITERAL_STRING("ServerAddService failed"));
    mServer->mAddServiceState.Reset();
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerAddService(
  const BluetoothUuid& aAppUuid,
  const BluetoothGattServiceId& aServiceId,
  uint16_t aHandleCount,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    index = sServers->Length();
    sServers->AppendElement(new BluetoothGattServer(aAppUuid));
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if there is an ongoing add service request.
  if (server->mAddServiceState.mRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mAddServiceState.Assign(aServiceId, aHandleCount, aRunnable);

  if (server->mServerIf > 0) {
    sBluetoothGattInterface->AddService(
      server->mServerIf,
      aServiceId,
      aHandleCount,
      new ServerAddServiceResultHandler(server));
  } else if (!server->mIsRegistering) { /* avoid triggering another registration
                                         * procedure if there is an on-going one
                                         * already */
    // add service will be proceeded after server registered
    sBluetoothGattInterface->RegisterServer(
      aAppUuid, new RegisterServerResultHandler(server));
  }
}

class BluetoothGattManager::ServerAddIncludedServiceResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerAddIncludedServiceResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::AddIncludedService failed: %d",
               (int)aStatus);

    // Reject the add included service request
    if (mServer->mAddIncludedServiceRunnable) {
      DispatchReplyError(mServer->mAddIncludedServiceRunnable,
                         NS_LITERAL_STRING("Add GATT included service failed"));
      mServer->mAddIncludedServiceRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerAddIncludedService(
  const BluetoothUuid& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aIncludedServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing add included service request.
  if (server->mAddIncludedServiceRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mAddIncludedServiceRunnable = aRunnable;

  sBluetoothGattInterface->AddIncludedService(
    server->mServerIf,
    aServiceHandle,
    aIncludedServiceHandle,
    new ServerAddIncludedServiceResultHandler(server));
}

class BluetoothGattManager::ServerAddCharacteristicResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerAddCharacteristicResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::AddCharacteristic failed: %d",
               (int)aStatus);

    // Reject the add characteristic request
    if (mServer->mAddCharacteristicRunnable) {
      DispatchReplyError(mServer->mAddCharacteristicRunnable,
                         NS_LITERAL_STRING("Add GATT characteristic failed"));
      mServer->mAddCharacteristicRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerAddCharacteristic(
  const BluetoothUuid& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothUuid& aCharacteristicUuid,
  BluetoothGattAttrPerm aPermissions,
  BluetoothGattCharProp aProperties,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing add characteristic request.
  if (server->mAddCharacteristicRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mAddCharacteristicRunnable = aRunnable;

  sBluetoothGattInterface->AddCharacteristic(
    server->mServerIf,
    aServiceHandle,
    aCharacteristicUuid,
    aProperties,
    aPermissions,
    new ServerAddCharacteristicResultHandler(server));
}

class BluetoothGattManager::ServerAddDescriptorResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerAddDescriptorResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::AddDescriptor failed: %d",
               (int)aStatus);

    // Reject the add descriptor request
    if (mServer->mAddDescriptorState.mRunnable) {
      DispatchReplyError(mServer->mAddDescriptorState.mRunnable,
                         NS_LITERAL_STRING("Add GATT descriptor failed"));
      mServer->mAddDescriptorState.Reset();
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerAddDescriptor(
  const BluetoothUuid& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aCharacteristicHandle,
  const BluetoothUuid& aDescriptorUuid,
  BluetoothGattAttrPerm aPermissions,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing add descriptor request.
  if (server->mAddDescriptorState.mRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mAddDescriptorState.Assign(aServiceHandle,
                                     aCharacteristicHandle,
                                     aDescriptorUuid,
                                     aRunnable);

  sBluetoothGattInterface->AddDescriptor(
    server->mServerIf,
    aServiceHandle,
    aDescriptorUuid,
    aPermissions,
    new ServerAddDescriptorResultHandler(server));
}

class BluetoothGattManager::ServerRemoveDescriptorResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerRemoveDescriptorResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::RemoveService failed: %d",
               (int)aStatus);

    // Reject the remove service request
    if (mServer->mRemoveServiceRunnable) {
      DispatchReplyError(mServer->mRemoveServiceRunnable,
                         NS_LITERAL_STRING("Remove GATT service failed"));
      mServer->mRemoveServiceRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerRemoveService(
  const BluetoothUuid& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing remove service request.
  if (server->mRemoveServiceRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mRemoveServiceRunnable = aRunnable;

  sBluetoothGattInterface->DeleteService(
    server->mServerIf,
    aServiceHandle,
    new ServerRemoveDescriptorResultHandler(server));
}

class BluetoothGattManager::ServerStartServiceResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerStartServiceResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::StartService failed: %d",
               (int)aStatus);

    // Reject the remove service request
    if (mServer->mStartServiceRunnable) {
      DispatchReplyError(mServer->mStartServiceRunnable,
                         NS_LITERAL_STRING("Start GATT service failed"));
      mServer->mStartServiceRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerStartService(
  const BluetoothUuid& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing start service request.
  if (server->mStartServiceRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mStartServiceRunnable = aRunnable;

  sBluetoothGattInterface->StartService(
    server->mServerIf,
    aServiceHandle,
    TRANSPORT_AUTO,
    new ServerStartServiceResultHandler(server));
}

class BluetoothGattManager::ServerStopServiceResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerStopServiceResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::StopService failed: %d",
               (int)aStatus);

    // Reject the remove service request
    if (mServer->mStopServiceRunnable) {
      DispatchReplyError(mServer->mStopServiceRunnable,
                         NS_LITERAL_STRING("Stop GATT service failed"));
      mServer->mStopServiceRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerStopService(
  const BluetoothUuid& aAppUuid,
  const BluetoothAttributeHandle& aServiceHandle,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (NS_WARN_IF(index == sServers->NoIndex)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the service has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing stop service request.
  if (server->mStopServiceRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  server->mStopServiceRunnable = aRunnable;

  sBluetoothGattInterface->StopService(
    server->mServerIf,
    aServiceHandle,
    new ServerStopServiceResultHandler(server));
}

class BluetoothGattManager::ServerSendResponseResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerSendResponseResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void SendResponse() override
  {
    if (mServer->mSendResponseRunnable) {
      DispatchReplySuccess(mServer->mSendResponseRunnable);
      mServer->mSendResponseRunnable = nullptr;
    }
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::SendResponse failed: %d",
               (int)aStatus);

    // Reject the send response request
    if (mServer->mSendResponseRunnable) {
      DispatchReplyError(mServer->mSendResponseRunnable,
                         NS_LITERAL_STRING("Send response failed"));
      mServer->mSendResponseRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerSendResponse(const BluetoothUuid& aAppUuid,
                                         const BluetoothAddress& aAddress,
                                         uint16_t aStatus,
                                         int aRequestId,
                                         const BluetoothGattResponse& aRsp,
                                         BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  if (index == sServers->NoIndex) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (server->mSendResponseRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  int connId = 0;
  server->mConnectionMap.Get(aAddress, &connId);
  if (!connId) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }

  sBluetoothGattInterface->SendResponse(
    connId,
    aRequestId,
    aStatus,
    aRsp,
    new ServerSendResponseResultHandler(server));
}

class BluetoothGattManager::ServerSendIndicationResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ServerSendIndicationResultHandler(BluetoothGattServer* aServer)
  : mServer(aServer)
  {
    MOZ_ASSERT(mServer);
  }

  void SendIndication() override
  {
    if (mServer->mSendIndicationRunnable) {
      DispatchReplySuccess(mServer->mSendIndicationRunnable);
      mServer->mSendIndicationRunnable = nullptr;
    }
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothGattServerInterface::NotifyCharacteristicChanged"
               "failed: %d", (int)aStatus);

    // Reject the send indication request
    if (mServer->mSendIndicationRunnable) {
      DispatchReplyError(mServer->mSendIndicationRunnable,
                         NS_LITERAL_STRING("Send GATT indication failed"));
      mServer->mSendIndicationRunnable = nullptr;
    }
  }

private:
  RefPtr<BluetoothGattServer> mServer;
};

void
BluetoothGattManager::ServerSendIndication(
  const BluetoothUuid& aAppUuid,
  const BluetoothAddress& aAddress,
  const BluetoothAttributeHandle& aCharacteristicHandle,
  bool aConfirm,
  const nsTArray<uint8_t>& aValue,
  BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRunnable);

  ENSURE_GATT_INTF_IS_READY_VOID(aRunnable);

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  // Reject the request if the server has not been registered yet.
  if (index == sServers->NoIndex) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  // Reject the request if the server has not been registered successfully.
  if (!server->mServerIf) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }
  // Reject the request if there is an ongoing send indication request.
  if (server->mSendIndicationRunnable) {
    DispatchReplyError(aRunnable, STATUS_BUSY);
    return;
  }

  int connId = 0;
  if (!server->mConnectionMap.Get(aAddress, &connId)) {
    DispatchReplyError(aRunnable, STATUS_PARM_INVALID);
    return;
  }

  if (!connId) {
    DispatchReplyError(aRunnable, STATUS_NOT_READY);
    return;
  }

  server->mSendIndicationRunnable = aRunnable;

  sBluetoothGattInterface->SendIndication(
    server->mServerIf,
    aCharacteristicHandle,
    connId,
    aValue,
    aConfirm,
    new ServerSendIndicationResultHandler(server));
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

  size_t index = sClients->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  if (aStatus != GATT_STATUS_SUCCESS) {
    nsAutoString appUuidStr;
    UuidToString(aAppUuid, appUuidStr);

    BT_LOGD("RegisterClient failed: clientIf = %d, status = %d, appUuid = %s",
            aClientIf, aStatus, NS_ConvertUTF16toUTF8(appUuidStr).get());

    // Notify BluetoothGatt for client disconnected
    bs->DistributeSignal(
      NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
      aAppUuid, BluetoothValue(false)); // Disconnected

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
    aAppUuid, BluetoothValue(uint32_t(aClientIf)));

  if (client->mStartLeScanRunnable) {
    // Client just registered, proceed remaining startLeScan request.
    ENSURE_GATT_INTF_IS_READY_VOID(client->mStartLeScanRunnable);
    sBluetoothGattInterface->Scan(
      aClientIf, true /* start */,
      new StartLeScanResultHandler(client));
  } else if (client->mConnectRunnable) {
    // Client just registered, proceed remaining connect request.
    ENSURE_GATT_INTF_IS_READY_VOID(client->mConnectRunnable);
    sBluetoothGattInterface->Connect(
      aClientIf, client->mDeviceAddr, true /* direct connect */,
      TRANSPORT_AUTO,
      new ConnectResultHandler(client));
  }
}

class BluetoothGattManager::ScanDeviceTypeResultHandler final
  : public BluetoothGattResultHandler
{
public:
  ScanDeviceTypeResultHandler(const BluetoothAddress& aBdAddr, int aRssi,
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

    nsAutoString addressStr;
    AddressToString(mBdAddr, addressStr);

    AppendNamedValue(properties, "Address", addressStr);
    AppendNamedValue(properties, "Rssi", mRssi);
    AppendNamedValue(properties, "GattAdv", mAdvData);
    AppendNamedValue(properties, "Type", static_cast<uint32_t>(type));

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE_VOID(bs);

    bs->DistributeSignal(NS_LITERAL_STRING("LeDeviceFound"),
                         NS_LITERAL_STRING(KEY_ADAPTER),
                         BluetoothValue(properties));
  }

  BluetoothAddress mBdAddr;
  int32_t mRssi;
  nsTArray<uint8_t> mAdvData;
};

void
BluetoothGattManager::ScanResultNotification(
  const BluetoothAddress& aBdAddr, int aRssi,
  const BluetoothGattAdvData& aAdvData)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBluetoothGattInterface);

  // Distribute "LeDeviceFound" signal after we know the corresponding
  // BluetoothTypeOfDevice of the device
  sBluetoothGattInterface->GetDeviceType(
    aBdAddr,
    new ScanDeviceTypeResultHandler(aBdAddr, aRssi, aAdvData));
}

void
BluetoothGattManager::ConnectNotification(int aConnId,
                                          BluetoothGattStatus aStatus,
                                          int aClientIf,
                                          const BluetoothAddress& aDeviceAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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
BluetoothGattManager::DisconnectNotification(
  int aConnId,
  BluetoothGattStatus aStatus,
  int aClientIf,
  const BluetoothAddress& aDeviceAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
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
    ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client);
    sBluetoothGattInterface->GetIncludedService(
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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
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
    ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client);
    sBluetoothGattInterface->GetCharacteristic(
      aConnId,
      aServiceId,
      false,
      aCharId,
      new DiscoverResultHandler(client));
  } else { // all characteristics of this service are discovered
    // Notify BluetoothGatt to make BluetoothGattService create characteristics
    // then proceed
    nsTArray<BluetoothNamedValue> values;
    AppendNamedValue(values, "serviceId", aServiceId);
    AppendNamedValue(values, "characteristics", client->mCharacteristics);

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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  if (aStatus == GATT_STATUS_SUCCESS) {
    // Save to mDescriptors for distributing to applications
    client->mDescriptors.AppendElement(aDescriptorId);

    // Get next descriptor of this characteristic
    ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client);
    sBluetoothGattInterface->GetDescriptor(
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
    AppendNamedValue(values, "serviceId", aServiceId);
    AppendNamedValue(values, "characteristicId", aCharId);
    AppendNamedValue(values, "descriptors", client->mDescriptors);

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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);
  MOZ_ASSERT(client->mDiscoverRunnable);

  ENSURE_GATT_INTF_IN_ATTR_DISCOVER(client);
  if (aStatus == GATT_STATUS_SUCCESS) {
    // Save to mIncludedServices for distributing to applications
    client->mIncludedServices.AppendElement(aIncludedServId);

    // Get next included service of this service
    sBluetoothGattInterface->GetIncludedService(
      aConnId,
      aServiceId,
      false,
      aIncludedServId,
      new DiscoverResultHandler(client));
  } else { // all included services of this service are discovered
    // Notify BluetoothGatt to make BluetoothGattService create included
    // services
    nsTArray<BluetoothNamedValue> values;
    AppendNamedValue(values, "serviceId", aServiceId);
    AppendNamedValue(values, "includedServices", client->mIncludedServices);

    bs->DistributeSignal(NS_LITERAL_STRING("IncludedServicesDiscovered"),
                         client->mAppUuid,
                         BluetoothValue(values));
    client->mIncludedServices.Clear();

    // Start to discover characteristics of this service
    sBluetoothGattInterface->GetCharacteristic(
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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mReadCharacteristicState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
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
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mReadCharacteristicState.Reset();
      // Reject the promise
      DispatchReplyError(runnable,
                         NS_LITERAL_STRING("ReadCharacteristicValue failed"));
      return;
    }

    client->mReadCharacteristicState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattInterface->ReadCharacteristic(
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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mWriteCharacteristicState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
    client->mWriteCharacteristicState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mWriteCharacteristicState.Reset();
    // Resolve the promise
    DispatchReplySuccess(runnable);
  } else if (!client->mWriteCharacteristicState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mWriteCharacteristicState.Reset();
      // Reject the promise
      DispatchReplyError(runnable,
                         NS_LITERAL_STRING("WriteCharacteristicValue failed"));
      return;
    }

    client->mWriteCharacteristicState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattInterface->WriteCharacteristic(
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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mReadDescriptorState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
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
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mReadDescriptorState.Reset();
      // Reject the promise
      DispatchReplyError(runnable,
                         NS_LITERAL_STRING("ReadDescriptorValue failed"));
      return;
    }

    client->mReadDescriptorState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattInterface->ReadDescriptor(
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

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  MOZ_ASSERT(client->mWriteDescriptorState.mRunnable);
  RefPtr<BluetoothReplyRunnable> runnable =
    client->mWriteDescriptorState.mRunnable;

  if (aStatus == GATT_STATUS_SUCCESS) {
    client->mWriteDescriptorState.Reset();
    // Resolve the promise
    DispatchReplySuccess(runnable);
  } else if (!client->mWriteDescriptorState.mAuthRetry &&
             (aStatus == GATT_STATUS_INSUFFICIENT_AUTHENTICATION ||
              aStatus == GATT_STATUS_INSUFFICIENT_ENCRYPTION)) {
    if (NS_WARN_IF(!sBluetoothGattInterface)) {
      client->mWriteDescriptorState.Reset();
      // Reject the promise
      DispatchReplyError(runnable,
                         NS_LITERAL_STRING("WriteDescriptorValue failed"));
      return;
    }

    client->mWriteDescriptorState.mAuthRetry = true;
    // Retry with another authentication requirement
    sBluetoothGattInterface->WriteDescriptor(
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
BluetoothGattManager::ReadRemoteRssiNotification(
  int aClientIf,
  const BluetoothAddress& aBdAddr,
  int aRssi,
  BluetoothGattStatus aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sClients->IndexOf(aClientIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sClients->NoIndex);

  RefPtr<BluetoothGattClient> client = sClients->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) { // operation failed
    nsAutoString addressStr;
    AddressToString(aBdAddr, addressStr);
    BT_LOGD("ReadRemoteRssi failed: clientIf = %d, bdAddr = %s, rssi = %d, "
            "status = %d", aClientIf, NS_ConvertUTF16toUTF8(addressStr).get(),
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

/*
 * Some actions will trigger the registration procedure. These actions will
 * be taken only after the registration has been done successfully.
 * If the registration fails, all the existing actions above should be
 * rejected.
 */
void
BluetoothGattManager::RegisterServerNotification(BluetoothGattStatus aStatus,
                                                 int aServerIf,
                                                 const BluetoothUuid& aAppUuid)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aAppUuid, 0 /* Start */, UuidComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  server->mIsRegistering = false;

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || aStatus != GATT_STATUS_SUCCESS || !sBluetoothGattInterface) {
    nsAutoString appUuidStr;
    UuidToString(aAppUuid, appUuidStr);

    BT_LOGD("RegisterServer failed: serverIf = %d, status = %d, appUuid = %s",
             aServerIf, aStatus, NS_ConvertUTF16toUTF8(appUuidStr).get());

    if (server->mConnectPeripheralRunnable) {
      // Reject the connect peripheral request
      DispatchReplyError(
        server->mConnectPeripheralRunnable,
        NS_LITERAL_STRING(
          "ConnectPeripheral failed due to registration failed"));
      server->mConnectPeripheralRunnable = nullptr;
    }

    if (server->mAddServiceState.mRunnable) {
      // Reject the add service request
      DispatchReplyError(
        server->mAddServiceState.mRunnable,
        NS_LITERAL_STRING(
          "AddService failed due to registration failed"));
      server->mAddServiceState.Reset();
    }

    sServers->RemoveElement(server);
    return;
  }

  server->mServerIf = aServerIf;

  // Notify BluetoothGattServer to update the serverIf
  bs->DistributeSignal(
    NS_LITERAL_STRING("ServerRegistered"),
    aAppUuid, BluetoothValue(uint32_t(aServerIf)));

  if (server->mConnectPeripheralRunnable) {
    // Only one entry exists in the map during first connect peripheral request
    const BluetoothAddress& deviceAddr = server->mConnectionMap.Iter().Key();

    sBluetoothGattInterface->ConnectPeripheral(
      aServerIf, deviceAddr, true /* direct connect */, TRANSPORT_AUTO,
      new ConnectPeripheralResultHandler(server, deviceAddr));
  }

  if (server->mAddServiceState.mRunnable) {
    sBluetoothGattInterface->AddService(
      server->mServerIf,
      server->mAddServiceState.mServiceId,
      server->mAddServiceState.mHandleCount,
      new ServerAddServiceResultHandler(server));
  }
}

void
BluetoothGattManager::ConnectionNotification(int aConnId,
                                             int aServerIf,
                                             bool aConnected,
                                             const BluetoothAddress& aBdAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  // Update the connection map based on the connection status
  if (aConnected) {
    server->mConnectionMap.Put(aBdAddr, aConnId);
  } else {
    server->mConnectionMap.Remove(aBdAddr);
  }

  nsAutoString bdAddrStr;
  AddressToString(aBdAddr, bdAddrStr);

  // Notify BluetoothGattServer that connection status changed
  InfallibleTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "Connected", aConnected);
  AppendNamedValue(props, "Address", bdAddrStr);
  bs->DistributeSignal(
    NS_LITERAL_STRING(GATT_CONNECTION_STATE_CHANGED_ID),
    server->mAppUuid,
    BluetoothValue(props));

  // Resolve or reject connect/disconnect peripheral requests
  if (server->mConnectPeripheralRunnable) {
    if (aConnected) {
      DispatchReplySuccess(server->mConnectPeripheralRunnable);
    } else {
      DispatchReplyError(server->mConnectPeripheralRunnable,
                         NS_LITERAL_STRING("ConnectPeripheral failed"));
    }
    server->mConnectPeripheralRunnable = nullptr;
  } else if (server->mDisconnectPeripheralRunnable) {
    if (!aConnected) {
      DispatchReplySuccess(server->mDisconnectPeripheralRunnable);
    } else {
      DispatchReplyError(server->mDisconnectPeripheralRunnable,
                         NS_LITERAL_STRING("DisconnectPeripheral failed"));
    }
    server->mDisconnectPeripheralRunnable = nullptr;
  }
}

void
BluetoothGattManager::ServiceAddedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothGattServiceId& aServiceId,
  const BluetoothAttributeHandle& aServiceHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || aStatus != GATT_STATUS_SUCCESS) {
    if (server->mAddServiceState.mRunnable) {
      DispatchReplyError(server->mAddServiceState.mRunnable,
                         NS_LITERAL_STRING("ServiceAddedNotification failed"));
      server->mAddServiceState.Reset();
    }
    return;
  }

  // Notify BluetoothGattServer to update service handle
  InfallibleTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "ServiceId", aServiceId);
  AppendNamedValue(props, "ServiceHandle", aServiceHandle);
  bs->DistributeSignal(NS_LITERAL_STRING("ServiceHandleUpdated"),
                       server->mAppUuid,
                       BluetoothValue(props));

  if (server->mAddServiceState.mRunnable) {
    DispatchReplySuccess(server->mAddServiceState.mRunnable);
    server->mAddServiceState.Reset();
  }
}

void
BluetoothGattManager::IncludedServiceAddedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aIncludedServiceHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    if (server->mAddIncludedServiceRunnable) {
      DispatchReplyError(
        server->mAddIncludedServiceRunnable,
        NS_LITERAL_STRING("IncludedServiceAddedNotification failed"));
      server->mAddIncludedServiceRunnable = nullptr;
    }
    return;
  }

  if (server->mAddIncludedServiceRunnable) {
    DispatchReplySuccess(server->mAddIncludedServiceRunnable);
    server->mAddIncludedServiceRunnable = nullptr;
  }
}

void
BluetoothGattManager::CharacteristicAddedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothUuid& aCharId,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aCharacteristicHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || aStatus != GATT_STATUS_SUCCESS) {
    if (server->mAddCharacteristicRunnable) {
      DispatchReplyError(
        server->mAddCharacteristicRunnable,
        NS_LITERAL_STRING("CharacteristicAddedNotification failed"));
      server->mAddCharacteristicRunnable = nullptr;
    }
    return;
  }

  // Notify BluetoothGattServer to update characteristic handle
  InfallibleTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "CharacteristicUuid", aCharId);
  AppendNamedValue(props, "ServiceHandle", aServiceHandle);
  AppendNamedValue(props, "CharacteristicHandle", aCharacteristicHandle);
  bs->DistributeSignal(NS_LITERAL_STRING("CharacteristicHandleUpdated"),
                       server->mAppUuid,
                       BluetoothValue(props));

  if (server->mAddCharacteristicRunnable) {
    DispatchReplySuccess(server->mAddCharacteristicRunnable);
    server->mAddCharacteristicRunnable = nullptr;
  }
}

void
BluetoothGattManager::DescriptorAddedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothUuid& aCharId,
  const BluetoothAttributeHandle& aServiceHandle,
  const BluetoothAttributeHandle& aDescriptorHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);
  MOZ_ASSERT(aServiceHandle == server->mAddDescriptorState.mServiceHandle);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || aStatus != GATT_STATUS_SUCCESS) {
    if (server->mAddDescriptorState.mRunnable) {
      DispatchReplyError(
        server->mAddDescriptorState.mRunnable,
        NS_LITERAL_STRING("DescriptorAddedNotification failed"));
      server->mAddDescriptorState.Reset();
    }
    return;
  }

  // Notify BluetoothGattServer to update descriptor handle
  InfallibleTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "CharacteristicUuid", aCharId);
  AppendNamedValue(props, "ServiceHandle", aServiceHandle);
  AppendNamedValue(props, "CharacteristicHandle",
    server->mAddDescriptorState.mCharacteristicHandle);
  AppendNamedValue(props, "DescriptorHandle", aDescriptorHandle);
  bs->DistributeSignal(NS_LITERAL_STRING("DescriptorHandleUpdated"),
                       server->mAppUuid,
                       BluetoothValue(props));

  if (server->mAddDescriptorState.mRunnable) {
    DispatchReplySuccess(server->mAddDescriptorState.mRunnable);
    server->mAddDescriptorState.Reset();
  }
}

void
BluetoothGattManager::ServiceStartedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothAttributeHandle& aServiceHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    if (server->mStartServiceRunnable) {
      DispatchReplyError(
        server->mStartServiceRunnable,
        NS_LITERAL_STRING("ServiceStartedNotification failed"));
      server->mStartServiceRunnable = nullptr;
    }
    return;
  }

  if (server->mStartServiceRunnable) {
    DispatchReplySuccess(server->mStartServiceRunnable);
    server->mStartServiceRunnable = nullptr;
  }
}

void
BluetoothGattManager::ServiceStoppedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothAttributeHandle& aServiceHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    if (server->mStopServiceRunnable) {
      DispatchReplyError(
        server->mStopServiceRunnable,
        NS_LITERAL_STRING("ServiceStoppedNotification failed"));
      server->mStopServiceRunnable = nullptr;
    }
    return;
  }

  if (server->mStopServiceRunnable) {
    DispatchReplySuccess(server->mStopServiceRunnable);
    server->mStopServiceRunnable = nullptr;
  }
}

void
BluetoothGattManager::ServiceDeletedNotification(
  BluetoothGattStatus aStatus,
  int aServerIf,
  const BluetoothAttributeHandle& aServiceHandle)
{
  MOZ_ASSERT(NS_IsMainThread());

  size_t index = sServers->IndexOf(aServerIf, 0 /* Start */,
                                   InterfaceIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = sServers->ElementAt(index);

  if (aStatus != GATT_STATUS_SUCCESS) {
    if (server->mRemoveServiceRunnable) {
      DispatchReplyError(
        server->mRemoveServiceRunnable,
        NS_LITERAL_STRING("ServiceStoppedNotification failed"));
      server->mRemoveServiceRunnable = nullptr;
    }
    return;
  }

  if (server->mRemoveServiceRunnable) {
    DispatchReplySuccess(server->mRemoveServiceRunnable);
    server->mRemoveServiceRunnable = nullptr;
  }
}

void
BluetoothGattManager::RequestReadNotification(
  int aConnId,
  int aTransId,
  const BluetoothAddress& aBdAddr,
  const BluetoothAttributeHandle& aAttributeHandle,
  int aOffset,
  bool aIsLong)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(aConnId);
  NS_ENSURE_TRUE_VOID(sBluetoothGattInterface);
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sServers->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  // Send an error response for unsupported requests
  if (aIsLong || aOffset > 0) {
    BT_LOGR("Unsupported long attribute read requests");
    BluetoothGattResponse response;
    memset(&response, 0, sizeof(BluetoothGattResponse));
    sBluetoothGattInterface->SendResponse(
      aConnId,
      aTransId,
      GATT_STATUS_REQUEST_NOT_SUPPORTED,
      response,
      new ServerSendResponseResultHandler(server));
    return;
  }

  nsAutoString bdAddrStr;
  AddressToString(aBdAddr, bdAddrStr);

  // Distribute a signal to gattServer
  InfallibleTArray<BluetoothNamedValue> properties;

  AppendNamedValue(properties, "TransId", aTransId);
  AppendNamedValue(properties, "AttrHandle", aAttributeHandle);
  AppendNamedValue(properties, "Address", bdAddrStr);
  AppendNamedValue(properties, "NeedResponse", true);
  AppendNamedValue(properties, "Value", nsTArray<uint8_t>());

  bs->DistributeSignal(NS_LITERAL_STRING("ReadRequested"),
                       server->mAppUuid,
                       properties);
}

void
BluetoothGattManager::RequestWriteNotification(
  int aConnId,
  int aTransId,
  const BluetoothAddress& aBdAddr,
  const BluetoothAttributeHandle& aAttributeHandle,
  int aOffset,
  int aLength,
  const uint8_t* aValue,
  bool aNeedResponse,
  bool aIsPrepareWrite)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(aConnId);
  NS_ENSURE_TRUE_VOID(sBluetoothGattInterface);
  BluetoothService* bs = BluetoothService::Get();
  NS_ENSURE_TRUE_VOID(bs);

  size_t index = sServers->IndexOf(aConnId, 0 /* Start */, ConnIdComparator());
  NS_ENSURE_TRUE_VOID(index != sServers->NoIndex);

  RefPtr<BluetoothGattServer> server = (*sServers)[index];

  // Send an error response for unsupported requests
  if (aIsPrepareWrite || aOffset > 0) {
    BT_LOGR("Unsupported prepare write or long attribute write requests");
    if (aNeedResponse) {
      BluetoothGattResponse response;
      memset(&response, 0, sizeof(BluetoothGattResponse));
      sBluetoothGattInterface->SendResponse(
        aConnId,
        aTransId,
        GATT_STATUS_REQUEST_NOT_SUPPORTED,
        response,
        new ServerSendResponseResultHandler(server));
    }
    return;
  }

  nsAutoString bdAddrStr;
  AddressToString(aBdAddr, bdAddrStr);

  // Distribute a signal to gattServer
  InfallibleTArray<BluetoothNamedValue> properties;

  AppendNamedValue(properties, "TransId", aTransId);
  AppendNamedValue(properties, "AttrHandle", aAttributeHandle);
  AppendNamedValue(properties, "Address", bdAddrStr);
  AppendNamedValue(properties, "NeedResponse", aNeedResponse);

  nsTArray<uint8_t> value;
  value.AppendElements(aValue, aLength);
  AppendNamedValue(properties, "Value", value);

  bs->DistributeSignal(NS_LITERAL_STRING("WriteRequested"),
                       server->mAppUuid,
                       properties);
}

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
  sServers = nullptr;
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
  MOZ_ASSERT(aClient->mDiscoverRunnable);
  ENSURE_GATT_INTF_IN_ATTR_DISCOVER(aClient);

  if (!aClient->mCharacteristics.IsEmpty()) {
    sBluetoothGattInterface->GetDescriptor(
      aClient->mConnId,
      aServiceId,
      aClient->mCharacteristics[0].mId,
      true, // first descriptor
      BluetoothGattId(),
      new DiscoverResultHandler(aClient));
    aClient->mCharacteristics.RemoveElementAt(0);
  } else if (!aClient->mServices.IsEmpty()) {
    sBluetoothGattInterface->GetIncludedService(
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
