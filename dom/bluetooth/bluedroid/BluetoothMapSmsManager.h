/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluedroid_BluetoothMapSmsManager_h
#define mozilla_dom_bluetooth_bluedroid_BluetoothMapSmsManager_h

#include "BluetoothCommon.h"
#include "BluetoothMapFolder.h"
#include "BluetoothProfileManagerBase.h"
#include "BluetoothSocketObserver.h"
#include "mozilla/ipc/SocketBase.h"

BEGIN_BLUETOOTH_NAMESPACE

struct Map {
  enum AppParametersTagId {
    MaxListCount                  = 0x1,
    StartOffset                   = 0x2,
    FilterMessageType             = 0x3,
    FilterPeriodBegin             = 0x4,
    FilterPeriodEnd               = 0x5,
    FilterReadStatus              = 0x6,
    FilterRecipient               = 0x7,
    FilterOriginator              = 0x8,
    FilterPriority                = 0x9,
    Attachment                    = 0x0A,
    Transparent                   = 0x0B,
    Retry                         = 0x0C,
    NewMessage                    = 0x0D,
    NotificationStatus            = 0x0E,
    MASInstanceId                 = 0x0F,
    ParameterMask                 = 0x10,
    FolderListingSize             = 0x11,
    MessagesListingSize           = 0x12,
    SubjectLength                 = 0x13,
    Charset                       = 0x14,
    FractionRequest               = 0x15,
    FractionDeliver               = 0x16,
    StatusIndicator               = 0x17,
    StatusValue                   = 0x18,
    MSETime                       = 0x19
  };
};

class BluetoothNamedValue;
class BluetoothSocket;
class ObexHeaderSet;

/*
 * BluetoothMapSmsManager acts as Message Server Equipment (MSE) and runs both
 * MAS server and MNS client to exchange SMS/MMS message.
 */

class BluetoothMapSmsManager : public BluetoothSocketObserver
                             , public BluetoothProfileManagerBase
{
public:
  BT_DECL_PROFILE_MGR_BASE
  BT_DECL_SOCKET_OBSERVER
  virtual void GetName(nsACString& aName)
  {
    aName.AssignLiteral("MapSms");
  }

  static const int MAX_PACKET_LENGTH = 0xFFFE;
  static const int MAX_INSTANCE_ID = 255;
  // SDP record for SupportedMessageTypes
  static const int SDP_MESSAGE_TYPE_EMAIL = 0x01;
  static const int SDP_MESSAGE_TYPE_SMS_GSM = 0x02;
  static const int SDP_MESSAGE_TYPE_SMS_CDMA = 0x04;
  static const int SDP_MESSAGE_TYPE_MMS = 0x08;
  // By defualt SMS/MMS is default supported
  static const int SDP_SMS_MMS_INSTANCE_ID = 0;

  static BluetoothMapSmsManager* Get();
  bool Listen();

protected:
  virtual ~BluetoothMapSmsManager();

private:
  BluetoothMapSmsManager();
  bool Init();
  void HandleShutdown();

  void ReplyToConnect();
  void ReplyToDisconnectOrAbort();
  void ReplyToSetPath();
  void ReplyToPut();
  void ReplyError(uint8_t aError);

  void HandleNotificationRegistration(const ObexHeaderSet& aHeader);
  void HandleEventReport(const ObexHeaderSet& aHeader);
  void HandleMessageStatus(const ObexHeaderSet& aHeader);
  void HandleSmsMmsFolderListing(const ObexHeaderSet& aHeader);
  void HandleSmsMmsMsgListing(const ObexHeaderSet& aHeader);
  void HandleSmsMmsGetMessage(const ObexHeaderSet& aHeader);
  void AppendBtNamedValueByTagId(const ObexHeaderSet& aHeader,
    InfallibleTArray<BluetoothNamedValue>& aValues,
    const Map::AppParametersTagId aTagId);
  void SendMasObexData(uint8_t* aData, uint8_t aOpcode, int aSize);
  void SendMnsObexData(uint8_t* aData, uint8_t aOpcode, int aSize);

  uint8_t SetPath(uint8_t flags, const ObexHeaderSet& aHeader);
  bool CompareHeaderTarget(const ObexHeaderSet& aHeader);
  void AfterMapSmsConnected();
  void AfterMapSmsDisconnected();
  void CreateMnsObexConnection();
  void DestroyMnsObexConnection();
  void SendMnsConnectRequest();
  void SendMnsDisconnectRequest();
  void MnsDataHandler(mozilla::ipc::UnixSocketBuffer* aMessage);
  void MasDataHandler(mozilla::ipc::UnixSocketBuffer* aMessage);
  /*
   * Build mandatory folders
   */
  void BuildDefaultFolderStructure();
  /**
   * Current virtual folder path
   */
  BluetoothMapFolder* mCurrentFolder;
  nsRefPtr<BluetoothMapFolder> mRootFolder;

  /*
   * Record the last command
   */
  int mLastCommand;
  // MAS OBEX session status. Set when MAS OBEX session is established.
  bool mMasConnected;
  // MNS OBEX session status. Set when MNS OBEX session is established.
  bool mMnsConnected;
  bool mNtfRequired;
  nsString mDeviceAddress;
  unsigned int mRemoteMaxPacketLength;

  // If a connection has been established, mMasSocket will be the socket
  // communicating with the remote socket. We maintain the invariant that if
  // mMasSocket is non-null, mServerSocket must be null (and vice versa).
  nsRefPtr<BluetoothSocket> mMasSocket;

  // Server socket. Once an inbound connection is established, it will hand
  // over the ownership to mMasSocket, and get a new server socket while Listen()
  // is called.
  nsRefPtr<BluetoothSocket> mMasServerSocket;

  // Message notification service client socket
  nsRefPtr<BluetoothSocket> mMnsSocket;
};

END_BLUETOOTH_NAMESPACE

#endif //mozilla_dom_bluetooth_bluedroid_BluetoothMapSmsManager_h
