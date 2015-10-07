/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BluetoothAvrcpManager.h"
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
  StaticRefPtr<BluetoothAvrcpManager> sBluetoothAvrcpManager;
  bool sInShutdown = false;
  static BluetoothAvrcpInterface* sBtAvrcpInterface;
} // namespace

/*
 * This function maps attribute id and returns corresponding values
 */
static void
ConvertAttributeString(BluetoothAvrcpMediaAttribute aAttrId,
                       nsAString& aAttrStr)
{
  BluetoothAvrcpManager* avrcp = BluetoothAvrcpManager::Get();
  NS_ENSURE_TRUE_VOID(avrcp);

  switch (aAttrId) {
    case AVRCP_MEDIA_ATTRIBUTE_TITLE:
      avrcp->GetTitle(aAttrStr);
      /*
       * bluedroid can only send string length AVRC_MAX_ATTR_STR_LEN - 1
       */
      if (aAttrStr.Length() >= AVRC_MAX_ATTR_STR_LEN) {
        aAttrStr.Truncate(AVRC_MAX_ATTR_STR_LEN - 1);
        BT_WARNING("Truncate media item attribute title, length is over 255");
      }
      break;
    case AVRCP_MEDIA_ATTRIBUTE_ARTIST:
      avrcp->GetArtist(aAttrStr);
      if (aAttrStr.Length() >= AVRC_MAX_ATTR_STR_LEN) {
        aAttrStr.Truncate(AVRC_MAX_ATTR_STR_LEN - 1);
        BT_WARNING("Truncate media item attribute artist, length is over 255");
      }
      break;
    case AVRCP_MEDIA_ATTRIBUTE_ALBUM:
      avrcp->GetAlbum(aAttrStr);
      if (aAttrStr.Length() >= AVRC_MAX_ATTR_STR_LEN) {
        aAttrStr.Truncate(AVRC_MAX_ATTR_STR_LEN - 1);
        BT_WARNING("Truncate media item attribute album, length is over 255");
      }
      break;
    case AVRCP_MEDIA_ATTRIBUTE_TRACK_NUM:
      aAttrStr.AppendInt(avrcp->GetMediaNumber());
      break;
    case AVRCP_MEDIA_ATTRIBUTE_NUM_TRACKS:
      aAttrStr.AppendInt(avrcp->GetTotalMediaNumber());
      break;
    case AVRCP_MEDIA_ATTRIBUTE_GENRE:
      // TODO: we currently don't support genre from music player
      aAttrStr.Truncate();
      break;
    case AVRCP_MEDIA_ATTRIBUTE_PLAYING_TIME:
      aAttrStr.AppendInt(avrcp->GetDuration());
      break;
  }
}

NS_IMETHODIMP
BluetoothAvrcpManager::Observe(nsISupports* aSubject,
                               const char* aTopic,
                               const char16_t* aData)
{
  MOZ_ASSERT(sBluetoothAvrcpManager);

  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    HandleShutdown();
    return NS_OK;
  }

  MOZ_ASSERT(false, "BluetoothAvrcpManager got unexpected topic!");
  return NS_ERROR_UNEXPECTED;
}

BluetoothAvrcpManager::BluetoothAvrcpManager()
{
  Reset();
}

void
BluetoothAvrcpManager::Reset()
{
  mAvrcpConnected = false;
  mDuration = 0;
  mMediaNumber = 0;
  mTotalMediaCount = 0;
  mPosition = 0;
  mPlayStatus = ControlPlayStatus::PLAYSTATUS_STOPPED;
}

class BluetoothAvrcpManager::InitResultHandler final
  : public BluetoothAvrcpResultHandler
{
public:
  InitResultHandler(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothAvrcpInterface::Init failed: %d",
               (int)aStatus);
    if (mRes) {
      if (aStatus == STATUS_UNSUPPORTED) {
        /* Not all versions of Bluedroid support AVRCP. So if the
         * initialization fails with STATUS_UNSUPPORTED, we still
         * signal success.
         */
        mRes->Init();
      } else {
        mRes->OnError(NS_ERROR_FAILURE);
      }
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

class BluetoothAvrcpManager::OnErrorProfileResultHandlerRunnable final
  : public nsRunnable
{
public:
  OnErrorProfileResultHandlerRunnable(BluetoothProfileResultHandler* aRes,
                                      nsresult aRv)
    : mRes(aRes)
    , mRv(aRv)
  {
    MOZ_ASSERT(mRes);
  }

  NS_IMETHOD Run() override
  {
    mRes->OnError(mRv);
    return NS_OK;
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
  nsresult mRv;
};

/*
 * This function will be only called when Bluetooth is turning on.
 */
// static
void
BluetoothAvrcpManager::InitAvrcpInterface(BluetoothProfileResultHandler* aRes)
{
  BluetoothInterface* btInf = BluetoothInterface::GetInstance();
  if (NS_WARN_IF(!btInf)) {
    // If there's no HFP interface, we dispatch a runnable
    // that calls the profile result handler.
    nsRefPtr<nsRunnable> r =
      new OnErrorProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  sBtAvrcpInterface = btInf->GetBluetoothAvrcpInterface();
  if (NS_WARN_IF(!sBtAvrcpInterface)) {
    // If there's no AVRCP interface, we dispatch a runnable
    // that calls the profile result handler.
    nsRefPtr<nsRunnable> r =
      new OnErrorProfileResultHandlerRunnable(aRes, NS_ERROR_FAILURE);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch HFP OnError runnable");
    }
    return;
  }

  BluetoothAvrcpManager* avrcpManager = BluetoothAvrcpManager::Get();
  sBtAvrcpInterface->Init(avrcpManager, new InitResultHandler(aRes));
}

BluetoothAvrcpManager::~BluetoothAvrcpManager()
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

//static
BluetoothAvrcpManager*
BluetoothAvrcpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If sBluetoothAvrcpManager already exists, exit early
  if (sBluetoothAvrcpManager) {
    return sBluetoothAvrcpManager;
  }

  // If we're in shutdown, don't create a new instance
  NS_ENSURE_FALSE(sInShutdown, nullptr);

  // Create a new instance, register, and return
  BluetoothAvrcpManager* manager = new BluetoothAvrcpManager();
  sBluetoothAvrcpManager = manager;
  return sBluetoothAvrcpManager;
}

class BluetoothAvrcpManager::CleanupResultHandler final
  : public BluetoothAvrcpResultHandler
{
public:
  CleanupResultHandler(BluetoothProfileResultHandler* aRes)
    : mRes(aRes)
  { }

  void OnError(BluetoothStatus aStatus) override
  {
    BT_WARNING("BluetoothAvrcpInterface::Cleanup failed: %d",
               (int)aStatus);

    sBtAvrcpInterface = nullptr;

    if (mRes) {
      if (aStatus == STATUS_UNSUPPORTED) {
        /* Not all versions of Bluedroid support AVRCP. So if the
         * cleanup fails with STATUS_UNSUPPORTED, we still signal
         * success.
         */
        mRes->Deinit();
      } else {
        mRes->OnError(NS_ERROR_FAILURE);
      }
    }
  }

  void Cleanup() override
  {
    sBtAvrcpInterface = nullptr;
    if (mRes) {
      mRes->Deinit();
    }
  }

private:
  nsRefPtr<BluetoothProfileResultHandler> mRes;
};

class BluetoothAvrcpManager::CleanupResultHandlerRunnable final
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
BluetoothAvrcpManager::DeinitAvrcpInterface(BluetoothProfileResultHandler* aRes)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sBtAvrcpInterface) {
    sBtAvrcpInterface->Cleanup(new CleanupResultHandler(aRes));
  } else if (aRes) {
    // We dispatch a runnable here to make the profile resource handler
    // behave as if AVRCP was initialized.
    nsRefPtr<nsRunnable> r = new CleanupResultHandlerRunnable(aRes);
    if (NS_FAILED(NS_DispatchToMainThread(r))) {
      BT_LOGR("Failed to dispatch cleanup-result-handler runnable");
    }
  }
}

void
BluetoothAvrcpManager::HandleShutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  sInShutdown = true;
  Disconnect(nullptr);
  sBluetoothAvrcpManager = nullptr;
}

class BluetoothAvrcpManager::ConnectRunnable final : public nsRunnable
{
public:
  ConnectRunnable(BluetoothAvrcpManager* aManager)
    : mManager(aManager)
  {
    MOZ_ASSERT(mManager);
  }
  NS_METHOD Run() override
  {
    mManager->OnConnect(EmptyString());
    return NS_OK;
  }
private:
  BluetoothAvrcpManager* mManager;
};

void
BluetoothAvrcpManager::Connect(const nsAString& aDeviceAddress,
                               BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!aDeviceAddress.IsEmpty());
  MOZ_ASSERT(aController);

  // AVRCP doesn't require connecting. We just set the remote address here.
  mDeviceAddress = aDeviceAddress;
  mController = aController;
  SetConnected(true);

  NS_DispatchToMainThread(new ConnectRunnable(this));
}

class BluetoothAvrcpManager::DisconnectRunnable final : public nsRunnable
{
public:
  DisconnectRunnable(BluetoothAvrcpManager* aManager)
    : mManager(aManager)
  {
    MOZ_ASSERT(mManager);
  }
  NS_METHOD Run() override
  {
    mManager->OnDisconnect(EmptyString());
    return NS_OK;
  }
private:
  BluetoothAvrcpManager* mManager;
};

void
BluetoothAvrcpManager::Disconnect(BluetoothProfileController* aController)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mController);

  mDeviceAddress.Truncate();
  mController = aController;
  SetConnected(false);

  NS_DispatchToMainThread(new DisconnectRunnable(this));
}

void
BluetoothAvrcpManager::OnConnect(const nsAString& aErrorStr)
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
BluetoothAvrcpManager::OnDisconnect(const nsAString& aErrorStr)
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

void
BluetoothAvrcpManager::OnGetServiceChannel(const nsAString& aDeviceAddress,
                                          const nsAString& aServiceUuid,
                                          int aChannel)
{ }

void
BluetoothAvrcpManager::OnUpdateSdpRecords(const nsAString& aDeviceAddress)
{ }

void
BluetoothAvrcpManager::GetAddress(nsAString& aDeviceAddress)
{
  aDeviceAddress = mDeviceAddress;
}

bool
BluetoothAvrcpManager::IsConnected()
{
  return mAvrcpConnected;
}

/*
 * In bluedroid stack case, there is no interface to know exactly
 * avrcp connection status. All connection are managed by bluedroid stack.
 */
void
BluetoothAvrcpManager::SetConnected(bool aConnected)
{
  mAvrcpConnected = aConnected;
  if (!aConnected) {
    Reset();
  }
}

/*
 * This function only updates meta data in BluetoothAvrcpManager. Send
 * "Get Element Attributes response" in AvrcpGetElementAttrCallback
 */
void
BluetoothAvrcpManager::UpdateMetaData(const nsAString& aTitle,
                                      const nsAString& aArtist,
                                      const nsAString& aAlbum,
                                      uint64_t aMediaNumber,
                                      uint64_t aTotalMediaCount,
                                      uint32_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);

  // Send track changed and position changed if track num is not the same.
  // See also AVRCP 1.3 Spec 5.4.2
  if (mMediaNumber != aMediaNumber &&
      mTrackChangedNotifyType == AVRCP_NTF_INTERIM) {
    BluetoothAvrcpNotificationParam param;
    // convert to network big endian format
    // since track stores as uint8[8]
    // 56 = 8 * (AVRCP_UID_SIZE -1)
    for (int i = 0; i < AVRCP_UID_SIZE; ++i) {
      param.mTrack[i] = (aMediaNumber >> (56 - 8 * i));
    }
    mTrackChangedNotifyType = AVRCP_NTF_CHANGED;
    sBtAvrcpInterface->RegisterNotificationRsp(AVRCP_EVENT_TRACK_CHANGE,
                                               AVRCP_NTF_CHANGED,
                                               param, nullptr);
    if (mPlayPosChangedNotifyType == AVRCP_NTF_INTERIM) {
      param.mSongPos = mPosition;
      // EVENT_PLAYBACK_POS_CHANGED shall be notified if changed current track
      mPlayPosChangedNotifyType = AVRCP_NTF_CHANGED;
      sBtAvrcpInterface->RegisterNotificationRsp(AVRCP_EVENT_PLAY_POS_CHANGED,
                                                 AVRCP_NTF_CHANGED,
                                                 param, nullptr);
    }
  }

  mTitle.Assign(aTitle);
  mArtist.Assign(aArtist);
  mAlbum.Assign(aAlbum);
  mMediaNumber = aMediaNumber;
  mTotalMediaCount = aTotalMediaCount;
  mDuration = aDuration;
}

/*
 * This function is to reply AvrcpGetPlayStatusCallback (play-status-request)
 * from media player application (Gaia side)
 */
void
BluetoothAvrcpManager::UpdatePlayStatus(uint32_t aDuration,
                                        uint32_t aPosition,
                                        ControlPlayStatus aPlayStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);
  // always update playstatus first
  sBtAvrcpInterface->GetPlayStatusRsp(aPlayStatus, aDuration,
                                      aPosition, nullptr);
  // when play status changed, send both play status and position
  if (mPlayStatus != aPlayStatus &&
      mPlayStatusChangedNotifyType == AVRCP_NTF_INTERIM) {
    BluetoothAvrcpNotificationParam param;
    param.mPlayStatus = aPlayStatus;
    mPlayStatusChangedNotifyType = AVRCP_NTF_CHANGED;
    sBtAvrcpInterface->RegisterNotificationRsp(AVRCP_EVENT_PLAY_STATUS_CHANGED,
                                               AVRCP_NTF_CHANGED,
                                               param, nullptr);
  }

  if (mPosition != aPosition &&
      mPlayPosChangedNotifyType == AVRCP_NTF_INTERIM) {
    BluetoothAvrcpNotificationParam param;
    param.mSongPos = aPosition;
    mPlayPosChangedNotifyType = AVRCP_NTF_CHANGED;
    sBtAvrcpInterface->RegisterNotificationRsp(AVRCP_EVENT_PLAY_POS_CHANGED,
                                               AVRCP_NTF_CHANGED,
                                               param, nullptr);
  }

  mDuration = aDuration;
  mPosition = aPosition;
  mPlayStatus = aPlayStatus;
}

/*
 * This function handles RegisterNotification request from
 * AvrcpRegisterNotificationCallback, which updates current
 * track/status/position status in the INTERRIM response.
 *
 * aParam is only valid when position changed
 */
void
BluetoothAvrcpManager::UpdateRegisterNotification(BluetoothAvrcpEvent aEvent,
                                                  uint32_t aParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_ENSURE_TRUE_VOID(sBtAvrcpInterface);

  BluetoothAvrcpNotificationParam param;

  switch (aEvent) {
    case AVRCP_EVENT_PLAY_STATUS_CHANGED:
      mPlayStatusChangedNotifyType = AVRCP_NTF_INTERIM;
      param.mPlayStatus = mPlayStatus;
      break;
    case AVRCP_EVENT_TRACK_CHANGE:
      // In AVRCP 1.3 and 1.4, the identifier parameter of EVENT_TRACK_CHANGED
      // is different.
      // AVRCP 1.4: If no track is selected, we shall return 0xFFFFFFFFFFFFFFFF,
      // otherwise return 0x0 in the INTERRIM response. The expanded text in
      // version 1.4 is to allow for new UID feature. As for AVRCP 1.3, we shall
      // return 0xFFFFFFFF. Since PTS enforces to check this part to comply with
      // the most updated spec.
      mTrackChangedNotifyType = AVRCP_NTF_INTERIM;
      // needs to convert to network big endian format since track stores
      // as uint8[8]. 56 = 8 * (BTRC_UID_SIZE -1).
      for (int index = 0; index < AVRCP_UID_SIZE; ++index) {
        // We cannot easily check if a track is selected, so whenever A2DP is
        // streaming, we assume a track is selected.
        if (mPlayStatus == ControlPlayStatus::PLAYSTATUS_PLAYING) {
          param.mTrack[index] = 0x0;
        } else {
          param.mTrack[index] = 0xFF;
        }
      }
      break;
    case AVRCP_EVENT_PLAY_POS_CHANGED:
      // If no track is selected, return 0xFFFFFFFF in the INTERIM response
      mPlayPosChangedNotifyType = AVRCP_NTF_INTERIM;
      if (mPlayStatus == ControlPlayStatus::PLAYSTATUS_PLAYING) {
        param.mSongPos = mPosition;
      } else {
        param.mSongPos = 0xFFFFFFFF;
      }
      mPlaybackInterval = aParam;
      break;
    case AVRCP_EVENT_APP_SETTINGS_CHANGED:
      mAppSettingsChangedNotifyType = AVRCP_NTF_INTERIM;
      param.mNumAttr = 2;
      param.mIds[0] = AVRCP_PLAYER_ATTRIBUTE_REPEAT;
      param.mValues[0] = AVRCP_PLAYER_VAL_OFF_REPEAT;
      param.mIds[1] = AVRCP_PLAYER_ATTRIBUTE_SHUFFLE;
      param.mValues[1] = AVRCP_PLAYER_VAL_OFF_SHUFFLE;
      break;
    default:
      break;
  }

  sBtAvrcpInterface->RegisterNotificationRsp(aEvent, AVRCP_NTF_INTERIM,
                                             param, nullptr);
}

void
BluetoothAvrcpManager::GetAlbum(nsAString& aAlbum)
{
  aAlbum.Assign(mAlbum);
}

uint32_t
BluetoothAvrcpManager::GetDuration()
{
  return mDuration;
}

ControlPlayStatus
BluetoothAvrcpManager::GetPlayStatus()
{
  return mPlayStatus;
}

uint32_t
BluetoothAvrcpManager::GetPosition()
{
  return mPosition;
}

uint64_t
BluetoothAvrcpManager::GetMediaNumber()
{
  return mMediaNumber;
}

uint64_t
BluetoothAvrcpManager::GetTotalMediaNumber()
{
  return mTotalMediaCount;
}

void
BluetoothAvrcpManager::GetTitle(nsAString& aTitle)
{
  aTitle.Assign(mTitle);
}

void
BluetoothAvrcpManager::GetArtist(nsAString& aArtist)
{
  aArtist.Assign(mArtist);
}

/*
 * Notifications
 */

void
BluetoothAvrcpManager::GetPlayStatusNotification()
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    return;
  }

  bs->DistributeSignal(NS_LITERAL_STRING(REQUEST_MEDIA_PLAYSTATUS_ID),
                       NS_LITERAL_STRING(KEY_ADAPTER));
}

/* Player application settings is optional for AVRCP 1.3. B2G
 * currently does not support player-application-setting related
 * functionality.
 */
void
BluetoothAvrcpManager::ListPlayerAppAttrNotification()
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP application-setting-related functions
}

void
BluetoothAvrcpManager::ListPlayerAppValuesNotification(
  BluetoothAvrcpPlayerAttribute aAttrId)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP application-setting-related functions
}

void
BluetoothAvrcpManager::GetPlayerAppValueNotification(
  uint8_t aNumAttrs, const BluetoothAvrcpPlayerAttribute* aAttrs)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP application-setting-related functions
}

void
BluetoothAvrcpManager::GetPlayerAppAttrsTextNotification(
  uint8_t aNumAttrs, const BluetoothAvrcpPlayerAttribute* aAttrs)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP application-setting-related functions
}

void
BluetoothAvrcpManager::GetPlayerAppValuesTextNotification(
  uint8_t aAttrId, uint8_t aNumVals, const uint8_t* aValues)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP application-setting-related functions
}

void
BluetoothAvrcpManager::SetPlayerAppValueNotification(
  const BluetoothAvrcpPlayerSettings& aSettings)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP application-setting-related functions
}

/* This method returns element attributes, which are requested from
 * CT. Unlike BlueZ it calls only UpdateMetaData. Bluedroid does not cache
 * meta-data information, but instead uses |GetElementAttrNotifications|
 * and |GetElementAttrRsp| request them.
 */
void
BluetoothAvrcpManager::GetElementAttrNotification(
  uint8_t aNumAttrs, const BluetoothAvrcpMediaAttribute* aAttrs)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsAutoArrayPtr<BluetoothAvrcpElementAttribute> attrs(
    new BluetoothAvrcpElementAttribute[aNumAttrs]);

  for (uint8_t i = 0; i < aNumAttrs; ++i) {
    attrs[i].mId = aAttrs[i];
    ConvertAttributeString(
      static_cast<BluetoothAvrcpMediaAttribute>(attrs[i].mId),
      attrs[i].mValue);
  }

  MOZ_ASSERT(sBtAvrcpInterface);
  sBtAvrcpInterface->GetElementAttrRsp(aNumAttrs, attrs, nullptr);
}

void
BluetoothAvrcpManager::RegisterNotificationNotification(
  BluetoothAvrcpEvent aEvent, uint32_t aParam)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothAvrcpManager* avrcp = BluetoothAvrcpManager::Get();
  if (!avrcp) {
    return;
  }

  avrcp->UpdateRegisterNotification(aEvent, aParam);
}

/* This method is used to get CT features from the Feature Bit Mask. If
 * Advanced Control Player bit is set, the CT supports volume sync (absolute
 * volume feature). If Browsing bit is set, AVRCP 1.4 Browse feature will be
 * supported.
 */
void
BluetoothAvrcpManager::RemoteFeatureNotification(
  const BluetoothAddress& aBdAddr, unsigned long aFeatures)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP 1.4 absolute volume/browse
}

/* This method is used to get notifications about volume changes on the
 * remote car kit (if it supports AVRCP 1.4), not notification from phone.
 */
void
BluetoothAvrcpManager::VolumeChangeNotification(uint8_t aVolume,
                                                uint8_t aCType)
{
  MOZ_ASSERT(NS_IsMainThread());

  // TODO: Support AVRCP 1.4 absolute volume/browse
}

void
BluetoothAvrcpManager::PassthroughCmdNotification(int aId, int aKeyState)
{
  MOZ_ASSERT(NS_IsMainThread());

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
      return;
  }

  NS_NAMED_LITERAL_STRING(type, "media-button");
  BroadcastSystemMessage(type, BluetoothValue(name));
}

NS_IMPL_ISUPPORTS(BluetoothAvrcpManager, nsIObserver)

