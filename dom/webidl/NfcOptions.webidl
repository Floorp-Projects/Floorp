/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum RFState {
  "idle",
  "listen",
  "discovery"
};

/**
 * Type of the Request used in NfcCommandOptions.
 */
enum NfcRequestType {
  "changeRFState",
  "readNDEF",
  "writeNDEF",
  "makeReadOnly",
  "format",
  "transceive"
};

/**
 * Type of the Response used in NfcEventOptions.
 */
enum NfcResponseType {
  "changeRFStateRsp",
  "readNDEFRsp",
  "writeNDEFRsp",
  "makeReadOnlyRsp",
  "formatRsp",
  "transceiveRsp",
};

/**
 * Type of the Notification used in NfcEventOptions.
 */
enum NfcNotificationType {
  "initialized",
  "techDiscovered",
  "techLost",
  "hciEventTransaction",
  "ndefReceived",
};

/**
 * The source of HCI Transaction Event.
 */
enum HCIEventOrigin {
  "SIM",
  "eSE",
  "ASSD"
};

dictionary NfcCommandOptions
{
  required NfcRequestType type;

  long sessionId;
  required DOMString requestId;

  RFState rfState;

  long techType;

  boolean isP2P;
  sequence<MozNDEFRecordOptions> records;

  NFCTechType technology;
  Uint8Array command;
};

dictionary NfcEventOptions
{
  NfcResponseType rspType;
  NfcNotificationType ntfType;

  long status;
  NfcErrorMessage errorMsg;
  long sessionId;
  DOMString requestId;

  long majorVersion;
  long minorVersion;

  boolean isP2P;
  sequence<NFCTechType> techList;
  Uint8Array tagId;
  sequence<MozNDEFRecordOptions> records;

  NFCTagType tagType;
  long maxNDEFSize;
  boolean isReadOnly;
  boolean isFormatable;

  RFState rfState;

  // HCI Event Transaction fields
  DOMString origin;
  Uint8Array aid;
  Uint8Array payload;

  // Tag transceive response data
  Uint8Array response;
};
