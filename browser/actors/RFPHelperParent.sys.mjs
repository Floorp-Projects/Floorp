1; /* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  RFPHelper: "resource://gre/modules/RFPHelper.sys.mjs",
});

const kPrefLetterboxing = "privacy.resistFingerprinting.letterboxing";

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "isLetterboxingEnabled",
  kPrefLetterboxing,
  false
);

export class RFPHelperParent extends JSWindowActorParent {
  receiveMessage(aMessage) {
    if (
      lazy.isLetterboxingEnabled &&
      aMessage.name == "Letterboxing:ContentSizeUpdated"
    ) {
      let browser = this.browsingContext.top.embedderElement;
      let window = browser.ownerGlobal;
      lazy.RFPHelper.contentSizeUpdated(window);
    }
  }
}
