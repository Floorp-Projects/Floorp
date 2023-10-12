/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests primary telemetry for navigational suggestions, a.k.a.
 * navigational top picks.
 */

"use strict";

const { TELEMETRY_SCALARS } = UrlbarProviderQuickSuggest;

const MERINO_SUGGESTION = {
  title: "Navigational suggestion",
  url: "https://example.com/navigational-suggestion",
  provider: "top_picks",
  is_sponsored: false,
  score: 0.25,
  block_id: 0,
  is_top_pick: true,
};

const suggestion_type = "navigational";
const index = 1;
const position = index + 1;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable tab-to-search since like best match it's also shown with
      // `suggestedIndex` = 1.
      ["browser.urlbar.suggest.engines", false],
    ],
  });

  await setUpTelemetryTest({
    merinoSuggestions: [MERINO_SUGGESTION],
  });
});

// Clicks the heuristic when a nav suggestion is not matched
add_task(async function notMatched_clickHeuristic() {
  await doTest({
    suggestion: null,
    shouldBeShown: false,
    pickRowIndex: 0,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_NOTMATCHED]: "search_engine",
      [TELEMETRY_SCALARS.CLICK_NAV_NOTMATCHED]: "search_engine",
    },
    events: [],
  });
});

// Clicks a non-heuristic row when a nav suggestion is not matched
add_task(async function notMatched_clickOther() {
  await PlacesTestUtils.addVisits("http://mochi.test:8888/example");
  await doTest({
    suggestion: null,
    shouldBeShown: false,
    pickRowIndex: 1,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_NOTMATCHED]: "search_engine",
    },
    events: [],
  });
});

// Clicks the heuristic when a nav suggestion is shown
add_task(async function shown_clickHeuristic() {
  await doTest({
    suggestion: MERINO_SUGGESTION,
    shouldBeShown: true,
    pickRowIndex: 0,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_SHOWN]: "search_engine",
      [TELEMETRY_SCALARS.CLICK_NAV_SHOWN_HEURISTIC]: "search_engine",
    },
    events: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "impression_only",
        extra: {
          suggestion_type,
          match_type: "best-match",
          position: position.toString(),
          source: "merino",
        },
      },
    ],
  });
});

// Clicks the nav suggestion
add_task(async function shown_clickNavSuggestion() {
  await doTest({
    suggestion: MERINO_SUGGESTION,
    shouldBeShown: true,
    pickRowIndex: index,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_SHOWN]: "search_engine",
      [TELEMETRY_SCALARS.CLICK_NAV_SHOWN_NAV]: "search_engine",
      "urlbar.picked.navigational": "1",
    },
    events: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "click",
        extra: {
          suggestion_type,
          match_type: "best-match",
          position: position.toString(),
          source: "merino",
        },
      },
    ],
  });
});

// Clicks a non-heuristic non-nav-suggestion row when the nav suggestion is
// shown
add_task(async function shown_clickOther() {
  await PlacesTestUtils.addVisits("http://mochi.test:8888/example");
  await doTest({
    suggestion: MERINO_SUGGESTION,
    shouldBeShown: true,
    pickRowIndex: 2,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_SHOWN]: "search_engine",
    },
    events: [
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "impression_only",
        extra: {
          suggestion_type,
          match_type: "best-match",
          position: position.toString(),
          source: "merino",
        },
      },
    ],
  });
});

// Clicks the heuristic when it dupes the nav suggestion
add_task(async function duped_clickHeuristic() {
  // Add enough visits to example.com so it autofills.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits("https://example.com/");
  }

  // Set the nav suggestion's URL to the same URL, example.com.
  let suggestion = {
    ...MERINO_SUGGESTION,
    url: "https://example.com/",
  };

  await doTest({
    suggestion,
    shouldBeShown: false,
    pickRowIndex: 0,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_SUPERCEDED]: "autofill_origin",
      [TELEMETRY_SCALARS.CLICK_NAV_SUPERCEDED]: "autofill_origin",
    },
    events: [],
  });
});

// Clicks a non-heuristic row when the heuristic dupes the nav suggestion
add_task(async function duped_clickOther() {
  // Add enough visits to example.com so it autofills.
  for (let i = 0; i < 5; i++) {
    await PlacesTestUtils.addVisits("https://example.com/");
  }

  // Set the nav suggestion's URL to the same URL, example.com.
  let suggestion = {
    ...MERINO_SUGGESTION,
    url: "https://example.com/",
  };

  // Add a visit to another URL so it appears in the search below.
  await PlacesTestUtils.addVisits("https://example.com/some-other-url");

  await doTest({
    suggestion,
    shouldBeShown: false,
    pickRowIndex: 1,
    scalars: {
      [TELEMETRY_SCALARS.IMPRESSION_NAV_SUPERCEDED]: "autofill_origin",
    },
    events: [],
  });
});

// Telemetry specific to nav suggestions should not be recorded when the
// `recordNavigationalSuggestionTelemetry` Nimbus variable is false.
add_task(async function recordNavigationalSuggestionTelemetry_false() {
  await doTest({
    valueOverrides: {
      recordNavigationalSuggestionTelemetry: false,
    },
    suggestion: MERINO_SUGGESTION,
    shouldBeShown: true,
    pickRowIndex: index,
    scalars: {},
    events: [
      // The legacy engagement event should still be recorded as it is for all
      // quick suggest results.
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "click",
        extra: {
          suggestion_type,
          match_type: "best-match",
          position: position.toString(),
          source: "merino",
        },
      },
    ],
  });
});

// Telemetry specific to nav suggestions should not be recorded when the
// `recordNavigationalSuggestionTelemetry` Nimbus variable is left out.
add_task(async function recordNavigationalSuggestionTelemetry_undefined() {
  await doTest({
    valueOverrides: {},
    suggestion: MERINO_SUGGESTION,
    shouldBeShown: true,
    pickRowIndex: index,
    scalars: {},
    events: [
      // The legacy engagement event should still be recorded as it is for all
      // quick suggest results.
      {
        category: QuickSuggest.TELEMETRY_EVENT_CATEGORY,
        method: "engagement",
        object: "click",
        extra: {
          suggestion_type,
          match_type: "best-match",
          position: position.toString(),
          source: "merino",
        },
      },
    ],
  });
});

/**
 * Does the following:
 *
 * 1. Sets up a Merino nav suggestion
 * 2. Enrolls in a Nimbus experiment with the specified variables
 * 3. Does a search
 * 4. Makes sure the nav suggestion is or isn't shown as expected
 * 5. Clicks a specified row
 * 6. Makes sure the expected telemetry is recorded
 *
 * @param {object} options
 *   Options object
 * @param {object} options.suggestion
 *   The nav suggestion or null if Merino shouldn't serve one.
 * @param {boolean} options.shouldBeShown
 *   Whether the nav suggestion is expected to be shown.
 * @param {number} options.pickRowIndex
 *   The index of the row to pick.
 * @param {object} options.scalars
 *   An object that specifies the nav suggest keyed scalars that are expected to
 *   be recorded.
 * @param {Array} options.events
 *   An object that specifies the legacy engagement events that are expected to
 *   be recorded.
 * @param {object} options.valueOverrides
 *   The Nimbus variables to use.
 */
async function doTest({
  suggestion,
  shouldBeShown,
  pickRowIndex,
  scalars,
  events,
  valueOverrides = {
    recordNavigationalSuggestionTelemetry: true,
  },
}) {
  await PlacesFrecencyRecalculator.recalculateAnyOutdatedFrecencies();
  MerinoTestUtils.server.response.body.suggestions = suggestion
    ? [suggestion]
    : [];

  Services.telemetry.clearEvents();
  let { spy, spyCleanup } = QuickSuggestTestUtils.createTelemetryPingSpy();

  await QuickSuggestTestUtils.withExperiment({
    valueOverrides,
    callback: async () => {
      await BrowserTestUtils.withNewTab("about:blank", async () => {
        gURLBar.focus();
        await UrlbarTestUtils.promiseAutocompleteResultPopup({
          window,
          value: "example",
          fireInputEvent: true,
        });

        if (shouldBeShown) {
          await QuickSuggestTestUtils.assertIsQuickSuggest({
            window,
            index,
            url: suggestion.url,
            isBestMatch: true,
            isSponsored: false,
          });
        } else {
          await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
        }

        let loadPromise = BrowserTestUtils.browserLoaded(
          gBrowser.selectedBrowser
        );
        if (pickRowIndex > 0) {
          info("Arrowing down to row index " + pickRowIndex);
          EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: pickRowIndex });
        }
        info("Pressing Enter and waiting for page load");
        EventUtils.synthesizeKey("KEY_Enter");
        await loadPromise;
      });
    },
  });

  info("Checking scalars");
  QuickSuggestTestUtils.assertScalars(scalars);

  info("Checking events");
  QuickSuggestTestUtils.assertEvents(events);

  info("Checking pings");
  QuickSuggestTestUtils.assertPings(spy, []);

  await spyCleanup();
  await PlacesUtils.history.clear();
  MerinoTestUtils.server.response.body.suggestions = [MERINO_SUGGESTION];
}
