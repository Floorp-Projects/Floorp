/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Create a new window, then move the stop/reload button to the menu and check both windows have
// customizationchange events fire on the toolbox:
add_task(async function() {
  let newWindow = await openAndLoadWindow();
  let otherToolbox = newWindow.gNavToolbox;

  let handlerCalledCount = 0;
  let handler = ev => {
    handlerCalledCount++;
  };

  let stopReloadButton = document.getElementById("stop-reload-button");

  gNavToolbox.addEventListener("customizationchange", handler);
  otherToolbox.addEventListener("customizationchange", handler);

  await gCustomizeMode.addToPanel(stopReloadButton);

  is(handlerCalledCount, 2, "Should be called for both windows.");

  handlerCalledCount = 0;
  gCustomizeMode.addToToolbar(stopReloadButton);
  is(handlerCalledCount, 2, "Should be called for both windows.");

  gNavToolbox.removeEventListener("customizationchange", handler);
  otherToolbox.removeEventListener("customizationchange", handler);

  await promiseWindowClosed(newWindow);
});

add_task(async function asyncCleanup() {
  await resetCustomization();
});
