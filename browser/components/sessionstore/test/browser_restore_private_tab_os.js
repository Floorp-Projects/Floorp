/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_URI =
  "https://example.com/" +
  "browser/browser/components/sessionstore/test/empty.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Set the pref to true so we know exactly how many tabs should be restoring at
      // any given time. This guarantees that a finishing load won't start another.
      ["browser.sessionstore.restore_on_demand", true],
      // Don't restore tabs lazily.
      ["browser.sessionstore.restore_tabs_lazily", true],
      // don't preload tabs so we don't have extra XULFrameLoaderCreated events
      // firing
      ["browser.newtab.preload", false],
      // We want changes to this pref to be reverted at the end of the test
      ["browser.tabs.remote.useOriginAttributesInRemoteType", false],
    ],
  });

  var expectedRemoteTypeWithOA = gFissionBrowser
    ? "webIsolated=https://example.com^privateBrowsingId=1"
    : "web";
  add_task(async function testWithOA() {
    Services.prefs.setBoolPref(
      "browser.tabs.remote.useOriginAttributesInRemoteType",
      true
    );
    await testRestore(expectedRemoteTypeWithOA);
  });
  if (gFissionBrowser) {
    add_task(async function testWithoutOA() {
      Services.prefs.setBoolPref(
        "browser.tabs.remote.useOriginAttributesInRemoteType",
        false
      );
      await testRestore("webIsolated=https://example.com");
    });
  }
});

async function testRestore(aExpectedRemoteType) {
  // Clear the list of closed windows.
  forgetClosedWindows();

  // Create a new private window
  let win = await promiseNewWindowLoaded({ private: true });

  // Create a new private tab
  let tab = BrowserTestUtils.addTab(win.gBrowser, TEST_URI);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);
  await TabStateFlusher.flush(browser);

  // Ensure that closed tabs in a private windows can be restored.
  win.gBrowser.removeTab(tab);
  is(ss.getClosedTabCount(win), 1, "there is a single tab to restore");

  tab = SessionStore.undoCloseTab(win, 0);
  info(`Undo close tab`);
  browser = tab.linkedBrowser;
  await promiseTabRestored(tab);
  info(`Private tab restored`);

  is(browser.remoteType, aExpectedRemoteType, "correct remote type");

  await BrowserTestUtils.closeWindow(win);

  // Cleanup
  info("Forgetting closed tabs");
  while (ss.getClosedTabCount(window)) {
    ss.forgetClosedTab(window, 0);
  }
}
