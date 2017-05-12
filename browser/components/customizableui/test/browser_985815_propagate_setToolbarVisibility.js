/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  ok(CustomizableUI.inDefaultState, "Should start in default state.");
  this.otherWin = await openAndLoadWindow({private: true}, true);
  await startCustomizing(this.otherWin);
  let resetButton = this.otherWin.document.getElementById("customization-reset-button");
  ok(resetButton.disabled, "Reset button should be disabled");

  if (typeof CustomizableUI.setToolbarVisibility == "function") {
    CustomizableUI.setToolbarVisibility("PersonalToolbar", true);
  } else {
    setToolbarVisibility(document.getElementById("PersonalToolbar"), true);
  }

  let otherPersonalToolbar = this.otherWin.document.getElementById("PersonalToolbar");
  let personalToolbar = document.getElementById("PersonalToolbar");
  ok(!otherPersonalToolbar.collapsed, "Toolbar should be uncollapsed in private window");
  ok(!personalToolbar.collapsed, "Toolbar should be uncollapsed in normal window");
  ok(!resetButton.disabled, "Reset button should be enabled");

  await this.otherWin.gCustomizeMode.reset();

  ok(otherPersonalToolbar.collapsed, "Toolbar should be collapsed in private window");
  ok(personalToolbar.collapsed, "Toolbar should be collapsed in normal window");
  ok(resetButton.disabled, "Reset button should be disabled");

  await endCustomizing(this.otherWin);

  await promiseWindowClosed(this.otherWin);
});


add_task(async function asyncCleanup() {
  if (this.otherWin && !this.otherWin.closed) {
    await promiseWindowClosed(this.otherWin);
  }
  if (!CustomizableUI.inDefaultState) {
    CustomizableUI.reset();
  }
});
