/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests that switch to tab avoids closing a window with only
 * a single blank tab.
 */

"use strict";

add_task(async function test_switchToTab_avoids_closing_window() {
  let testURL =
    "https://example.org/browser/browser/components/urlbar/tests/browser/dummy_page.html";

  // Open the base tab
  let baseTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testURL);

  if (baseTab.linkedBrowser.currentURI.spec == "about:blank") {
    return;
  }

  // Open a new window to start the test from.
  let newWindow = await BrowserTestUtils.openNewBrowserWindow();

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: newWindow,
    value: "dummy",
  });

  // The first result is the heuristic, the second will be the switch to tab.
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(newWindow, 1);
  EventUtils.synthesizeMouseAtCenter(element, {}, newWindow);

  await UrlbarTestUtils.promisePopupClose(newWindow);

  Assert.equal(newWindow.closed, false, "Should have kept the window open");

  gBrowser.removeTab(baseTab);
  BrowserTestUtils.closeWindow(newWindow);
});
