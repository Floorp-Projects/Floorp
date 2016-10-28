"use strict";

const SCALAR_SEARCHBAR = "browser.engagement.navigation.searchbar";

let searchInSearchbar = Task.async(function* (inputText) {
  let win = window;
  yield new Promise(r => waitForFocus(r, win));
  let sb = BrowserSearch.searchBar;
  // Write the search query in the searchbar.
  sb.focus();
  sb.value = inputText;
  //sb.textbox.openPopup();
  sb.textbox.controller.startSearch(inputText);
  // Wait for the popup to show.
  yield BrowserTestUtils.waitForEvent(sb.textbox.popup, "popupshown");
  // And then for the search to complete.
  yield BrowserTestUtils.waitForCondition(() => sb.textbox.controller.searchStatus >=
                                                Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH,
                                          "The search in the searchbar must complete.");
});

/**
 * Click one of the entries in the search suggestion popup.
 *
 * @param {String} entryName
 *        The name of the elemet to click on.
 */
function clickSearchbarSuggestion(entryName) {
  let popup = BrowserSearch.searchBar.textbox.popup;
  let column = popup.tree.columns[0];

  for (let rowID = 0; rowID < popup.tree.view.rowCount; rowID++) {
    const suggestion = popup.tree.view.getValueAt(rowID, column);
    if (suggestion !== entryName) {
      continue;
    }

    // Make sure the suggestion is visible, just in case.
    let tbo = popup.tree.treeBoxObject;
    tbo.ensureRowIsVisible(rowID);
    // Calculate the click coordinates.
    let rect = tbo.getCoordsForCellItem(rowID, column, "text");
    let x = rect.x + rect.width / 2;
    let y = rect.y + rect.height / 2;
    // Simulate the click.
    EventUtils.synthesizeMouse(popup.tree.body, x, y, {},
                               popup.tree.ownerGlobal);
    break;
  }
}

add_task(function* setup() {
  // Create two new search engines. Mark one as the default engine, so
  // the test don't crash. We need to engines for this test as the searchbar
  // doesn't display the default search engine among the one-off engines.
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

add_task(function* test_plainQuery() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Simulate entering a simple search.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInSearchbar("simple query");
  EventUtils.sendKey("return");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_SEARCHBAR, "search_enter", 1);
  Assert.equal(Object.keys(scalars[SCALAR_SEARCHBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_oneOff() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Perform a one-off search using the first engine.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInSearchbar("query");

  info("Pressing Alt+Down to highlight the first one off engine.");
  EventUtils.synthesizeKey("VK_DOWN", { altKey: true });
  EventUtils.sendKey("return");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_SEARCHBAR, "search_oneoff", 1);
  Assert.equal(Object.keys(scalars[SCALAR_SEARCHBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  yield BrowserTestUtils.removeTab(tab);
});

add_task(function* test_suggestion() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();

  // Create an engine to generate search suggestions and add it as default
  // for this test.
  const url = getRootDirectory(gTestPath) + "usageTelemetrySearchSuggestions.xml";
  let suggestionEngine = yield new Promise((resolve, reject) => {
    Services.search.addEngine(url, null, "", false, {
      onSuccess(engine) { resolve(engine) },
      onError() { reject() }
    });
  });

  let previousEngine = Services.search.currentEngine;
  Services.search.currentEngine = suggestionEngine;

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");

  info("Perform a one-off search using the first engine.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  yield searchInSearchbar("query");
  info("Clicking the searchbar suggestion.");
  clickSearchbarSuggestion("queryfoo");
  yield p;

  // Check if the scalars contain the expected values.
  const scalars =
    Services.telemetry.snapshotKeyedScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  checkKeyedScalar(scalars, SCALAR_SEARCHBAR, "search_suggestion", 1);
  Assert.equal(Object.keys(scalars[SCALAR_SEARCHBAR]).length, 1,
               "This search must only increment one entry in the scalar.");

  Services.search.currentEngine = previousEngine;
  Services.search.removeEngine(suggestionEngine);
  yield BrowserTestUtils.removeTab(tab);
});
