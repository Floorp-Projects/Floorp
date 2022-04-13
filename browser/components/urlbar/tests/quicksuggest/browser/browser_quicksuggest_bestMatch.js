/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Browser test for the best match feature as it relates to quick suggest. See
// also:
//
// browser_bestMatch.js
//   Basic view test for best match rows independent of quick suggest
// test_quicksuggest_bestMatch.js
//   Tests triggering quick suggest best matches and things that don't depend on
//   the view

"use strict";

const SUGGESTIONS = [1, 2, 3].map(i => ({
  id: i,
  title: `Best match ${i}`,
  url: `http://example.com/bestmatch${i}`,
  keywords: [`bestmatch${i}`],
  click_url: "http://example.com/click",
  impression_url: "http://example.com/impression",
  advertiser: "TestAdvertiser",
  _test_is_best_match: true,
}));

const NON_BEST_MATCH_SUGGESTION = {
  id: 99,
  title: "Non-best match",
  url: "http://example.com/nonbestmatch",
  keywords: ["non"],
  click_url: "http://example.com/click",
  impression_url: "http://example.com/impression",
  advertiser: "TestAdvertiser",
};

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  await UrlbarProviderQuickSuggest._blockTaskQueue.emptyPromise;
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();

  await QuickSuggestTestUtils.ensureQuickSuggestInit(
    SUGGESTIONS.concat(NON_BEST_MATCH_SUGGESTION)
  );
});

// When the user is enrolled in a best match experiment with the feature enabled
// (i.e., the treatment branch), the Nimbus exposure event should be recorded
// after triggering a best match.
add_task(async function nimbusExposure_featureEnabled() {
  await doNimbusExposureTest({
    bestMatchEnabled: true,
    bestMatchExpected: true,
    isBestMatchExperiment: true,
    exposureEventExpected: true,
  });
  await doNimbusExposureTest({
    bestMatchEnabled: true,
    bestMatchExpected: true,
    experimentType: "best-match",
    exposureEventExpected: true,
  });
});

// When the user is enrolled in a best match experiment with the feature enabled
// (i.e., the treatment branch) but the user disabled best match, the Nimbus
// exposure event should not be recorded at all.
add_task(async function nimbusExposure_featureEnabled_userDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.bestmatch", false]],
  });

  await doNimbusExposureTest({
    bestMatchEnabled: true,
    bestMatchExpected: false,
    isBestMatchExperiment: true,
    exposureEventExpected: false,
  });
  await doNimbusExposureTest({
    bestMatchEnabled: true,
    bestMatchExpected: false,
    experimentType: "best-match",
    exposureEventExpected: false,
  });

  await SpecialPowers.popPrefEnv();
});

// When the user is enrolled in a best match experiment with the feature
// disabled (i.e., the control branch), the Nimbus exposure event should be
// recorded when the user would have triggered a best match.
add_task(async function nimbusExposure_featureDisabled() {
  await doNimbusExposureTest({
    bestMatchEnabled: false,
    bestMatchExpected: false,
    isBestMatchExperiment: true,
    exposureEventExpected: true,
  });
  await doNimbusExposureTest({
    bestMatchEnabled: false,
    bestMatchExpected: false,
    experimentType: "best-match",
    exposureEventExpected: true,
  });
});

add_task(async function nimbusExposure_notBestMatchExperimentType() {
  await doNimbusExposureTest({
    bestMatchEnabled: false,
    bestMatchExpected: false,
    skipFirstSearch: true,
    experimentType: "",
    exposureEventExpected: true,
  });
  await doNimbusExposureTest({
    bestMatchEnabled: false,
    bestMatchExpected: false,
    skipFirstSearch: true,
    exposureEventExpected: true,
  });
  await doNimbusExposureTest({
    bestMatchEnabled: false,
    bestMatchExpected: false,
    experimentType: "modal",
    exposureEventExpected: false,
  });
});

/**
 * Installs a mock experiment, triggers best match, and asserts that the Nimbus
 * exposure event was or was not recorded appropriately.
 *
 * @param {boolean} bestMatchEnabled
 *   The value to set for the experiment's `bestMatchEnabled` Nimbus variable.
 * @param {boolean} bestMatchExpected
 *   Whether a best match result is expected to be shown.
 * @param {boolean} exposureEventExpected
 *   Whether an exposure event is expected to be recorded.
 */
async function doNimbusExposureTest({
  bestMatchEnabled,
  bestMatchExpected,
  experimentType,
  isBestMatchExperiment,
  skipFirstSearch,
  exposureEventExpected,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", false]],
  });
  await QuickSuggestTestUtils.clearExposureEvent();
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      bestMatchEnabled,
      experimentType,
      isBestMatchExperiment,
    },
    callback: async () => {
      // No exposure event should be recorded after only enrolling.
      await QuickSuggestTestUtils.assertExposureEvent(false);

      // Do a search that doesn't trigger a best match. No exposure event should
      // be recorded.
      if (!skipFirstSearch) {
        info("Doing first search");
        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window,
          value: NON_BEST_MATCH_SUGGESTION.keywords[0],
          fireInputEvent: true,
        });
        await QuickSuggestTestUtils.assertIsQuickSuggest({
          window,
          url: NON_BEST_MATCH_SUGGESTION.url,
        });
        await UrlbarTestUtils.promisePopupClose(window);

        await QuickSuggestTestUtils.assertExposureEvent(false);
      }

      // Do a search that triggers (or would have triggered) a best match. The
      // exposure event should be recorded.
      info("Doing second search");
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: SUGGESTIONS[0].keywords[0],
        fireInputEvent: true,
      });
      await QuickSuggestTestUtils.assertIsQuickSuggest({
        window,
        originalUrl: SUGGESTIONS[0].url,
        isBestMatch: bestMatchExpected,
      });
      await QuickSuggestTestUtils.assertExposureEvent(
        exposureEventExpected,
        "control"
      );

      await UrlbarTestUtils.promisePopupClose(window);
    },
  });

  await SpecialPowers.popPrefEnv();
}
