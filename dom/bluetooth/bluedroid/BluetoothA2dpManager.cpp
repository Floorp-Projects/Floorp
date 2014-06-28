/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothA2dpManager.h"
#include "BluetoothInterface.h"
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

namespace {
  StaticRefPtr<BluetoothA2dpManager> sBluetoothA2dpManager;
  bool sInShutdown = false;
  static BluetoothA2dpInterface* sBtA2dpInterface;
#if ANDROID_VERSION > 17
  static BluetoothAvrcpInterface* sBtAvrcpInterface;
#endif
} // anonymous namespace

class SinkPropertyChangedHandler : public nsRunnable
{
public:
  SinkPropertyChangedHandler(const BluetoothSignal& aSignal)
    : mSignal(aSignal)
  {
  }

  NS_IMETHOD
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
    NS_ENSURE_TRUE(a2dp, NS_ERROR_FAILURE);
    a2dp->HandleSinkPropertyChanged(mSignal);

    return NS_OK;
  }

private:
  BluetoothSignal mSignal;
};

class RequestPlayStatusTask : public nsRunnable
{
public:
  RequestPlayStatusTask()
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothSignal signal(NS_LITERAL_STRING(REQUEST_MEDIA_PLAYSTATUS_ID),
                           NS_LITERAL_STRING(KEY_ADAPTER),
                           InfallibleTArray<BluetoothNamedValue>());

    BluetoothService* bs = BluetoothService::Get();
    NS_ENSURE_TRUE(bs, NS_ERROR_FAILURE);
    bs->DistributeSignal(signal);

    return NS_OK;
  }
};

#if ANDROID_VERSION > 17
class UpdateRegisterNotificationTask : public nsRunnable
{
public:
  UpdateRegisterNotificationTask(btrc_event_id_t aEventId, uint32_t aParam)
    : mEventId(aEventId)
    , mParam(aParam)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
    NS_ENSURE_TRUE(a2dp, NS_OK);
    a2dp->UpdateRegisterNotification(mEventId, mParam);
    return NS_OK;
  }
private:
  btrc_event_id_t mEventId;
  uint32_t mParam;
};

/*
 * This function maps attribute id and returns corresponding values
 * Attribute id refers to btrc_media_attr_t in bt_rc.h
 */
static void
ConvertAttributeString(int aAttrId, nsAString& aAttrStr)
{
  BluetoothA2dpManager* a2dp = BluetoothA2dpManager::Get();
  NS_ENSURE_TRUE_VOID(a2dp);

  switch (aAttrId) {
    case BTRC_MEDIA_ATTR_TITLE:
      a2dp->GetTitle(aAttrStr);
      break;
    case BTRC_MEDIA_ATTR_ARTIST:
      a2dp->GetArtist(aAttrStr);
      break;
    case BTRC_MEDIA_ATTR_ALBUM:
      a2dp->GetAlbum(aAttrStr);
      break;
    case BTRC_MEDIA_ATTR_TRACK_NUM:
      aAttrStr.AppendInt(a2dp->GetMediaNumber());
      break;
    case BTRC_MEDIA_ATTR_NUM_TRACKS:
      aAttrStr.AppendInt(a2dp->GetTotalMediaNumber());
      break;
    case BTRC_MEDIA_ATTR_GENRE:
      // TODO: we currently don't support genre from music player
      aAttrStr.Truncate();
      break;
    case BTRC_MEDIA_ATTR_PLAYING_TIME:
      aAttrStr.AppendInt(a2dp->GetDuration());
      break;
  }
}

class UpdateElementAttrsTask : public nsRunnable
{
public:
  UpdateElementAttrsTask(uint8_t aNumAttr, btrc_media_attr_t* aPlayerAttrs)
    : mNumAttr(aNumAttr)
    , mPlayerAttrs(aPlayerAttrs)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    btrc_element_attr_val_t* attrs = new btrc_element_attr_val_t[mNumAttr];
    for (int i = 0; i < mNumAttr; i++) {
      nsAutoString attrText;
      attrs[i].attr_id = mPlayerAttrs[i];
      ConvertAttributeString(mPlayerAttrs[i], attrText);
      strcpy((char *)attrs[i].text, NS_ConvertUTF16toUTF8(attrText).get());
    }

    NS_ENSURE_TRUE(sBtAvrcpInterface, NS_OK);
    sBtAvrcpInterface->GetElementAttrRsp(mNumAttr, attrs);

    return NS_OK;
  }
private:
  uint8_t mNumAttr;
  btrc_media_attr_t* mPlayerAttrs;
};

class UpdatePassthroughCmdTask : public nsRunnable
{
public:
  UpdatePassthroughCmdTask(const nsAString& aName)
    : mName(aName)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  nsresult Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    NS_NAMED_LITERAL_STRING(type, "media-button");
    BroadcastSystemMessage(type, BluetoothValue(mName));

    return NS_OK;
  }
private:
  nsString mName;
};

#endif

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
  ResetA2dp();
  ResetAvrcp();
}

static void
AvStatusToSinkString(btav_connection_state_t aStatus, nsAString& aState)
{
  nsAutoString state;
  if (aStatus == BTAV_CONNECTION_STATE_DISCONNECTED) {
    aState = NS_LITERAL_STRING("disconnected");
  } else if (aStatus == BTAV_CONNECTION_STATE_CONNECTING) {
    aState = NS_LITERAL_STRING("connecting");
  } else if (aStatus == BTAV_CONNECTION_STATE_CONNECTED) {
    aState = NS_LITERAL_STRING("connected");
  } else if (aStatus == BTAV_CONNECTION_STATE_DISCONNECTING) {
    aState = NS_LITERAL_STRING("disconnecting");
  } else {
    BT_WARNING("Unknown sink state");
  }
}

static void
A2dpConnectionStateCallback(btav_connection_state_t aState,
                            bt_bdaddr_t* aBdAddress)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsString remoteDeviceBdAddress;
  BdAddressTypeToString(aBdAddress, remoteDeviceBdAddress);

  nsString a2dpState;
  AvStatusToSinkString(aState, a2dpState);

  InfallibleTArray<BluetoothNamedValue> props;
  BT_APPEND_NAMED_VALUE(props, "State", a2dpState);

  BluetoothSignal signal(NS_LITERAL_STRING("AudioSink"),
                         remoteDeviceBdAddress, props);
  NS_DispatchToMainThread(new SinkPropertyChangedHandler(signal));
}

static void
A2dpAudioStateCallback(btav_audio_state_t aState,
                       bt_bdaddr_t* aBdAddress)
{
  MOZ_ASSERT(!NS_IsMainThread());

  nsString remoteDeviceBdAddress;
  BdAddressTypeToString(aBdAddress, remoteDeviceBdAddress);

  nsString a2dpState;

  if (aState == BTAV_AUDIO_STATE_STARTED) {
    a2dpState = NS_LITERAL_STRING("playing");
  } else if (aState == BTAV_AUDIO_STATE_STOPPED) {
    // for avdtp state stop stream
    a2dpState = NS_LITERAL_STRING("connected");
  } else if (aState == BTAV_AUDIO_STATE_REMOTE_SUSPEND) {
    // for avdtp state suspend stream from remote side
    a2dpState = NS_LITERAL_STRING("connected");
  }

  InfallibleTArray<BluetoothNamedValue> props;
  BT_APPEND_NAMED_VALUE(props, "State", a2dpState);

  BluetoothSignal signal(NS_LITERAL_STRING("AudioSink"),
                         remoteDeviceBdAddress, props);
  NS_DispatchToMainThread(new SinkPropertyChangedHandler(signal));
}

#if ANDROID_VERSION > 17
/*
 * Avrcp 1.3 callbacks
 */

/*
 * This function is to request Gaia player application to update
 * current play status.
 * Callback for play status request
 */
static void
AvrcpGetPlayStatusCallback()
{
  MOZ_ASSERT(!NS_IsMainThread());

  NS_DispatchToMainThread(new RequestPlayStatusTask());
}

/*
 * This function is trying to get element attributes, which request from CT
 * Unlike BlueZ only calls UpdateMetaData, bluedroid does not cache meta data
 * information, but instead uses callback AvrcpGetElementAttrCallback and
 * call get_element_attr_rsp() to reply request.
 *
 * Callback to fetch the get element attributes of the current song
 * aNumAttr: It represents the number of attributes requested in aPlayerAttrs
 * aPlayerAttrs: It represents Attribute Ids
 */
static void
AvrcpGetElementAttrCallback(uint8_t aNumAttr, btrc_media_attr_t* aPlayerAttrs)
{
  MOZ_ASSERT(!NS_IsMainThread());

  NS_DispatchToMainThread(new UpdateElementAttrsTask(aNumAttr, aPlayerAttrs));
}

/*
 * Callback for register notification (Play state change/track change/...)
 * To reply RegisterNotification INTERIM response
 * See AVRCP 1.3 Spec 25.2
 * aParam: It only valids if event_id is BTRC_EVT_PLAY_POS_CHANGED,
 * which is playback interval time
 */
static void
AvrcpRegisterNotificationCallback(btrc_event_id_t aEventId, uint32_t aParam)
{
  MOZ_ASSERT(!NS_IsMainThread());

  NS_DispatchToMainThread(new UpdateRegisterNotificationTask(aEventId, aParam));
}

/*
 * Player application settings is optional for Avrcp 1.3
 * B2G 1.3 currently does not support Player application setting
 * related functions. Support Player Setting in the future version
 */
static void
AvrcpListPlayerAppAttributeCallback()
{
  MOZ_ASSERT(!NS_IsMainThread());

// TODO: Support avrcp application setting related functions
}

static void
AvrcpListPlayerAppValuesCallback(btrc_player_attr_t aAttrId)
{
  MOZ_ASSERT(!NS_IsMainThread());

// TODO: Support avrcp application setting related functions
}

static void
AvrcpGetPlayerAppValueCallback(uint8_t aNumAttr,
                               btrc_player_attr_t* aPlayerAttrs)
{
  MOZ_ASSERT(!NS_IsMainThread());

// TODO: Support avrcp application setting related functions
}

static void
AvrcpGetPlayerAppAttrsTextCallback(uint8_t aNumAttr,
                                   btrc_player_attr_t* PlayerAttrs)
{
  MOZ_ASSERT(!NS_IsMainThread());

// TODO: Support avrcp application setting related functions
}

static void
AvrcpGetPlayerAppValuesTextCallback(uint8_t aAttrId, uint8_t aNumVal,
                                    uint8_t* PlayerVals)
{
  MOZ_ASSERT(!NS_IsMainThread());

// TODO: Support avrcp application setting related functions
}

static void
AvrcpSetPlayerAppValueCallback(btrc_player_settings_t* aPlayerVals)
{
  MOZ_ASSERT(!NS_IsMainThread());

// TODO: Support avrcp application setting related functions
}
#endif

#if ANDROID_VERSION > 18
/*
 * This callback function is to get CT features from Feature Bit Mask.
 * If Advanced Control Player bit is set, CT supports
 * volume sync (absolute volume feature). If Browsing bit is set, Avrcp 1.4
 * Browse feature will be supported
 */
static void
AvrcpRemoteFeaturesCallback(bt_bdaddr_t* aBdAddress,
                            btrc_remote_features_t aFeatures)
{
// TODO: Support avrcp 1.4 absolute volume/browse
}

/*
 * This callback function is to get notification that volume changed on the
 * remote car kit (if it supports Avrcp 1.4), not notification from phone.
 */
static void
AvrcpRemoteVolumeChangedCallback(uint8_t aVolume, uint8_t aCType)
{
// TODO: Support avrcp 1.4 absolute volume/browse
}

/*
 * This callback function is to handle passthrough commands.
 */
static void
AvrcpPassThroughCallback(int aId, int aKeyState)
{
  // Fast-forward and rewind key events won't be generated from bluedroid
  // stack after ANDROID_VERSION > 18, but via passthrough callback.
  nsAutoString name;
  NS_ENSURE_TRUE_VOID(aKeyState == AVRC_KEY_PRESS_STATE ||
                      aKeyState == AVRC_KEY_RELEASE_STATE);
  switch (aId) {
    case AVRC_ID_FAST_FOR:
      if (aKeyState == AVRC_KEY_PRESS_STATE) {
        name.AssignLiteral("media-fast-forward-button-press");
      } else {
        name.AssignLiteral("media-fast-forward-button-release");
      }
      break;
    case AVRC_ID_REWIND:
      if (aKeyState == AVRC_KEY_PRESS_STATE) {
        name.AssignLiteral("media-rewind-button-press");
      } else {
        name.AssignLiteral("media-rewind-button-release");
      }
      break;
    default:
      BT_WARNING("Unable to handle the unknown PassThrough command %d", aId);
      break;
  }
  if (!name.IsEmpty()) {
    NS_DispatchToMainThread(new UpdatePassthroughCmdTask(name));
  }
}
#endif

static btav_callbacks_t sBtA2dpCallbacks = {
  sizeof(sBtA2dpCallbacks),
  A2dpConnectionStateCallback,
  A2dpAudioStateCallback
};

#if ANDROID_VERSION > 17
static btrc_callbacks_t sBtAvrcpCallbacks = {
  sizeof(sBtAvrcpCallbacks),
#if ANDROID_VERSION > 18
  AvrcpRemoteFeaturesCallback,
#endif
  AvrcpGetPlayStatusCallback,
  AvrcpListPlayerAppAttributeCallback,
  AvrcpListPlayerAppValuesCallback,
  AvrcpGetPlayerAppValueCallback,
  AvrcpGetPlayerAppAttrsTextCallback,
  AvrcpGetPlayerAppValuesTextCallback,
  AvrcpSetPlayerAppValueCallback,
  AvrcpGetElementAttrCallback,
  AvrcpRegisterNotificationCallback,
#if ANDROID_VERSION > 18
  AvrcpRemoteVolumeChangedCallback,
  AvrcpPassThroughCallback
#endif
};
#endif

/*
 * This function will be only called when Bluetooth is turning on.
 * It is important to register a2dp callbacks before enable() gets called.
 * It is required to register a2dp callbacks before a2dp media task
 * starts up.
 */
// static
void
BluetoothA2dpManager::InitA2dpInterface()
{
  BluetoothInterface* btInf = BluetoothInterface::GetInstance();
  NS_ENSURE_TRUE_VOID(btInf);

  sBtA2dpInterface = btInf->GetBluetoothA2dpInterface();
  NS_ENSURE_TRUE_VOID(sBtA2dpInterface);

  int ret = sBtA2dpInterface->Init(&sBtA2dpCallbacks);
  if (ret != BT_STATUS_SUCCESS) {
    BT_LOGR("Warning: failed to init a2dp module");
  }

#if ANDROID_VERSION > 17
  sBtAvrcpInterface = btInf->GetBluetoothAvrcpInterface();
  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);

  ret = sBtAvrcpInterface->Init(&sBtAvrcpCallbacks);
  if (ret != BT_STATUS_SUCCESS) {
    BT_LOGR("Warning: failed to init avrcp module");
  }
#endif
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

// static
void
BluetoothA2dpManager::DeinitA2dpInterface()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sBtA2dpInterface) {
    sBtA2dpInterface->Cleanup();
    sBtA2dpInterface = nullptr;
  }
#if ANDROID_VERSION > 17
  if (sBtAvrcpInterface) {
    sBtAvrcpInterface->Cleanup();
    sBtAvrcpInterface = nullptr;
  }
#endif
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

  bt_bdaddr_t remoteAddress;
  StringToBdAddressType(aDeviceAddress, &remoteAddress);

  bt_status_t result = sBtA2dpInterface->Connect(&remoteAddress);
  if (BT_STATUS_SUCCESS != result) {
    BT_LOGR("Failed to connect: %x", result);
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_CONNECTION_FAILED));
    return;
  }
}

void
BluetoothA2dpManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    if (aController) {
      aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    }
    return;
  }

  if (!mA2dpConnected) {
    if (aController) {
      aController->NotifyCompletion(NS_LITERAL_STRING(ERR_ALREADY_DISCONNECTED));
    }
    return;
  }

  MOZ_ASSERT(!mDeviceAddress.IsEmpty());

  mController = aController;

  if (!sBtA2dpInterface) {
    BT_LOGR("sBluetoothA2dpInterface is null");
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_NO_AVAILABLE_RESOURCE));
    return;
  }

  bt_bdaddr_t remoteAddress;
  StringToBdAddressType(mDeviceAddress, &remoteAddress);

  bt_status_t result = sBtA2dpInterface->Disconnect(&remoteAddress);
  if (BT_STATUS_SUCCESS != result) {
    BT_LOGR("Failed to disconnect: %x", result);
    aController->NotifyCompletion(NS_LITERAL_STRING(ERR_DISCONNECTION_FAILED));
    return;
  }
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

  nsRefPtr<BluetoothProfileController> controller = mController.forget();
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

  const nsString& address = aSignal.path();
  /**
   * Update sink property only if
   * - mDeviceAddress is empty (A2dp is disconnected), or
   * - this property change is from the connected sink.
   */
  NS_ENSURE_TRUE_VOID(mDeviceAddress.IsEmpty() ||
                      mDeviceAddress.Equals(address));

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
      mDeviceAddress.Truncate();
      OnDisconnect(EmptyString());
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

/*
 * In bluedroid stack case, there is no interface to know exactly
 * avrcp connection status. All connection are managed by bluedroid stack.
 */
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

/*
 * This function only updates meta data in BluetoothA2dpManager
 * Send "Get Element Attributes response" in AvrcpGetElementAttrCallback
 */
void
BluetoothA2dpManager::UpdateMetaData(const nsAString& aTitle,
                                     const nsAString& aArtist,
                                     const nsAString& aAlbum,
                                     uint64_t aMediaNumber,
                                     uint64_t aTotalMediaCount,
                                     uint32_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());

#if ANDROID_VERSION > 17
  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);

  // Send track changed and position changed if track num is not the same.
  // See also AVRCP 1.3 Spec 5.4.2
  if (mMediaNumber != aMediaNumber &&
      mTrackChangedNotifyType == BTRC_NOTIFICATION_TYPE_INTERIM) {
    btrc_register_notification_t param;
    // convert to network big endian format
    // since track stores as uint8[8]
    // 56 = 8 * (BTRC_UID_SIZE -1)
    for (int i = 0; i < BTRC_UID_SIZE; ++i) {
      param.track[i] = (aMediaNumber >> (56 - 8 * i));
    }
    mTrackChangedNotifyType = BTRC_NOTIFICATION_TYPE_CHANGED;
    sBtAvrcpInterface->RegisterNotificationRsp(BTRC_EVT_TRACK_CHANGE,
                                               BTRC_NOTIFICATION_TYPE_CHANGED,
                                               &param);
    if (mPlayPosChangedNotifyType == BTRC_NOTIFICATION_TYPE_INTERIM) {
      param.song_pos = mPosition;
      // EVENT_PLAYBACK_POS_CHANGED shall be notified if changed current track
      mPlayPosChangedNotifyType = BTRC_NOTIFICATION_TYPE_CHANGED;
      sBtAvrcpInterface->RegisterNotificationRsp(
        BTRC_EVT_PLAY_POS_CHANGED,
        BTRC_NOTIFICATION_TYPE_CHANGED,
        &param);
    }
  }

  mTitle.Assign(aTitle);
  mArtist.Assign(aArtist);
  mAlbum.Assign(aAlbum);
  mMediaNumber = aMediaNumber;
  mTotalMediaCount = aTotalMediaCount;
  mDuration = aDuration;
#endif
}

/*
 * This function is to reply AvrcpGetPlayStatusCallback (play-status-request)
 * from media player application (Gaia side)
 */
void
BluetoothA2dpManager::UpdatePlayStatus(uint32_t aDuration,
                                       uint32_t aPosition,
                                       ControlPlayStatus aPlayStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

#if ANDROID_VERSION > 17
  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);
  // always update playstatus first
  sBtAvrcpInterface->GetPlayStatusRsp((btrc_play_status_t)aPlayStatus,
                                      aDuration, aPosition);
  // when play status changed, send both play status and position
  if (mPlayStatus != aPlayStatus &&
      mPlayStatusChangedNotifyType == BTRC_NOTIFICATION_TYPE_INTERIM) {
    btrc_register_notification_t param;
    param.play_status = (btrc_play_status_t)aPlayStatus;
    mPlayStatusChangedNotifyType = BTRC_NOTIFICATION_TYPE_CHANGED;
    sBtAvrcpInterface->RegisterNotificationRsp(BTRC_EVT_PLAY_STATUS_CHANGED,
                                               BTRC_NOTIFICATION_TYPE_CHANGED,
                                               &param);
  }

  if (mPosition != aPosition &&
      mPlayPosChangedNotifyType == BTRC_NOTIFICATION_TYPE_INTERIM) {
    btrc_register_notification_t param;
    param.song_pos = aPosition;
    mPlayPosChangedNotifyType = BTRC_NOTIFICATION_TYPE_CHANGED;
    sBtAvrcpInterface->RegisterNotificationRsp(BTRC_EVT_PLAY_POS_CHANGED,
                                               BTRC_NOTIFICATION_TYPE_CHANGED,
                                               &param);
  }

  mDuration = aDuration;
  mPosition = aPosition;
  mPlayStatus = aPlayStatus;
#endif
}

/*
 * This function handles RegisterNotification request from
 * AvrcpRegisterNotificationCallback, which updates current
 * track/status/position status in the INTERRIM response.
 *
 * aParam is only valid when position changed
 */
void
BluetoothA2dpManager::UpdateRegisterNotification(int aEventId, int aParam)
{
  MOZ_ASSERT(NS_IsMainThread());

#if ANDROID_VERSION > 17
  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);

  btrc_register_notification_t param;

  switch (aEventId) {
    case BTRC_EVT_PLAY_STATUS_CHANGED:
      mPlayStatusChangedNotifyType = BTRC_NOTIFICATION_TYPE_INTERIM;
      param.play_status = (btrc_play_status_t)mPlayStatus;
      break;
    case BTRC_EVT_TRACK_CHANGE:
      // In AVRCP 1.3 and 1.4, the identifier parameter of EVENT_TRACK_CHANGED
      // is different.
      // AVRCP 1.4: If no track is selected, we shall return 0xFFFFFFFFFFFFFFFF,
      // otherwise return 0x0 in the INTERRIM response. The expanded text in
      // version 1.4 is to allow for new UID feature. As for AVRCP 1.3, we shall
      // return 0xFFFFFFFF. Since PTS enforces to check this part to comply with
      // the most updated spec.
      mTrackChangedNotifyType = BTRC_NOTIFICATION_TYPE_INTERIM;
      // needs to convert to network big endian format since track stores
      // as uint8[8]. 56 = 8 * (BTRC_UID_SIZE -1).
      for (int index = 0; index < BTRC_UID_SIZE; ++index) {
        // We cannot easily check if a track is selected, so whenever A2DP is
        // streaming, we assume a track is selected.
        if (mSinkState == BluetoothA2dpManager::SinkState::SINK_PLAYING) {
          param.track[index] = 0x0;
        } else {
          param.track[index] = 0xFF;
        }
      }
      break;
    case BTRC_EVT_PLAY_POS_CHANGED:
      // If no track is selected, return 0xFFFFFFFF in the INTERIM response
      mPlayPosChangedNotifyType = BTRC_NOTIFICATION_TYPE_INTERIM;
      if (mSinkState == BluetoothA2dpManager::SinkState::SINK_PLAYING) {
        param.song_pos = mPosition;
      } else {
        param.song_pos = 0xFFFFFFFF;
      }
      mPlaybackInterval = aParam;
      break;
    default:
      break;
  }

  sBtAvrcpInterface->RegisterNotificationRsp((btrc_event_id_t)aEventId,
                                              BTRC_NOTIFICATION_TYPE_INTERIM,
                                              &param);
#endif
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

uint64_t
BluetoothA2dpManager::GetMediaNumber()
{
  return mMediaNumber;
}

uint64_t
BluetoothA2dpManager::GetTotalMediaNumber()
{
  return mTotalMediaCount;
}

void
BluetoothA2dpManager::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
}

void
BluetoothA2dpManager::GetArtist(nsAString& aArtist)
{
  aArtist.Assign(mArtist);
}

NS_IMPL_ISUPPORTS(BluetoothA2dpManager, nsIObserver)

