/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly]
interface BluetoothPbapRequestHandle
{
  /**
   * Reply vCard object to the PBAP request. The DOMRequest will get onerror
   * callback if the PBAP request type is not 'pullvcardentryreq' or operation
   * fails.
   */
  [NewObject, Throws]
  DOMRequest replyTovCardPulling(Blob vcardObject);

  /**
   * Reply vCard object to the PBAP request. The DOMRequest will get onerror
   * callback if the PBAP request type is not 'pullphonebookreq' or operation
   * fails.
   */
  [NewObject, Throws]
  DOMRequest replyToPhonebookPulling(Blob vcardObject,
                                     unsigned long long phonebookSize);
  /**
   * Reply vCard object to the PBAP request. The DOMRequest will get onerror
   * callback if the PBAP request type is not 'pullvcardlistingreq' or operation
   * fails.
   */
  [NewObject, Throws]
  DOMRequest replyTovCardListing(Blob vcardObject,
                                 unsigned long long phonebookSize);
};
