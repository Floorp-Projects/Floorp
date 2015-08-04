/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/Services.jsm");

addEventListener("dialogaccept", function () {
  let pane = document.getElementById("sync-customize-pane");
  // First determine what the preference for the "global" sync enabled pref
  // should be based on the engines selected.
  let prefElts = pane.querySelectorAll("preferences > preference");
  let syncEnabled = false;
  for (let elt of prefElts) {
    if (elt.name.startsWith("services.sync.") && elt.value) {
      syncEnabled = true;
      break;
    }
  }
  Services.prefs.setBoolPref("services.sync.enabled", syncEnabled);
  // and write the individual prefs.
  pane.writePreferences(true);
  window.arguments[0].accepted = true;
});
