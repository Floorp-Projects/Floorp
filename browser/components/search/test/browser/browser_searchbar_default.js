/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests the correct default engines in the search bar.
 */

"use strict";

const { SearchSuggestionController } = ChromeUtils.importESModule(
  "resource://gre/modules/SearchSuggestionController.sys.mjs"
);

const templateNormal = "https://example.com/?q=";
const templatePrivate = "https://example.com/?query=";

const searchPopup = document.getElementById("PopupSearchAutoComplete");

add_setup(async function () {
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
  await Services.search.setDefault(
    engineDefault,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  registerCleanupFunction(async function () {
    gCUITestUtils.removeSearchBar();
    await Services.search.setDefault(
      originalEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
    await Services.search.setDefaultPrivate(
      originalPrivateEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
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
    Services.search.getEngineByName("MozSearch2"),
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
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

add_task(async function test_form_history_delete() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  await FormHistoryTestUtils.clear("searchbar-history");
  await FormHistoryTestUtils.add("searchbar-history", ["first", "second"]);

  let sb = BrowserSearch.searchBar;
  sb.focus();
  sb.value = "";
  let popupshown = BrowserTestUtils.waitForEvent(
    sb.textbox.popup,
    "popupshown"
  );
  EventUtils.synthesizeKey("KEY_ArrowDown");
  await popupshown;
  EventUtils.synthesizeKey("KEY_ArrowDown");

  let initialEntriesLength = searchPopup.richlistbox.itemChildren.filter(
    child => !child.getAttribute("collapsed")
  ).length;

  Assert.equal(initialEntriesLength, 2, "Should have two items in the popup");
  Assert.equal(
    searchPopup.selectedIndex,
    0,
    "Should have selected the first entry"
  );
  Assert.equal(
    searchPopup.children[2].selectedItems[0].getAttribute("ac-value"),
    "first",
    "Should have selected the expected first result"
  );

  let promiseRemoved = TestUtils.topicObserved(
    "satchel-storage-changed",
    (_subject, data) => data == "formhistory-remove"
  );

  EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });

  await promiseRemoved;

  // Give the listbox time to update.
  await TestUtils.waitForCondition(
    () =>
      searchPopup.richlistbox.itemChildren.filter(
        child => !child.getAttribute("collapsed")
      ).length ==
      initialEntriesLength - 1,
    "Should reduced the entries in the listbox"
  );

  Assert.equal(
    searchPopup.selectedIndex,
    0,
    "Should have the second entry selected; now in the first index"
  );
  Assert.equal(
    searchPopup.children[2].selectedItems[0].getAttribute("ac-value"),
    "second",
    "Should have selected the second item in the list"
  );
  Assert.equal(
    searchPopup.richlistbox.itemChildren.filter(
      child => !child.getAttribute("collapsed")
    ).length,
    initialEntriesLength - 1,
    "Should have reduced the number of autocomplete results by 1"
  );

  let entries = (await FormHistoryTestUtils.search("searchbar-history")).map(
    entry => entry.value
  );
  Assert.deepEqual(entries, ["second"], "Should have deleted the entry");

  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_searchbar_revert() {
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );

  await doSearch(window, tab, "MozSearch1", templateNormal, "testQuery");

  let searchbar = window.BrowserSearch.searchBar;
  is(
    searchbar.value,
    "testQuery",
    "Search value should be the the last search"
  );

  // focus search bar
  let promise = promiseEvent(searchPopup, "popupshown");
  info("Opening search panel");
  searchbar.focus();
  await promise;

  searchbar.value = "aQuery";
  searchbar.value = "anotherQuery";

  // close the panel using the escape key.
  promise = promiseEvent(searchPopup, "popuphidden");
  EventUtils.synthesizeKey("KEY_Escape");
  await promise;

  is(searchbar.value, "anotherQuery", "The search value should be the same");
  // revert the search bar value
  EventUtils.synthesizeKey("KEY_Escape");
  is(
    searchbar.value,
    "testQuery",
    "The search value should have been reverted"
  );

  EventUtils.synthesizeKey("KEY_Escape");
  is(searchbar.value, "testQuery", "The search value should be the same");

  await doSearch(window, tab, "MozSearch1", templateNormal, "query");

  is(searchbar.value, "query", "The search value should be query");
  EventUtils.synthesizeKey("KEY_Escape");
  is(searchbar.value, "query", "The search value should be the same");

  BrowserTestUtils.removeTab(tab);
});
