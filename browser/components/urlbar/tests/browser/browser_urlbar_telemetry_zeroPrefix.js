/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests urlbar telemetry related to the zero-prefix view, i.e., when
 * the search string is empty.
 */

"use strict";

const HISTOGRAM_DWELL_TIME = "FX_URLBAR_ZERO_PREFIX_DWELL_TIME_MS";
const SCALARS = {
  ABANDONMENT: "urlbar.zeroprefix.abandonment",
  ENGAGEMENT: "urlbar.zeroprefix.engagement",
  EXPOSURE: "urlbar.zeroprefix.exposure",
};

add_setup(async function () {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  Services.telemetry.clearScalars();

  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });
  await updateTopSitesAndAwaitChanged();
});

// zero prefix engagement
add_task(async function engagement() {
  let dwellHistogram =
    TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_DWELL_TIME);

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await showZeroPrefix();
    checkScalars({
      [SCALARS.EXPOSURE]: 1,
    });
    checkAndClearHistogram(dwellHistogram, false);

    info("Finding row with result type URL");
    let foundURLRow = false;
    let count = UrlbarTestUtils.getResultCount(window);
    for (let i = 0; i < count && !foundURLRow; i++) {
      EventUtils.synthesizeKey("KEY_ArrowDown");
      let index = UrlbarTestUtils.getSelectedRowIndex(window);
      Assert.equal(index, i, "The expected row index should be selected");
      let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
      info(`Checked row at index ${i}, result type is: ${details.type}`);
      if (details.type == UrlbarUtils.RESULT_TYPE.URL) {
        foundURLRow = true;
      }
    }
    Assert.ok(foundURLRow, "Should have found a row with result type URL");

    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;
  });

  checkScalars({
    [SCALARS.ENGAGEMENT]: 1,
  });
  checkAndClearHistogram(dwellHistogram, true);
});

// zero prefix abandonment
add_task(async function abandonment() {
  let dwellHistogram =
    TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_DWELL_TIME);

  // Open and close the view twice. The second time the view will used a cached
  // query context and that shouldn't interfere with telemetry.
  for (let i = 0; i < 2; i++) {
    await showZeroPrefix();
    checkScalars({
      [SCALARS.EXPOSURE]: 1,
    });
    checkAndClearHistogram(dwellHistogram, false);

    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
    checkScalars({
      [SCALARS.ABANDONMENT]: 1,
    });
    dwellHistogram = checkAndClearHistogram(dwellHistogram, true);
  }
});

// Shows the zero-prefix view, does some searches, then shows it again by doing
// a search for an empty string.
add_task(async function searches() {
  let dwellHistogram =
    TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_DWELL_TIME);

  info("Show zero prefix");
  await showZeroPrefix();
  checkScalars({
    [SCALARS.EXPOSURE]: 1,
  });
  checkAndClearHistogram(dwellHistogram, false);

  info("Search for 't'");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "t",
  });
  checkScalars({});
  dwellHistogram = checkAndClearHistogram(dwellHistogram, true);

  info("Search for 'te'");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "te",
  });
  checkScalars({});
  checkAndClearHistogram(dwellHistogram, false);

  info("Search for 't'");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "t",
  });
  checkScalars({});
  checkAndClearHistogram(dwellHistogram, false);

  info("Search for ''");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "",
  });
  checkScalars({
    [SCALARS.EXPOSURE]: 1,
  });
  checkAndClearHistogram(dwellHistogram, false);

  info("Blur urlbar and close view");
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  checkScalars({
    [SCALARS.ABANDONMENT]: 1,
  });
  checkAndClearHistogram(dwellHistogram, true);
});

// A zero prefix engagement should not be recorded when the view isn't showing
// zero prefix.
add_task(async function notZeroPrefix_engagement() {
  let dwellHistogram =
    TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_DWELL_TIME);

  await BrowserTestUtils.withNewTab("about:blank", async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "test",
    });
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;
  });

  checkScalars({});
  checkAndClearHistogram(dwellHistogram, false);
});

// A zero prefix abandonment should not be recorded when the view isn't showing
// zero prefix.
add_task(async function notZeroPrefix_abandonment() {
  let dwellHistogram =
    TelemetryTestUtils.getAndClearHistogram(HISTOGRAM_DWELL_TIME);

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
  });
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());

  checkScalars({});
  checkAndClearHistogram(dwellHistogram, false);
});

function checkScalars(expected) {
  let scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
  for (let scalar of Object.values(SCALARS)) {
    if (expected.hasOwnProperty(scalar)) {
      TelemetryTestUtils.assertScalar(scalars, scalar, expected[scalar]);
    } else {
      Assert.ok(
        !scalars.hasOwnProperty(scalar),
        "Scalar should not be recorded: " + scalar
      );
    }
  }
}

function checkAndClearHistogram(histogram, expected) {
  if (expected) {
    Assert.deepEqual(
      Object.values(histogram.snapshot().values).filter(v => v > 0),
      [1],
      "Dwell histogram should be updated"
    );
  } else {
    Assert.strictEqual(
      histogram.snapshot().sum,
      0,
      "Dwell histogram should not be updated"
    );
  }

  return TelemetryTestUtils.getAndClearHistogram(histogram.name());
}

async function showZeroPrefix() {
  let { promise, cleanup } = waitForQueryFinished();
  await SimpleTest.promiseFocus(window);
  await UrlbarTestUtils.promisePopupOpen(window, () =>
    document.getElementById("Browser:OpenLocation").doCommand()
  );
  await promise;
  cleanup();

  Assert.greater(
    UrlbarTestUtils.getResultCount(window),
    0,
    "There should be at least one row in the zero prefix view"
  );
}

/**
 * Returns a promise that's resolved on the next `onQueryFinished()`. It's
 * important to wait for `onQueryFinished()` because that's when the view checks
 * whether it's showing zero prefix.
 *
 * @returns {object}
 *   An object with the following properties:
 *     {Promise} promise
 *       Resolved when `onQueryFinished()` is called.
 *     {Function} cleanup
 *       This should be called to remove the listener.
 */
function waitForQueryFinished() {
  let deferred = Promise.withResolvers();
  let listener = {
    onQueryFinished: () => deferred.resolve(),
  };
  gURLBar.controller.addQueryListener(listener);

  return {
    promise: deferred.promise,
    cleanup() {
      gURLBar.controller.removeQueryListener(listener);
    },
  };
}

async function updateTopSitesAndAwaitChanged() {
  let url = "http://mochi.test:8888/topsite";
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits(url);
  }

  info("Updating top sites and awaiting newtab-top-sites-changed");
  let changedPromise = TestUtils.topicObserved("newtab-top-sites-changed").then(
    () => info("Observed newtab-top-sites-changed")
  );
  await updateTopSites(sites => sites?.length);
  await changedPromise;
}
