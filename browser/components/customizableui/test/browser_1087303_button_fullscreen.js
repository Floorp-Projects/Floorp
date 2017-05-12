/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({set: [["browser.photon.structure.enabled", false]]});
  info("Check fullscreen button existence and functionality");

  await PanelUI.show();

  let fullscreenButton = document.getElementById("fullscreen-button");
  ok(fullscreenButton, "Fullscreen button appears in Panel Menu");

  let fullscreenPromise = promiseFullscreenChange();
  fullscreenButton.click();
  await fullscreenPromise;

  ok(window.fullScreen, "Fullscreen mode was opened");

  // exit full screen mode
  fullscreenPromise = promiseFullscreenChange();
  window.fullScreen = !window.fullScreen;
  await fullscreenPromise;

  ok(!window.fullScreen, "Successfully exited fullscreen");
});

function promiseFullscreenChange() {
  return new Promise((resolve, reject) => {
    info("Wait for fullscreen change");

    let timeoutId = setTimeout(() => {
      window.removeEventListener("fullscreen", onFullscreenChange, true);
      reject("Fullscreen change did not happen within " + 20000 + "ms");
    }, 20000);

    function onFullscreenChange(event) {
      clearTimeout(timeoutId);
      window.removeEventListener("fullscreen", onFullscreenChange, true);
      info("Fullscreen event received");
      resolve();
    }
    window.addEventListener("fullscreen", onFullscreenChange, true);
  });
}
