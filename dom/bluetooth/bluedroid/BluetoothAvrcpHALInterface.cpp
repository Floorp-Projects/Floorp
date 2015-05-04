/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothAvrcpHALInterface.h"
#include "BluetoothHALHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

typedef
  BluetoothHALInterfaceRunnable0<BluetoothAvrcpResultHandler, void>
  BluetoothAvrcpHALResultRunnable;

typedef
  BluetoothHALInterfaceRunnable1<BluetoothAvrcpResultHandler, void,
                                 BluetoothStatus, BluetoothStatus>
  BluetoothAvrcpHALErrorRunnable;

static nsresult
DispatchBluetoothAvrcpHALResult(
  BluetoothAvrcpResultHandler* aRes,
  void (BluetoothAvrcpResultHandler::*aMethod)(),
  BluetoothStatus aStatus)
{
  MOZ_ASSERT(aRes);

  nsRefPtr<nsRunnable> runnable;

  if (aStatus == STATUS_SUCCESS) {
    runnable = new BluetoothAvrcpHALResultRunnable(aRes, aMethod);
  } else {
    runnable = new BluetoothAvrcpHALErrorRunnable(aRes,
      &BluetoothAvrcpResultHandler::OnError, aStatus);
  }
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    BT_WARNING("NS_DispatchToMainThread failed: %X", rv);
  }
  return rv;
}

// Notification handling
//

static BluetoothAvrcpNotificationHandler* sAvrcpNotificationHandler;

struct BluetoothAvrcpCallback
{
  class AvrcpNotificationHandlerWrapper
  {
  public:
    typedef BluetoothAvrcpNotificationHandler ObjectType;

    static ObjectType* GetInstance()
    {
      MOZ_ASSERT(NS_IsMainThread());

      return sAvrcpNotificationHandler;
    }
  };

  // Notifications

  typedef BluetoothNotificationHALRunnable0<AvrcpNotificationHandlerWrapper,
                                            void>
    GetPlayStatusNotification;

  typedef BluetoothNotificationHALRunnable0<AvrcpNotificationHandlerWrapper,
                                            void>
    ListPlayerAppAttrNotification;

  typedef BluetoothNotificationHALRunnable1<AvrcpNotificationHandlerWrapper,
                                            void,
                                            BluetoothAvrcpPlayerAttribute>
    ListPlayerAppValuesNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpPlayerAttribute>,
    uint8_t, const BluetoothAvrcpPlayerAttribute*>
    GetPlayerAppValueNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpPlayerAttribute>,
    uint8_t, const BluetoothAvrcpPlayerAttribute*>
    GetPlayerAppAttrsTextNotification;

  typedef BluetoothNotificationHALRunnable3<AvrcpNotificationHandlerWrapper,
                                            void,
                                            uint8_t, uint8_t,
                                            nsAutoArrayPtr<uint8_t>,
                                            uint8_t, uint8_t, const uint8_t*>
    GetPlayerAppValuesTextNotification;

  typedef BluetoothNotificationHALRunnable1<AvrcpNotificationHandlerWrapper,
                                            void,
                                            BluetoothAvrcpPlayerSettings,
                                            const BluetoothAvrcpPlayerSettings&>
    SetPlayerAppValueNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpMediaAttribute>,
    uint8_t, const BluetoothAvrcpMediaAttribute*>
    GetElementAttrNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper,
                                            void,
                                            BluetoothAvrcpEvent, uint32_t>
    RegisterNotificationNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper,
                                            void,
                                            nsString, unsigned long,
                                            const nsAString&>
    RemoteFeatureNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper,
                                            void,
                                            uint8_t, uint8_t>
    VolumeChangeNotification;

  typedef BluetoothNotificationHALRunnable2<AvrcpNotificationHandlerWrapper,
                                            void,
                                            int, int>
    PassthroughCmdNotification;

  // Bluedroid AVRCP callbacks

#if ANDROID_VERSION >= 18
  static void
  GetPlayStatus()
  {
    GetPlayStatusNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayStatusNotification);
  }

  static void
  ListPlayerAppAttr()
  {
    ListPlayerAppAttrNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::ListPlayerAppAttrNotification);
  }

  static void
  ListPlayerAppValues(btrc_player_attr_t aAttrId)
  {
    ListPlayerAppValuesNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::ListPlayerAppValuesNotification,
      aAttrId);
  }

  static void
  GetPlayerAppValue(uint8_t aNumAttrs, btrc_player_attr_t* aAttrs)
  {
    GetPlayerAppValueNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayerAppValueNotification,
      aNumAttrs, ConvertArray<btrc_player_attr_t>(aAttrs, aNumAttrs));
  }

  static void
  GetPlayerAppAttrsText(uint8_t aNumAttrs, btrc_player_attr_t* aAttrs)
  {
    GetPlayerAppAttrsTextNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayerAppAttrsTextNotification,
      aNumAttrs, ConvertArray<btrc_player_attr_t>(aAttrs, aNumAttrs));
  }

  static void
  GetPlayerAppValuesText(uint8_t aAttrId, uint8_t aNumVals, uint8_t* aVals)
  {
    GetPlayerAppValuesTextNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetPlayerAppValuesTextNotification,
      aAttrId, aNumVals, ConvertArray<uint8_t>(aVals, aNumVals));
  }

  static void
  SetPlayerAppValue(btrc_player_settings_t* aVals)
  {
    SetPlayerAppValueNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::SetPlayerAppValueNotification,
      *aVals);
  }

  static void
  GetElementAttr(uint8_t aNumAttrs, btrc_media_attr_t* aAttrs)
  {
    GetElementAttrNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::GetElementAttrNotification,
      aNumAttrs, ConvertArray<btrc_media_attr_t>(aAttrs, aNumAttrs));
  }

  static void
  RegisterNotification(btrc_event_id_t aEvent, uint32_t aParam)
  {
    RegisterNotificationNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::RegisterNotificationNotification,
      aEvent, aParam);
  }
#endif // ANDROID_VERSION >= 18

#if ANDROID_VERSION >= 19
  static void
  RemoteFeature(bt_bdaddr_t* aBdAddr, btrc_remote_features_t aFeatures)
  {
    RemoteFeatureNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::RemoteFeatureNotification,
      aBdAddr, aFeatures);
  }

  static void
  VolumeChange(uint8_t aVolume, uint8_t aCType)
  {
    VolumeChangeNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::VolumeChangeNotification,
      aVolume, aCType);
  }

  static void
  PassthroughCmd(int aId, int aKeyState)
  {
    PassthroughCmdNotification::Dispatch(
      &BluetoothAvrcpNotificationHandler::PassthroughCmdNotification,
      aId, aKeyState);
  }
#endif // ANDROID_VERSION >= 19
};

// Interface
//

BluetoothAvrcpHALInterface::BluetoothAvrcpHALInterface(
#if ANDROID_VERSION >= 18
  const btrc_interface_t* aInterface
#endif
  )
#if ANDROID_VERSION >= 18
: mInterface(aInterface)
#endif
{
#if ANDROID_VERSION >= 18
  MOZ_ASSERT(mInterface);
#endif
}

BluetoothAvrcpHALInterface::~BluetoothAvrcpHALInterface()
{ }

void
BluetoothAvrcpHALInterface::Init(
  BluetoothAvrcpNotificationHandler* aNotificationHandler,
  BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  static btrc_callbacks_t sCallbacks = {
    sizeof(sCallbacks),
#if ANDROID_VERSION >= 19
    BluetoothAvrcpCallback::RemoteFeature,
#endif
    BluetoothAvrcpCallback::GetPlayStatus,
    BluetoothAvrcpCallback::ListPlayerAppAttr,
    BluetoothAvrcpCallback::ListPlayerAppValues,
    BluetoothAvrcpCallback::GetPlayerAppValue,
    BluetoothAvrcpCallback::GetPlayerAppAttrsText,
    BluetoothAvrcpCallback::GetPlayerAppValuesText,
    BluetoothAvrcpCallback::SetPlayerAppValue,
    BluetoothAvrcpCallback::GetElementAttr,
    BluetoothAvrcpCallback::RegisterNotification
#if ANDROID_VERSION >= 19
    ,
    BluetoothAvrcpCallback::VolumeChange,
    BluetoothAvrcpCallback::PassthroughCmd
#endif
  };
#endif // ANDROID_VERSION >= 18

  sAvrcpNotificationHandler = aNotificationHandler;

#if ANDROID_VERSION >= 18
  bt_status_t status = mInterface->init(&sCallbacks);
#else
  bt_status_t status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(aRes, &BluetoothAvrcpResultHandler::Init,
                                    ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::Cleanup(BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  mInterface->cleanup();
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(aRes,
      &BluetoothAvrcpResultHandler::Cleanup, STATUS_SUCCESS);
  }
}

void
BluetoothAvrcpHALInterface::GetPlayStatusRsp(
  ControlPlayStatus aPlayStatus, uint32_t aSongLen, uint32_t aSongPos,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_play_status_t playStatus = BTRC_PLAYSTATE_STOPPED;

  if (!(NS_FAILED(Convert(aPlayStatus, playStatus)))) {
    status = mInterface->get_play_status_rsp(playStatus, aSongLen, aSongPos);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayStatusRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::ListPlayerAppAttrRsp(
  int aNumAttr, const BluetoothAvrcpPlayerAttribute* aPAttrs,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  ConvertArray<BluetoothAvrcpPlayerAttribute> pAttrsArray(aPAttrs, aNumAttr);
  nsAutoArrayPtr<btrc_player_attr_t> pAttrs;

  if (NS_SUCCEEDED(Convert(pAttrsArray, pAttrs))) {
    status = mInterface->list_player_app_attr_rsp(aNumAttr, pAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::ListPlayerAppAttrRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::ListPlayerAppValueRsp(
  int aNumVal, uint8_t* aPVals, BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 18
  bt_status_t status = mInterface->list_player_app_value_rsp(aNumVal, aPVals);
#else
  bt_status_t status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::ListPlayerAppValueRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::GetPlayerAppValueRsp(
  uint8_t aNumAttrs, const uint8_t* aIds, const uint8_t* aValues,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_player_settings_t pVals;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any player app values currently */) {
    status = mInterface->get_player_app_value_rsp(&pVals);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayerAppValueRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::GetPlayerAppAttrTextRsp(
  int aNumAttr, const uint8_t* aIds, const char** aTexts,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_player_setting_text_t* aPAttrs;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any attributes currently */) {
    status = mInterface->get_player_app_attr_text_rsp(aNumAttr, aPAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayerAppAttrTextRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::GetPlayerAppValueTextRsp(
  int aNumVal, const uint8_t* aIds, const char** aTexts,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_player_setting_text_t* pVals;

  /* FIXME: you need to implement the missing conversion functions */
  NS_NOTREACHED("Conversion function missing");

  if (false /* TODO: we don't support any values currently */) {
    status = mInterface->get_player_app_value_text_rsp(aNumVal, pVals);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::GetPlayerAppValueTextRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::GetElementAttrRsp(
  uint8_t aNumAttr, const BluetoothAvrcpElementAttribute* aAttrs,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  ConvertArray<BluetoothAvrcpElementAttribute> pAttrsArray(aAttrs, aNumAttr);
  nsAutoArrayPtr<btrc_element_attr_val_t> pAttrs;

  if (NS_SUCCEEDED(Convert(pAttrsArray, pAttrs))) {
    status = mInterface->get_element_attr_rsp(aNumAttr, pAttrs);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::GetElementAttrRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::SetPlayerAppValueRsp(
  BluetoothAvrcpStatus aRspStatus, BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  btrc_status_t rspStatus = BTRC_STS_BAD_CMD; // silences compiler warning

  if (NS_SUCCEEDED(Convert(aRspStatus, rspStatus))) {
    status = mInterface->set_player_app_value_rsp(rspStatus);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::SetPlayerAppValueRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::RegisterNotificationRsp(
  BluetoothAvrcpEvent aEvent, BluetoothAvrcpNotification aType,
  const BluetoothAvrcpNotificationParam& aParam,
  BluetoothAvrcpResultHandler* aRes)
{
  bt_status_t status;

#if ANDROID_VERSION >= 18
  nsresult rv;
  btrc_event_id_t event = { };
  btrc_notification_type_t type = BTRC_NOTIFICATION_TYPE_INTERIM;
  btrc_register_notification_t param;

  switch (aEvent) {
    case AVRCP_EVENT_PLAY_STATUS_CHANGED:
      rv = Convert(aParam.mPlayStatus, param.play_status);
      break;
    case AVRCP_EVENT_TRACK_CHANGE:
      MOZ_ASSERT(sizeof(aParam.mTrack) == sizeof(param.track));
      memcpy(param.track, aParam.mTrack, sizeof(param.track));
      rv = NS_OK;
      break;
    case AVRCP_EVENT_TRACK_REACHED_END:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    case AVRCP_EVENT_TRACK_REACHED_START:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    case AVRCP_EVENT_PLAY_POS_CHANGED:
      param.song_pos = aParam.mSongPos;
      rv = NS_OK;
      break;
    case AVRCP_EVENT_APP_SETTINGS_CHANGED:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
    default:
      NS_NOTREACHED("Unknown conversion");
      rv = NS_ERROR_ILLEGAL_VALUE;
      break;
  }

  if (NS_SUCCEEDED(rv) &&
      NS_SUCCEEDED(Convert(aEvent, event)) &&
      NS_SUCCEEDED(Convert(aType, type))) {
    status = mInterface->register_notification_rsp(event, type, &param);
  } else {
    status = BT_STATUS_PARM_INVALID;
  }
#else
  status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::RegisterNotificationRsp,
      ConvertDefault(status, STATUS_FAIL));
  }
}

void
BluetoothAvrcpHALInterface::SetVolume(uint8_t aVolume,
                                      BluetoothAvrcpResultHandler* aRes)
{
#if ANDROID_VERSION >= 19
  bt_status_t status = mInterface->set_volume(aVolume);
#else
  bt_status_t status = BT_STATUS_UNSUPPORTED;
#endif

  if (aRes) {
    DispatchBluetoothAvrcpHALResult(
      aRes, &BluetoothAvrcpResultHandler::SetVolume,
      ConvertDefault(status, STATUS_FAIL));
  }
}

END_BLUETOOTH_NAMESPACE
