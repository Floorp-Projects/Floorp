/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BrowserTabParent"];

const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);

class BrowserTabParent extends JSWindowActorParent {
  receiveMessage(message) {
    let browser = this.manager.browsingContext.embedderElement;
    if (!browser) {
      return; // Can happen sometimes if browser is being destroyed
    }

    let gBrowser = browser.ownerGlobal.gBrowser;

    switch (message.name) {
      case "Browser:WindowCreated": {
        gBrowser.announceWindowCreated(browser, message.data.userContextId);
        BrowserWindowTracker.windowCreated(browser);
        break;
      }

      case "Browser:FirstPaint": {
        browser.ownerGlobal.gBrowserInit._firstContentWindowPaintDeferred.resolve();
        break;
      }

      case "Browser:LoadURI": {
        gBrowser.ownerGlobal.RedirectLoad(browser, message.data);
        break;
      }

      case "PointerLock:Entered": {
        browser.ownerGlobal.PointerLock.entered(message.data.originNoSuffix);
        break;
      }

      case "PointerLock:Exited":
        browser.ownerGlobal.PointerLock.exited();
        break;
    }
  }
}
