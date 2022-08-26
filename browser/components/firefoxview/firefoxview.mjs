/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

window.addEventListener("DOMContentLoaded", () => {
  Services.telemetry.setEventRecordingEnabled("firefoxview", true);
  Services.telemetry.recordEvent("firefoxview", "entered", "firefoxview", null);
  document.getElementById("recently-closed-tabs-container").onLoad();
});

window.addEventListener("unload", () => {
  const tabPickupList = document.querySelector("tab-pickup-list");
  if (tabPickupList) {
    tabPickupList.cleanup();
  }
  document.getElementById("tab-pickup-container").cleanup();
  document.getElementById("recently-closed-tabs-container").cleanup();
});
