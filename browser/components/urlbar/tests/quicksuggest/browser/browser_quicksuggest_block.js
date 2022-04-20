/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests blocking quick suggest results, including best matches. See also:
//
// browser_bestMatch.js
//   Includes tests for blocking best match rows independent of quick suggest,
//   especially the superficial UI part that should be common to all types of
//   best matches

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  CONTEXTUAL_SERVICES_PING_TYPES:
    "resource:///modules/PartnerLinkAttribution.jsm",
});

const { timestampTemplate } = UrlbarProviderQuickSuggest;

// Include the timestamp template in the suggestion URLs so we can make sure
// their original URLs with the unreplaced templates are blocked and not their
// URLs with timestamps.
const SUGGESTIONS = [
  {
    id: 1,
    url: `http://example.com/sponsored?t=${timestampTemplate}`,
    title: "Sponsored suggestion",
    keywords: ["sponsored"],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    iab_category: "22 - Shopping",
  },
  {
    id: 2,
    url: `http://example.com/nonsponsored?t=${timestampTemplate}`,
    title: "Non-sponsored suggestion",
    keywords: ["nonsponsored"],
    click_url: "http://example.com/click",
    impression_url: "http://example.com/impression",
    advertiser: "TestAdvertiser",
    iab_category: "5 - Education",
  },
];

// Spy for the custom impression/click sender
let spy;

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.bestMatch.blockingEnabled", true],
      ["browser.urlbar.quicksuggest.blockingEnabled", true],
    ],
  });

  ({ spy } = QuickSuggestTestUtils.createTelemetryPingSpy());

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  await UrlbarProviderQuickSuggest._blockTaskQueue.emptyPromise;
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();

  Services.telemetry.clearScalars();
  Services.telemetry.clearEvents();

  await QuickSuggestTestUtils.ensureQuickSuggestInit(SUGGESTIONS);
});

/**
 * Adds a test task that runs the given callback with combinations of the
 * following:
 *
 * - Best match disabled and enabled
 * - Each suggestion in `SUGGESTIONS`
 *
 * @param {function} fn
 *   The callback function. It's passed: `{ isBestMatch, suggestion }`
 */
function add_combo_task(fn) {
  let taskFn = async () => {
    for (let isBestMatch of [false, true]) {
      UrlbarPrefs.set("bestMatch.enabled", isBestMatch);
      for (let suggestion of SUGGESTIONS) {
        info(
          `Running ${fn.name}: ${JSON.stringify({ isBestMatch, suggestion })}`
        );
        await fn({ isBestMatch, suggestion });
      }
      UrlbarPrefs.clear("bestMatch.enabled");
    }
  };
  Object.defineProperty(taskFn, "name", { value: fn.name });
  add_task(taskFn);
}

// Picks the block button with the keyboard.
add_combo_task(async function basic_keyboard({ suggestion, isBestMatch }) {
  await doBasicBlockTest({
    suggestion,
    isBestMatch,
    block: () => {
      // Arrow down twice to select the block button: once to select the main
      // part of the row, once to select the block button.
      EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
      EventUtils.synthesizeKey("KEY_Enter");
    },
  });
});

// Picks the block button with the mouse.
add_combo_task(async function basic_mouse({ suggestion, isBestMatch }) {
  await doBasicBlockTest({
    suggestion,
    isBestMatch,
    block: blockButton => {
      EventUtils.synthesizeMouseAtCenter(blockButton, {});
    },
  });
});

// Uses the key shortcut to block a suggestion.
add_combo_task(async function basic_keyShortcut({ suggestion, isBestMatch }) {
  await doBasicBlockTest({
    suggestion,
    isBestMatch,
    block: () => {
      // Arrow down once to select the row.
      EventUtils.synthesizeKey("KEY_ArrowDown");
      EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });
    },
  });
});

async function doBasicBlockTest({ suggestion, isBestMatch, block }) {
  spy.resetHistory();

  // Do a search that triggers the suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: suggestion.keywords[0],
  });
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    2,
    "Two rows are present after searching (heuristic + suggestion)"
  );

  let isSponsored = suggestion.keywords[0] == "sponsored";
  let details = await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    isBestMatch,
    isSponsored,
    originalUrl: suggestion.url,
  });

  // Block the suggestion.
  let blockButton = details.element.row._buttons.get("block");
  await block(blockButton);

  // The row should have been removed.
  Assert.ok(
    UrlbarTestUtils.isPopupOpen(window),
    "View remains open after blocking result"
  );
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    1,
    "Only one row after blocking suggestion"
  );
  await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);

  // The URL should be blocked.
  Assert.ok(
    await UrlbarProviderQuickSuggest.isSuggestionBlocked(suggestion.url),
    "Suggestion is blocked"
  );

  // Check telemetry scalars.
  let index = 2;
  let scalars = {
    [QuickSuggestTestUtils.SCALARS.IMPRESSION]: index,
  };
  if (isSponsored) {
    scalars[QuickSuggestTestUtils.SCALARS.BLOCK_SPONSORED] = index;
  } else {
    scalars[QuickSuggestTestUtils.SCALARS.BLOCK_NONSPONSORED] = index;
  }
  if (isBestMatch) {
    if (isSponsored) {
      scalars = {
        ...scalars,
        [QuickSuggestTestUtils.SCALARS.IMPRESSION_SPONSORED_BEST_MATCH]: index,
        [QuickSuggestTestUtils.SCALARS.BLOCK_SPONSORED_BEST_MATCH]: index,
      };
    } else {
      scalars = {
        ...scalars,
        [QuickSuggestTestUtils.SCALARS
          .IMPRESSION_NONSPONSORED_BEST_MATCH]: index,
        [QuickSuggestTestUtils.SCALARS.BLOCK_NONSPONSORED_BEST_MATCH]: index,
      };
    }
  }
  QuickSuggestTestUtils.assertScalars(scalars);

  // Check the engagement event.
  let match_type = isBestMatch ? "best-match" : "firefox-suggest";
  QuickSuggestTestUtils.assertEvents([
    {
      category: QuickSuggestTestUtils.TELEMETRY_EVENT_CATEGORY,
      method: "engagement",
      object: "block",
      extra: {
        match_type,
        position: String(index),
        suggestion_type: isSponsored ? "sponsored" : "nonsponsored",
      },
    },
  ]);

  // Check the custom telemetry pings.
  QuickSuggestTestUtils.assertPings(spy, [
    {
      type: CONTEXTUAL_SERVICES_PING_TYPES.QS_IMPRESSION,
      payload: {
        match_type,
        block_id: suggestion.id,
        is_clicked: false,
        position: index,
      },
    },
    {
      type: CONTEXTUAL_SERVICES_PING_TYPES.QS_BLOCK,
      payload: {
        match_type,
        block_id: suggestion.id,
        iab_category: suggestion.iab_category,
        position: index,
      },
    },
  ]);

  await UrlbarTestUtils.promisePopupClose(window);
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
}

// Blocks multiple suggestions one after the other.
add_task(async function blockMultiple() {
  for (let isBestMatch of [false, true]) {
    UrlbarPrefs.set("bestMatch.enabled", isBestMatch);
    info(`Testing with best match enabled: ${isBestMatch}`);

    for (let i = 0; i < SUGGESTIONS.length; i++) {
      // Do a search that triggers the i'th suggestion.
      let { keywords, url } = SUGGESTIONS[i];
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: keywords[0],
      });
      await QuickSuggestTestUtils.assertIsQuickSuggest({
        window,
        isBestMatch,
        originalUrl: url,
        isSponsored: keywords[0] == "sponsored",
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
    UrlbarPrefs.clear("bestMatch.enabled");
  }
});

// Tests with blocking disabled for both best matches and non-best-matches.
add_combo_task(async function disabled_both({ suggestion, isBestMatch }) {
  await doDisabledTest({
    suggestion,
    isBestMatch,
    quickSuggestBlockingEnabled: false,
    bestMatchBlockingEnabled: false,
  });
});

// Tests with blocking disabled only for non-best-matches.
add_combo_task(async function disabled_quickSuggest({
  suggestion,
  isBestMatch,
}) {
  await doDisabledTest({
    suggestion,
    isBestMatch,
    quickSuggestBlockingEnabled: false,
    bestMatchBlockingEnabled: true,
  });
});

// Tests with blocking disabled only for best matches.
add_combo_task(async function disabled_bestMatch({ suggestion, isBestMatch }) {
  await doDisabledTest({
    suggestion,
    isBestMatch,
    quickSuggestBlockingEnabled: true,
    bestMatchBlockingEnabled: false,
  });
});

async function doDisabledTest({
  suggestion,
  isBestMatch,
  bestMatchBlockingEnabled,
  quickSuggestBlockingEnabled,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.bestMatch.blockingEnabled", bestMatchBlockingEnabled],
      [
        "browser.urlbar.quicksuggest.blockingEnabled",
        quickSuggestBlockingEnabled,
      ],
    ],
  });

  // Do a search to show a suggestion.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: suggestion.keywords[0],
  });
  let expectedResultCount = 2;
  Assert.equal(
    UrlbarTestUtils.getResultCount(window),
    expectedResultCount,
    "Two rows are present after searching (heuristic + suggestion)"
  );
  let details = await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    isBestMatch,
    originalUrl: suggestion.url,
    isSponsored: suggestion.keywords[0] == "sponsored",
  });
  let blockButton = details.element.row._buttons.get("block");

  // Arrow down to select the suggestion and press the key shortcut to block.
  EventUtils.synthesizeKey("KEY_ArrowDown");
  EventUtils.synthesizeKey("KEY_Delete", { shiftKey: true });
  Assert.ok(
    UrlbarTestUtils.isPopupOpen(window),
    "View remains open after trying to block result"
  );

  if (
    (isBestMatch && !bestMatchBlockingEnabled) ||
    (!isBestMatch && !quickSuggestBlockingEnabled)
  ) {
    // Blocking is disabled. The key shortcut shouldn't have done anything.
    Assert.ok(!blockButton, "Block button is not present");
    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      expectedResultCount,
      "Same number of results after key shortcut"
    );
    await QuickSuggestTestUtils.assertIsQuickSuggest({
      window,
      isBestMatch,
      originalUrl: suggestion.url,
      isSponsored: suggestion.keywords[0] == "sponsored",
    });
    Assert.ok(
      !(await UrlbarProviderQuickSuggest.isSuggestionBlocked(suggestion.url)),
      "Suggestion is not blocked"
    );
  } else {
    // Blocking is enabled. The suggestion should have been blocked.
    Assert.ok(blockButton, "Block button is present");
    Assert.equal(
      UrlbarTestUtils.getResultCount(window),
      1,
      "Only one row after blocking suggestion"
    );
    await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
    Assert.ok(
      await UrlbarProviderQuickSuggest.isSuggestionBlocked(suggestion.url),
      "Suggestion is blocked"
    );
    await UrlbarProviderQuickSuggest.clearBlockedSuggestions();
  }

  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
}
