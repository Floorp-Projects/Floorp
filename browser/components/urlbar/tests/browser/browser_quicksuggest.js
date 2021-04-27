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

const EXPERIMENT_RECIPE = ExperimentFakes.recipe(
  "mochitest-quicksuggest-experiment",
  {
    branches: [
      {
        slug: "treatment-branch",
        ratio: 1,
        feature: {
          featureId: "urlbar",
          enabled: true,
          value: { quickSuggestEnabled: true },
        },
      },
    ],
    bucketConfig: {
      start: 0,
      // Ensure 100% enrollment
      count: 10000,
      total: 10000,
      namespace: "my-mochitest",
      randomizationUnit: "normandy_id",
    },
  }
);

const ABOUT_BLANK = "about:blank";
const SUGGESTIONS_PREF = "browser.search.suggest.enabled";
const PRIVATE_SUGGESTIONS_PREF = "browser.search.suggest.enabled.private";

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
 * @param {boolean} [isSponsored]
 *   True if the result is expected to be sponsored and false if non-sponsored
 *   (i.e., "Firefox Suggest").
 * @param {object} [win]
 *   The window in which to read the results from.
 * @returns {result}
 *   The result at the given index.
 */
async function assertIsQuickSuggest({
  index = -1,
  isSponsored = true,
  win = window,
} = {}) {
  if (index < 0) {
    index = UrlbarTestUtils.getResultCount(win) - 1;
    Assert.greater(index, -1, "Sanity check: Result count should be > 0");
  }

  let result = await UrlbarTestUtils.getDetailsOfResultAt(win, index);
  Assert.equal(result.type, UrlbarUtils.RESULT_TYPE.URL);

  // Confusingly, `isSponsored` is set on the result payload for all quick
  // suggest results, even non-sponsored ones.  It's just a marker of whether
  // the result is a quick suggest.
  Assert.ok(result.isSponsored, "Result isSponsored");

  let url;
  let actionText;
  if (isSponsored) {
    url = `${TEST_URL}?q=frabbits`;
    actionText = "Sponsored";
  } else {
    url = `${TEST_URL}?q=nonsponsored`;
    actionText = "Firefox Suggest";
  }
  Assert.equal(result.url, url, "Result URL");
  Assert.equal(
    result.element.row._elements.get("action").textContent,
    actionText,
    "Result action text"
  );

  let helpButton = result.element.row._elements.get("helpButton");
  Assert.ok(helpButton, "The help button should be present");

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

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.startup.upgradeDialog.version", 89],
    ],
  });
  // Wait for Experiment Store to initialize before trying to enroll
  await ExperimentAPI.ready();
  let {
    enrollmentPromise,
    doExperimentCleanup,
  } = ExperimentFakes.enrollmentHelper(EXPERIMENT_RECIPE);

  await enrollmentPromise;

  // Add a mock engine so we don't hit the network loading the SERP.
  await SearchTestUtils.installSearchExtension();
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(Services.search.getEngineByName("Example"));

  await UrlbarQuickSuggest.init();
  await UrlbarQuickSuggest._processSuggestionsJSON(TEST_DATA);
  let onEnabled = UrlbarQuickSuggest.onEnabledUpdate;
  UrlbarQuickSuggest.onEnabledUpdate = () => {};

  registerCleanupFunction(async function() {
    Services.search.setDefault(oldDefaultEngine);
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
    await doExperimentCleanup();
    UrlbarQuickSuggest.onEnabledUpdate = onEnabled;
  });
});

add_task(async function test_onboarding() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await assertNoQuickSuggestResults();

  // Simulate 3 restarts. this function is only called by BrowserGlue
  // on startup, the first restart would be where MR1 was shown then
  // we will show onboarding the 2nd restart after that.
  UrlbarQuickSuggest.maybeShowOnboardingDialog();
  UrlbarQuickSuggest.maybeShowOnboardingDialog();
  UrlbarQuickSuggest.maybeShowOnboardingDialog();

  await BrowserTestUtils.promiseAlertDialog(
    "accept",
    "chrome://browser/content/urlbar/quicksuggestOnboarding.xhtml",
    { isSubDialog: true }
  );

  TestUtils.waitForPrefChange(
    "browser.urlbar.quicksuggest.showedOnboardingDialog",
    value => value === true
  );
});

add_task(async function basic_test() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "fra",
  });
  await assertIsQuickSuggest({ index: 1 });
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
  await assertIsQuickSuggest({ index: -1, win });
  await BrowserTestUtils.closeWindow(win);
  await SpecialPowers.popPrefEnv();
});

// Tests a non-sponsored result.
add_task(async function nonSponsored() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "nonspon",
  });
  await assertIsQuickSuggest({ index: 1, isSponsored: false });
  await UrlbarTestUtils.promisePopupClose(window);
});
