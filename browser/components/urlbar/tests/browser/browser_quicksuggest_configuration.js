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
const SEEN_RESTART_PREF = "browser.urlbar.quicksuggest.seenRestarts";

add_task(async function init() {
  await PlacesUtils.history.clear();
  await UrlbarTestUtils.formHistory.clear();
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.searches", true],
      ["browser.startup.upgradeDialog.version", 89],
      // Reset those prefs in case they were set by other tests
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

// The default is to wait for 2 browser restarts to show the onboarding dialog
// on the 3rd restart. This tests that we can override it by configuring the
// `showOnboardingDialogOnNthRestart`
add_task(async function test_override_wait_after_n_restarts() {
  let doExperimentCleanup = await UrlbarTestUtils.enrollExperiment({
    valueOverrides: {
      quickSuggestEnabled: true,
      quickSuggestShuoldShowOnboardingDialog: true,
      // Just wait for 1 browser restart instead of the default 2
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
