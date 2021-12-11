/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Preferences = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm",
  {}
).Preferences;

function test() {
  waitForExplicitFinish();
  resetPreferences();

  let calledTestTelemetry = false;
  function testTelemetry() {
    calledTestTelemetry = true;
    // Find the right bucket for the "Foo" engine.
    let engine = Services.search.getEngineByName("Foo");
    let histogramKey = (engine.identifier || "other-Foo") + ".searchbar";
    let numSearchesBefore = 0;
    try {
      let hs = Services.telemetry
        .getKeyedHistogramById("SEARCH_COUNTS")
        .snapshot();
      if (histogramKey in hs) {
        numSearchesBefore = hs[histogramKey].sum;
      }
    } catch (ex) {
      // No searches performed yet, not a problem, |numSearchesBefore| is 0.
    }

    // Now perform a search and ensure the count is incremented.
    let tab = BrowserTestUtils.addTab(gBrowser);
    gBrowser.selectedTab = tab;
    let searchBar = BrowserSearch.searchBar;

    searchBar.value = "firefox health report";
    searchBar.focus();

    function afterSearch() {
      searchBar.value = "";
      gBrowser.removeTab(tab);

      // Make sure that the context searches are correctly recorded.
      let hs = Services.telemetry
        .getKeyedHistogramById("SEARCH_COUNTS")
        .snapshot();
      Assert.ok(
        histogramKey in hs,
        "The histogram must contain the correct key"
      );
      Assert.equal(
        hs[histogramKey].sum,
        numSearchesBefore + 1,
        "Performing a search increments the related SEARCH_COUNTS key by 1."
      );

      let fooEngine = Services.search.getEngineByName("Foo");
      Services.search.removeEngine(fooEngine);
    }

    EventUtils.synthesizeKey("KEY_Enter");
    executeSoon(() => executeSoon(afterSearch));
  }

  function observer(subject, topic, data) {
    switch (data) {
      case "engine-added":
        let engine = Services.search.getEngineByName("Foo");
        ok(engine, "Engine was added.");
        Services.search.defaultEngine = engine;
        break;

      case "engine-default":
        // We may be called again when resetting the engine at the end.
        if (!calledTestTelemetry) {
          is(
            Services.search.defaultEngine.name,
            "Foo",
            "Current engine is Foo"
          );
          testTelemetry();
        }
        break;

      case "engine-removed":
        Services.obs.removeObserver(observer, "browser-search-engine-modified");
        gCUITestUtils.removeSearchBar();
        finish();
        break;
    }
  }

  Services.obs.addObserver(observer, "browser-search-engine-modified");
  gCUITestUtils.addSearchBar().then(function() {
    Services.search.addOpenSearchEngine(
      "http://mochi.test:8888/browser/browser/components/search/test/browser/testEngine.xml",
      "data:image/x-icon,%00"
    );
  });
}

function resetPreferences() {
  Preferences.resetBranch("datareporting.policy.");
  Preferences.set(
    "datareporting.policy.dataSubmissionPolicyBypassNotification",
    true
  );
}
