/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
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
#include "nsIAudioManager.h"
#include "nsIObserverService.h"
#include "MainThreadUtils.h"


using namespace mozilla;
USING_BLUETOOTH_NAMESPACE

namespace {
  StaticRefPtr<BluetoothA2dpManager> sBluetoothA2dpManager;
  bool sInShutdown = false;
} // anonymous namespace

NS_IMETHODIMP
BluetoothA2dpManager::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
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
  ResetA2dp();
  ResetAvrcp();
}

bool
BluetoothA2dpManager::Init()
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, false);
  if (NS_FAILED(obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false))) {
    BT_WARNING("Failed to add shutdown observer!");
    return false;
  }

  return true;
}

BluetoothA2dpManager::~BluetoothA2dpManager()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);
  if (NS_FAILED(obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID))) {
    BT_WARNING("Failed to remove shutdown observer!");
  }
}

void
BluetoothA2dpManager::ResetA2dp()
{
  mA2dpConnected = false;
  mSinkState = SinkState::SINK_DISCONNECTED;
  mController = nullptr;
}

void
BluetoothA2dpManager::ResetAvrcp()
{
  mAvrcpConnected = false;
  mDuration = 0;
  mMediaNumber = 0;
  mTotalMediaCount = 0;
  mPosition = 0;
  mPlayStatus = ControlPlayStatus::PLAYSTATUS_UNKNOWN;
}

static BluetoothA2dpManager::SinkState
StatusStringToSinkState(const nsAString& aStatus)
{
  BluetoothA2dpManager::SinkState state;
  if (aStatus.EqualsLiteral("disconnected")) {
    state = BluetoothA2dpManager::SinkState::SINK_DISCONNECTED;
  } else if (aStatus.EqualsLiteral("connecting")) {
    state = BluetoothA2dpManager::SinkState::SINK_CONNECTING;
  } else if (aStatus.EqualsLiteral("connected")) {
    state = BluetoothA2dpManager::SinkState::SINK_CONNECTED;
  } else if (aStatus.EqualsLiteral("playing")) {
    state = BluetoothA2dpManager::SinkState::SINK_PLAYING;
  } else if (aStatus.EqualsLiteral("disconnecting")) {
    state = BluetoothA2dpManager::SinkState::SINK_DISCONNECTING;
  } else {
    MOZ_ASSERT(false, "Unknown sink state");
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
  NS_ENSURE_TRUE(manager->Init(), nullptr);

  sBluetoothA2dpManager = manager;
  return sBluetoothA2dpManager;
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
BluetoothA2dpManager::Connect(const nsAString& aDeviceAddress,
                              BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(aController && !mController);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs || sInShutdown) {
    aController->OnConnect(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  if (mA2dpConnected) {
    aController->OnConnect(NS_LITERAL_STRING(ERR_ALREADY_CONNECTED));
    return;
  }

  mDeviceAddress = aDeviceAddress;
  mController = aController;

  bs->SendSinkMessage(aDeviceAddress, NS_LITERAL_STRING("Connect"));
}

void
BluetoothA2dpManager::Disconnect(BluetoothProfileController* aController)
{
  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    if (aController) {
      aController->OnDisconnect(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  if (!mA2dpConnected) {
    if (aController) {
      aController->OnDisconnect(NS_LITERAL_STRING(ERR_ALREADY_DISCONNECTED));
    }
    return;
  }

  MOZ_ASSERT(!mDeviceAddress.IsEmpty());
  MOZ_ASSERT(!mController);

  mController = aController;

  bs->SendSinkMessage(mDeviceAddress, NS_LITERAL_STRING("Disconnect"));
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

  nsRefPtr<BluetoothProfileController> controller = mController.forget();
  controller->OnConnect(aErrorStr);
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

  nsRefPtr<BluetoothProfileController> controller = mController.forget();
  controller->OnDisconnect(aErrorStr);
}

/* HandleSinkPropertyChanged update sink state in A2dp
 *
 * Possible values: "disconnected", "disconnecting",
 *                  "connecting", "connected",
 *                  "playing"
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
 *    Disconnected from the remote device
 * 7. "disconnecting" -> "disconnected"
 *    Disconnected from local
 */
void
BluetoothA2dpManager::HandleSinkPropertyChanged(const BluetoothSignal& aSignal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aSignal.value().type() == BluetoothValue::TArrayOfBluetoothNamedValue);

  const nsString& address = aSignal.path();
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
  SinkState prevState = mSinkState;
  mSinkState = StatusStringToSinkState(value.get_nsString());

  NS_ENSURE_TRUE_VOID(mSinkState != prevState);

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
      if (prevState == SinkState::SINK_PLAYING) {
        break;
      }
      
      // case 3: Successfully connected
      MOZ_ASSERT(prevState == SinkState::SINK_CONNECTING);

      mA2dpConnected = true;
      mDeviceAddress = address;
      NotifyConnectionStatusChanged();

      OnConnect(EmptyString());
      break;
    case SinkState::SINK_DISCONNECTED:
      // XXX
      // case 2: Connection attempt failed
      if (prevState == SinkState::SINK_CONNECTING) {  
        OnConnect(NS_LITERAL_STRING("A2dpConnectionError"));
        break;
      }
      
      // case 6: Disconnected from the remote device
      // case 7: Disconnected from local
      MOZ_ASSERT(prevState == SinkState::SINK_CONNECTED ||
                 prevState == SinkState::SINK_PLAYING ||
                 prevState == SinkState::SINK_DISCONNECTING);
  
      mA2dpConnected = false;
      NotifyConnectionStatusChanged();
      mDeviceAddress.Truncate();

      // case 7 only
      if (prevState == SinkState::SINK_DISCONNECTING) {
        OnDisconnect(EmptyString());
      }
      break;
    default:
      break;
  }
}

void
BluetoothA2dpManager::NotifyConnectionStatusChanged()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Notify Gecko observers
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE_VOID(obs);

  if (NS_FAILED(obs->NotifyObservers(this,
                                     BLUETOOTH_A2DP_STATUS_CHANGED_ID,
                                     mDeviceAddress.get()))) {
    BT_WARNING("Failed to notify bluetooth-a2dp-status-changed observsers!");
  }

  // Dispatch an event of status change
  DispatchStatusChangedEvent(
    NS_LITERAL_STRING(A2DP_STATUS_CHANGED_ID), mDeviceAddress, mA2dpConnected);
}

void
BluetoothA2dpManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                          const nsAString& aServiceUuid,
                                          int aChannel)
{
}

void
BluetoothA2dpManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{
}

void
BluetoothA2dpManager::GetAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

bool
BluetoothA2dpManager::IsConnected()
{
  return mA2dpConnected;
}

void
BluetoothA2dpManager::SetAvrcpConnected(bool aConnected)
{
  mAvrcpConnected = aConnected;
  if (!aConnected) {
    ResetAvrcp();
  }
}

bool
BluetoothA2dpManager::IsAvrcpConnected()
{
  return mAvrcpConnected;
}

void
BluetoothA2dpManager::UpdateMetaData(const nsAString& aTitle,
                                     const nsAString& aArtist,
                                     const nsAString& aAlbum,
                                     uint32_t aMediaNumber,
                                     uint32_t aTotalMediaCount,
                                     uint32_t aDuration)
{
  mTitle.Assign(aTitle);
  mArtist.Assign(aArtist);
  mAlbum.Assign(aAlbum);
  mMediaNumber = aMediaNumber;
  mTotalMediaCount = aTotalMediaCount;
  mDuration = aDuration;
}

void
BluetoothA2dpManager::UpdatePlayStatus(uint32_t aDuration,
                                       uint32_t aPosition,
                                       ControlPlayStatus aPlayStatus)
{
  mDuration = aDuration;
  mPosition = aPosition;
  mPlayStatus = aPlayStatus;
}

void
BluetoothA2dpManager::GetAlbum(nsAString& aAlbum)
{
    aAlbum.Assign(mAlbum);
}

uint32_t
BluetoothA2dpManager::GetDuration()
{
  return mDuration;
}

ControlPlayStatus
BluetoothA2dpManager::GetPlayStatus()
{
  return mPlayStatus;
}

uint32_t
BluetoothA2dpManager::GetPosition()
{
  return mPosition;
}

uint32_t
BluetoothA2dpManager::GetMediaNumber()
{
  return mMediaNumber;
}

void
BluetoothA2dpManager::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
}

NS_IMPL_ISUPPORTS1(BluetoothA2dpManager, nsIObserver)

