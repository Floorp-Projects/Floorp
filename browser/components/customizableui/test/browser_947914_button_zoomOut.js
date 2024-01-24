/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function () {
  info("Check zoom out button existence and functionality");

  is(ZoomManager.zoom, 1, "Initial zoom factor should be 1");

  CustomizableUI.addWidgetToArea(
    "zoom-controls",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  registerCleanupFunction(async () => {
    CustomizableUI.reset();
    let gContentPrefs = Cc["@mozilla.org/content-pref/service;1"].getService(
      Ci.nsIContentPrefService2
    );
    let gLoadContext = Cu.createLoadContext();
    await new Promise(resolve => {
      gContentPrefs.removeByName(window.FullZoom.name, gLoadContext, {
        handleResult() {},
        handleCompletion() {
          resolve();
        },
      });
    });
  });

  await waitForOverflowButtonShown();

  await document.getElementById("nav-bar").overflowable.show();
  info("Menu panel was opened");

  let zoomOutButton = document.getElementById("zoom-out-button");
  ok(zoomOutButton, "Zoom out button exists in Panel Menu");

  zoomOutButton.click();
  let pageZoomLevel = Math.round(ZoomManager.zoom * 100);
  console.log("Page zoom level is: ", pageZoomLevel);

  let zoomResetButton = document.getElementById("zoom-reset-button");
  await TestUtils.waitForCondition(() => {
    console.log(
      "Current zoom is ",
      parseInt(zoomResetButton.getAttribute("label"), 10)
    );
    return parseInt(zoomResetButton.getAttribute("label"), 10) == pageZoomLevel;
  });

  Assert.less(pageZoomLevel, 100, "Page zoomed out correctly");

  // close the panel
  let panelHiddenPromise = promiseOverflowHidden(window);
  document.getElementById("widget-overflow").hidePopup();
  await panelHiddenPromise;
  info("Menu panel was closed");
});
