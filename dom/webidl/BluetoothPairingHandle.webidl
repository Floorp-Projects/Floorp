/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[CheckPermissions="bluetooth"]
interface BluetoothPairingHandle
{
  /**
   * A 6-digit string ranging from decimal 000000 to 999999.
   * This attribute is an empty string for enterpincodereq and
   * pairingconsentreq.
   */
  readonly attribute DOMString passkey;

  [NewObject, Throws]
  Promise<void> setPinCode(DOMString aPinCode);
  [NewObject, Throws]
  Promise<void> setPairingConfirmation(boolean aConfirm);
};
