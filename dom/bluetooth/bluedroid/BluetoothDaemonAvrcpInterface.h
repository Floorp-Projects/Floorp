/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothdaemonavrcpinterface_h
#define mozilla_dom_bluetooth_bluetoothdaemonavrcpinterface_h

#include "BluetoothDaemonHelpers.h"
#include "BluetoothInterface.h"
#include "BluetoothInterfaceHelpers.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSetupResultHandler;

class BluetoothDaemonAvrcpModule
{
public:
  enum {
    SERVICE_ID = 0x08
  };

  enum {
    OPCODE_ERROR = 0x00,
    OPCODE_GET_PLAY_STATUS_RSP = 0x01,
    OPCODE_LIST_PLAYER_APP_ATTR_RSP = 0x02,
    OPCODE_LIST_PLAYER_APP_VALUE_RSP = 0x03,
    OPCODE_GET_PLAYER_APP_VALUE_RSP = 0x04,
    OPCODE_GET_PLAYER_APP_ATTR_TEXT_RSP = 0x05,
    OPCODE_GET_PLAYER_APP_VALUE_TEXT_RSP = 0x06,
    OPCODE_GET_ELEMENT_ATTR_RSP = 0x07,
    OPCODE_SET_PLAYER_APP_VALUE_RSP = 0x08,
    OPCODE_REGISTER_NOTIFICATION_RSP = 0x09,
    OPCODE_SET_VOLUME = 0x0a,
#if ANDROID_VERSION >= 19
    OPCODE_REMOTE_FEATURES_NTF = 0x81,
    OPCODE_GET_PLAY_STATUS_NTF = 0x82,
    OPCODE_LIST_PLAYER_APP_ATTR_NTF = 0x83,
    OPCODE_LIST_PLAYER_APP_VALUES_NTF = 0x84,
    OPCODE_GET_PLAYER_APP_VALUE_NTF = 0x85,
    OPCODE_GET_PLAYER_APP_ATTRS_TEXT_NTF = 0x86,
    OPCODE_GET_PLAYER_APP_VALUES_TEXT_NTF = 0x87,
    OPCODE_SET_PLAYER_APP_VALUE_NTF = 0x88,
    OPCODE_GET_ELEMENT_ATTR_NTF = 0x89,
    OPCODE_REGISTER_NOTIFICATION_NTF = 0x8a,
    OPCODE_VOLUME_CHANGE_NTF = 0x8b,
    OPCODE_PASSTHROUGH_CMD_NTF = 0x8c
#else /* defined by BlueZ 5.14 */
    OPCODE_GET_PLAY_STATUS_NTF = 0x81,
    OPCODE_LIST_PLAYER_APP_ATTR_NTF = 0x82,
    OPCODE_LIST_PLAYER_APP_VALUES_NTF = 0x83,
    OPCODE_GET_PLAYER_APP_VALUE_NTF = 0x84,
    OPCODE_GET_PLAYER_APP_ATTRS_TEXT_NTF = 0x85,
    OPCODE_GET_PLAYER_APP_VALUES_TEXT_NTF = 0x86,
    OPCODE_SET_PLAYER_APP_VALUE_NTF = 0x87,
    OPCODE_GET_ELEMENT_ATTR_NTF = 0x88,
    OPCODE_REGISTER_NOTIFICATION_NTF = 0x89
#endif
  };

  static const int MAX_NUM_CLIENTS;

  virtual nsresult Send(BluetoothDaemonPDU* aPDU, void* aUserData) = 0;

  virtual nsresult RegisterModule(uint8_t aId, uint8_t aMode,
                                  uint32_t aMaxNumClients,
                                  BluetoothSetupResultHandler* aRes) = 0;

  virtual nsresult UnregisterModule(uint8_t aId,
                                    BluetoothSetupResultHandler* aRes) = 0;

  void SetNotificationHandler(
    BluetoothAvrcpNotificationHandler* aNotificationHandler);

  //
  // Commands
  //

  nsresult GetPlayStatusRspCmd(
    ControlPlayStatus aPlayStatus, uint32_t aSongLen, uint32_t aSongPos,
    BluetoothAvrcpResultHandler* aRes);

  nsresult ListPlayerAppAttrRspCmd(
    int aNumAttr, const BluetoothAvrcpPlayerAttribute* aPAttrs,
    BluetoothAvrcpResultHandler* aRes);

  nsresult ListPlayerAppValueRspCmd(
    int aNumVal, uint8_t* aPVals, BluetoothAvrcpResultHandler* aRes);

  nsresult GetPlayerAppValueRspCmd(
    uint8_t aNumAttrs, const uint8_t* aIds, const uint8_t* aValues,
    BluetoothAvrcpResultHandler* aRes);

  nsresult GetPlayerAppAttrTextRspCmd(
    int aNumAttr, const uint8_t* aIds, const char** aTexts,
    BluetoothAvrcpResultHandler* aRes);

  nsresult GetPlayerAppValueTextRspCmd(
    int aNumVal, const uint8_t* aIds, const char** aTexts,
    BluetoothAvrcpResultHandler* aRes);

  nsresult GetElementAttrRspCmd(
    uint8_t aNumAttr, const BluetoothAvrcpElementAttribute* aAttr,
    BluetoothAvrcpResultHandler* aRes);

  nsresult SetPlayerAppValueRspCmd(
    BluetoothAvrcpStatus aRspStatus, BluetoothAvrcpResultHandler* aRes);

  nsresult RegisterNotificationRspCmd(
    BluetoothAvrcpEvent aEvent, BluetoothAvrcpNotification aType,
    const BluetoothAvrcpNotificationParam& aParam,
    BluetoothAvrcpResultHandler* aRes);

  nsresult SetVolumeCmd(uint8_t aVolume, BluetoothAvrcpResultHandler* aRes);

protected:
  nsresult Send(BluetoothDaemonPDU* aPDU,
                BluetoothAvrcpResultHandler* aRes);

  void HandleSvc(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU, void* aUserData);

  //
  // Responses
  //

  typedef BluetoothResultRunnable0<BluetoothAvrcpResultHandler, void>
    ResultRunnable;

  typedef BluetoothResultRunnable1<BluetoothAvrcpResultHandler, void,
                                   BluetoothStatus, BluetoothStatus>
    ErrorRunnable;

  void ErrorRsp(const BluetoothDaemonPDUHeader& aHeader,
                BluetoothDaemonPDU& aPDU,
                BluetoothAvrcpResultHandler* aRes);

  void GetPlayStatusRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                           BluetoothDaemonPDU& aPDU,
                           BluetoothAvrcpResultHandler* aRes);

  void ListPlayerAppAttrRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                               BluetoothDaemonPDU& aPDU,
                               BluetoothAvrcpResultHandler* aRes);

  void ListPlayerAppValueRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                                BluetoothDaemonPDU& aPDU,
                                BluetoothAvrcpResultHandler* aRes);

  void GetPlayerAppValueRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                               BluetoothDaemonPDU& aPDU,
                               BluetoothAvrcpResultHandler* aRes);

  void GetPlayerAppAttrTextRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                                  BluetoothDaemonPDU& aPDU,
                                  BluetoothAvrcpResultHandler* aRes);

  void GetPlayerAppValueTextRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                                   BluetoothDaemonPDU& aPDU,
                                   BluetoothAvrcpResultHandler* aRes);

  void GetElementAttrRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                            BluetoothDaemonPDU& aPDU,
                            BluetoothAvrcpResultHandler* aRes);

  void SetPlayerAppValueRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                               BluetoothDaemonPDU& aPDU,
                               BluetoothAvrcpResultHandler* aRes);

  void RegisterNotificationRspRsp(const BluetoothDaemonPDUHeader& aHeader,
                                  BluetoothDaemonPDU& aPDU,
                                  BluetoothAvrcpResultHandler* aRes);

  void SetVolumeRsp(const BluetoothDaemonPDUHeader& aHeader,
                    BluetoothDaemonPDU& aPDU,
                    BluetoothAvrcpResultHandler* aRes);

  void HandleRsp(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 void* aUserData);

  //
  // Notifications
  //

  class NotificationHandlerWrapper;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         nsString, unsigned long,
                                         const nsAString&>
    RemoteFeatureNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    GetPlayStatusNotification;

  typedef BluetoothNotificationRunnable0<NotificationHandlerWrapper, void>
    ListPlayerAppAttrNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         BluetoothAvrcpPlayerAttribute>
    ListPlayerAppValuesNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpPlayerAttribute>,
    uint8_t, const BluetoothAvrcpPlayerAttribute*>
    GetPlayerAppValueNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpPlayerAttribute>,
    uint8_t, const BluetoothAvrcpPlayerAttribute*>
    GetPlayerAppAttrsTextNotification;

  typedef BluetoothNotificationRunnable3<NotificationHandlerWrapper, void,
                                         uint8_t, uint8_t,
                                         nsAutoArrayPtr<uint8_t>, uint8_t,
                                         uint8_t, const uint8_t*>
    GetPlayerAppValuesTextNotification;

  typedef BluetoothNotificationRunnable1<NotificationHandlerWrapper, void,
                                         BluetoothAvrcpPlayerSettings,
                                         const BluetoothAvrcpPlayerSettings&>
    SetPlayerAppValueNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
    uint8_t, nsAutoArrayPtr<BluetoothAvrcpMediaAttribute>,
    uint8_t, const BluetoothAvrcpMediaAttribute*>
    GetElementAttrNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         BluetoothAvrcpEvent, uint32_t>
    RegisterNotificationNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         uint8_t, uint8_t>
    VolumeChangeNotification;

  typedef BluetoothNotificationRunnable2<NotificationHandlerWrapper, void,
                                         int, int>
    PassthroughCmdNotification;

  class GetElementAttrInitOp;
  class GetPlayerAppAttrsTextInitOp;
  class GetPlayerAppValueInitOp;
  class GetPlayerAppValuesTextInitOp;
  class PassthroughCmdInitOp;
  class RemoteFeatureInitOp;

  void RemoteFeatureNtf(const BluetoothDaemonPDUHeader& aHeader,
                        BluetoothDaemonPDU& aPDU);

  void GetPlayStatusNtf(const BluetoothDaemonPDUHeader& aHeader,
                        BluetoothDaemonPDU& aPDU);

  void ListPlayerAppAttrNtf(const BluetoothDaemonPDUHeader& aHeader,
                            BluetoothDaemonPDU& aPDU);

  void ListPlayerAppValuesNtf(const BluetoothDaemonPDUHeader& aHeader,
                              BluetoothDaemonPDU& aPDU);

  void GetPlayerAppValueNtf(const BluetoothDaemonPDUHeader& aHeader,
                            BluetoothDaemonPDU& aPDU);

  void GetPlayerAppAttrsTextNtf(const BluetoothDaemonPDUHeader& aHeader,
                                BluetoothDaemonPDU& aPDU);

  void GetPlayerAppValuesTextNtf(const BluetoothDaemonPDUHeader& aHeader,
                                 BluetoothDaemonPDU& aPDU);

  void SetPlayerAppValueNtf(const BluetoothDaemonPDUHeader& aHeader,
                            BluetoothDaemonPDU& aPDU);

  void GetElementAttrNtf(const BluetoothDaemonPDUHeader& aHeader,
                         BluetoothDaemonPDU& aPDU);

  void RegisterNotificationNtf(const BluetoothDaemonPDUHeader& aHeader,
                               BluetoothDaemonPDU& aPDU);

  void VolumeChangeNtf(const BluetoothDaemonPDUHeader& aHeader,
                       BluetoothDaemonPDU& aPDU);

  void PassthroughCmdNtf(const BluetoothDaemonPDUHeader& aHeader,
                         BluetoothDaemonPDU& aPDU);

  void HandleNtf(const BluetoothDaemonPDUHeader& aHeader,
                 BluetoothDaemonPDU& aPDU,
                 void* aUserData);

  static BluetoothAvrcpNotificationHandler* sNotificationHandler;
};

class BluetoothDaemonAvrcpInterface final
  : public BluetoothAvrcpInterface
{
  class CleanupResultHandler;
  class InitResultHandler;

public:
  BluetoothDaemonAvrcpInterface(BluetoothDaemonAvrcpModule* aModule);
  ~BluetoothDaemonAvrcpInterface();

  void Init(BluetoothAvrcpNotificationHandler* aNotificationHandler,
            BluetoothAvrcpResultHandler* aRes) override;

  void Cleanup(BluetoothAvrcpResultHandler* aRes) override;

  void GetPlayStatusRsp(ControlPlayStatus aPlayStatus,
                        uint32_t aSongLen, uint32_t aSongPos,
                        BluetoothAvrcpResultHandler* aRes) override;

  void ListPlayerAppAttrRsp(int aNumAttr,
                            const BluetoothAvrcpPlayerAttribute* aPAttrs,
                            BluetoothAvrcpResultHandler* aRes) override;

  void ListPlayerAppValueRsp(int aNumVal, uint8_t* aPVals,
                             BluetoothAvrcpResultHandler* aRes) override;

  void GetPlayerAppValueRsp(uint8_t aNumAttrs, const uint8_t* aIds,
                            const uint8_t* aValues,
                            BluetoothAvrcpResultHandler* aRes) override;

  void GetPlayerAppAttrTextRsp(int aNumAttr, const uint8_t* aIds,
                               const char** aTexts,
                               BluetoothAvrcpResultHandler* aRes) override;

  void GetPlayerAppValueTextRsp(int aNumVal, const uint8_t* aIds,
                                const char** aTexts,
                                BluetoothAvrcpResultHandler* aRes) override;

  void GetElementAttrRsp(uint8_t aNumAttr,
                         const BluetoothAvrcpElementAttribute* aAttr,
                         BluetoothAvrcpResultHandler* aRes) override;

  void SetPlayerAppValueRsp(BluetoothAvrcpStatus aRspStatus,
                            BluetoothAvrcpResultHandler* aRes) override;

  void RegisterNotificationRsp(BluetoothAvrcpEvent aEvent,
                               BluetoothAvrcpNotification aType,
                               const BluetoothAvrcpNotificationParam& aParam,
                               BluetoothAvrcpResultHandler* aRes) override;

  void SetVolume(uint8_t aVolume,
                 BluetoothAvrcpResultHandler* aRes) override;

private:
  void DispatchError(BluetoothAvrcpResultHandler* aRes,
                     BluetoothStatus aStatus);
  void DispatchError(BluetoothAvrcpResultHandler* aRes, nsresult aRv);

  BluetoothDaemonAvrcpModule* mModule;
};

END_BLUETOOTH_NAMESPACE

#endif
