/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests browser quick suggestions.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const TEST_URL =
  "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/quicksuggest.sjs";

const TEST_DATA = [
  {
    id: 1,
    url: `${TEST_URL}?q=frabbits`,
    title: "frabbits",
    keywords: ["frab"],
  },
];

const ABOUT_BLANK = "about:blank";
const SUGGESTIONS_PREF = "browser.search.suggest.enabled";
const PRIVATE_SUGGESTIONS_PREF = "browser.search.suggest.enabled.private";

function sleep(ms) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, ms));
}

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });

  // Add a mock engine so we don't hit the network loading the SERP.
  let engine = await Services.search.addEngineWithDetails("Test", {
    template: "http://example.com/?search={searchTerms}",
  });
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(engine);

  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);

  registerCleanupFunction(async function() {
    Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function basic_test() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_BLANK);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
  });
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.url, `${TEST_URL}?q=frabbits`);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_suggestions_disabled() {
  await SpecialPowers.pushPrefEnv({ set: [[SUGGESTIONS_PREF, false]] });
  await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_BLANK);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
  });
  // We can't waitForResultAt because we don't want a result, give enough time
  // that a result would most likely have appeared.
  await sleep(100);
  Assert.ok(
    window.gURLBar.view._rows.children.length == 1,
    "There are no additional suggestions"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_suggestions_disabled_private() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SUGGESTIONS_PREF, true],
      [PRIVATE_SUGGESTIONS_PREF, false],
    ],
  });

  let window = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  let browser = window.gBrowser.selectedTab.linkedBrowser;
  BrowserTestUtils.loadURI(browser, ABOUT_BLANK);
  await BrowserTestUtils.browserLoaded(browser, false, ABOUT_BLANK);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
  });
  await sleep(100);
  Assert.ok(
    window.gURLBar.view._rows.children.length == 1,
    "There are no additional suggestions"
  );
  await BrowserTestUtils.closeWindow(window);
  await SpecialPowers.popPrefEnv();
});
