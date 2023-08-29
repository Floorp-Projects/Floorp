/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ShoppingSidebarParent extends JSWindowActorParent {
  static SHOPPING_ACTIVE_PREF = "browser.shopping.experience2023.active";

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

  /**
   * Called when the user clicks the URL bar button.
   */
  static urlbarButtonClick(event) {
    if (event.button > 0) {
      return;
    }
    this.toggleAllSidebars();
  }

  /**
   * Toggles opening or closing all Shopping sidebars.
   * Sets the active pref value for all windows to respond to.
   */
  static toggleAllSidebars() {
    let activeState = Services.prefs.getBoolPref(
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF
    );
    Services.prefs.setBoolPref(
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF,
      !activeState
    );
  }
}
