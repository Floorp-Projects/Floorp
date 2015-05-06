/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function isFullscreenSizeMode() {
  let sizemode = document.documentElement.getAttribute("sizemode");
  return sizemode == "fullscreen";
}

// Observers should be disabled when in customization mode.
add_task(function() {
  // Open and close the panel to make sure that the
  // area is generated before getting a child of the area.
  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  yield shownPanelPromise;
  let hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  yield hiddenPanelPromise;

  let fullscreenButton = document.getElementById("fullscreen-button");
  ok(!fullscreenButton.checked, "Fullscreen button should not be checked when not in fullscreen.")
  ok(!isFullscreenSizeMode(), "Should not be in fullscreen sizemode before we enter fullscreen.");

  BrowserFullScreen();
  yield waitForCondition(() => isFullscreenSizeMode());
  ok(fullscreenButton.checked, "Fullscreen button should be checked when in fullscreen.")

  yield startCustomizing();

  let fullscreenButtonWrapper = document.getElementById("wrapper-fullscreen-button");
  ok(fullscreenButtonWrapper.hasAttribute("itemobserves"), "Observer should be moved to wrapper");
  fullscreenButton = document.getElementById("fullscreen-button");
  ok(!fullscreenButton.hasAttribute("observes"), "Observer should be removed from button");
  ok(!fullscreenButton.checked, "Fullscreen button should no longer be checked during customization mode");

  yield endCustomizing();

  BrowserFullScreen();
  fullscreenButton = document.getElementById("fullscreen-button");
  yield waitForCondition(() => !isFullscreenSizeMode());
  ok(!fullscreenButton.checked, "Fullscreen button should not be checked when not in fullscreen.")
});
