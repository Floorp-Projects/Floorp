/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that clicking "Open a Private Window" button on about:privatebrowsing
// opens a new private window.
add_task(function* test_aboutprivatebrowsing_open_window() {
  let win = yield BrowserTestUtils.openNewBrowserWindow();
  let browser = win.gBrowser.addTab("about:privatebrowsing").linkedBrowser;
  yield BrowserTestUtils.browserLoaded(browser);

  let promiseWindowOpened = BrowserTestUtils.domWindowOpened();
  yield ContentTask.spawn(browser, {}, function* () {
    content.document.getElementById("startPrivateBrowsing").click();
  });
  let private_window = yield promiseWindowOpened;

  ok(PrivateBrowsingUtils.isWindowPrivate(private_window),
     "The opened window should be private.");

  // Cleanup.
  yield BrowserTestUtils.closeWindow(private_window);
  yield BrowserTestUtils.closeWindow(win);
});
