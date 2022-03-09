/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests quick suggest best match results. See also:
//
// browser_bestMatch.js
//   Basic view test for best match rows independent of quick suggest
// test_quicksuggest_bestMatch.js
//   Tests triggering quick suggest best matches and things that don't depend on
//   the view

"use strict";

const { timestampTemplate } = UrlbarProviderQuickSuggest;

const SUGGESTIONS = [1, 2, 3].map(i => ({
  id: i,
  title: `Best match ${i}`,
  // Include the timestamp template in the suggestion URLs so we can make sure
  // their original URLs with the unreplaced templates are blocked and not their
  // URLs with timestamps.
  url: `http://example.com/bestmatch${i}?t=${timestampTemplate}`,
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
    set: [
      ["browser.urlbar.bestMatch.enabled", true],
      ["browser.urlbar.bestMatch.blockingEnabled", true],
    ],
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

// Picks the block button with the keyboard.
add_task(async function basicBlock_keyboard() {
  await doBasicBlockTest(() => {
    // Arrow down twice to select the block button: once to select the main
    // part of the best match row, once to select the block button.
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
    EventUtils.synthesizeKey("KEY_Enter");
  });
});

// Picks the block button with the mouse.
add_task(async function basicBlock_mouse() {
  await doBasicBlockTest(blockButton => {
    EventUtils.synthesizeMouseAtCenter(blockButton, {});
  });
});

// Uses the key shortcut to block a best match.
add_task(async function basicBlock_keyShortcut() {
  await doBasicBlockTest(() => {
    // Arrow down once to select the best match row.
    EventUtils.synthesizeKey("KEY_ArrowDown");
    EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });
  });
});

async function doBasicBlockTest(doBlock) {
  // Do a search that triggers the best match.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: SUGGESTIONS[0].keywords[0],
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Two rows are present after searching (heuristic + best match)"
  );

  let details = await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    originalUrl: SUGGESTIONS[0].url,
    isBestMatch: true,
  });

  // Block the suggestion.
  let blockButton = details.element.row._buttons.get("block");
  doBlock(blockButton);

  // The row should have been removed.
  Assert.ok(
    UrlbarTestUtils.isPopupOpen(window),
    "View remains open after blocking result"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "Only one row after blocking best match"
  );
  await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);

  // The URL should be blocked.
  Assert.ok(
    await UrlbarProviderQuickSuggest.isSuggestionBlocked(SUGGESTIONS[0].url),
    "Suggestion is blocked"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
}

// Blocks multiple suggestions one after the other.
add_task(async function blockMultiple() {
  for (let i = 0; i < SUGGESTIONS.length; i++) {
    // Do a search that triggers the i'th best match.
    let { keywords, url } = SUGGESTIONS[i];
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: keywords[0],
    });
    await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      originalUrl: url,
      isBestMatch: true,
    });

    // Block it.
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
    EventUtils.synthesizeKey("KEY_Enter");
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(url),
      "Suggestion is blocked after picking block button"
    );

    // Make sure all previous suggestions remain blocked and no other
    // suggestions are blocked yet.
    for (let j = 0; j < SUGGESTIONS.length; j++) {
      Assert.equal(
        await UrlbarProviderQuickSuggest.isSuggestionBlocked(
          SUGGESTIONS[j].url
        ),
        j <= i,
        `Suggestion at index ${j} is blocked or not as expected`
      );
    }
  }

  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
});

// Tests with blocking disabled.
add_task(async function blockingDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.blockingEnabled", false]],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: SUGGESTIONS[0].keywords[0],
  });

  let expectedResultCount = 2;
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    expectedResultCount,
    "Two rows are present after searching (heuristic + best match)"
  );

  // `assertIsQuickSuggest()` asserts that the block button is not present when
  // `bestMatch.blockingEnabled` is false, but check it again here since it's
  // central to this test.
  let details = await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    originalUrl: SUGGESTIONS[0].url,
    isBestMatch: true,
  });
  Assert.ok(
    !details.element.row._buttons.get("block"),
    "Block button is not present"
  );

  // Arrow down once to select the best match row and then press the key
  // shortcut to block.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });

  // Nothing should happen.
  Assert.ok(
    UrlbarTestUtils.isPopupOpen(window),
    "View remains open after key shortcut"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    expectedResultCount,
    "Same number of results after key shortcut"
  );
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    originalUrl: SUGGESTIONS[0].url,
    isBestMatch: true,
  });
  Assert.ok(
    !(await UrlbarProviderQuickSuggest.isSuggestionBlocked(SUGGESTIONS[0].url)),
    "Suggestion is not blocked"
  );

  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
});

// When the user is enrolled in a best match experiment with the feature enabled
// (i.e., the treatment branch), the Nimbus exposure event should be recorded
// after triggering a best match.
add_task(async function nimbusExposure_featureEnabled() {
  await doNimbusExposureTest({
    bestMatchEnabled: true,
    bestMatchExpected: true,
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
    exposureEventExpected: true,
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
  exposureEventExpected,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", false]],
  });
  await QuickSuggestTestUtils.clearExposureEvent();
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      bestMatchEnabled,
      isBestMatchExperiment: true,
    },
    callback: async () => {
      // No exposure event should be recorded after only enrolling.
      await QuickSuggestTestUtils.assertExposureEvent(false);

      // Do a search that doesn't trigger a best match. No exposure event should
      // be recorded.
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
