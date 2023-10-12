/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// End-to-end browser smoke test for Merino sessions. More comprehensive tests
// are in test_quicksuggest_merinoSessions.js. This test essentially makes sure
// engagements occur as expected when interacting with the urlbar. If you need
// to add tests that do not depend on a new definition of "engagement", consider
// adding them to test_quicksuggest_merinoSessions.js instead.

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quicksuggest.dataCollection.enabled", true]],
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  // Install a mock default engine so we don't hit the network.
  await SearchTestUtils.installSearchExtension({}, { setAsDefault: true });

  await MerinoTestUtils.server.start();
});

// In a single engagement, all requests should use the same session ID and the
// sequence number should be incremented.
add_task(async function singleEngagement() {
  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: i,
        },
      },
    ]);
  }

  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
});

// In a single engagement, all requests should use the same session ID and the
// sequence number should be incremented. This task closes the panel between
// searches but keeps the input focused, so the engagement should not end.
add_task(async function singleEngagement_panelClosed() {
  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: i,
        },
      },
    ]);

    EventUtils.synthesizeKey("KEY_Escape");
    Assert.ok(!UrlbarTestUtils.isPopupOpen(window), "Panel is closed");
    Assert.ok(gURLBar.focused, "Input remains focused");
  }

  // End the engagement to reset the session for the next test.
  gURLBar.blur();
});

// New engagements should not use the same session ID as previous engagements
// and the sequence number should be reset. This task completes each engagement
// successfully.
add_task(async function manyEngagements_engagement() {
  for (let i = 0; i < 3; i++) {
    // Open a new tab since we'll load the mock default search engine page.
    await BrowserTestUtils.withNewTab("about:blank", async () => {
      let searchString = "search" + i;
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: searchString,
        fireInputEvent: true,
      });

      MerinoTestUtils.server.checkAndClearRequests([
        {
          params: {
            [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString,
            [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
          },
        },
      ]);

      // Press enter on the heuristic result to load the search engine page and
      // complete the engagement.
      let loadPromise = BrowserTestUtils.browserLoaded(
        gBrowser.selectedBrowser
      );
      EventUtils.synthesizeKey("KEY_Enter");
      await loadPromise;
    });
  }
});

// New engagements should not use the same session ID as previous engagements
// and the sequence number should be reset. This task abandons each engagement.
add_task(async function manyEngagements_abandonment() {
  for (let i = 0; i < 3; i++) {
    let searchString = "search" + i;
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    MerinoTestUtils.server.checkAndClearRequests([
      {
        params: {
          [MerinoTestUtils.SEARCH_PARAMS.QUERY]: searchString,
          [MerinoTestUtils.SEARCH_PARAMS.SEQUENCE_NUMBER]: 0,
        },
      },
    ]);

    // Blur the urlbar to abandon the engagement.
    await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  }
});
