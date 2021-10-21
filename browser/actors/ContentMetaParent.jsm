/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ContentMetaParent"];

class ContentMetaParent extends JSWindowActorParent {
  receiveMessage(message) {
    if (message.name == "Meta:SetPageInfo") {
      let browser = this.manager.browsingContext.top.embedderElement;
      if (browser) {
        let gBrowser = browser.ownerGlobal.gBrowser;
        if (gBrowser) {
          gBrowser.setPageInfo(
            message.data.url,
            message.data.description,
            message.data.previewImageURL
          );
        }
      }
    }
  }
}
