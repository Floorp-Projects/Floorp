/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Part of this IDL file is from:
 * http://w3c.github.io/nfc/proposals/common/nfc.html#idl-def-NFCPeer
 *
 * Copyright © 2013 Deutsche Telekom, Inc.
 */

[JSImplementation="@mozilla.org/nfc/NFCPeer;1"]
interface MozNFCPeer {
  DOMRequest sendNDEF(sequence<MozNdefRecord> records);
};

// Mozilla Only
partial interface MozNFCPeer {
  [ChromeOnly]
  attribute DOMString session;
  [ChromeOnly]
  void setSessionToken(DOMString sessionToken);
};
