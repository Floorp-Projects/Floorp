/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ShoppingSidebarParent extends JSWindowActorParent {
  updateProductURL(uri) {
    this.sendAsyncMessage("ShoppingSidebar:UpdateProductURL", {
      url: uri?.spec ?? null,
    });
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "GetProductURL":
        let sidebarBrowser = this.browsingContext.top.embedderElement;
        let panel = sidebarBrowser.closest(".browserSidebarContainer");
        let associatedTabbedBrowser = panel.querySelector(
          "browser[messagemanagergroup=browsers]"
        );
        return associatedTabbedBrowser.currentURI?.spec ?? null;
    }
    return null;
  }
}
