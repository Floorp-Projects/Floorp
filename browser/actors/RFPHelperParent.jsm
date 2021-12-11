1; /* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["RFPHelperParent"];

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

class RFPHelperParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    if (
      isLetterboxingEnabled &&
      aMessage.name == "Letterboxing:ContentSizeUpdated"
    ) {
      let browser = this.browsingContext.top.embedderElement;
      let window = browser.ownerGlobal;
      window.RFPHelper.contentSizeUpdated(window);
    }
  }
}
