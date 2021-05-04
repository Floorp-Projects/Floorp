/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the correct default engines in the search bar.
 */

"use strict";

const { SearchSuggestionController } = ChromeUtils.import(
  "resource://gre/modules/SearchSuggestionController.jsm"
);

const templateNormal = "https://example.com/?q=";
const templatePrivate = "https://example.com/?query=";

add_task(async function setup() {
  await gCUITestUtils.addSearchBar();

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", false]],
  });

  // Create two new search engines. Mark one as the default engine, so
  // the test don't crash. We need to engines for this test as the searchbar
  // doesn't display the default search engine among the one-off engines.
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch1",
    keyword: "mozalias",
  });
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch2",
    keyword: "mozalias2",
    search_url_get_params: "query={searchTerms}",
  });

  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", false],
    ],
  });

  let originalEngine = await Services.search.getDefault();
  let originalPrivateEngine = await Services.search.getDefaultPrivate();

  let engineDefault = Services.search.getEngineByName("MozSearch1");
  await Services.search.setDefault(engineDefault);

  registerCleanupFunction(async function() {
    gCUITestUtils.removeSearchBar();
    await Services.search.setDefault(originalEngine);
    await Services.search.setDefaultPrivate(originalPrivateEngine);
  });
});

async function doSearch(
  win,
  tab,
  engineName,
  templateUrl,
  inputText = "query"
) {
  await searchInSearchbar(inputText, win);

  Assert.ok(
    win.BrowserSearch.searchBar.textbox.popup.searchbarEngineName
      .getAttribute("value")
      .includes(engineName),
    "Should have the correct engine name displayed in the bar"
  );

  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  EventUtils.synthesizeKey("KEY_Enter", {}, win);
  await p;

  Assert.equal(
    tab.linkedBrowser.currentURI.spec,
    templateUrl + inputText,
    "Should have loaded the expected search page."
  );
}

add_task(async function test_default_search() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await doSearch(window, tab, "MozSearch1", templateNormal);

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_default_search_private_no_separate() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await doSearch(win, win.gBrowser.selectedTab, "MozSearch1", templateNormal);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_default_search_private_no_separate() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.separatePrivateDefault", true]],
  });

  await Services.search.setDefaultPrivate(
    Services.search.getEngineByName("MozSearch2")
  );

  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });

  await doSearch(win, win.gBrowser.selectedTab, "MozSearch2", templatePrivate);

  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_form_history() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  await FormHistoryTestUtils.clear("searchbar-history");
  const gShortString = new Array(
    SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH
  )
    .fill("a")
    .join("");
  let promiseAdd = TestUtils.topicObserved("satchel-storage-changed");
  await doSearch(window, tab, "MozSearch1", templateNormal, gShortString);
  await promiseAdd;
  let entries = (await FormHistoryTestUtils.search("searchbar-history")).map(
    entry => entry.value
  );
  Assert.deepEqual(
    entries,
    [gShortString],
    "Should have stored search history"
  );

  await FormHistoryTestUtils.clear("searchbar-history");
  const gLongString = new Array(
    SearchSuggestionController.SEARCH_HISTORY_MAX_VALUE_LENGTH + 1
  )
    .fill("a")
    .join("");
  await doSearch(window, tab, "MozSearch1", templateNormal, gLongString);
  // There's nothing we can wait for, since addition should not be happening.
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 500));
  entries = (await FormHistoryTestUtils.search("searchbar-history")).map(
    entry => entry.value
  );
  Assert.deepEqual(entries, [], "Should not find form history");

  await FormHistoryTestUtils.clear("searchbar-history");
  BrowserTestUtils.removeTab(tab);
});
