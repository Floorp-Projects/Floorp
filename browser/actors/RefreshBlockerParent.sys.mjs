/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class RefreshBlockerParent extends JSWindowActorParent {
  receiveMessage(message) {
    if (message.name == "RefreshBlocker:Blocked") {
      let browser = this.browsingContext.top.embedderElement;
      if (browser) {
        let gBrowser = browser.ownerGlobal.gBrowser;
        if (gBrowser) {
          gBrowser.refreshBlocked(this, browser, message.data);
        }
      }
    }
  }
}
