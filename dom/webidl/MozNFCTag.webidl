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
  "NDEF_WRITABLE",
  "NDEF_FORMATABLE",
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

[JSImplementation="@mozilla.org/nfc/NFCTag;1", AvailableIn="CertifiedApps"]
interface MozNFCTag {
  DOMRequest readNDEF();
  DOMRequest writeNDEF(sequence<MozNDEFRecord> records);
  DOMRequest makeReadOnlyNDEF();
};

// Mozilla Only
partial interface MozNFCTag {
  [ChromeOnly]
  attribute DOMString session;
};
