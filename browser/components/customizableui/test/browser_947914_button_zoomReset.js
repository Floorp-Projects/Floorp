/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check zoom reset button existence and functionality");

  ZoomManager.zoom = 0.5;
  yield PanelUI.show();

  let zoomResetButton = document.getElementById("zoom-reset-button");
  ok(zoomResetButton, "Zoom reset button exists in Panel Menu");

  zoomResetButton.click();
  let pageZoomLevel = parseInt(ZoomManager.zoom * 100);
  let expectedZoomLevel = parseInt(zoomResetButton.getAttribute("label"), 10);
  ok(pageZoomLevel == expectedZoomLevel && pageZoomLevel == 100, "Page zoom reset correctly");

  // close the panel
  yield PanelUI.hide();
});
