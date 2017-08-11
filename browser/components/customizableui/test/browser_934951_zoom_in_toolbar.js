/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTimeoutInMS = 20000;

// Bug 934951 - Zoom controls percentage label doesn't update when it's in the toolbar and you navigate.
add_task(async function() {
  CustomizableUI.addWidgetToArea("zoom-controls", CustomizableUI.AREA_NAVBAR);
  let tab1 = BrowserTestUtils.addTab(gBrowser, "about:mozilla");
  await BrowserTestUtils.browserLoaded(tab1.linkedBrowser);
  let tab2 = BrowserTestUtils.addTab(gBrowser, "about:robots");
  await BrowserTestUtils.browserLoaded(tab2.linkedBrowser);
  gBrowser.selectedTab = tab1;
  let zoomResetButton = document.getElementById("zoom-reset-button");

  registerCleanupFunction(() => {
    info("Cleaning up.");
    CustomizableUI.reset();
    gBrowser.removeTab(tab2);
    gBrowser.removeTab(tab1);
  });

  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:mozilla");
  let zoomChangePromise = BrowserTestUtils.waitForEvent(window, "FullZoomChange");
  FullZoom.enlarge();
  await zoomChangePromise;
  is(parseInt(zoomResetButton.label, 10), 110, "Zoom is changed to 110% for about:mozilla");

  let tabSelectPromise = promiseObserverNotification("browser-fullZoom:location-change");
  gBrowser.selectedTab = tab2;
  await tabSelectPromise;
  await new Promise(resolve => executeSoon(resolve));
  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:robots");

  gBrowser.selectedTab = tab1;
  let zoomResetPromise = BrowserTestUtils.waitForEvent(window, "FullZoomChange");
  FullZoom.reset();
  await zoomResetPromise;
  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:mozilla");

  // Test zoom label updates while navigating pages in the same tab.
  FullZoom.enlarge();
  await zoomChangePromise;
  is(parseInt(zoomResetButton.label, 10), 110, "Zoom is changed to 110% for about:mozilla");
  let attributeChangePromise = promiseAttributeMutation(zoomResetButton, "label", (v) => {
    return parseInt(v, 10) == 100;
  });
  await promiseTabLoadEvent(tab1, "about:home");
  await attributeChangePromise;
  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:home");
  await promiseTabHistoryNavigation(-1, function() {
    return parseInt(zoomResetButton.label, 10) == 110;
  });
  is(parseInt(zoomResetButton.label, 10), 110, "Zoom is still 110% for about:mozilla");
  FullZoom.reset();
});

function promiseObserverNotification(aObserver) {
  return new Promise((resolve, reject) => {
    function notificationCallback(e) {
      Services.obs.removeObserver(notificationCallback, aObserver);
      clearTimeout(timeoutId);
      resolve();
    }
    let timeoutId = setTimeout(() => {
      Services.obs.removeObserver(notificationCallback, aObserver);
      reject("Notification '" + aObserver + "' did not happen within 20 seconds.");
    }, kTimeoutInMS);
    Services.obs.addObserver(notificationCallback, aObserver);
  });
}

function promiseTabSelect() {
  return new Promise((resolve, reject) => {
    let container = window.gBrowser.tabContainer;
    let timeoutId = setTimeout(() => {
      container.removeEventListener("TabSelect", callback);
      reject("TabSelect did not happen within 20 seconds");
    }, kTimeoutInMS);
    function callback(e) {
      container.removeEventListener("TabSelect", callback);
      clearTimeout(timeoutId);
      executeSoon(resolve);
    }
    container.addEventListener("TabSelect", callback);
  });
}
