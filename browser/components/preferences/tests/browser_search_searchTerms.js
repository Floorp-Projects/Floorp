/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
  Tests the showSearchTerms option on the about:preferences#search page.
*/

"use strict";

ChromeUtils.defineLazyGetter(this, "QuickSuggestTestUtils", () => {
  const { QuickSuggestTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/QuickSuggestTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

const CHECKBOX_ID = "searchShowSearchTermCheckbox";
const PREF_SEARCHTERMS = "browser.urlbar.showSearchTerms.enabled";
const PREF_FEATUREGATE = "browser.urlbar.showSearchTerms.featureGate";

/*
  If Nimbus experiment is enabled, check option visibility.
*/
add_task(async function showSearchTermsVisibility_experiment_beforeOpen() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, false]],
  });
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      showSearchTermsFeatureGate: true,
    },
    callback: async () => {
      await openPreferencesViaOpenPreferencesAPI("search", {
        leaveOpen: true,
      });
      let doc = gBrowser.selectedBrowser.contentDocument;
      let container = doc.getElementById(CHECKBOX_ID);
      Assert.ok(
        BrowserTestUtils.isVisible(container),
        "The option box is visible"
      );
      gBrowser.removeCurrentTab();
    },
  });
  await SpecialPowers.popPrefEnv();
});

/*
  If Nimbus experiment is not enabled initially but eventually enabled,
  check option visibility on Preferences page.
*/
add_task(async function showSearchTermsVisibility_experiment_afterOpen() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, false]],
  });
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CHECKBOX_ID);
  Assert.ok(
    BrowserTestUtils.isHidden(container),
    "The option box is initially hidden."
  );

  // Install experiment.
  await QuickSuggestTestUtils.withExperiment({
    valueOverrides: {
      showSearchTermsFeatureGate: true,
    },
    callback: async () => {
      Assert.ok(
        BrowserTestUtils.isVisible(container),
        "The option box is visible"
      );
    },
  });

  Assert.ok(
    BrowserTestUtils.isHidden(container),
    "The option box is hidden again after the experiment is uninstalled."
  );

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

/*
  Check using the checkbox modifies the preference.
*/
add_task(async function showSearchTerms_checkbox() {
  // Enable the feature.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let option = doc.getElementById(CHECKBOX_ID);

  // Evaluate checkbox pref is true.
  Assert.ok(option.checked, "Option box should be checked.");

  // Evaluate checkbox when pref is false.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_SEARCHTERMS, false]],
  });
  Assert.ok(!option.checked, "Option box should not be checked.");
  await SpecialPowers.popPrefEnv();

  // Evaluate pref when checkbox is un-checked.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.equal(
    Services.prefs.getBoolPref(PREF_SEARCHTERMS),
    false,
    "Preference should be false if un-checked."
  );

  // Evaluate pref when checkbox is checked.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  Assert.equal(
    Services.prefs.getBoolPref(PREF_SEARCHTERMS),
    true,
    "Preference should be true if checked."
  );

  // Clean-up.
  Services.prefs.clearUserPref(PREF_SEARCHTERMS);
  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

/*
  When loading the search preferences panel, the
  showSearchTerms checkbox should be hidden if
  the search bar is enabled.
*/
add_task(async function showSearchTerms_and_searchBar_preference_load() {
  // Enable the feature.
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_FEATUREGATE, true],
      ["browser.search.widget.inNavBar", true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.ok(
    checkbox.hidden,
    "showSearchTerms checkbox should be hidden when search bar is enabled."
  );

  // Clean-up.
  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

/*
  If the search bar is enabled while the search
  preferences panel is open, the showSearchTerms
  checkbox should not be clickable.
*/
add_task(async function showSearchTerms_and_searchBar_preference_change() {
  // Enable the feature.
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_FEATUREGATE, true]],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;

  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.ok(!checkbox.hidden, "showSearchTerms checkbox should be shown.");

  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.widget.inNavBar", true]],
  });
  Assert.ok(
    checkbox.hidden,
    "showSearchTerms checkbox should be hidden when search bar is enabled."
  );

  // Clean-up.
  await SpecialPowers.popPrefEnv();
  Assert.ok(!checkbox.hidden, "showSearchTerms checkbox should be shown.");

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});
