/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

import { showFeatureCallout } from "./featureCallout.mjs";

window.addEventListener("DOMContentLoaded", () => {
  document.getElementById("recently-closed-tabs-container").onLoad();
  showFeatureCallout("FIREFOX_VIEW_FEATURE_TOUR");
});

window.addEventListener("unload", () => {
  const tabPickupList = document.querySelector("tab-pickup-list");
  if (tabPickupList) {
    tabPickupList.cleanup();
  }
  document.getElementById("tab-pickup-container").cleanup();
  document.getElementById("recently-closed-tabs-container").cleanup();
});
