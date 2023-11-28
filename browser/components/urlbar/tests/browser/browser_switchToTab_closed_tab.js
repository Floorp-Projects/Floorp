/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This tests that the "switch to tab" result in the urlbar
 * will still load the relevant URL if the tab being referred
 * to does not exist.
 */

"use strict";

const { UrlbarProviderOpenTabs } = ChromeUtils.importESModule(
  "resource:///modules/UrlbarProviderOpenTabs.sys.mjs"
);

async function openPagesCount() {
  let conn = await PlacesUtils.promiseLargeCacheDBConnection();
  let res = await conn.executeCached(
    "SELECT COUNT(*) AS count FROM moz_openpages_temp;"
  );
  return res[0].getResultByName("count");
}

add_task(async function test_switchToTab_tab_closed() {
  let testURL =
    "https://example.org/browser/browser/components/urlbar/tests/browser/dummy_page.html";

  // Open a blank tab to start the test from.
  let testTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org"
  );

  // Check how many currently open pages are registered
  let pagesCount = await openPagesCount();

  // Register an open tab that does not exist, this simulates a tab being
  // opened but not properly unregistered.
  await UrlbarProviderOpenTabs.registerOpenTab(
    testURL,
    gBrowser.contentPrincipal.userContextId,
    false
  );

  Assert.equal(
    await openPagesCount(),
    pagesCount + 1,
    "We registered a new open page"
  );

  let tabOpenPromise = BrowserTestUtils.waitForEvent(
    gBrowser.tabContainer,
    "TabOpen",
    false
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: testURL,
  });

  // The first result is the heuristic, the second will be the switch to tab.
  let element = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
  EventUtils.synthesizeMouseAtCenter(element, {}, window);

  await tabOpenPromise;
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    testURL
  );

  Assert.equal(
    gBrowser.selectedBrowser.currentURI.spec,
    testURL,
    "We opened a new tab with the URL"
  );

  gBrowser.removeTab(gBrowser.selectedTab);

  Assert.equal(
    await openPagesCount(),
    pagesCount,
    "We unregistered the orphaned open tab"
  );

  gBrowser.removeTab(testTab);
  await PlacesUtils.history.clear();
});
