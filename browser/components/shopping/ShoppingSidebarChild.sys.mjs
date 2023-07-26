/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

export class ShoppingSidebarChild extends JSWindowActorChild {
  constructor() {
    super();

    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "optedIn",
      "browser.shopping.experience2023.optedIn",
      null,
      () => this.updateContent()
    );
  }

  receiveMessage(message) {
    switch (message.name) {
      case "ShoppingSidebar:UpdateProductURL":
        this.updateContent();
        break;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "ContentReady":
        this.updateContent();
        break;
    }
  }

  async updateContent() {
    let url = await this.sendQuery("GetProductURL");
    this.sendToContent("Update", {
      optedIn: this.optedIn,
      url,
    });
  }

  sendToContent(eventName, detail) {
    let win = this.contentWindow;
    let evt = new win.CustomEvent(eventName, {
      bubbles: true,
      detail: Cu.cloneInto(detail, win),
    });
    win.document.dispatchEvent(evt);
  }
}
