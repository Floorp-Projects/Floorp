/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

window.addEventListener("DOMContentLoaded", async () => {
  Services.telemetry.recordEvent("firefoxview", "entered", "firefoxview", null);
  if (Cu.isInAutomation) {
    Services.obs.notifyObservers(null, "firefoxview-entered");
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "visible") {
        Services.obs.notifyObservers(null, "firefoxview-entered");
      }
    });
  }

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
});

window.addEventListener("unload", () => {
  const tabPickupList = document.querySelector("tab-pickup-list");
  if (tabPickupList) {
    tabPickupList.cleanup();
  }
  document.getElementById("tab-pickup-container").cleanup();
  document.getElementById("recently-closed-tabs-container").cleanup();
});
