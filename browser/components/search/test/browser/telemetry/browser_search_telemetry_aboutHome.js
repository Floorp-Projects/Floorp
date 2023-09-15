"use strict";

const SCALAR_ABOUT_HOME = "browser.engagement.navigation.about_home";

add_setup(async function () {
  // about:home uses IndexedDB. However, the test finishes too quickly and doesn't
  // allow it enougth time to save. So it throws. This disables all the uncaught
  // exception in this file and that's the reason why we split about:home tests
  // out of the other UsageTelemetry files.
  ignoreAllUncaughtExceptions();

  // Create two new search engines. Mark one as the default engine, so
  // the test don't crash. We need to engines for this test as the searchbar
  // in content doesn't display the default search engine among the one-off engines.
  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      keyword: "mozalias",
    },
    { setAsDefault: true }
  );
  await SearchTestUtils.installSearchExtension({
    name: "MozSearch2",
    keyword: "mozalias2",
  });

  // Move the second engine at the beginning of the one-off list.
  let engineOneOff = Services.search.getEngineByName("MozSearch2");
  await Services.search.moveEngine(engineOneOff, 0);

  // Enable local telemetry recording for the duration of the tests.
  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  // Enable event recording for the events tested here.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "browser.newtabpage.activity-stream.improvesearch.handoffToAwesomebar",
        false,
      ],
    ],
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.history.clear();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

add_task(async function test_abouthome_activitystream_simpleQuery() {
  // Let's reset the counts.
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  Services.fog.testResetFOG();
  let search_hist =
    TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Load about:home.");
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:home");
  await BrowserTestUtils.browserStopped(tab.linkedBrowser, "about:home");

  info("Wait for ContentSearchUI search provider to initialize.");
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    await ContentTaskUtils.waitForCondition(
      () => content.wrappedJSObject.gContentSearchController.defaultEngine
    );
  });

  info("Trigger a simple search, just test + enter.");
  let p = BrowserTestUtils.browserStopped(
    tab.linkedBrowser,
    "https://example.com/?q=test+query"
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

  // Also also check Glean events.
  const record = Glean.newtabSearch.issued.testGetValue();
  Assert.ok(!!record, "Must have recorded a search issuance");
  Assert.equal(record.length, 1, "One search, one event");
  Assert.deepEqual(
    {
      search_access_point: "about_home",
      telemetry_id: "other-MozSearch",
    },
    record[0].extra,
    "Must have recorded the expected information."
  );

  BrowserTestUtils.removeTab(tab);
});
