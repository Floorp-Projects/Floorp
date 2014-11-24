/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Part of this idl is from:
 * http://w3c.github.io/nfc/proposals/common/nfc.html#nfctag-interface
 *
 * Copyright © 2013 Deutsche Telekom, Inc.
 */

enum NFCTechType {
  "NDEF",
  "P2P",
  "NFC_A",
  "NFC_B",
  "NFC_F",
  "NFC_V",
  "NFC_ISO_DEP",
  "MIFARE_CLASSIC",
  "MIFARE_ULTRALIGHT",
  "NFC_BARCODE"
};

/**
 * The enumeration of the types of the tag, the type of a tag could be either
 * one of those types defined in NFC Forum (type1 ~ type 4), or it could be a
 * NXP-specific tag, like Mifare Classic.
 */
enum NFCTagType {
  "type1",
  "type2",
  "type3",
  "type4",
  "mifare_classic"
};

[JSImplementation="@mozilla.org/nfc/NFCTag;1", AvailableIn="CertifiedApps"]
interface MozNFCTag {
  /**
   * The supported technologies of this tag, null if unknown.
   */
  [Cached, Pure] readonly attribute sequence<NFCTechType>? techList;

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
};

// Mozilla Only
partial interface MozNFCTag {
  [ChromeOnly]
  attribute DOMString session;

  /**
   * Indicate if this tag is already lost.
   */
  readonly attribute boolean isLost;
};
