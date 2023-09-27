/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export class ShoppingSidebarParent extends JSWindowActorParent {
  static SHOPPING_ACTIVE_PREF = "browser.shopping.experience2023.active";
  static SHOPPING_OPTED_IN_PREF = "browser.shopping.experience2023.optedIn";

  updateProductURL(uri, flags) {
    this.sendAsyncMessage("ShoppingSidebar:UpdateProductURL", {
      url: uri?.spec ?? null,
      isReload: !!(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD),
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
    this.toggleAllSidebars("urlBar");
  }

  /**
   * Toggles opening or closing all Shopping sidebars.
   * Sets the active pref value for all windows to respond to.
   * params:
   *
   *  @param {string?} source
   *  Optional value, describes where the call came from.
   */
  static toggleAllSidebars(source) {
    let activeState = Services.prefs.getBoolPref(
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF
    );
    Services.prefs.setBoolPref(
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF,
      !activeState
    );

    let optedIn = Services.prefs.getIntPref(
      ShoppingSidebarParent.SHOPPING_OPTED_IN_PREF
    );
    // If the user was opted out, then clicked the button, reset the optedIn
    // pref so they see onboarding.
    if (optedIn == 2) {
      Services.prefs.setIntPref(
        ShoppingSidebarParent.SHOPPING_OPTED_IN_PREF,
        0
      );
    }
    if (source == "urlBar") {
      if (activeState) {
        Glean.shopping.surfaceClosed.record({ source: "addressBarIcon" });
        Glean.shopping.addressBarIconClicked.record({ action: "closed" });
      } else {
        Glean.shopping.addressBarIconClicked.record({ action: "opened" });
      }
    }
  }
}
