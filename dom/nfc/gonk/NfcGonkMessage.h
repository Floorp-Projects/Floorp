/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcGonkMessage_h
#define NfcGonkMessage_h

namespace mozilla {

#define NFCD_MAJOR_VERSION 1
#define NFCD_MINOR_VERSION 11

enum NfcRequest {
  ConfigReq = 0,
  ConnectReq,
  CloseReq,
  ReadNDEFReq,
  WriteNDEFReq,
  MakeReadOnlyNDEFReq,
};

enum NfcResponse {
  GeneralRsp = 1000,
  ConfigRsp,
  ReadNDEFRsp,
};

enum NfcNotification {
  Initialized = 2000,
  TechDiscovered,
  TechLost,
  HCIEventTransaction,
};

enum NfcTechlogy {
  NDEF = 0,
  NDEFWritable,
  NDEFFormattable,
  P2P,
};

enum NfcErrorCode {
  Success = 0,
  IOErr = -1,
  Cancelled = -2,
  Timeout = -3,
  BusyErr = -4,
  ConnectErr = -5,
  DisconnectErr = -6,
  ReadErr = -7,
  WriteErr = -8,
  InvalidParam = -9,
  InsufficientResources = -10,
  SocketCreation = -11,
  SocketNotConnected = -12,
  BufferTooSmall = -13,
  SapUsed = -14,
  ServiceNameUsed = -15,
  SocketOptions = -16,
  FailEnableDiscovery = -17,
  FailDisableDiscovery = -18,
  NotInitialized = -19,
  InitializeFail = -20,
  DeinitializeFail = -21,
  SeConnected = -22,
  NoSeConnected = -23,
  NotSupported = -24,
  BadSessionId = -25,
  LostTech = -26,
  BadTechType = -27,
  SelectSeFail = -28,
  DeselectSeFail = -29,
  FailEnableLowPowerMode = -30,
  FailDisableLowPowerMode = -31,
};

enum SecureElementOrigin {
  SIM = 0,
  ESE = 1,
  ASSD = 2,
  OriginEndGuard = 3
};

enum NdefType {
  UNKNOWN = -1,
  TYPE1_TAG = 0,
  TYPE2_TAG = 1,
  TYPE3_TAG = 2,
  TYPE4_TAG = 3,
  MIFARE_CLASSIC_TAG = 4
};

} // namespace mozilla

#endif // NfcGonkMessage_h
