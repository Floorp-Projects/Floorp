/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests browser quick suggestions.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const TEST_URL = "http://example.com/quicksuggest";

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

add_task(async function init() {
  await PlacesUtils.history.clear();
  await PlacesUtils.bookmarks.eraseEverything();
  await UrlbarTestUtils.formHistory.clear();

  await QuickSuggestTestUtils.ensureQuickSuggestInit(TEST_DATA);
});

add_task(async function test_onboarding() {
  // Set up prefs so that onboarding will be shown.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.shouldShowOnboardingDialog", true],
      [
        "browser.urlbar.quicksuggest.quicksuggest.showedOnboardingDialog",
        false,
      ],
      ["browser.urlbar.quicksuggest.seenRestarts", 0],
      ["browser.urlbar.quicksuggest.dataCollection.enabled", false],
      ["browser.urlbar.suggest.quicksuggest.nonsponsored", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
    ],
  });

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await QuickSuggestTestUtils.assertNoQuickSuggestResults(window);
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

  await SpecialPowers.popPrefEnv();

  // Clear prefs that are set by virtue of showing and accepting the onboarding.
  UrlbarPrefs.clear("quicksuggest.shouldShowOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.showedOnboardingDialog");
  UrlbarPrefs.clear("quicksuggest.seenRestarts");
  UrlbarPrefs.clear("quicksuggest.dataCollection.enabled");
  UrlbarPrefs.clear("suggest.quicksuggest.nonsponsored");
  UrlbarPrefs.clear("suggest.quicksuggest.sponsored");
});

// Tests a sponsored result and keyword highlighting.
add_task(async function sponsored() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index: 1,
    isSponsored: true,
    url: `${TEST_URL}?q=frabbits`,
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
  await QuickSuggestTestUtils.assertIsQuickSuggest({
    window,
    index: 1,
    isSponsored: false,
    url: `${TEST_URL}?q=nonsponsored`,
  });
  await UrlbarTestUtils.promisePopupClose(window);
});
