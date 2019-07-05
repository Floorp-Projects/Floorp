/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["RFPHelperChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const kPrefLetterboxing = "privacy.resistFingerprinting.letterboxing";

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "isLetterboxingEnabled",
  kPrefLetterboxing,
  false
);

class RFPHelperChild extends ActorChild {
  handleEvent() {
    if (isLetterboxingEnabled) {
      this.mm.sendAsyncMessage("Letterboxing:ContentSizeUpdated");
    }
  }
  receiveMessage(aMessage) {
    if (isLetterboxingEnabled) {
      this.mm.sendAsyncMessage("Letterboxing:ContentSizeUpdated");
    }
  }
}
