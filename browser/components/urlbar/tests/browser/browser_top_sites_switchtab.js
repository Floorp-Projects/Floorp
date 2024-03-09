/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that TopSites are showing an appropriate Switch-tab status, depending
 * on the state of the `browser.urlbar.switchTabs.searchAllContainers` pref.
 * When the feature is enabled, in a normal window they should show the
 * tab container, otherwise it's only possible to switch to a tab in the same
 * container.
 * In private windows it's only possible to switch to private tabs in the
 * private container. Similarly non-private windows don't see private tabs.
 * This test is not checking that switching to the appropriate tab works as that
 * is already covered by other tests.
 */

ChromeUtils.defineESModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.sys.mjs",
  NewTabUtils: "resource://gre/modules/NewTabUtils.sys.mjs",
});

const EN_US_TOPSITES =
  "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/";
const OUR_TOPSITE_URL = "https://example.com/";
const REF_TOPSITE_URL = OUR_TOPSITE_URL + "#someref";
const TOPSITES_COUNT = EN_US_TOPSITES.split(",").length + 1;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.topsites", true],
      ["browser.urlbar.suggest.quickactions", false],
      ["browser.newtabpage.activity-stream.default.sites", EN_US_TOPSITES],
    ],
  });
  registerCleanupFunction(PlacesUtils.history.clear);
});

add_task(async function test_ignoreRef() {
  info("Add some visits to a URL.");
  await addAsFirstTopSite(REF_TOPSITE_URL);

  for (let val of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.switchTabs.searchAllContainers", val]],
    });
    info("Test with searchAllContainer set to " + val.toString());
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      REF_TOPSITE_URL
    );
    // Switch back to the originating tab, to check for switch to the current tab.
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
    await openAddressBarAndCheckResults(window, TOPSITES_COUNT, new Set([0]));
    await BrowserTestUtils.removeTab(tab);
    await SpecialPowers.popPrefEnv();
  }
  await PlacesUtils.history.remove(REF_TOPSITE_URL);
});

add_task(async function test_topSitesTabSwitch() {
  await addAsFirstTopSite(OUR_TOPSITE_URL);

  for (let val of [true, false]) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.urlbar.switchTabs.searchAllContainers", val]],
    });
    info("Test with searchAllContainer set to " + val.toString());
    await doTest();
    await SpecialPowers.popPrefEnv();
  }
  await PlacesUtils.history.remove(OUR_TOPSITE_URL);
});

async function doTest() {
  info("Non-private window");
  // Add a normal tab and a container tab.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    OUR_TOPSITE_URL
  );
  let containerTab = await loadNewForegroundContainerTab(OUR_TOPSITE_URL, 1);
  // Switch back to the originating tab, to check for switch to the current tab.
  await BrowserTestUtils.switchTab(gBrowser, gBrowser.tabs[0]);
  let expectedUserContextIds = UrlbarPrefs.get("switchTabs.searchAllContainers")
    ? new Set([0, 1])
    : new Set([0]);
  await openAddressBarAndCheckResults(
    window,
    TOPSITES_COUNT + expectedUserContextIds.size - 1,
    expectedUserContextIds
  );

  info("Private window");
  let pbWin = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await openAddressBarAndCheckResults(pbWin, TOPSITES_COUNT, new Set());

  info("Close the original tab and open the url in the private window instead");
  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.openNewForegroundTab(pbWin.gBrowser, OUR_TOPSITE_URL);
  // Switch back to the originating tab, to check for switch to the current tab.
  await BrowserTestUtils.switchTab(pbWin.gBrowser, pbWin.gBrowser.tabs[0]);
  await openAddressBarAndCheckResults(
    window,
    TOPSITES_COUNT,
    UrlbarPrefs.get("switchTabs.searchAllContainers") ? new Set([1]) : new Set()
  );
  await openAddressBarAndCheckResults(pbWin, TOPSITES_COUNT, new Set([-1]));

  // We're done with the private window.
  await BrowserTestUtils.closeWindow(pbWin);

  info("Check Top sites in the same container tab");
  let blankSameContainerTab = await loadNewForegroundContainerTab(
    "about:blank",
    1
  );
  await openAddressBarAndCheckResults(window, TOPSITES_COUNT, new Set([1]));
  await BrowserTestUtils.removeTab(blankSameContainerTab);

  info("Check Top sites in a different container tab");
  let blankDiffContainerTab = await loadNewForegroundContainerTab(
    "about:blank",
    2
  );
  await openAddressBarAndCheckResults(
    window,
    TOPSITES_COUNT,
    UrlbarPrefs.get("switchTabs.searchAllContainers") ? new Set([1]) : new Set()
  );
  await BrowserTestUtils.removeTab(blankDiffContainerTab);

  await BrowserTestUtils.removeTab(containerTab);
}

async function openAddressBarAndCheckResults(
  win,
  expectedResultCount,
  expectedTabSwitchUserContextIds
) {
  info("Open zero-prefix results.");
  await UrlbarTestUtils.promisePopupOpen(win, () => {
    win.gURLBar.blur();
    if (win.gURLBar.getAttribute("pageproxystate") == "invalid") {
      win.gURLBar.handleRevert();
    }
    EventUtils.synthesizeMouseAtCenter(win.gURLBar.inputField, {}, win);
  });
  await UrlbarTestUtils.promiseSearchComplete(win);
  let resultCount = UrlbarTestUtils.getResultCount(win);
  Assert.equal(expectedResultCount, resultCount, "Check number of results.");

  for (let i = 0; i < resultCount; ++i) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(win, i);

    if (result.url != "https://example.com/") {
      // We don't care about other top sites.
      continue;
    }

    if (!expectedTabSwitchUserContextIds.size) {
      // No more tab switch results expected.
      Assert.notEqual(
        UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
        result.type,
        "Should not be a tab switch result."
      );
      continue;
    }

    // Must be a tab switch result with an expected userContextId.
    Assert.equal(
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      result.type,
      "Should be a tab switch result."
    );
    let hasUserContextId = expectedTabSwitchUserContextIds.delete(
      result.userContextId
    );
    Assert.ok(
      hasUserContextId,
      `UserContextId ${result.userContextId} tab switch was expected in
        ${expectedTabSwitchUserContextIds}`
    );
  }
}

async function addAsFirstTopSite(url) {
  info("Add some visits to a URL.");
  await PlacesTestUtils.addVisits(Array(10).fill(url));
  info("Add top sites and await for our page to be the first");
  await updateTopSites(sites => {
    return sites && sites.length == TOPSITES_COUNT && sites[0].url == url;
  });
}

async function loadNewForegroundContainerTab(url, userContextId, win = window) {
  let tab = BrowserTestUtils.addTab(win.gBrowser, url, {
    userContextId,
  });
  await Promise.all([
    BrowserTestUtils.browserLoaded(tab.linkedBrowser),
    BrowserTestUtils.switchTab(win.gBrowser, tab),
  ]);
  return tab;
}
