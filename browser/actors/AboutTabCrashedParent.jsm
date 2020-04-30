/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutTabCrashedParent"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyModuleGetters(this, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
  TabCrashHandler: "resource:///modules/ContentCrashHandlers.jsm",
});

// A list of all of the open about:tabcrashed pages.
let gAboutTabCrashedPages = new Map();

class AboutTabCrashedParent extends JSWindowActorParent {
  didDestroy() {
    this.removeCrashedPage();
  }

  async receiveMessage(message) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      // If there is no browser, remove the crashed page from the set
      // and return.
      this.removeCrashedPage();
      return;
    }

    let gBrowser = browser.getTabBrowser();
    let tab = gBrowser.getTabForBrowser(browser);

    switch (message.name) {
      case "Load": {
        gAboutTabCrashedPages.set(this, this.browser);
        this.updateTabCrashedCount();

        let report = TabCrashHandler.onAboutTabCrashedLoad(browser);
        this.sendAsyncMessage("SetCrashReportAvailable", report);
        break;
      }

      case "closeTab": {
        TabCrashHandler.maybeSendCrashReport(browser, message);
        gBrowser.removeTab(tab, { animate: true });
        break;
      }

      case "restoreTab": {
        TabCrashHandler.maybeSendCrashReport(browser, message);
        SessionStore.reviveCrashedTab(tab);
        break;
      }

      case "restoreAll": {
        TabCrashHandler.maybeSendCrashReport(browser, message);
        SessionStore.reviveAllCrashedTabs();
        break;
      }
    }
  }

  removeCrashedPage() {
    let browser =
      this.browsingContext.top.embedderElement ||
      gAboutTabCrashedPages.get(this);

    gAboutTabCrashedPages.delete(this);
    this.updateTabCrashedCount();

    TabCrashHandler.onAboutTabCrashedUnload(browser);
  }

  updateTabCrashedCount() {
    // Broadcast to all about:tabcrashed pages a count of
    // how many about:tabcrashed pages exist, so that they
    // can decide whether or not to display the "Restore All
    // Crashed Tabs" button.
    let count = gAboutTabCrashedPages.size;

    for (let actor of gAboutTabCrashedPages.keys()) {
      let browser = actor.browsingContext.top.embedderElement;
      if (browser) {
        browser.sendMessageToActor("UpdateCount", { count }, "AboutTabCrashed");
      }
    }
  }
}
