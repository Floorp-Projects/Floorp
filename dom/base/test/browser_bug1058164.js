/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

/**
 * Returns a Promise that resolves when it sees a pageshow and
 * pagehide events in a particular order, where each event must
 * have the persisted property set to true. Will cause a test
 * failure to be logged if it sees an event out of order.
 *
 * @param browser (<xul:browser>)
 *        The browser to expect the events from.
 * @param expectedOrder (array)
 *        An array of strings describing what order the pageshow
 *        and pagehide events should be in.
 *        Example:
 *        ["pageshow", "pagehide", "pagehide", "pageshow"]
 * @returns Promise
 */
function prepareForVisibilityEvents(browser, expectedOrder) {
  return new Promise((resolve) => {
    let order = [];

    let checkSatisfied = () => {
      if (order.length < expectedOrder.length) {
        // We're still waiting...
        return;
      } else {
        browser.removeEventListener("pagehide", eventListener);
        browser.removeEventListener("pageshow", eventListener);

        for (let i = 0; i < expectedOrder.length; ++i) {
          is(order[i], expectedOrder[i], "Got expected event");
        }
        resolve();
      }
    };

    let eventListener = (e) => {
      if (e.persisted) {
        order.push(e.type);
        checkSatisfied();
      }
    };

    browser.addEventListener("pagehide", eventListener);
    browser.addEventListener("pageshow", eventListener);
  });
}

/**
 * Tests that frame scripts get pageshow / pagehide events when
 * swapping browser frameloaders (which occurs when moving a tab
 * into a different window).
 */
add_task(function* test_swap_frameloader_pagevisibility_events() {
  // Load a new tab that we'll tear out...
  let tab = BrowserTestUtils.addTab(gBrowser, PAGE);
  gBrowser.selectedTab = tab;
  let firstBrowser = tab.linkedBrowser;
  yield BrowserTestUtils.browserLoaded(firstBrowser);

  // Swap the browser out to a new window
  let newWindow = gBrowser.replaceTabWithWindow(tab);

  // We have to wait for the window to load so we can get the selected browser
  // to listen to.
  yield BrowserTestUtils.waitForEvent(newWindow, "load");
  let newWindowBrowser = newWindow.gBrowser.selectedBrowser;

  // Wait for the expected pagehide and pageshow events on the initial browser
  yield prepareForVisibilityEvents(newWindowBrowser, ["pagehide", "pageshow"]);

  // Now let's send the browser back to the original window

  // First, create a new, empty browser tab to replace the window with
  let newTab = BrowserTestUtils.addTab(gBrowser);
  gBrowser.selectedTab = newTab;
  let emptyBrowser = newTab.linkedBrowser;

  // Wait for that initial browser to show its pageshow event so that we
  // don't confuse it with the other expected events. Note that we can't
  // use BrowserTestUtils.waitForEvent here because we're using the
  // e10s add-on shims in the e10s-case. I'm doing this because I couldn't
  // find a way of sending down a frame script to the newly opened windows
  // and tabs fast enough to attach the event handlers before they were
  // fired.
  yield new Promise((resolve) => {
    emptyBrowser.addEventListener("pageshow", function() {
      resolve();
    }, {once: true});
  });

  // The empty tab we just added show now fire a pagehide as its replaced,
  // and a pageshow once the swap is finished.
  let emptyBrowserPromise =
    prepareForVisibilityEvents(emptyBrowser, ["pagehide", "pageshow"]);

  gBrowser.swapBrowsersAndCloseOther(newTab, newWindow.gBrowser.selectedTab);

  yield emptyBrowserPromise;

  gBrowser.removeTab(gBrowser.selectedTab);
});
