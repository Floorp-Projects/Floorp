/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let initialPageZoom = ZoomManager.zoom;

add_task(function() {
  info("Check zoom out button existence and functionality");

  is(initialPageZoom, 1, "Initial zoom factor should be 1");

  yield PanelUI.show();
  info("Menu panel was opened");

  let zoomOutButton = document.getElementById("zoom-out-button");
  ok(zoomOutButton, "Zoom out button exists in Panel Menu");

  zoomOutButton.click();
  let pageZoomLevel = Math.round(ZoomManager.zoom * 100);

  let zoomResetButton = document.getElementById("zoom-reset-button");
  let expectedZoomLevel = parseInt(zoomResetButton.getAttribute("label"), 10);
  ok(pageZoomLevel < 100 && pageZoomLevel == expectedZoomLevel, "Page zoomed out correctly");

  // close the panel
  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHiddenPromise;
  info("Menu panel was closed");
});

add_task(function asyncCleanup() {
  // reset zoom level
  ZoomManager.zoom = initialPageZoom;
  info("Zoom level was restored");
});
