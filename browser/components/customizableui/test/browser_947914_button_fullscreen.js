/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check fullscreen button existence and functionality");
  yield PanelUI.show();

  let fullscreenButton = document.getElementById("fullscreen-button");
  ok(fullscreenButton, "Fullscreen button appears in Panel Menu");
  fullscreenButton.click();

  yield waitForCondition(function() window.fullScreen);
  ok(window.fullScreen, "Fullscreen mode was opened");

  // exit full screen mode
  window.fullScreen = !window.fullScreen;
  yield waitForCondition(function() !window.fullScreen);
});
