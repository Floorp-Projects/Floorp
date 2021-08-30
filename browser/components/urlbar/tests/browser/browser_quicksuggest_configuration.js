/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests QuickSuggest configurations.
 */

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarQuickSuggest: "resource:///modules/UrlbarQuickSuggest.jsm",
});

const SHOWED_ONBOARDING_DIALOG_PREF =
  "browser.urlbar.quicksuggest.showedOnboardingDialog";
const OPT_IN_PREF = "browser.urlbar.suggest.quicksuggest";
const SEEN_RESTART_PREF = "browser.urlbar.quicksuggest.seenRestarts";

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      // Reset those prefs in case they were set by other tests
      [OPT_IN_PREF, false],
      [SHOWED_ONBOARDING_DIALOG_PREF, false],
      [SEEN_RESTART_PREF, 0],
    ],
  });

  // Add a mock engine so we don't hit the network loading the SERP.
  await SearchTestUtils.installSearchExtension();
  let oldDefaultEngine = await Services.search.getDefault();
  await Services.search.setDefault(Services.search.getEngineByName("Example"));

  await UrlbarQuickSuggest.init();
  let onEnabled = UrlbarQuickSuggest.onEnabledUpdate;
  UrlbarQuickSuggest.onEnabledUpdate = () => {};

  registerCleanupFunction(async function() {
    Services.search.setDefault(oldDefaultEngine);
    await PlacesUtils.history.clear();
    await UrlbarTestUtils.formHistory.clear();
    UrlbarQuickSuggest.onEnabledUpdate = onEnabled;
  });
});

// The default is to wait for no browser restarts to show the onboarding dialog
// on the first restart. This tests that we can override it by configuring the
// `showOnboardingDialogOnNthRestart`
add_task(async function test_override_wait_after_n_restarts() {
  let doExperimentCleanup = await UrlbarTestUtils.enrollExperiment({
    valueOverrides: {
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: true,
      // Wait for 1 browser restart
      quickSuggestShowOnboardingDialogAfterNRestarts: 1,
    },
  });

  let dialogPromise = BrowserTestUtils.promiseAlertDialog(
    "accept",
    "chrome://browser/content/urlbar/quicksuggestOnboarding.xhtml",
    { isSubDialog: true }
  ).then(() => info("Saw dialog"));
  let prefPromise = TestUtils.waitForPrefChange(
    SHOWED_ONBOARDING_DIALOG_PREF,
    value => value === true
  ).then(() => info("Saw pref change"));

  // Simulate 2 restarts. this function is only called by BrowserGlue
  // on startup, the first restart would be where MR1 was shown then
  // we will show onboarding the 2nd restart after that.
  for (let i = 0; i < 2; i++) {
    info(`Simulating restart ${i + 1}`);
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();
  }

  info("Waiting for dialog and pref change");
  await Promise.all([dialogPromise, prefPromise]);

  await doExperimentCleanup();
});

add_task(async function test_skip_onboarding_dialog() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [OPT_IN_PREF, false],
      [SHOWED_ONBOARDING_DIALOG_PREF, false],
      [SEEN_RESTART_PREF, 0],
    ],
  });
  let doExperimentCleanup = await UrlbarTestUtils.enrollExperiment({
    valueOverrides: {
      quickSuggestEnabled: true,
      quickSuggestShouldShowOnboardingDialog: false,
    },
  });

  // Simulate 3 restarts.
  for (let i = 0; i < 3; i++) {
    info(`Simulating restart ${i + 1}`);
    await UrlbarQuickSuggest.maybeShowOnboardingDialog();
  }

  Assert.ok(
    !Services.prefs.getBoolPref(SHOWED_ONBOARDING_DIALOG_PREF),
    "The showed onboarding dialog pref should not be set"
  );

  await doExperimentCleanup();
});

add_task(async function test_indexes() {
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      quickSuggestNonSponsoredIndex: 99,
      quickSuggestSponsoredIndex: -1337,
    },
    callback: () => {
      Assert.equal(
        UrlbarPrefs.get("quickSuggestNonSponsoredIndex"),
        99,
        "quickSuggestNonSponsoredIndex"
      );
      Assert.equal(
        UrlbarPrefs.get("quickSuggestSponsoredIndex"),
        -1337,
        "quickSuggestSponsoredIndex"
      );
    },
  });
});

add_task(async function test_merino() {
  await UrlbarTestUtils.withExperiment({
    valueOverrides: {
      merinoEnabled: true,
      merinoEndpointURL: "http://example.com/test_merino_config",
      merinoEndpointParamQuery: "test_merino_config_param",
    },
    callback: () => {
      Assert.equal(UrlbarPrefs.get("merinoEnabled"), true, "merinoEnabled");
      Assert.equal(
        UrlbarPrefs.get("merinoEndpointURL"),
        "http://example.com/test_merino_config",
        "merinoEndpointURL"
      );
      Assert.equal(
        UrlbarPrefs.get("merinoEndpointParamQuery"),
        "test_merino_config_param",
        "merinoEndpointParamQuery"
      );
    },
  });
});
