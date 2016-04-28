/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHidManager.h"
#include "BluetoothService.h"
#include "BluetoothUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsIObserverService.h"

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

#define ENSURE_HID_DEV_IS_CONNECTED                                           \
  do {                                                                        \
    if(!IsConnected()) {                                                      \
      BT_LOGR("Device is not connected");                                     \
      return;                                                                 \
    }                                                                         \
  } while(0)                                                                  \

#define ENSURE_HID_INTF_IS_EXISTED                                            \
  do {                                                                        \
    if(!sBluetoothHidInterface) {                                             \
      BT_LOGR("sBluetoothHidInterface is null");                              \
      return;                                                                 \
    }                                                                         \
  } while(0)                                                                  \

namespace {
  StaticRefPtr<BluetoothHidManager> sBluetoothHidManager;
  static BluetoothHidInterface* sBluetoothHidInterface = nullptr;
  bool sInShutdown = false;
} // namesapce

const int BluetoothHidManager::MAX_NUM_CLIENTS = 1;

BluetoothHidManager::BluetoothHidManager()
  : mHidConnected(false)
{
}

void
BluetoothHidManager::Reset()
{
  mDeviceAddress.Clear();
  mController = nullptr;
  mHidConnected = false;
}

class BluetoothHidManager::RegisterModuleResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  RegisterModuleResultHandler(BluetoothHidInterface* aInterface,
                              BluetoothProfileResultHandler* aRes)
    : mInterface(aInterface)
    , mRes(aRes)
  {
    MOZ_ASSERT(mInterface);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::RegisterModule failed for HID: %d",
               (int)aStatus);

    mInterface->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void RegisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothHidInterface = mInterface;

    if (mRes) {
      mRes->Init();
    }
  }

private:
  BluetoothHidInterface* mInterface;
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothHidManager::InitProfileResultHandlerRunnable final
  : public Runnable
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
BluetoothHidManager::InitHidInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sBluetoothHidInterface) {
    BT_LOGR("Bluetooth HID interface is already initialized.");
    RefPtr<Runnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID Init runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID OnError runnable");
    }
    return;
  }

  auto hidinterface = btInf->GetBluetoothHidInterface();

  if (NS_WARN_IF(!hidinterface)) {
    // If there's no HID interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID OnError runnable");
    }
    return;
  }

  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  hidinterface->SetNotificationHandler(BluetoothHidManager::Get());

  setupInterface->RegisterModule(
    SETUP_SERVICE_ID_HID, 0, MAX_NUM_CLIENTS,
    new RegisterModuleResultHandler(hidinterface, aRes));
}

BluetoothHidManager::~BluetoothHidManager()
{ }

void
BluetoothHidManager::Uninit()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

class BluetoothHidManager::UnregisterModuleResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  UnregisterModuleResultHandler(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::UnregisterModule failed for HID: %d",
               (int)aStatus);

    sBluetoothHidInterface->SetNotificationHandler(nullptr);
    sBluetoothHidInterface = nullptr;

    sBluetoothHidManager->Uninit();
    sBluetoothHidManager = nullptr;

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void UnregisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBluetoothHidInterface->SetNotificationHandler(nullptr);
    sBluetoothHidInterface = nullptr;

    sBluetoothHidManager->Uninit();
    sBluetoothHidManager = nullptr;

    if (mRes) {
      mRes->Deinit();
    }
  }

private:
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothHidManager::DeinitProfileResultHandlerRunnable final
  : public Runnable
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
BluetoothHidManager::DeinitHidInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sBluetoothHidInterface) {
    BT_LOGR("Bluetooth Hid interface has not been initialized.");
    RefPtr<Runnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID Deinit runnable");
    }
    return;
  }

  auto btInf = BluetoothInterface::GetInstance();

  if (NS_WARN_IF(!btInf)) {
    // If there's no backend interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID OnError runnable");
    }
    return;
  }

  auto setupInterface = btInf->GetBluetoothSetupInterface();

  if (NS_WARN_IF(!setupInterface)) {
    // If there's no Setup interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<Runnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HID OnError runnable");
    }
    return;
  }

  setupInterface->UnregisterModule(
    SETUP_SERVICE_ID_HID,
    new UnregisterModuleResultHandler(aRes));
}

NS_IMETHODIMP
BluetoothHidManager::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothHidManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothHidManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

// static
BluetoothHidManager*
BluetoothHidManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (sBluetoothHidManager) {
    return sBluetoothHidManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  sBluetoothHidManager = new BluetoothHidManager();

  return sBluetoothHidManager;
}

void
BluetoothHidManager::NotifyConnectionStateChanged(const nsAString& aType)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  if (NS_FAILED(obs->NotifyObservers(this, NS_ConvertUTF16toUTF8(aType).get(),
                                     deviceAddressStr.get()))) {
    BT_WARNING("Failed to notify observsers!");
  }

  // Dispatch an event of status change
  DispatchStatusChangedEvent(
    NS_LITERAL_STRING(HID_STATUS_CHANGED_ID), mDeviceAddress, IsConnected());
}

bool
BluetoothHidManager::IsConnected()
{
  return mHidConnected;
}

void
BluetoothHidManager::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());

  mController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
  Reset();
}

class BluetoothHidManager::ConnectResultHandler final
  : public BluetoothHidResultHandler
{
public:
  ConnectResultHandler(BluetoothHidManager* aHidManager)
    : mHidManager(aHidManager)
  {
    MOZ_ASSERT(mHidManager);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHidInterface::Connect failed: %d",
               (int)aStatus);
    mHidManager->OnConnectError();
  }

private:
  BluetoothHidManager* mHidManager;
};

void
BluetoothHidManager::Connect(const BluetoothAddress& aDeviceAddress,
                             BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsCleared());
  MOZ_ASSERT(aController && !mController);

  if(sInShutdown) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  if(IsConnected()) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_ALREADY_CONNECTED));
    return;
  }

  if(!sBluetoothHidInterface) {
    BT_LOGR("sBluetoothHidInterface is null");
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;

  sBluetoothHidInterface->Connect(mDeviceAddress,
                                  new ConnectResultHandler(this));
}

void
BluetoothHidManager::OnDisconnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));

  mController = nullptr;
}

class BluetoothHidManager::DisconnectResultHandler final
  : public BluetoothHidResultHandler
{
public:
  DisconnectResultHandler(BluetoothHidManager* aHidManager)
    : mHidManager(aHidManager)
  {
    MOZ_ASSERT(mHidManager);
  }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHidInterface::Disconnect failed: %d",
               (int)aStatus);
    mHidManager->OnDisconnectError();
  }

private:
  BluetoothHidManager* mHidManager;
};

void
BluetoothHidManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  if (!IsConnected()) {
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_ALREADY_DISCONNECTED));
    }
    return;
  }

  MOZ_ASSERT(!mDeviceAddress.IsCleared());

  if (!sBluetoothHidInterface) {
    BT_LOGR("sBluetoothHidInterface is null");
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  mController = aController;

  sBluetoothHidInterface->Disconnect(mDeviceAddress,
                                     new DisconnectResultHandler(this));
}

void
BluetoothHidManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(aErrorStr);
  mController = nullptr;
}

void
BluetoothHidManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(aErrorStr);
  Reset();
}

class BluetoothHidManager::VirtualUnplugResultHandler final
  : public BluetoothHidResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHidInterface::VirtualUnplug failed: %d", (int)aStatus);
  }
};

void
BluetoothHidManager::VirtualUnplug()
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_HID_DEV_IS_CONNECTED;
  MOZ_ASSERT(!mDeviceAddress.IsCleared());
  ENSURE_HID_INTF_IS_EXISTED;

  sBluetoothHidInterface->VirtualUnplug(
    mDeviceAddress, new VirtualUnplugResultHandler());
}

class BluetoothHidManager::GetReportResultHandler final
  : public BluetoothHidResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHidInterface::GetReport failed: %d", (int)aStatus);
  }
};

void
BluetoothHidManager::GetReport(const BluetoothHidReportType& aReportType,
                               const uint8_t aReportId,
                               const uint16_t aBufSize)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_HID_DEV_IS_CONNECTED;
  MOZ_ASSERT(!mDeviceAddress.IsCleared());
  ENSURE_HID_INTF_IS_EXISTED;

  sBluetoothHidInterface->GetReport(
    mDeviceAddress, aReportType, aReportId, aBufSize,
    new GetReportResultHandler());
}

class BluetoothHidManager::SetReportResultHandler final
  : public BluetoothHidResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHidInterface::SetReport failed: %d", (int)aStatus);
  }
};

void
BluetoothHidManager::SetReport(const BluetoothHidReportType& aReportType,
                               const BluetoothHidReport& aReport)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_HID_DEV_IS_CONNECTED;
  MOZ_ASSERT(!mDeviceAddress.IsCleared());
  ENSURE_HID_INTF_IS_EXISTED;

  sBluetoothHidInterface->SetReport(
    mDeviceAddress, aReportType, aReport, new SetReportResultHandler());
}

class BluetoothHidManager::SendDataResultHandler final
  : public BluetoothHidResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothHidInterface::SendData failed: %d", (int)aStatus);
  }
};

void
BluetoothHidManager::SendData(const uint16_t aDataLen, const uint8_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  ENSURE_HID_DEV_IS_CONNECTED;
  MOZ_ASSERT(!mDeviceAddress.IsCleared());
  ENSURE_HID_INTF_IS_EXISTED;

  sBluetoothHidInterface->SendData(
    mDeviceAddress, aDataLen, aData, new SendDataResultHandler());
}

void
BluetoothHidManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  sBluetoothHidManager = nullptr;
}

void
BluetoothHidManager::HandleBackendError()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mHidConnected) {
    ConnectionStateNotification(mDeviceAddress,
                                HID_CONNECTION_STATE_DISCONNECTED);
  }
}

void
BluetoothHidManager::GetAddress(BluetoothAddress& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

void
BluetoothHidManager::OnUpdateSdpRecords(
  const BluetoothAddress& aDeviceAddress)
{
  // Bluedroid handles this part
  MOZ_ASSERT(false);
}

void
BluetoothHidManager::OnGetServiceChannel(
  const BluetoothAddress& aDeviceAddress,
  const BluetoothUuid& aServiceUuid,
  int aChannel)
{
  // Bluedroid handles this part
  MOZ_ASSERT(false);
}

//
// Bluetooth notifications
//

/**
 * There are totally 10 connection states, and will receive 4 possible
 * states: "connected", "connecting", "disconnected", "disconnecting".
 * Here we only handle connected and disconnected states. We do nothing
 * for remaining states.
 *
 * Possible cases are listed below:
 * CONNECTED:
 *   1. Successful inbound or outbound connection
 * DISCONNECTED:
 *   2. Attempt disconnection succeeded
 *   3. Attempt connection failed
 *   4. Either unpair from the remote device or the remote device is
 *      out of range in connected state
 */
void
BluetoothHidManager::ConnectionStateNotification(
  const BluetoothAddress& aBdAddr, BluetoothHidConnectionState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  BT_LOGR("state %d", aState);

  if (aState == HID_CONNECTION_STATE_CONNECTED) {
    mHidConnected = true;
    mDeviceAddress = aBdAddr;
    NotifyConnectionStateChanged(
      NS_LITERAL_STRING(BLUETOOTH_HID_STATUS_CHANGED_ID));
    OnConnect(EmptyString());
  } else if (aState == HID_CONNECTION_STATE_DISCONNECTED) {
    mHidConnected = false;
    NotifyConnectionStateChanged(
      NS_LITERAL_STRING(BLUETOOTH_HID_STATUS_CHANGED_ID));
    OnDisconnect(EmptyString());
  }
}

NS_IMPL_ISUPPORTS(BluetoothHidManager, nsIObserver)
