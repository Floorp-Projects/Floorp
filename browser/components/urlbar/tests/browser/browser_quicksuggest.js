/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests browser quick suggestions.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const TEST_URL =
  "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/quicksuggest.sjs";

const TEST_DATA = [
  {
    id: 1,
    url: `${TEST_URL}?q=frabbits`,
    title: "frabbits",
    keywords: ["frab"],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
  },
];

const ABOUT_BLANK = "about:blank";
const SUGGESTIONS_PREF = "browser.search.suggest.enabled";
const PRIVATE_SUGGESTIONS_PREF = "browser.search.suggest.enabled.private";

const ONBOARDING_COUNT_PREF = "quicksuggest.onboardingCount";
const ONBOARDING_MAX_COUNT_PREF = "quicksuggest.onboardingMaxCount";

function sleep(ms) {
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Asserts that a result is a Quick Suggest result.
 *
 * @param {number} [index]
 *   The expected index of the Quick Suggest result.  Pass -1 to use the index
 *   of the last result.
 * @param {object} [win]
 *   The window in which to read the results from.
 * @returns {result}
 *   The result at the given index.
 */
async function assertIsQuickSuggest(index = -1, win = window) {
  if (index < 0) {
    index = UrlbarTestUtils.getResultCount(win) - 1;
    Assert.greater(index, -1, "Sanity check: Result count should be > 0");
  }
  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, index);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);
  Assert.equal(result.url, `${TEST_URL}?q=frabbits`);
  Assert.ok(result.isSponsored, "Result isSponsored");
  return result;
}

/**
 * Asserts that none of the results are Quick Suggest results.
 */
async function assertNoQuickSuggestResults() {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
    let r = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.ok(
      r.type != UrlbarUtils.RESULT_TYPE.URL ||
        !r.url.includes(TEST_URL) ||
        !r.isSponsored,
      `Result at index ${i} should not be a QuickSuggest result`
    );
  }
}

/**
 * Sets the onboarding-count pref to zero.
 */
function resetOnboardingCount() {
  UrlbarPrefs.clear(ONBOARDING_COUNT_PREF);
  Assert.equal(
    UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
    0,
    "Sanity check: Initial onboarding count is zero"
  );
}

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.searches", true],
    ],
  });

  // Add a mock engine so we don't hit the network loading the SERP.
  let engine = await Services.search.addEngineWithDetails("Test", {
    template: "http://example.com/?search={searchTerms}",
  });
  let oldDefaultEngine = await Services.search.getDefault();
  Services.search.setDefault(engine);

  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);

  registerCleanupFunction(async function() {
    Services.search.setDefault(oldDefaultEngine);
    await Services.search.removeEngine(engine);
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
  });
});

add_task(async function basic_test() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_BLANK);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
  });
  await assertIsQuickSuggest(1);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function test_case_insensitive() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: " Frab",
  });
  await assertIsQuickSuggest(1);
  await UrlbarTestUtils.promisePopupClose(window);
});

add_task(async function test_suggestions_disabled() {
  await SpecialPowers.pushPrefEnv({ set: [[SUGGESTIONS_PREF, false]] });
  await BrowserTestUtils.openNewForegroundTab(gBrowser, ABOUT_BLANK);
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
  });
  // We can't waitForResultAt because we don't want a result, give enough time
  // that a result would most likely have appeared.
  await sleep(100);
  Assert.ok(
    window.gURLBar.view._rows.children.length == 1,
    "There are no additional suggestions"
  );
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_suggestions_disabled_private() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SUGGESTIONS_PREF, true],
      [PRIVATE_SUGGESTIONS_PREF, false],
    ],
  });

  let window = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
  });
  await sleep(100);
  Assert.ok(
    window.gURLBar.view._rows.children.length == 1,
    "There are no additional suggestions"
  );
  await BrowserTestUtils.closeWindow(window);
  await SpecialPowers.popPrefEnv();
});

add_task(async function test_suggestions_enabled_private() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [SUGGESTIONS_PREF, true],
      [PRIVATE_SUGGESTIONS_PREF, true],
    ],
  });

  let win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window: win,
    value: "frab",
  });
  await assertIsQuickSuggest(-1, win);
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

// Starts an engagement with a query that doesn't trigger the Quick Suggest
// result and then ends the engagement with a query that does trigger it.  The
// onboarding count should be incremented.
add_task(async function onboarding_endOfEngagement() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    resetOnboardingCount();

    // Start an engagement with a query that doesn't trigger the Quick Suggest
    // result.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "bogus",
      fireInputEvent: true,
    });
    await assertNoQuickSuggestResults();
    Assert.equal(
      UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
      0,
      "Onboarding count should remain zero"
    );

    // Continue the engagement with a query that does trigger the result.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "frab",
      fireInputEvent: true,
    });
    let result = await assertIsQuickSuggest();
    let helpButton = result.element.row._elements.get("helpButton");
    Assert.ok(helpButton, "The help button should be present");
    Assert.equal(
      UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
      0,
      "Onboarding count should remain zero before engagement ends"
    );

    // Pick a result to end the engagement.  The onboarding count should be
    // incremented.
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    Assert.equal(
      UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
      1,
      "Onboarding count should be incremented after engagement ends"
    );
  });

  await PlacesUtils.history.clear();
});

// Starts an engagement with a query that triggers the Quick Suggest result and
// then ends the engagement with a query that doesn't trigger it.  The
// onboarding count should not be incremented.
add_task(async function onboarding_notEndOfEngagement() {
  resetOnboardingCount();

  // Start an engagement with a query that triggers the Quick Suggest result.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
    fireInputEvent: true,
  });
  let result = await assertIsQuickSuggest(1);
  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The help button should be present");
  Assert.equal(
    UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
    0,
    "Onboarding count should remain zero"
  );

  // Continue the engagement with a query that doesn't trigger the result.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "bogus",
    fireInputEvent: true,
  });
  await assertNoQuickSuggestResults();
  Assert.equal(
    UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
    0,
    "Onboarding count should remain zero"
  );

  // End the engagement.  The onboarding count should remain zero.
  await UrlbarTestUtils.promisePopupClose(window, () => {
    gURLBar.blur();
  });
  Assert.equal(
    UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
    0,
    "Onboarding count should be remain zero after engagement ends"
  );
});

// Starts an engagement with a query that triggers the Quick Suggest result but
// abandons the engagement.  The onboarding count should not be incremented.
add_task(async function onboarding_abandonment() {
  resetOnboardingCount();

  // Start an engagement with a query that triggers the Quick Suggest result.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "frab",
    fireInputEvent: true,
  });
  let result = await assertIsQuickSuggest(1);
  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The help button should be present");
  Assert.equal(
    UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
    0,
    "Onboarding count should remain zero"
  );

  // Abandon the engagement.  The onboarding count should remain zero.
  await UrlbarTestUtils.promisePopupClose(window, () => {
    gURLBar.blur();
  });
  Assert.equal(
    UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
    0,
    "Onboarding count should be remain zero after abandoning engagement"
  );
});

// Makes sure the onboarding help button appears the correct number of times.
add_task(async function onboarding_maxCount() {
  await BrowserTestUtils.withNewTab("about:blank", async () => {
    resetOnboardingCount();

    let maxCount = UrlbarPrefs.get(ONBOARDING_MAX_COUNT_PREF);
    Assert.greater(
      maxCount,
      0,
      "Sanity check: Default onboarding max count pref exists and is > 0"
    );

    // Complete maxCount engagements while showing a QuickSuggest result.
    for (let count = 1; count <= maxCount; count++) {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "frab",
        fireInputEvent: true,
      });
      let result = await assertIsQuickSuggest();
      let helpButton = result.element.row._elements.get("helpButton");
      Assert.ok(helpButton, "The help button should be present");

      await UrlbarTestUtils.promisePopupClose(window, () => {
        EventUtils.synthesizeKey("KEY_Enter");
      });
      Assert.equal(
        UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
        count,
        "Onboarding count should be incremented after the engagement ends"
      );

      // Do another engagement without showing a QuickSuggest result.  The count
      // should not be incremented.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "bogus",
        fireInputEvent: true,
      });
      await assertNoQuickSuggestResults();
      await UrlbarTestUtils.promisePopupClose(window, () => {
        EventUtils.synthesizeKey("KEY_Enter");
      });
      Assert.equal(
        UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
        count,
        "Onboarding count should remain the same after not showing a QS result"
      );
    }

    // Do one more engagement.  Since the onboarding count has reached the max,
    // the help button should be absent and the count shouldn't be incremented
    // again.
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: "frab",
      fireInputEvent: true,
    });
    let result = await assertIsQuickSuggest();
    let helpButton = result.element.row._elements.get("helpButton");
    Assert.ok(!helpButton, "The help button should be absent");
    await UrlbarTestUtils.promisePopupClose(window, () => {
      EventUtils.synthesizeKey("KEY_Enter");
    });
    Assert.equal(
      UrlbarPrefs.get(ONBOARDING_COUNT_PREF),
      maxCount,
      "Onboarding count should remain the max count"
    );
  });

  await PlacesUtils.history.clear();
});
