/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ShoppingSidebarParent extends JSWindowActorParent {
  updateProductURL() {
    this.sendAsyncMessage("ShoppingSidebar:UpdateProductURL");
  }

  async receiveMessage(message) {
    let win = this.browsingContext.top.embedderElement.ownerGlobal;
    switch (message.name) {
      case "GetProductURL":
        let url = win.gBrowser.selectedBrowser.currentURI.spec;
        return url;
    }
    return null;
  }
}
