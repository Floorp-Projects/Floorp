/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothCommon.h"
#include "BluetoothService.h"
#include "BluetoothSocket.h"
#include "BluetoothUtils.h"

#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "MainThreadUtils.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

using namespace mozilla;
USING_BLUETOOTH_NAMESPACE
// AVRC_ID op code follows bluedroid avrc_defs.h
#define AVRC_ID_REWIND  0x48
#define AVRC_ID_FAST_FOR 0x49
#define AVRC_KEY_PRESS_STATE  1
#define AVRC_KEY_RELEASE_STATE  0
// bluedroid bt_rc.h
#define AVRC_MAX_ATTR_STR_LEN 255

namespace {
  StaticRefPtr<BluetoothA2dpManager> sBluetoothA2dpManager;
  bool sInShutdown = false;
  static BluetoothA2dpInterface* sBtA2dpInterface;
} // namespace

const int BluetoothA2dpManager::MAX_NUM_CLIENTS = 1;

NS_IMETHODIMP
BluetoothA2dpManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothA2dpManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothA2dpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

BluetoothA2dpManager::BluetoothA2dpManager()
{
  Reset();
}

void
BluetoothA2dpManager::Reset()
{
  mA2dpConnected = false;
  mSinkState = SinkState::SINK_DISCONNECTED;
  mController = nullptr;
}

static void
AvStatusToSinkString(BluetoothA2dpConnectionState aState, nsAString& aString)
{
  switch (aState) {
    case A2DP_CONNECTION_STATE_DISCONNECTED:
      aString.AssignLiteral("disconnected");
      break;
    case A2DP_CONNECTION_STATE_CONNECTING:
      aString.AssignLiteral("connecting");
      break;
    case A2DP_CONNECTION_STATE_CONNECTED:
      aString.AssignLiteral("connected");
      break;
    case A2DP_CONNECTION_STATE_DISCONNECTING:
      aString.AssignLiteral("disconnecting");
      break;
    default:
      BT_WARNING("Unknown sink state %d", static_cast<int>(aState));
      return;
  }
}

class BluetoothA2dpManager::RegisterModuleResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  RegisterModuleResultHandler(BluetoothA2dpInterface* aInterface,
                              BluetoothProfileResultHandler* aRes)
    : mInterface(aInterface)
    , mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::RegisterModule failed for A2DP: %d",
               (int)aStatus);

    mInterface->SetNotificationHandler(nullptr);

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void RegisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBtA2dpInterface = mInterface;

    if (mRes) {
      mRes->Init();
    }
  }

private:
  BluetoothA2dpInterface* mInterface;
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothA2dpManager::InitProfileResultHandlerRunnable final
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

/*
 * This function will be only called when Bluetooth is turning on.
 * It is important to register a2dp callbacks before enable() gets called.
 * It is required to register a2dp callbacks before a2dp media task
 * starts up.
 */
// static
void
BluetoothA2dpManager::InitA2dpInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sBtA2dpInterface) {
    BT_LOGR("Bluetooth A2DP interface is already initalized.");
    RefPtr<nsRunnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch A2DP Init runnable");
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
      BT_LOGR("Failed to dispatch A2DP OnError runnable");
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
      BT_LOGR("Failed to dispatch A2DP OnError runnable");
    }
    return;
  }

  auto a2dpInterface = btInf->GetBluetoothA2dpInterface();

  if (NS_WARN_IF(!a2dpInterface)) {
    // If there's no A2DP interface, we dispatch a runnable
    // that calls the profile result handler.
    RefPtr<nsRunnable> r =
      new InitProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch A2DP OnError runnable");
    }
    return;
  }

  // Set notification handler _before_ registering the module. It could
  // happen that we receive notifications, before the result handler runs.
  a2dpInterface->SetNotificationHandler(BluetoothA2dpManager::Get());

  setupInterface->RegisterModule(
    SETUP_SERVICE_ID_A2DP, 0, MAX_NUM_CLIENTS,
    new RegisterModuleResultHandler(a2dpInterface, aRes));
}

BluetoothA2dpManager::~BluetoothA2dpManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

/*
 * Static functions
 */

static BluetoothA2dpManager::SinkState
StatusStringToSinkState(const nsAString& aStatus)
{
  BluetoothA2dpManager::SinkState state =
    BluetoothA2dpManager::SinkState::SINK_UNKNOWN;
  if (aStatus.EqualsLiteral("disconnected")) {
    state = BluetoothA2dpManager::SinkState::SINK_DISCONNECTED;
  } else if (aStatus.EqualsLiteral("connecting")) {
    state = BluetoothA2dpManager::SinkState::SINK_CONNECTING;
  } else if (aStatus.EqualsLiteral("connected")) {
    state = BluetoothA2dpManager::SinkState::SINK_CONNECTED;
  } else if (aStatus.EqualsLiteral("playing")) {
    state = BluetoothA2dpManager::SinkState::SINK_PLAYING;
  } else {
    BT_WARNING("Unknown sink state");
  }
  return state;
}

//static
BluetoothA2dpManager*
BluetoothA2dpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothA2dpManager already exists, exit early
  if (sBluetoothA2dpManager) {
    return sBluetoothA2dpManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothA2dpManager* manager = new BluetoothA2dpManager();
  sBluetoothA2dpManager = manager;
  return sBluetoothA2dpManager;
}

class BluetoothA2dpManager::UnregisterModuleResultHandler final
  : public BluetoothSetupResultHandler
{
public:
  UnregisterModuleResultHandler(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    MOZ_ASSERT(NS_IsMainThread());

    BT_WARNING("BluetoothSetupInterface::UnregisterModule failed for A2DP: %d",
               (int)aStatus);

    sBtA2dpInterface->SetNotificationHandler(nullptr);
    sBtA2dpInterface = nullptr;

    if (mRes) {
      mRes->OnError(NS_ERROR_FAILURE);
    }
  }

  void UnregisterModule() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    sBtA2dpInterface->SetNotificationHandler(nullptr);
    sBtA2dpInterface = nullptr;

    if (mRes) {
      mRes->Deinit();
    }
  }

private:
  RefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothA2dpManager::DeinitProfileResultHandlerRunnable final
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
BluetoothA2dpManager::DeinitA2dpInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!sBtA2dpInterface) {
    BT_LOGR("Bluetooth A2DP interface has not been initalized.");
    RefPtr<nsRunnable> r =
      new DeinitProfileResultHandlerRunnable(aRes, NS_OK);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch A2DP Deinit runnable");
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
      BT_LOGR("Failed to dispatch A2DP OnError runnable");
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
      BT_LOGR("Failed to dispatch A2DP OnError runnable");
    }
    return;
  }

  setupInterface->UnregisterModule(
    SETUP_SERVICE_ID_A2DP,
    new UnregisterModuleResultHandler(aRes));
}

void
BluetoothA2dpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  sBluetoothA2dpManager = nullptr;
}

void
BluetoothA2dpManager::OnConnectError()
{
  MOZ_ASSERT(NS_IsMainThread());

  mController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));

  mController = nullptr;
  mDeviceAddress.Clear();
}

class BluetoothA2dpManager::ConnectResultHandler final
  : public BluetoothA2dpResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_LOGR("BluetoothA2dpInterface::Connect failed: %d", (int)aStatus);

    NS_ENSURE_TRUE_VOID(sBluetoothA2dpManager);
    sBluetoothA2dpManager->OnConnectError();
  }
};

void
BluetoothA2dpManager::Connect(const BluetoothAddress& aDeviceAddress,
                              BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsCleared());
  MOZ_ASSERT(aController);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || sInShutdown) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  if (mA2dpConnected) {
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_ALREADY_CONNECTED));
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;

  if (!sBtA2dpInterface) {
    BT_LOGR("sBluetoothA2dpInterface is null");
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  sBtA2dpInterface->Connect(mDeviceAddress, new ConnectResultHandler());
}

void
BluetoothA2dpManager::OnDisconnectError()
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_TRUE_VOID(mController);

  mController->NotifyCompletion(NS_LITERAL_STRING(ERR_DISCONNECTION_FAILED));
}

class BluetoothA2dpManager::DisconnectResultHandler final
  : public BluetoothA2dpResultHandler
{
public:
  void OnError(BluetoothStatus aStatus) override
  {
    BT_LOGR("BluetoothA2dpInterface::Disconnect failed: %d", (int)aStatus);

    NS_ENSURE_TRUE_VOID(sBluetoothA2dpManager);
    sBluetoothA2dpManager->OnDisconnectError();
  }
};

void
BluetoothA2dpManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  if (!mA2dpConnected) {
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_ALREADY_DISCONNECTED));
    }
    return;
  }

  MOZ_ASSERT(!mDeviceAddress.IsCleared());

  mController = aController;

  if (!sBtA2dpInterface) {
    BT_LOGR("sBluetoothA2dpInterface is null");
    if (aController) {
      aController->NotifyCompletion(
        NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  sBtA2dpInterface->Disconnect(mDeviceAddress, new DisconnectResultHandler());
}

void
BluetoothA2dpManager::OnConnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  RefPtr<BluetoothProfileController> controller = mController.forget();
  controller->NotifyCompletion(aErrorStr);
}

void
BluetoothA2dpManager::OnDisconnect(const nsAString& aErrorStr)
{
  MOZ_ASSERT(NS_IsMainThread());

  /**
   * On the one hand, notify the controller that we've done for outbound
   * connections. On the other hand, we do nothing for inbound connections.
   */
  NS_ENSURE_TRUE_VOID(mController);

  RefPtr<BluetoothProfileController> controller = mController.forget();
  controller->NotifyCompletion(aErrorStr);

  Reset();
}

/* HandleSinkPropertyChanged update sink state in A2dp
 *
 * Possible values: "disconnected", "connecting", "connected", "playing"
 *
 * 1. "disconnected" -> "connecting"
 *    Either an incoming or outgoing connection attempt ongoing
 * 2. "connecting" -> "disconnected"
 *    Connection attempt failed
 * 3. "connecting" -> "connected"
 *    Successfully connected
 * 4. "connected" -> "playing"
 *    Audio stream active
 * 5. "playing" -> "connected"
 *    Audio stream suspended
 * 6. "connected" -> "disconnected"
 *    "playing" -> "disconnected"
 *    Disconnected from local or the remote device
 */
void
BluetoothA2dpManager::HandleSinkPropertyChanged(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSignal.value().type() ==
             BluetoothValue::TArrayOfBluetoothNamedValue);

  BluetoothAddress address;
  NS_ENSURE_TRUE_VOID(NS_SUCCEEDED(StringToAddress(aSignal.path(), address)));

  /**
   * Update sink property only if
   * - mDeviceAddress is empty (A2dp is disconnected), or
   * - this property change is from the connected sink.
   */
  NS_ENSURE_TRUE_VOID(mDeviceAddress.IsCleared() || mDeviceAddress == address);

  const InfallibleTArray<BluetoothNamedValue>& arr =
    aSignal.value().get_ArrayOfBluetoothNamedValue();
  MOZ_ASSERT(arr.Length() == 1);

  /**
   * There are three properties:
   * - "State": a string
   * - "Connected": a boolean value
   * - "Playing": a boolean value
   *
   * Note that only "State" is handled in this function.
   */

  const nsString& name = arr[0].name();
  NS_ENSURE_TRUE_VOID(name.EqualsLiteral("State"));

  const BluetoothValue& value = arr[0].value();
  MOZ_ASSERT(value.type() == BluetoothValue::TnsString);
  SinkState newState = StatusStringToSinkState(value.get_nsString());
  NS_ENSURE_TRUE_VOID((newState != SinkState::SINK_UNKNOWN) &&
                      (newState != mSinkState));

  SinkState prevState = mSinkState;
  mSinkState = newState;

  switch(mSinkState) {
    case SinkState::SINK_CONNECTING:
      // case 1: Either an incoming or outgoing connection attempt ongoing
      MOZ_ASSERT(prevState == SinkState::SINK_DISCONNECTED);
      break;
    case SinkState::SINK_PLAYING:
      // case 4: Audio stream active
      MOZ_ASSERT(prevState == SinkState::SINK_CONNECTED);
      break;
    case SinkState::SINK_CONNECTED:
      // case 5: Audio stream suspended
      if (prevState == SinkState::SINK_PLAYING ||
          prevState == SinkState::SINK_CONNECTED) {
        break;
      }

      // case 3: Successfully connected
      mA2dpConnected = true;
      mDeviceAddress = address;
      NotifyConnectionStatusChanged();

      OnConnect(EmptyString());
      break;
    case SinkState::SINK_DISCONNECTED:
      // case 2: Connection attempt failed
      if (prevState == SinkState::SINK_CONNECTING) {
        OnConnect(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
        break;
      }

      // case 6: Disconnected from the remote device
      MOZ_ASSERT(prevState == SinkState::SINK_CONNECTED ||
                 prevState == SinkState::SINK_PLAYING) ;

      mA2dpConnected = false;
      NotifyConnectionStatusChanged();
      mDeviceAddress.Clear();
      OnDisconnect(EmptyString());
      break;
    default:
      break;
  }
}

/*
 * Reset connection state to DISCONNECTED to handle backend error. The state
 * change triggers UI status bar update as ordinary bluetooth turn-off sequence.
 */
void
BluetoothA2dpManager::HandleBackendError()
{
  if (mSinkState != SinkState::SINK_DISCONNECTED) {
    ConnectionStateNotification(A2DP_CONNECTION_STATE_DISCONNECTED,
                                mDeviceAddress);
  }
}

void
BluetoothA2dpManager::NotifyConnectionStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  nsAutoString deviceAddressStr;
  AddressToString(mDeviceAddress, deviceAddressStr);

  if (NS_FAILED(obs->NotifyObservers(this,
                                     BLUETOOTH_A2DP_STATUS_CHANGED_ID,
                                     deviceAddressStr.get()))) {
    BT_WARNING("Failed to notify bluetooth-a2dp-status-changed observsers!");
  }

  // Dispatch an event of status change
  DispatchStatusChangedEvent(
    NS_LITERAL_STRING(A2DP_STATUS_CHANGED_ID), deviceAddressStr, mA2dpConnected);
}

void
BluetoothA2dpManager::OnGetServiceChannel(const BluetoothAddress& aDeviceAddress,
                                          const BluetoothUuid& aServiceUuid,
                                          int aChannel)
{
}

void
BluetoothA2dpManager::OnUpdateSdpRecords(const BluetoothAddress& aDeviceAddress)
{
}

void
BluetoothA2dpManager::GetAddress(BluetoothAddress& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

bool
BluetoothA2dpManager::IsConnected()
{
  return mA2dpConnected;
}

/*
 * Notifications
 */

void
BluetoothA2dpManager::ConnectionStateNotification(
  BluetoothA2dpConnectionState aState, const BluetoothAddress& aBdAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString a2dpState;
  AvStatusToSinkString(aState, a2dpState);

  InfallibleTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "State", a2dpState);

  nsAutoString addressStr;
  AddressToString(aBdAddr, addressStr);

  HandleSinkPropertyChanged(BluetoothSignal(NS_LITERAL_STRING("AudioSink"),
                                            addressStr, props));
}

void
BluetoothA2dpManager::AudioStateNotification(BluetoothA2dpAudioState aState,
                                             const BluetoothAddress& aBdAddr)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsString a2dpState;

  if (aState == A2DP_AUDIO_STATE_STARTED) {
    a2dpState = NS_LITERAL_STRING("playing");
  } else if (aState == A2DP_AUDIO_STATE_STOPPED) {
    // for avdtp state stop stream
    a2dpState = NS_LITERAL_STRING("connected");
  } else if (aState == A2DP_AUDIO_STATE_REMOTE_SUSPEND) {
    // for avdtp state suspend stream from remote side
    a2dpState = NS_LITERAL_STRING("connected");
  }

  InfallibleTArray<BluetoothNamedValue> props;
  AppendNamedValue(props, "State", a2dpState);

  nsAutoString addressStr;
  AddressToString(aBdAddr, addressStr);

  HandleSinkPropertyChanged(BluetoothSignal(NS_LITERAL_STRING("AudioSink"),
                                            addressStr, props));
}

NS_IMPL_ISUPPORTS(BluetoothA2dpManager, nsIObserver)
