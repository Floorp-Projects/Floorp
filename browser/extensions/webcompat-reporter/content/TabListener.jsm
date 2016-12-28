/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["TabListener"];

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
 "resource:///modules/CustomizableUI.jsm");

const WIDGET_ID = "webcompat-reporter-button";

// Class that watches for url/location/tab changes and enables or disables
// the Report Site Issue button accordingly
class TabListener {
  constructor(win) {
    this.win = win;
    this.browser = win.gBrowser;
    this.addListeners();
  }

  addListeners() {
    this.browser.addTabsProgressListener(this);
    this.browser.tabContainer.addEventListener("TabSelect", this);
  }

  removeListeners() {
    this.browser.removeTabsProgressListener(this);
    this.browser.tabContainer.removeEventListener("TabSelect", this);
  }

  handleEvent(e) {
    switch (e.type) {
      case "TabSelect":
        this.setButtonState(e.target.linkedBrowser.currentURI.scheme);
        break;
    }
  }

  onLocationChange(browser, webProgress, request, uri, flags) {
    this.setButtonState(uri.scheme);
  }

  static isReportableScheme(scheme) {
    return ["http", "https"].some((prefix) => scheme.startsWith(prefix));
  }

  setButtonState(scheme) {
    // Bail early if the button is in the palette.
    if (!CustomizableUI.getPlacementOfWidget(WIDGET_ID)) {
      return;
    }

    if (TabListener.isReportableScheme(scheme)) {
      CustomizableUI.getWidget(WIDGET_ID).forWindow(this.win).node.removeAttribute("disabled");
    } else {
      CustomizableUI.getWidget(WIDGET_ID).forWindow(this.win).node.setAttribute("disabled", true);
    }
  }
}
