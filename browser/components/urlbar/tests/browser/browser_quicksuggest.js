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
    keywords: ["fra", "frab"],
    click_url: "http://click.reporting.test.com/",
    impression_url: "http://impression.reporting.test.com/",
    advertiser: "TestAdvertiser",
  },
  {
    id: 2,
    url: `${TEST_URL}?q=nonsponsored`,
    title: "Non-Sponsored",
    keywords: ["nonspon"],
    click_url: "http://click.reporting.test.com/nonsponsored",
    impression_url: "http://impression.reporting.test.com/nonsponsored",
    advertiser: "TestAdvertiserNonSponsored",
    iab_category: "5 - Education",
  },
];

const SEEN_DIALOG_PREF = "browser.urlbar.quicksuggest.showedOnboardingDialog";

/**
 * Asserts that none of the results are Quick Suggest results.
 *
 * @param {window} [win]
 */
async function assertNoQuickSuggestResults(win = window) {
  for (let i = 0; i < UrlbarTestUtils.getResultCount(win); i++) {
    let r = await UrlbarTestUtils.getDetailsOfResultAt(win, i);
    Assert.ok(
      r.type != UrlbarUtils.RESULT_TYPE.URL ||
        !r.url.includes(TEST_URL) ||
        !r.isSponsored,
      `Result at index ${i} should not be a QuickSuggest result`
    );
  }
}

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();

  Services.prefs.clearUserPref(SEEN_DIALOG_PREF);
  Services.prefs.clearUserPref("browser.urlbar.quicksuggest.seenRestarts");

  let doExperimentCleanup = await UrlbarTestUtils.enrollExperiment({
    valueOverrides: {
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: true,
    },
  });

  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);
  let onEnabled = UrlbarQuickSuggest.onEnabledUpdate;
  UrlbarQuickSuggest.onEnabledUpdate = () => {};

  registerCleanupFunction(async function() {
    await doExperimentCleanup();
    UrlbarQuickSuggest.onEnabledUpdate = onEnabled;

    // The onboarding test task causes prefs to be set, so clear them when done
    // so we leave a blank slate for other tests.
    UrlbarPrefs.clear("suggest.quicksuggest");
    UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
    UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
    UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
    UrlbarPrefs.clear("quicksuggest.seenRestarts");
  });
});

// Tests the onboarding dialog. This task must run first because it fully
// enables the feature (both sponsored and non-sponsored suggestions) by virtue
// of showing the onboarding.
add_task(async function test_onboarding() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await assertNoQuickSuggestResults();
  await UrlbarTestUtils.promisePopupClose(window);

  let dialogPromise = BrowserTestUtils.promiseAlertDialog(
    "accept",
    "chrome://browser/content/urlbar/quicksuggestOnboarding.xhtml",
    { isSubDialog: true }
  ).then(() => info("Saw dialog"));
  let prefPromise = TestUtils.waitForPrefChange(
    SEEN_DIALOG_PREF,
    value => value === true
  ).then(() => info("Saw pref change"));

  // Simulate 3 restarts. this function is only called by BrowserGlue
  // on startup, the first restart would be where MR1 was shown then
  // we will show onboarding the 2nd restart after that.
  for (let i = 0; i < 3; i++) {
    info(`Simulating restart ${i + 1}`);
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();
  }

  info("Waiting for dialog and pref change");
  await Promise.all([dialogPromise, prefPromise]);
});

// Tests a sponsored result and keyword highlighting.
add_task(async function sponsored() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await assertIsQuickSuggest({
    index: 1,
    sponsoredURL: `${TEST_URL}?q=frabbits`,
    nonsponsoredURL: `${TEST_URL}?q=nonsponsored`,
  });
  let row = await UrlbarTestUtils.waitForAutocompleteResultAt(window, 1);
  Assert.equal(
    row.querySelector(".urlbarView-title").firstChild.textContent,
    "fra",
    "The part of the keyword that matches users input is not bold."
  );
  Assert.equal(
    row.querySelector(".urlbarView-title > strong").textContent,
    "b",
    "The auto completed section of the keyword is bolded."
  );
  await UrlbarTestUtils.promisePopupClose(window);
});

// Tests a non-sponsored result.
add_task(async function nonSponsored() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nonspon",
  });
  await assertIsQuickSuggest({
    index: 1,
    isSponsored: false,
    sponsoredURL: `${TEST_URL}?q=frabbits`,
    nonsponsoredURL: `${TEST_URL}?q=nonsponsored`,
  });
  await UrlbarTestUtils.promisePopupClose(window);
});
