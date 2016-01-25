/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialPageZoom = ZoomManager.zoom;

add_task(function*() {
  info("Check zoom reset button existence and functionality");

  is(initialPageZoom, 1, "Page zoom reset correctly");
  ZoomManager.zoom = 0.5;
  yield PanelUI.show();
  info("Menu panel was opened");

  let zoomResetButton = document.getElementById("zoom-reset-button");
  ok(zoomResetButton, "Zoom reset button exists in Panel Menu");

  zoomResetButton.click();
  let pageZoomLevel = Math.floor(ZoomManager.zoom * 100);
  let expectedZoomLevel = 100;
  let buttonZoomLevel = parseInt(zoomResetButton.getAttribute("label"), 10);
  is(pageZoomLevel, expectedZoomLevel, "Page zoom reset correctly");
  is(pageZoomLevel, buttonZoomLevel, "Button displays the correct zoom level");

  // close the panel
  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHiddenPromise;
  info("Menu panel was closed");
});

add_task(function* asyncCleanup() {
  // reset zoom level
  ZoomManager.zoom = initialPageZoom;
  info("Zoom level was restored");
});
