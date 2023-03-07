/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FeatureCallout } = ChromeUtils.importESModule(
  "resource:///modules/FeatureCallout.sys.mjs"
);

const MediaQueryDOMSorting = {
  init() {
    this.recentlyClosedTabs = document.getElementById(
      "recently-closed-tabs-container"
    );
    this.colorways = document.getElementById("colorways");
    this.mql = window.matchMedia("(max-width: 65rem)");
    this.mql.addEventListener("change", () => this.changeHandler());
    this.changeHandler();
  },
  cleanup() {
    this.mql.removeEventListener("change", () => this.changeHandler());
  },
  changeHandler() {
    const oldFocus = document.activeElement;
    if (this.mql.matches) {
      this.recentlyClosedTabs.before(this.colorways);
    } else {
      this.colorways.before(this.recentlyClosedTabs);
    }
    if (oldFocus) {
      Services.focus.setFocus(oldFocus, Ci.nsIFocusManager.FLAG_NOSCROLL);
    }
  },
};

const launchFeatureTour = () => {
  let callout = new FeatureCallout({
    win: window,
    prefName: "browser.firefox-view.feature-tour",
    page: "about:firefoxview",
  });
  callout.showFeatureCallout();
};

window.addEventListener("DOMContentLoaded", async () => {
  Services.telemetry.setEventRecordingEnabled("firefoxview", true);
  Services.telemetry.recordEvent("firefoxview", "entered", "firefoxview", null);
  document.getElementById("recently-closed-tabs-container").onLoad();
  MediaQueryDOMSorting.init();
  // If Firefox View was reloaded by the user, force syncing of tabs
  // to get the most up to date synced tabs.
  if (
    performance
      .getEntriesByType("navigation")
      .map(nav => nav.type)
      .includes("reload")
  ) {
    await document.getElementById("tab-pickup-container").onReload();
  }
  launchFeatureTour();
});

window.addEventListener("unload", () => {
  const tabPickupList = document.querySelector("tab-pickup-list");
  if (tabPickupList) {
    tabPickupList.cleanup();
  }
  document.getElementById("tab-pickup-container").cleanup();
  document.getElementById("recently-closed-tabs-container").cleanup();
  MediaQueryDOMSorting.cleanup();
});
