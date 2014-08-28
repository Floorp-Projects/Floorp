/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary NDEFRecord
{
  byte tnf;
  Uint8Array type;
  Uint8Array id;
  Uint8Array payload;
};

dictionary NfcCommandOptions
{
  DOMString type = "";

  long sessionId;
  DOMString requestId = "";

  long powerLevel;

  long techType;

  sequence<NDEFRecord> records;
};

dictionary NfcEventOptions
{
  DOMString type = "";

  long status;
  long sessionId;
  DOMString requestId;

  long majorVersion;
  long minorVersion;

  sequence<DOMString> techList;
  sequence<NDEFRecord> records;

  boolean isReadOnly;
  boolean canBeMadeReadOnly;
  long maxSupportedLength;

  long powerLevel;

  // HCI Event Transaction fields
  DOMString origin;
  long originType;
  long originIndex;
  Uint8Array aid;
  Uint8Array payload;
};
