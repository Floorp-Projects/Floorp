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

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.bestMatch.enabled", true]],
  });

  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  await UrlbarProviderQuickSuggest._blockTaskQueue.emptyPromise;
  await UrlbarProviderQuickSuggest.clearBlockedSuggestions();

  await QuickSuggestTestUtils.ensureQuickSuggestInit(SUGGESTIONS);
});

// Picks the block button with the keyboard.
add_task(async function basicBlock_keyboard() {
  await doBasicBlockTest(true);
});

// Picks the block button with the mouse.
add_task(async function basicBlock_mouse() {
  await doBasicBlockTest(false);
});

async function doBasicBlockTest(useKeyboard) {
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

  // Pick the block button.
  if (useKeyboard) {
    // Arrow down twice to select the block button: once to select the main part
    // of the best match row, once to select the block button.
    EventUtils.synthesizeKey("KEY_ArrowDown", { repeat: 2 });
    EventUtils.synthesizeKey("KEY_Enter");
  } else {
    EventUtils.synthesizeMouseAtCenter(
      details.element.row._buttons.get("block"),
      {}
    );
  }

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
