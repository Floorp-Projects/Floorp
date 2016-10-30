"use strict";

const BASE_PROBE_NAME = "browser.engagement.navigation.";
const SCALAR_CONTEXT_MENU = BASE_PROBE_NAME + "contextmenu";
const SCALAR_ABOUT_NEWTAB = BASE_PROBE_NAME + "about_newtab";

add_task(function* setup() {
  // Create two new search engines. Mark one as the default engine, so
  // the test don't crash. We need to engines for this test as the searchbar
  // in content doesn't display the default search engine among the one-off engines.
  Services.search.addEngineWithDetails("MozSearch", "", "mozalias", "", "GET",
                                       "http://example.com/?q={searchTerms}");

  Services.search.addEngineWithDetails("MozSearch2", "", "mozalias2", "", "GET",
                                       "http://example.com/?q={searchTerms}");

  // Make the first engine the default search engine.
  let engineDefault = Services.search.getEngineByName("MozSearch");
  let originalEngine = Services.search.currentEngine;
  Services.search.currentEngine = engineDefault;

  // Move the second engine at the beginning of the one-off list.
  let engineOneOff = Services.search.getEngineByName("MozSearch2");
  Services.search.moveEngine(engineOneOff, 0);

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(function* () {
    Services.search.currentEngine = originalEngine;
    Services.search.removeEngine(engineDefault);
    Services.search.removeEngine(engineOneOff);
  });
});

add_task(function* test_context_menu() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Open a new tab with a page containing some text.
  let tab =
    yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/plain;charset=utf8,test%20search");

  info("Select all the text in the page.");
  yield ContentTask.spawn(tab.linkedBrowser, "", function*() {
    return new Promise(resolve => {
      content.document.addEventListener("selectionchange", () => resolve(), { once: true });
      content.document.getSelection().selectAllChildren(content.document.body);
    });
  });

  info("Open the context menu.");
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter("body", { type: "contextmenu", button: 2 },
                                           gBrowser.selectedBrowser);
  yield popupPromise;

  info("Click on search.");
  let searchItem = contextMenu.getElementsByAttribute("id", "context-searchselect")[0];
  searchItem.click();

  info("Validate the search counts.");
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_CONTEXT_MENU, "search", 1);
  Assert.equal(Object.keys(scalars[SCALAR_CONTEXT_MENU]).length, 1,
               "This search must only increment one entry in the scalar.");

  contextMenu.hidePopup();
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_about_newtab() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:newtab", false);
  yield ContentTask.spawn(tab.linkedBrowser, null, function* () {
    yield ContentTaskUtils.waitForCondition(() => !content.document.hidden);
  });

  info("Trigger a simple serch, just text + enter.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield typeInSearchField(tab.linkedBrowser, "test query", "newtab-search-text");
  yield BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_ABOUT_NEWTAB, "search_enter", 1);
  Assert.equal(Object.keys(scalars[SCALAR_ABOUT_NEWTAB]).length, 1,
               "This search must only increment one entry in the scalar.");

  yield BrowserTestUtils.removeTab(tab);
});
