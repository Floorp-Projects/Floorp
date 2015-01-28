/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Part of this idl is from:
 * http://w3c.github.io/nfc/proposals/common/nfc.html#nfctag-interface
 *
 * Copyright © 2013 Deutsche Telekom, Inc.
 */

/**
 * The enumeration of NFC Tag technologies.
 */
enum NFCTechType {
  "NFC-A",  // NFCForum-TS-DigitalProtocol-1.1 NFC-A.
  "NFC-B",  // NFCForum-TS-DigitalProtocol-1.1 NFC-B.
  "NFC-F",  // NFCForum-TS-DigitalProtocol-1.1 NFC-F.
  "NFC-V",  // ISO 15693.
  "ISO-DEP",  // NFCForum-TS-DigitalProtocol-1.1 ISO-DEP.
  "MIFARE-Classic",  // MIFARE Classic from NXP.
  "MIFARE-Ultralight",  // MIFARE Ultralight from NXP.
  "NFC-Barcode", // NFC Barcode from Kovio.
  "Unknown"
};

/**
 * The enumeration of the types of the tag, the type of a tag could be either
 * one of those types defined in NFC Forum Tag Types (Type1 ~ Type 4), or it
 * could be a NXP-specific tag, like MIFARE Classic.
 */
enum NFCTagType {
  "Type1",
  "Type2",
  "Type3",
  "Type4",
  "MIFARE-Classic"
};

typedef MozIsoDepTech MozTagTech;

[JSImplementation="@mozilla.org/nfc/tag;1", AvailableIn="PrivilegedApps"]
interface MozNFCTag {
  /**
   * The supported technologies of this tag, null if unknown.
   */
  [Cached, Pure] readonly attribute sequence<NFCTechType>? techList;

  /**
   * The identifier of this tag.
   */
  [Constant] readonly attribute Uint8Array? id;

  /**
   * The type of this tag, null if unknown.
   */
  readonly attribute NFCTagType? type;

  /**
   * The maximum size of the NDEF supported on this tag, null if unknown.
   */
  readonly attribute long? maxNDEFSize;

  /**
   * Indicate if this tag is Read-Only, null if unknown.
   */
  readonly attribute boolean? isReadOnly;

  /**
   * Indicate if this tag is formatable, null if unknown.
   */
  readonly attribute boolean? isFormatable;

  /**
   * Indicate if this tag could be made Read-Only, null if unknown.
   */
  readonly attribute boolean? canBeMadeReadOnly;

  /**
   * Indicate if this tag is already lost.
   */
  readonly attribute boolean isLost;

  /**
   * Read current NDEF data on the tag.
   */
  [Throws]
  Promise<sequence<MozNDEFRecord>> readNDEF();

  /**
   * Write NDEF data to the tag.
   */
  [Throws]
  Promise<void> writeNDEF(sequence<MozNDEFRecord> records);

  /**
   * Make a tag read-only.
   */
  [Throws]
  Promise<void> makeReadOnly();

  /**
   * Format a tag as NDEF.
   */
  [Throws]
  Promise<void> format();

  [NewObject, Throws]
  MozTagTech selectTech(NFCTechType tech);
};

// Mozilla Only
partial interface MozNFCTag {
  [ChromeOnly]
  attribute DOMString session;

  [ChromeOnly]
  void notifyLost();

  [ChromeOnly, Throws]
  Promise<Uint8Array> transceive(NFCTechType tech, Uint8Array command);
};
