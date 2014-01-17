/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTimeoutInMS = 20000;

// Bug 934951 - Zoom controls percentage label doesn't update when it's in the toolbar and you navigate.
add_task(function() {
  CustomizableUI.addWidgetToArea("zoom-controls", CustomizableUI.AREA_NAVBAR);
  let tab1 = gBrowser.addTab("about:mozilla");
  let tab2 = gBrowser.addTab("about:newtab");
  gBrowser.selectedTab = tab1;
  let zoomResetButton = document.getElementById("zoom-reset-button");

  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:mozilla");
  let zoomChangePromise = promiseObserverNotification("browser-fullZoom:zoomChange");
  FullZoom.enlarge();
  yield zoomChangePromise;
  is(parseInt(zoomResetButton.label, 10), 110, "Zoom is changed to 110% for about:mozilla");

  let tabSelectPromise = promiseTabSelect();
  gBrowser.selectedTab = tab2;
  yield tabSelectPromise;
  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:newtab");

  gBrowser.selectedTab = tab1;
  let zoomResetPromise = promiseObserverNotification("browser-fullZoom:zoomReset");
  FullZoom.reset();
  yield zoomResetPromise;
  is(parseInt(zoomResetButton.label, 10), 100, "Default zoom is 100% for about:mozilla");

  CustomizableUI.reset();
  gBrowser.removeTab(tab2);
  gBrowser.removeTab(tab1);
});

function promiseObserverNotification(aObserver) {
  let deferred = Promise.defer();
  function notificationCallback(e) {
    Services.obs.removeObserver(notificationCallback, aObserver, false);
    clearTimeout(timeoutId);
    deferred.resolve();
  };
  let timeoutId = setTimeout(() => {
    Services.obs.removeObserver(notificationCallback, aObserver, false);
    deferred.reject("Notification '" + aObserver + "' did not happen within 20 seconds.");
  }, kTimeoutInMS);
  Services.obs.addObserver(notificationCallback, aObserver, false);
  return deferred.promise;
}

function promiseTabSelect() {
  let deferred = Promise.defer();
  let container = window.gBrowser.tabContainer;
  let timeoutId = setTimeout(() => {
    container.removeEventListener("TabSelect", callback);
    deferred.reject("TabSelect did not happen within 20 seconds");
  }, kTimeoutInMS);
  function callback(e) {
    container.removeEventListener("TabSelect", callback);
    clearTimeout(timeoutId);
    executeSoon(deferred.resolve);
  };
  container.addEventListener("TabSelect", callback);
  return deferred.promise;
}
