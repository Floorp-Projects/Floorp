/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutNewTabParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
});

class AboutNewTabParent extends JSWindowActorParent {
  async receiveMessage(message) {
    switch (message.name) {
      case "AboutNewTabVisible":
        await ASRouter.waitForInitialized;
        ASRouter.sendTriggerMessage({
          browser: this.browsingContext.top.embedderElement,
          // triggerId and triggerContext
          id: "defaultBrowserCheck",
          context: { source: "newtab" },
        });
    }
  }
}
