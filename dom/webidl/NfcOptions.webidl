/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary NfcCommandOptions
{
  DOMString type = "";

  long sessionId;
  DOMString requestId = "";

  long powerLevel;

  long techType;

  boolean isP2P;
  sequence<MozNDEFRecordOptions> records;
};

dictionary NfcEventOptions
{
  DOMString type = "";

  long status;
  long sessionId;
  DOMString requestId;

  long majorVersion;
  long minorVersion;

  sequence<NFCTechType> techList;
  sequence<MozNDEFRecordOptions> records;

  boolean isReadOnly;
  boolean canBeMadeReadOnly;
  long maxSupportedLength;

  long powerLevel;

  // HCI Event Transaction fields
  DOMString origin;
  Uint8Array aid;
  Uint8Array payload;
};
