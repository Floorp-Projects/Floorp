/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Stolen from https://searchfox.org/mozilla-central/source/browser/base/content/test/popups/browser_popup_close_main_window.js
// When calling this function, the main window where the test runs will be
// hidden from various APIs, so that they won't be able to find it. This makes
// it possible to test some behaviors when only private windows are present.
function concealMainWindow() {
  info("Concealing main window.");
  let oldWinType = document.documentElement.getAttribute("windowtype");
  // Check if we've already done this to allow calling multiple times:
  if (oldWinType != "navigator:testrunner") {
    // Make the main test window not count as a browser window any longer
    document.documentElement.setAttribute("windowtype", "navigator:testrunner");
    BrowserWindowTracker.untrackForTestsOnly(window);

    registerCleanupFunction(() => {
      info("Unconcealing the main window in the cleanup phase.");
      BrowserWindowTracker.track(window);
      document.documentElement.setAttribute("windowtype", oldWinType);
    });
  }
}

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

add_task(async function test_open_non_private_tab_from_private_window() {
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // Opening this tab from the private window should open it in the non-private window.
  const waitForTabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    EXAMPLE_URL
  );

  privateWindow.openWebLinkIn(EXAMPLE_URL, "tab", { forceNonPrivate: true });

  const nonPrivateTab = await waitForTabPromise;
  ok(
    !PrivateBrowsingUtils.isBrowserPrivate(nonPrivateTab),
    "The new window isn't a private window."
  );

  BrowserTestUtils.removeTab(nonPrivateTab);
  await BrowserTestUtils.closeWindow(privateWindow);
});

add_task(async function test_open_non_private_tab_from_only_private_window() {
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  // In this test we'll hide the existing window from window trackers, because
  // we want to test that we open a new window when there's only a private
  // window.
  concealMainWindow();

  // Opening this tab from the private window should open it in a new non-private window.
  const waitForWindowPromise = BrowserTestUtils.waitForNewWindow({
    url: EXAMPLE_URL,
  });
  privateWindow.openWebLinkIn(EXAMPLE_URL, "tab", { forceNonPrivate: true });

  const nonPrivateWindow = await waitForWindowPromise;
  ok(
    !PrivateBrowsingUtils.isBrowserPrivate(nonPrivateWindow),
    "The new window isn't a private window."
  );

  await BrowserTestUtils.closeWindow(nonPrivateWindow);
  await BrowserTestUtils.closeWindow(privateWindow);
});
