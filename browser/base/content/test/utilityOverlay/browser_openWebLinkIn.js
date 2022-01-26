/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const EXAMPLE_URL = "https://example.org/";
add_task(async function test_open_tab() {
  const waitForTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    EXAMPLE_URL
  );
  openWebLinkIn(EXAMPLE_URL, "tab");

  const tab = await waitForTabPromise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_open_window() {
  const waitForWindowPromise = BrowserTestUtils.waitForNewWindow();
  openWebLinkIn(EXAMPLE_URL, "window");

  const win = await waitForWindowPromise;
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_open_private_window() {
  const waitForWindowPromise = BrowserTestUtils.waitForNewWindow();
  openWebLinkIn(EXAMPLE_URL, "window", {
    private: true,
  });
  const win = await waitForWindowPromise;
  ok(
    PrivateBrowsingUtils.isBrowserPrivate(win),
    "The new window is a private window."
  );

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_open_private_tab_from_private_window() {
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  const waitForTabPromise = BrowserTestUtils.waitForNewTab(
    privateWindow.gBrowser,
    EXAMPLE_URL
  );
  privateWindow.openWebLinkIn(EXAMPLE_URL, "tab");
  const tab = await waitForTabPromise;
  ok(
    PrivateBrowsingUtils.isBrowserPrivate(tab),
    "The new tab was opened in a private browser."
  );
  await BrowserTestUtils.closeWindow(privateWindow);
});
