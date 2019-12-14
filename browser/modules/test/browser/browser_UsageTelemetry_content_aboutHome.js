"use strict";

const SCALAR_ABOUT_HOME = "browser.engagement.navigation.about_home";

add_task(async function setup() {
  // about:home uses IndexedDB. However, the test finishes too quickly and doesn't
  // allow it enougth time to save. So it throws. This disables all the uncaught
  // exception in this file and that's the reason why we split about:home tests
  // out of the other UsageTelemetry files.
  ignoreAllUncaughtExceptions();

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

add_task(async function test_abouthome_activitystream_simpleQuery() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  let search_hist = TelemetryTestUtils.getAndClearKeyedHistogram(
    "SEARCH_COUNTS"
  );

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Load about:home.");
  BrowserTestUtils.loadURI(tab.linkedBrowser, "about:home");
  await BrowserTestUtils.browserStopped(tab.linkedBrowser, "about:home");

  info("Wait for ContentSearchUI search provider to initialize.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function() {
    await ContentTaskUtils.waitForCondition(
      () => content.wrappedJSObject.gContentSearchController.defaultEngine
    );
  });

  info("Trigger a simple search, just test + enter.");
  let p = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    "http://example.com/?q=test+query"
  );
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
    SCALAR_ABOUT_HOME,
    "search_enter",
    1
  );
  Assert.equal(
    Object.keys(scalars[SCALAR_ABOUT_HOME]).length,
    1,
    "This search must only increment one entry in the scalar."
  );

  // Make sure SEARCH_COUNTS contains identical values.
  TelemetryTestUtils.assertKeyedHistogramSum(
    search_hist,
    "other-MozSearch.abouthome",
    1
  );

  // Also check events.
  TelemetryTestUtils.assertEvents(
    [
      {
        object: "about_home",
        value: "enter",
        extra: { engine: "other-MozSearch" },
      },
    ],
    { category: "navigation", method: "search" }
  );

  BrowserTestUtils.removeTab(tab);
});
