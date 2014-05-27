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
  "NFC_A",
  "NFC_B",
  "NFC_ISO_DEP",
  "NFC_F",
  "NFC_V",
  "NDEF",
  "NDEF_FORMATABLE",
  "MIFARE_CLASSIC",
  "MIFARE_ULTRALIGHT",
  "NFC_BARCODE",
  "P2P",
  "UNKNOWN_TECH"
};

[JSImplementation="@mozilla.org/nfc/NFCTag;1"]
interface MozNFCTag {
  DOMRequest getDetailsNDEF();
  DOMRequest readNDEF();
  DOMRequest writeNDEF(sequence<MozNDEFRecord> records);
  DOMRequest makeReadOnlyNDEF();

  DOMRequest connect(NFCTechType techType);
  DOMRequest close();
};

// Mozilla Only
partial interface MozNFCTag {
  [ChromeOnly]
  attribute DOMString session;
};
