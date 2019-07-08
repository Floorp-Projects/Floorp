/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Checks if the Customize Touch Bar button appears when a Touch Bar is
// initialized.
add_task(async function customizeTouchBarButtonAppears() {
  let updater = Cc["@mozilla.org/widget/touchbarupdater;1"].getService(
    Ci.nsITouchBarUpdater
  );
  // This value will be reset to its default the next time a window is opened.
  updater.setTouchBarInitialized(true);
  await startCustomizing();
  let touchbarButton = document.querySelector("#customization-touchbar-button");
  ok(!touchbarButton.hidden, "Customize Touch Bar button is not hidden.");
  let touchbarSpacer = document.querySelector("#customization-touchbar-spacer");
  ok(!touchbarSpacer.hidden, "Customize Touch Bar spacer is not hidden.");
  await endCustomizing();
});
