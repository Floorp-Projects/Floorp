/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Create a new window, then move the home button to the menu and check both windows have
// customizationchange events fire on the toolbox:
add_task(function() {
  let newWindow = yield openAndLoadWindow();
  let otherToolbox = newWindow.gNavToolbox;

  let handlerCalledCount = 0;
  let handler = (ev) => {
    handlerCalledCount++;
  };

  let homeButton = document.getElementById("home-button");

  gNavToolbox.addEventListener("customizationchange", handler);
  otherToolbox.addEventListener("customizationchange", handler);

  gCustomizeMode.addToPanel(homeButton);

  is(handlerCalledCount, 2, "Should be called for both windows.");

  // If the test is run in isolation and the panel has never been open,
  // the button will be in the palette. Deal with this case:
  if (homeButton.parentNode.id == "BrowserToolbarPalette") {
    yield PanelUI.ensureReady();
    isnot(homeButton.parentNode.id, "BrowserToolbarPalette", "Home button should now be in panel");
  }

  handlerCalledCount = 0;
  gCustomizeMode.addToToolbar(homeButton);
  is(handlerCalledCount, 2, "Should be called for both windows.");

  gNavToolbox.removeEventListener("customizationchange", handler);
  otherToolbox.removeEventListener("customizationchange", handler);

  yield promiseWindowClosed(newWindow);
});

add_task(function asyncCleanup() {
  yield resetCustomization();
});

