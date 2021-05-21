/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["EncryptedMediaChild"];

class EncryptedMediaChild extends JSWindowActorChild {
  // Expected to observe 'mediakeys-request' as notified from MediaKeySystemAccess.
  // @param aSubject the nsPIDOMWindowInner associated with the notifying MediaKeySystemAccess.
  // @param aTopic should be "mediakeys-request".
  // @param aData json containing a `status` and a `keysystem`.
  observe(aSubject, aTopic, aData) {
    this.sendAsyncMessage("EMEVideo:ContentMediaKeysRequest", aData);
  }
}
