/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialPageZoom = ZoomManager.zoom;
const kTimeoutInMS = 20000;

async function testZoomButtonAppearsAndDisappearsBasedOnZoomChanges(zoomEventType) {
  let tab = await BrowserTestUtils.openNewForegroundTab({ gBrowser, waitForStateStop: true });

  info("Running this test with " + zoomEventType.substring(0, 9));
  info("Confirm whether the browser zoom is set to the default level");
  is(initialPageZoom, 1, "Page zoom is set to default (100%)");
  let zoomResetButton = document.getElementById("urlbar-zoom-button");
  is(zoomResetButton.hidden, true, "Zoom reset button is currently hidden");

  info("Change zoom and confirm zoom button appears");
  let labelUpdatePromise = BrowserTestUtils.waitForAttribute("label", zoomResetButton);
  FullZoom.enlarge();
  await labelUpdatePromise;
  info("Zoom increased to " + Math.floor(ZoomManager.zoom * 100) + "%");
  is(zoomResetButton.hidden, false, "Zoom reset button is now visible");
  let pageZoomLevel = Math.floor(ZoomManager.zoom * 100);
  let expectedZoomLevel = 110;
  let buttonZoomLevel = parseInt(zoomResetButton.getAttribute("label"), 10);
  is(buttonZoomLevel, expectedZoomLevel, ("Button label updated successfully to " + Math.floor(ZoomManager.zoom * 100) + "%"));

  let zoomResetPromise = BrowserTestUtils.waitForEvent(window, zoomEventType);
  zoomResetButton.click();
  await zoomResetPromise;
  pageZoomLevel = Math.floor(ZoomManager.zoom * 100);
  expectedZoomLevel = 100;
  is(pageZoomLevel, expectedZoomLevel, "Clicking zoom button successfully resets browser zoom to 100%");
  is(zoomResetButton.hidden, true, "Zoom reset button returns to being hidden");

  await BrowserTestUtils.removeTab(tab);
}

add_task(async function() {
  await testZoomButtonAppearsAndDisappearsBasedOnZoomChanges("FullZoomChange");
  await SpecialPowers.pushPrefEnv({"set": [["browser.zoom.full", false]]});
  await testZoomButtonAppearsAndDisappearsBasedOnZoomChanges("TextZoomChange");
  await SpecialPowers.pushPrefEnv({"set": [["browser.zoom.full", true]]});
});

add_task(async function() {
  info("Confirm that URL bar zoom button doesn't appear when customizable zoom widget is added to toolbar");
  CustomizableUI.addWidgetToArea("zoom-controls", CustomizableUI.AREA_NAVBAR);
  let zoomCustomizableWidget = document.getElementById("zoom-reset-button");
  let zoomResetButton = document.getElementById("urlbar-zoom-button");
  let zoomChangePromise = BrowserTestUtils.waitForEvent(window, "FullZoomChange");
  FullZoom.enlarge();
  await zoomChangePromise;
  is(zoomResetButton.hidden, true, "URL zoom button remains hidden despite zoom increase");
  is(parseInt(zoomCustomizableWidget.label, 10), 110, "Customizable zoom widget's label has updated to " + zoomCustomizableWidget.label);
});

add_task(async function asyncCleanup() {
  // reset zoom level and customizable widget
  ZoomManager.zoom = initialPageZoom;
  is(ZoomManager.zoom, 1, "Zoom level was restored");
  if (document.getElementById("zoom-controls")) {
    CustomizableUI.removeWidgetFromArea("zoom-controls", CustomizableUI.AREA_NAVBAR);
    ok(!document.getElementById("zoom-controls"), "Customizable zoom widget removed from toolbar");
  }
});
