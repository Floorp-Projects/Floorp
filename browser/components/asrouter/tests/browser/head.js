"use strict";

ChromeUtils.defineESModuleGetters(this, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});
XPCOMUtils.defineLazyModuleGetters(this, {
  ASRouter: "resource:///modules/asrouter/ASRouter.jsm",
  QueryCache: "resource:///modules/asrouter/ASRouterTargeting.jsm",
});
const { FxAccounts } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccounts.sys.mjs"
);
// We import sinon here to make it available across all mochitest test files
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

function pushPrefs(...prefs) {
  return SpecialPowers.pushPrefEnv({ set: prefs });
}

async function clearHistoryAndBookmarks() {
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
  QueryCache.expireAll();
}

/**
 * Helper function to navigate and wait for page to load
 * https://searchfox.org/mozilla-central/rev/314b4297e899feaf260e7a7d1a9566a218216e7a/testing/mochitest/BrowserTestUtils/BrowserTestUtils.sys.mjs#404
 */
async function waitForUrlLoad(url) {
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, url);
  await BrowserTestUtils.browserLoaded(browser, false, url);
}
