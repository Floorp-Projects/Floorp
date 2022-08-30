/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry for topsite results.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutNewTab: "resource:///modules/AboutNewTab.jsm",
});

const EN_US_TOPSITES =
  "https://www.youtube.com/,https://www.facebook.com/,https://www.amazon.com/,https://www.reddit.com/,https://www.wikipedia.org/,https://twitter.com/";

function snapshotHistograms() {
  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();
  return {
    resultMethodHist: TelemetryTestUtils.getAndClearHistogram(
      "FX_URLBAR_SELECTED_RESULT_METHOD"
    ),
    search_hist: TelemetryTestUtils.getAndClearKeyedHistogram("SEARCH_COUNTS"),
  };
}

function assertTelemetryResults(histograms, type, index, method) {
  TelemetryTestUtils.assertHistogram(histograms.resultMethodHist, method, 1);

  TelemetryTestUtils.assertKeyedScalar(
    TelemetryTestUtils.getProcessScalars("parent", true, true),
    `urlbar.picked.${type}`,
    index,
    1
  );
}

/**
 * Updates the Top Sites feed.
 * @param {function} condition
 *   A callback that returns true after Top Sites are successfully updated.
 * @param {boolean} searchShortcuts
 *   True if Top Sites search shortcuts should be enabled.
 */
async function updateTopSites(condition, searchShortcuts = false) {
  // Toggle the pref to clear the feed cache and force an update.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtabpage.activity-stream.feeds.system.topsites", false],
      ["browser.newtabpage.activity-stream.feeds.system.topsites", true],
      [
        "browser.newtabpage.activity-stream.improvesearch.topSiteSearchShortcuts",
        searchShortcuts,
      ],
    ],
  });

  // Wait for the feed to be updated.
  await TestUtils.waitForCondition(() => {
    let sites = AboutNewTab.getTopSites();
    return condition(sites);
  }, "Waiting for top sites to be updated");
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.topsites", true],
      ["browser.newtabpage.activity-stream.default.sites", EN_US_TOPSITES],
      ["browser.urlbar.suggest.quickactions", false],
    ],
  });
  await updateTopSites(
    sites => sites && sites.length == EN_US_TOPSITES.split(",").length
  );
});

add_task(async function test() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    let sites = AboutNewTab.getTopSites();
    Assert.equal(
      sites.length,
      6,
      "The test suite browser should have 6 Top Sites."
    );

    const histograms = snapshotHistograms();

    await UrlbarTestUtils.promisePopupOpen(window, () => {
      EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {});
    });

    await UrlbarTestUtils.promiseSearchComplete(window);
    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      sites.length,
      "The number of results should be the same as the number of Top Sites (6)."
    );
    // Select the first resultm and confirm it.
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    EventUtils.synthesizeKey("KEY_ArrowDown");
    Assert.equal(
      UrlbarTestUtils.getSelectedRowIndex(window),
      0,
      "The first result should be selected"
    );

    let loadPromise = BrowserTestUtils.waitForDocLoadAndStopIt(
      result.url,
      gBrowser.selectedBrowser
    );
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;

    assertTelemetryResults(
      histograms,
      "topsite",
      0,
      UrlbarTestUtils.SELECTED_RESULT_METHODS.arrowEnterSelection
    );
    await UrlbarTestUtils.promisePopupClose(window, () => {
      gURLBar.blur();
    });
  });
});
