/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

let gZoomResetButton;

async function waitForZoom(zoom) {
  if (parseInt(gZoomResetButton.label) == zoom) {
    return;
  }
  await promiseAttributeMutation(gZoomResetButton, "label", v => {
    return parseInt(v) == zoom;
  });
}

// Bug 934951 - Zoom controls percentage label doesn't update when it's in the toolbar and you navigate.
add_task(async function() {
  CustomizableUI.addWidgetToArea("zoom-controls", CustomizableUI.AREA_NAVBAR);
  gZoomResetButton = document.getElementById("zoom-reset-button");
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:mozilla");
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  gBrowser.selectedTab = tab1;

  registerCleanupFunction(() => {
    info("Cleaning up.");
    CustomizableUI.reset();
    gBrowser.removeTab(tab2);
    gBrowser.removeTab(tab1);
  });

  is(parseInt(gZoomResetButton.label, 10), 100, "Default zoom is 100% for about:mozilla");
  FullZoom.enlarge();
  await waitForZoom(110);
  is(parseInt(gZoomResetButton.label, 10), 110, "Zoom is changed to 110% for about:mozilla");

  let tabSelectPromise = TestUtils.topicObserved("browser-fullZoom:location-change");
  gBrowser.selectedTab = tab2;
  await tabSelectPromise;
  await waitForZoom(100);
  is(parseInt(gZoomResetButton.label, 10), 100, "Default zoom is 100% for about:robots");

  gBrowser.selectedTab = tab1;
  await waitForZoom(110);
  FullZoom.reset();
  await waitForZoom(100);
  is(parseInt(gZoomResetButton.label, 10), 100, "Default zoom is 100% for about:mozilla");

  // Test zoom label updates while navigating pages in the same tab.
  FullZoom.enlarge();
  await waitForZoom(110);
  is(parseInt(gZoomResetButton.label, 10), 110, "Zoom is changed to 110% for about:mozilla");
  await promiseTabLoadEvent(tab1, "about:home");
  await waitForZoom(100);
  is(parseInt(gZoomResetButton.label, 10), 100, "Default zoom is 100% for about:home");
  gBrowser.selectedBrowser.goBack();
  await waitForZoom(110);
  is(parseInt(gZoomResetButton.label, 10), 110, "Zoom is still 110% for about:mozilla");
  FullZoom.reset();
});

