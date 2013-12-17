/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Observers should be disabled when in customization mode.
add_task(function() {
  BrowserFullScreen();
  let shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  yield shownPanelPromise;

  let fullscreenButton = document.getElementById("fullscreen-button");
  ok(fullscreenButton.checked, "Fullscreen button should be checked when in fullscreen.")

  yield startCustomizing();

  let fullscreenButtonWrapper = document.getElementById("wrapper-fullscreen-button");
  ok(fullscreenButtonWrapper.hasAttribute("itemobserves"), "Observer should be moved to wrapper");
  fullscreenButton = document.getElementById("fullscreen-button");
  ok(!fullscreenButton.hasAttribute("observes"), "Observer should be removed from button");
  ok(!fullscreenButton.checked, "Fullscreen button should no longer be checked during customization mode");

  yield endCustomizing();
  BrowserFullScreen();
  shownPanelPromise = promisePanelShown(window);
  PanelUI.toggle({type: "command"});
  yield shownPanelPromise;

  fullscreenButton = document.getElementById("fullscreen-button");
  ok(!fullscreenButton.checked, "Fullscreen button should not be checked when not in fullscreen.")

  let hiddenPanelPromise = promisePanelHidden(window);
  PanelUI.toggle({type: "command"});
  yield hiddenPanelPromise;
});
