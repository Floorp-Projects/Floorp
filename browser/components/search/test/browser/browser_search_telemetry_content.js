"use strict";

const BASE_PROBE_NAME = "browser.engagement.navigation.";
const SCALAR_CONTEXT_MENU = BASE_PROBE_NAME + "contextmenu";
const SCALAR_ABOUT_NEWTAB = BASE_PROBE_NAME + "about_newtab";

add_task(async function setup() {
  // Create two new search engines. Mark one as the default engine, so
  // the test don't crash. We need to engines for this test as the searchbar
  // in content doesn't display the default search engine among the one-off engines.
  await Services.search.addEngineWithDetails("MozSearch", {
    alias: "mozalias",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });

  await Services.search.addEngineWithDetails("MozSearch2", {
    alias: "mozalias2",
    method: "GET",
    template: "http://example.com/?q={searchTerms}",
  });

  // Make the first engine the default search engine.
  let engineDefault = Services.search.getEngineByName("MozSearch");
  let originalEngine = await Services.search.getDefault();
  await Services.search.setDefault(engineDefault);

  // Move the second engine at the beginning of the one-off list.
  let engineOneOff = Services.search.getEngineByName("MozSearch2");
  await Services.search.moveEngine(engineOneOff, 0);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.select_events.enabled", true], // We want select events to be fired.
    ],
  });

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Enable event recording for the events tested here.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    await Services.search.setDefault(originalEngine);
    await Services.search.removeEngine(engineDefault);
    await Services.search.removeEngine(engineOneOff);
    await PlacesUtils.history.clear();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

add_task(async function test_context_menu() {
  // Let's reset the Telemetry data.
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  // Open a new tab with a page containing some text.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/plain;charset=utf8,test%20search"
  );

  info("Select all the text in the page.");
  await SpecialPowers.spawn(tab.linkedBrowser, [""], async function() {
    return new Promise(resolve => {
      content.document.addEventListener("selectionchange", () => resolve(), {
        once: true,
      });
      content.document.getSelection().selectAllChildren(content.document.body);
    });
  });

  info("Open the context menu.");
  let contextMenu = document.getElementById("contentAreaContextMenu");
  let popupPromise = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    { type: "contextmenu", button: 2 },
    gBrowser.selectedBrowser
  );
  await popupPromise;

  info("Click on search.");
  let searchItem = contextMenu.getElementsByAttribute(
    "id",
    "context-searchselect"
  )[0];
  searchItem.click();

  info("Validate the search metrics.");

  // Telemetry is not updated synchronously here, we must wait for it.
  await TestUtils.waitForCondition(() => {
    const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
    return Object.keys(scalars[SCALAR_CONTEXT_MENU] || {}).length == 1;
  }, "This search must increment one entry in the scalar.");

  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_CONTEXT_MENU,
    "search",
    1
  );

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.contextmenu",
    1
  );

  // Also check events.
  TelemetryTestUtils.assertEvents(
    [
      {
        object: "contextmenu",
        value: null,
        extra: { engine: "other-MozSearch" },
      },
    ],
    { category: "navigation", method: "search" }
  );

  contextMenu.hidePopup();
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(tab);
});

add_task(async function test_about_newtab() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab",
    false
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(() => !content.document.hidden);
  });

  info("Trigger a simple serch, just text + enter.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await typeInSearchField(
    tab.linkedBrowser,
    "test query",
    "newtab-search-text"
  );
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await p;

  // Check if the scalars contain the expected values.
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, false);
  TelemetryTestUtils.assertKeyedScalar(
    scalars,
    SCALAR_ABOUT_NEWTAB,
    "search_enter",
    1
  );
  Assert.equal(
    Object.keys(scalars[SCALAR_ABOUT_NEWTAB]).length,
    1,
    "This search must only increment one entry in the scalar."
  );

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.newtab",
    1
  );

  // Also check events.
  TelemetryTestUtils.assertEvents(
    [
      {
        object: "about_newtab",
        value: "enter",
        extra: { engine: "other-MozSearch" },
      },
    ],
    { category: "navigation", method: "search" }
  );

  BrowserTestUtils.removeTab(tab);
});
