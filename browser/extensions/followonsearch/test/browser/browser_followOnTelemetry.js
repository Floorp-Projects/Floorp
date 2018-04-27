/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.defineModuleGetter(this, "SearchTestUtils",
  "resource://testing-common/SearchTestUtils.jsm");

SearchTestUtils.init(Assert, registerCleanupFunction);

const BASE_URL = "http://mochi.test:8888/browser/browser/extensions/followonsearch/test/browser/";
const TEST_ENGINE_BASENAME = "testEngine.xml";

add_task(async function test_followOnSearchTelemetry() {
  let engine = await SearchTestUtils.promiseNewSearchEngine(
    getRootDirectory(gTestPath) + TEST_ENGINE_BASENAME,
    registerCleanupFunction);
  let oldCurrentEngine = Services.search.currentEngine;
  Services.search.currentEngine = engine;

  let histogram = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
  histogram.clear();

  registerCleanupFunction(() => Services.search.currentEngine = oldCurrentEngine);

  await BrowserTestUtils.withNewTab({gBrowser}, async browser => {
    // Open the initial search page via entering a search on the URL bar.
    let loadPromise = BrowserTestUtils.waitForLocationChange(gBrowser,
      `${BASE_URL}test.html?searchm=test&mt=TEST`);

    gURLBar.focus();
    EventUtils.sendString("test");
    EventUtils.sendKey("return");

    await loadPromise;

    // Perform a follow-on search, selecting the form in the page.
    loadPromise = BrowserTestUtils.waitForLocationChange(gBrowser,
      `${BASE_URL}test2.html?mtfo=followonsearchtest&mt=TEST&m=test`);

    await ContentTask.spawn(browser, null, function() {
      content.document.getElementById("submit").click();
    });

    await loadPromise;

    let snapshot;

    // We have to wait for the snapshot to come in, as there's async functionality
    // in the extension.
    await TestUtils.waitForCondition(() => {
      snapshot = histogram.snapshot();
      return "mochitest.follow-on:unknown:TEST" in snapshot;
    });
    Assert.ok("mochitest.follow-on:unknown:TEST" in snapshot,
      "Histogram should have an entry for the follow-on search.");
    Assert.deepEqual(snapshot["mochitest.follow-on:unknown:TEST"], {
      counts: [ 1, 0, 0 ],
      histogram_type: 4,
      max: 2,
      min: 1,
      ranges: [ 0, 1, 2 ],
      sum: 1,
    }, "Histogram should have the correct snapshot data");
  });
});
