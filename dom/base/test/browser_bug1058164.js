/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const kTimeout = 5000; // ms

/**
 * Frame script injected in the test browser that sends
 * messages when it sees the pagehide and pageshow events
 * with the persisted property set to true.
 */
function frame_script() {
  addEventListener("pageshow", (event) => {
    if (event.persisted) {
      sendAsyncMessage("test:pageshow");
    }
  });
  addEventListener("pagehide", (event) => {
    if (event.persisted) {
      sendAsyncMessage("test:pagehide");
    }
  });
}

/**
 * Return a Promise that resolves when the browser's frame
 * script sees an event of type eventType. eventType can be
 * either "pagehide" or "pageshow". Times out after kTimeout
 * milliseconds if the event is never seen.
 */
function prepareForPageEvent(browser, eventType) {
  return new Promise((resolve, reject) => {
    let mm = browser.messageManager;

    let timeoutId = setTimeout(() => {
      ok(false, "Timed out waiting for " + eventType)
      reject();
    }, kTimeout);

    mm.addMessageListener("test:" + eventType, function onSawEvent(message) {
      mm.removeMessageListener("test:" + eventType, onSawEvent);
      ok(true, "Saw " + eventType + " event in frame script.");
      clearTimeout(timeoutId);
      resolve();
    });
  });
}

/**
 * Returns a Promise that resolves when both the pagehide
 * and pageshow events have been seen from the frame script.
 */
function prepareForPageHideAndShow(browser) {
  return Promise.all([prepareForPageEvent(browser, "pagehide"),
                      prepareForPageEvent(browser, "pageshow")]);
}

/**
 * Returns a Promise that resolves when the load event for
 * aWindow fires during the capture phase.
 */
function waitForLoad(aWindow) {
  return new Promise((resolve, reject) => {
    aWindow.addEventListener("load", function onLoad(aEvent) {
      aWindow.removeEventListener("load", onLoad, true);
      resolve();
    }, true);
  });
}

/**
 * Tests that frame scripts get pageshow / pagehide events when
 * swapping browser frameloaders (which occurs when moving a tab
 * into a different window).
 */
add_task(function* test_swap_frameloader_pagevisibility_events() {
  // Inject our frame script into a new browser.
  let tab = gBrowser.addTab();
  gBrowser.selectedTab = tab;
  let mm = window.getGroupMessageManager("browsers");
  mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);
  let browser = gBrowser.selectedBrowser;

  // Swap the browser out to a new window
  let newWindow = gBrowser.replaceTabWithWindow(tab);
  // We have to wait for the window to load so we can get the selected browser
  // to listen to.
  yield waitForLoad(newWindow);
  let pageHideAndShowPromise = prepareForPageHideAndShow(newWindow.gBrowser.selectedBrowser);
  // Yield waiting for the pagehide and pageshow events.
  yield pageHideAndShowPromise;

  // Send the browser back to its original window
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  browser = newWindow.gBrowser.selectedBrowser;
  pageHideAndShowPromise = prepareForPageHideAndShow(gBrowser.selectedBrowser);
  gBrowser.swapBrowsersAndCloseOther(newTab, newWindow.gBrowser.selectedTab);

  // Yield waiting for the pagehide and pageshow events.
  yield pageHideAndShowPromise;

  gBrowser.removeTab(gBrowser.selectedTab);
});
