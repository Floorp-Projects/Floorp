/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ShoppingSidebarChild extends JSWindowActorChild {
  async initContent() {
    let url = await this.sendQuery("GetProductURL");
    this.updateProductURL(url);
  }

  receiveMessage(message) {
    switch (message.name) {
      case "ShoppingSidebar:UpdateProductURL":
        this.updateProductURL(message.data.url);
        break;
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "ContentReady":
        this.initContent();
        break;
    }
  }

  updateProductURL(url) {
    this.sendToContent("UpdateProductURL", {
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
