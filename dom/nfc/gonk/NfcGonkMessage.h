/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NfcGonkMessage_h
#define NfcGonkMessage_h

namespace mozilla {

#define NFCD_MAJOR_VERSION 1
#define NFCD_MINOR_VERSION 14

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
  IOErr = 1,
  Timeout = 2,
  BusyErr = 3,
  ConnectErr = 4,
  DisconnectErr = 5,
  ReadErr = 6,
  WriteErr = 7,
  InvalidParam = 8,
  InsufficientResources = 9,
  SocketCreation = 10,
  FailEnableDiscovery = 11,
  FailDisableDiscovery = 12,
  NotInitialized = 13,
  InitializeFail = 14,
  DeinitializeFail = 15,
  NotSupported = 16,
  FailEnableLowPowerMode = 17,
  FailDisableLowPowerMode = 18,
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
