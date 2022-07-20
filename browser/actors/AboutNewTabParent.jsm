/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutNewTabParent"];

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ASRouter: "resource://activity-stream/lib/ASRouter.sys.mjs",
});

class AboutNewTabParent extends JSWindowActorParent {
  async receiveMessage(message) {
    switch (message.name) {
      case "AboutNewTabVisible":
        await lazy.ASRouter.waitForInitialized;
        lazy.ASRouter.sendTriggerMessage({
          browser: this.browsingContext.top.embedderElement,
          // triggerId and triggerContext
          id: "defaultBrowserCheck",
          context: { source: "newtab" },
        });
    }
  }
}
