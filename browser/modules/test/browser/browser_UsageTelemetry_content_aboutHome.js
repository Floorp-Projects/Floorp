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

  // Enable event recording for the events tested here.
  Services.telemetry.setEventRecordingEnabled("navigation", true);

  // Make sure to restore the engine once we're done.
  registerCleanupFunction(async function() {
    Services.search.currentEngine = originalEngine;
    Services.search.removeEngine(engineDefault);
    Services.search.removeEngine(engineOneOff);
    await PlacesTestUtils.clearHistory();
    Services.telemetry.setEventRecordingEnabled("navigation", false);
    Services.prefs.clearUserPref("browser.newtabpage.activity-stream.aboutHome.enabled");
  });
});

add_task(async function test_abouthome_simpleQuery() {
  // Make sure Activity Stream about:home is disabled.
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.aboutHome.enabled", false);

  // Let's reset the counts.
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  let search_hist = getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Setup waiting for AboutHomeLoadSnippetsCompleted.");
  let promiseAboutHomeLoaded = new Promise(resolve => {
    tab.linkedBrowser.addEventListener("AboutHomeLoadSnippetsCompleted", function loadListener(event) {
      tab.linkedBrowser.removeEventListener("AboutHomeLoadSnippetsCompleted", loadListener, true);
      resolve();
    }, true, true);
  });

  info("Load about:home.");
  tab.linkedBrowser.loadURI("about:home");
  info("Wait for AboutHomeLoadSnippetsCompleted.");
  await promiseAboutHomeLoaded;

  info("Wait for ContentSearchUI search provider to initialize.");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    await ContentTaskUtils.waitForCondition(() => content.wrappedJSObject.gContentSearchController.defaultEngine);
  });

  info("Trigger a simple serch, just test + enter.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await typeInSearchField(tab.linkedBrowser, "test query", "searchText");
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await p;

  // Check if the scalars contain the expected values.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, false);
  checkKeyedScalar(scalars, SCALAR_ABOUT_HOME, "search_enter", 1);
  Assert.equal(Object.keys(scalars[SCALAR_ABOUT_HOME]).length, 1,
               "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, "other-MozSearch.abouthome", 1);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  checkEvents(events, [["navigation", "search", "about_home", "enter", {engine: "other-MozSearch"}]]);

  await BrowserTestUtils.removeTab(tab);
});

add_task(async function test_abouthome_activitystream_simpleQuery() {
  // Make sure Activity Stream about:home is enabled.
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.aboutHome.enabled", true);

  // Let's reset the counts.
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  let search_hist = getAndClearKeyedHistogram("SEARCH_COUNTS");

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);

  info("Setup waiting for search input to initialise.");
  let promiseAboutHomeSearchLoaded = new Promise(resolve => {
    tab.linkedBrowser.addEventListener("ContentSearchClient", function loadListener(event) {
      tab.linkedBrowser.removeEventListener("ContentSearchClient", loadListener, true);
      executeSoon(resolve);
    }, true, true);
  });

  info("Load about:home.");
  tab.linkedBrowser.loadURI("about:home");
  info("Wait for ActivityStream search input.");
  await promiseAboutHomeSearchLoaded;

  info("Wait for ContentSearchUI search provider to initialize.");
  await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    await ContentTaskUtils.waitForCondition(() => content.wrappedJSObject.gContentSearchController.defaultEngine);
  });

  info("Trigger a simple serch, just test + enter.");
  let p = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  await typeInSearchField(tab.linkedBrowser, "test query", "newtab-search-text");
  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, tab.linkedBrowser);
  await p;

  // Check if the scalars contain the expected values.
  const scalars = getParentProcessScalars(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, true, false);
  checkKeyedScalar(scalars, SCALAR_ABOUT_HOME, "search_enter", 1);
  Assert.equal(Object.keys(scalars[SCALAR_ABOUT_HOME]).length, 1,
    "This search must only increment one entry in the scalar.");

  // Make sure SEARCH_COUNTS contains identical values.
  checkKeyedHistogram(search_hist, "other-MozSearch.abouthome", 1);

  // Also check events.
  let events = Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, false);
  events = (events.parent || []).filter(e => e[1] == "navigation" && e[2] == "search");
  checkEvents(events, [["navigation", "search", "about_home", "enter", {engine: "other-MozSearch"}]]);

  await BrowserTestUtils.removeTab(tab);
});
