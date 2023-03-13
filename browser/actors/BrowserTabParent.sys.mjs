/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { BrowserWindowTracker } = ChromeUtils.import(
  "resource:///modules/BrowserWindowTracker.jsm"
);

export class BrowserTabParent extends JSWindowActorParent {
  receiveMessage(message) {
    let browsingContext = this.manager.browsingContext;
    let browser = browsingContext.embedderElement;
    if (!browser) {
      return; // Can happen sometimes if browser is being destroyed
    }

    switch (message.name) {
      case "Browser:WindowCreated": {
        BrowserWindowTracker.windowCreated(browser);
        break;
      }
    }
  }
}
