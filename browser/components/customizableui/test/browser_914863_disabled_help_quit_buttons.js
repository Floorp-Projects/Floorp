/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Entering then exiting customization mode should reenable the Help and Exit buttons.
add_task(function() {
  yield startCustomizing();
  let helpButton = document.getElementById("PanelUI-help");
  let quitButton = document.getElementById("PanelUI-quit");
  ok(helpButton.getAttribute("disabled") == "true", "Help button should be disabled while in customization mode.");
  ok(quitButton.getAttribute("disabled") == "true", "Quit button should be disabled while in customization mode.");
  yield endCustomizing();

  ok(!helpButton.hasAttribute("disabled"), "Help button should not be disabled.");
  ok(!quitButton.hasAttribute("disabled"), "Quit button should not be disabled.");
});
