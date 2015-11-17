/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Func="Navigator::HasNFCSupport", AvailableIn="PrivilegedApps",
 ChromeConstructor(MozNFCTag tag)]
interface MozIsoDepTech {
  /**
   * Send raw ISO-DEP command to tag and receive the response.
   */
  [Throws]
  Promise<Uint8Array> transceive(Uint8Array command);
};
