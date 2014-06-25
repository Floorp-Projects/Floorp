/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary MmsDeliveryInfo {
  DOMString? receiver = null;
  DOMString? deliveryStatus = null;
  DOMTimeStamp deliveryTimestamp = 0; // 0 if not available (e.g.,
                                      // |delivery| = "received" or not yet delivered).
  DOMString? readStatus = null;
  DOMTimeStamp readTimestamp = 0; // 0 if not available (e.g.,
                                  // |delivery| = "received" or not yet read).
};
