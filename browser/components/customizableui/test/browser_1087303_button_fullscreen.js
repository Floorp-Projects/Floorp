/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check fullscreen button existence and functionality");

  yield PanelUI.show();

  let fullscreenButton = document.getElementById("fullscreen-button");
  ok(fullscreenButton, "Fullscreen button appears in Panel Menu");

  let fullscreenPromise = promiseFullscreenChange();
  fullscreenButton.click();
  yield fullscreenPromise;

  ok(window.fullScreen, "Fullscreen mode was opened");

  // exit full screen mode
  fullscreenPromise = promiseFullscreenChange();
  window.fullScreen = !window.fullScreen;
  yield fullscreenPromise;

  ok(!window.fullScreen, "Successfully exited fullscreen");
});

function promiseFullscreenChange() {
  let deferred = Promise.defer();
  info("Wait for fullscreen change");

  let timeoutId = setTimeout(() => {
    window.removeEventListener("fullscreen", onFullscreenChange, true);
    deferred.reject("Fullscreen change did not happen within " + 20000 + "ms");
  }, 20000);

  function onFullscreenChange(event) {
    clearTimeout(timeoutId);
    window.removeEventListener("fullscreen", onFullscreenChange, true);
    info("Fullscreen event received");
    deferred.resolve();
  }
  window.addEventListener("fullscreen", onFullscreenChange, true);
  return deferred.promise;
}
