/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly]
interface BluetoothPairingHandle
{
  /**
   * A 6-digit string ranging from decimal 000000 to 999999.
   * This attribute is an empty string for enterpincodereq and
   * pairingconsentreq.
   */
  readonly attribute DOMString passkey;

  /**
   * Reply pin code for enterpincodereq. The promise will be rejected if the
   * pairing request type is not enterpincodereq or operation fails.
   */
  [NewObject]
  Promise<void> setPinCode(DOMString aPinCode);

  /**
   * Accept pairing requests. The promise will be rejected if the pairing
   * request type is not pairingconfirmationreq or pairingconsentreq or
   * operation fails.
   */
  [NewObject]
  Promise<void> accept();

  // Reject pairing requests. The promise will be rejected if operation fails.
  [NewObject]
  Promise<void> reject();
};
