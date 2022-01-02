/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialPageZoom = ZoomManager.zoom;

add_task(async function() {
  info("Check zoom reset button existence and functionality");
  is(initialPageZoom, 1, "Page zoom reset correctly");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "http://example.com", waitForLoad: true },
    async function(browser) {
      CustomizableUI.addWidgetToArea(
        "zoom-controls",
        CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
      );

      registerCleanupFunction(() => CustomizableUI.reset());

      CustomizableUI.addWidgetToArea(
        "zoom-controls",
        CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
      );

      await waitForOverflowButtonShown();

      {
        let zoomChange = BrowserTestUtils.waitForEvent(
          gBrowser,
          "FullZoomChange"
        );
        ZoomManager.zoom = 0.5;
        await zoomChange;
      }

      await document.getElementById("nav-bar").overflowable.show();
      info("Menu panel was opened");

      let zoomResetButton = document.getElementById("zoom-reset-button");
      ok(zoomResetButton, "Zoom reset button exists in Panel Menu");

      let zoomChange = BrowserTestUtils.waitForEvent(
        gBrowser,
        "FullZoomChange"
      );
      zoomResetButton.click();
      await zoomChange;

      let pageZoomLevel = Math.floor(ZoomManager.zoom * 100);
      let expectedZoomLevel = 100;
      let buttonZoomLevel = parseInt(zoomResetButton.getAttribute("label"), 10);
      is(pageZoomLevel, expectedZoomLevel, "Page zoom reset correctly");
      is(
        pageZoomLevel,
        buttonZoomLevel,
        "Button displays the correct zoom level"
      );

      // close the panel
      let panelHiddenPromise = promiseOverflowHidden(window);
      document.getElementById("widget-overflow").hidePopup();
      await panelHiddenPromise;
      info("Menu panel was closed");
    }
  );
});

add_task(async function asyncCleanup() {
  // reset zoom level
  ZoomManager.zoom = initialPageZoom;
  info("Zoom level was restored");
});
