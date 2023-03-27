/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { FeatureCallout } = ChromeUtils.importESModule(
  "resource:///modules/FeatureCallout.sys.mjs"
);

const launchFeatureTour = () => {
  let callout = new FeatureCallout({
    win: window,
    prefName: "browser.firefox-view.feature-tour",
    page: "about:firefoxview",
    theme: { preset: "themed-content" },
  });
  callout.showFeatureCallout();
};

window.addEventListener("DOMContentLoaded", async () => {
  Services.telemetry.setEventRecordingEnabled("firefoxview", true);
  Services.telemetry.recordEvent("firefoxview", "entered", "firefoxview", null);
  document.getElementById("recently-closed-tabs-container").onLoad();
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
});
