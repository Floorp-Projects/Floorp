/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the Search pane's Quick Suggest checkbox.

"use strict";

const EXPERIMENT_PREF = "browser.urlbar.quicksuggest.enabled";
const QUICK_SUGGEST_PREF = "browser.urlbar.suggest.quicksuggest";
const MAIN_PREF = "browser.search.suggest.enabled";
const URLBAR_PREF = "browser.urlbar.suggest.searches";

const CONTAINER_ID = "showQuickSuggestContainer";
const CHECKBOX_ID = "showQuickSuggest";
const SEARCH_SUGGESTIONS_DESC_ID = "searchSuggestionsDesc";

const DESC_EXPERIMENT_DISABLED =
  "Choose how suggestions from search engines appear.";
const DESC_EXPERIMENT_ENABLED = "Choose how search suggestions appear.";

let originalExperimentPrefDefaultBranchValue;

add_task(async function init() {
  // Get the original value of the experiment pref on the default branch so we
  // can reset it later.  For robustness, don't assume it exists, so try-catch.
  // eslint-disable-next-line mozilla/use-default-preference-values
  try {
    originalExperimentPrefDefaultBranchValue = Services.prefs
      .getDefaultBranch(EXPERIMENT_PREF)
      .getBoolPref("");
  } catch (ex) {}
});

// The following tasks check the visibility of the checkbox based on the value
// of the experiment pref.  See doVisibilityTest().

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: undefined,
    initialUserBranchValue: undefined,
    initialExpectedVisibility: false,
    newDefaultBranchValue: false,
    newUserBranchValue: false,
    newExpectedVisibility: false,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: false,
    initialUserBranchValue: undefined,
    initialExpectedVisibility: false,
    newDefaultBranchValue: false,
    newUserBranchValue: false,
    newExpectedVisibility: false,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: undefined,
    initialUserBranchValue: false,
    initialExpectedVisibility: false,
    newDefaultBranchValue: false,
    newUserBranchValue: false,
    newExpectedVisibility: false,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: undefined,
    initialUserBranchValue: undefined,
    initialExpectedVisibility: false,
    newDefaultBranchValue: true,
    newUserBranchValue: undefined,
    newExpectedVisibility: true,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: undefined,
    initialUserBranchValue: undefined,
    initialExpectedVisibility: false,
    newDefaultBranchValue: undefined,
    newUserBranchValue: true,
    newExpectedVisibility: true,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: false,
    initialUserBranchValue: undefined,
    initialExpectedVisibility: false,
    newDefaultBranchValue: true,
    newUserBranchValue: undefined,
    newExpectedVisibility: true,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: false,
    initialUserBranchValue: undefined,
    initialExpectedVisibility: false,
    newDefaultBranchValue: undefined,
    newUserBranchValue: true,
    newExpectedVisibility: true,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: undefined,
    initialUserBranchValue: false,
    initialExpectedVisibility: false,
    newDefaultBranchValue: true,
    newUserBranchValue: undefined,
    newExpectedVisibility: true,
  });
});

add_task(async function() {
  await doVisibilityTest({
    initialDefaultBranchValue: undefined,
    initialUserBranchValue: false,
    initialExpectedVisibility: false,
    newDefaultBranchValue: undefined,
    newUserBranchValue: true,
    newExpectedVisibility: true,
  });
});

/**
 * Runs a test that checks the visibility of the Quick Suggest checkbox based on
 * the experiment pref.  To be thorough and to test all sensical combinations of
 * default-branch and user-branch prefs, this optionally sets the pref on both
 * the default and user branches.  Any of the `value` params may be undefined,
 * and in that case the pref is not set on that particular branch.
 *
 * In detail, this initializes the pref to a given value on the default and user
 * branches, checks the checkbox's visibility based on that initial pref value,
 * sets the pref again to a new value on the default and user branches, and then
 * checks that the visibility is correctly updated.
 *
 * @param {boolean} [initialDefaultBranchValue]
 *   The initial value of the pref to set on the default branch.
 * @param {boolean} [initialUserBranchValue]
 *   The initial value of the pref to set on the user branch.
 * @param {boolean} initialExpectedVisibility
 *   True if the checkbox should be visible initially or false if not.
 * @param {boolean} [newDefaultBranchValue]
 *   The updated value of the pref to set on the default branch.
 * @param {boolean} [newUserBranchValue]
 *   The updated value of the pref to set on the user branch.
 * @param {boolean} newExpectedVisibility
 *   True if the checkbox should be visible after updating the pref or false if
 *   not.
 */
async function doVisibilityTest({
  initialDefaultBranchValue,
  initialUserBranchValue,
  initialExpectedVisibility,
  newDefaultBranchValue,
  newUserBranchValue,
  newExpectedVisibility,
}) {
  info(
    "Running visibility test: " +
      JSON.stringify({
        initialDefaultBranchValue,
        initialUserBranchValue,
        initialExpectedVisibility,
        newDefaultBranchValue,
        newUserBranchValue,
        newExpectedVisibility,
      })
  );

  // Set the initial pref values.
  if (initialDefaultBranchValue !== undefined) {
    Services.prefs
      .getDefaultBranch(EXPERIMENT_PREF)
      .setBoolPref("", initialDefaultBranchValue);
  }
  if (initialUserBranchValue !== undefined) {
    Services.prefs.setBoolPref(EXPERIMENT_PREF, initialUserBranchValue);
  }

  Assert.equal(
    Services.prefs.getBoolPref(EXPERIMENT_PREF, false),
    initialExpectedVisibility,
    "Pref getter returns expected initial value"
  );

  // Open prefs and check the initial visibility.
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  Assert.equal(
    !BrowserTestUtils.is_hidden(container),
    initialExpectedVisibility,
    "The container has the expected initial visibility"
  );

  // Check the Search Suggestions description.
  let desc = doc.getElementById(SEARCH_SUGGESTIONS_DESC_ID);
  let initialExpectedDesc = initialExpectedVisibility
    ? DESC_EXPERIMENT_ENABLED
    : DESC_EXPERIMENT_DISABLED;
  await TestUtils.waitForCondition(
    () => desc.textContent == initialExpectedDesc,
    "Waiting for initial Search Suggestions description: " + initialExpectedDesc
  );

  // Translate the description and make sure it remains correct.
  await doc.l10n.translateElements([desc]);
  Assert.equal(
    desc.textContent,
    initialExpectedDesc,
    "Initial Search Suggestions description correct after forcing translation"
  );

  // Set the new pref values.
  if (newDefaultBranchValue !== undefined) {
    Services.prefs
      .getDefaultBranch(EXPERIMENT_PREF)
      .setBoolPref("", newDefaultBranchValue);
  }
  if (newUserBranchValue !== undefined) {
    Services.prefs.setBoolPref(EXPERIMENT_PREF, newUserBranchValue);
  }

  Assert.equal(
    Services.prefs.getBoolPref(EXPERIMENT_PREF, false),
    newExpectedVisibility,
    "Pref getter returns expected value after setting prefs"
  );

  // Check visibility again.
  Assert.equal(
    !BrowserTestUtils.is_hidden(container),
    newExpectedVisibility,
    "The container has the expected visibility after setting prefs"
  );

  // Check the Search Suggestions description.
  let newExpectedDesc = newExpectedVisibility
    ? DESC_EXPERIMENT_ENABLED
    : DESC_EXPERIMENT_DISABLED;
  await TestUtils.waitForCondition(
    () => desc.textContent == newExpectedDesc,
    "Waiting for new Search Suggestions description: " + newExpectedDesc
  );

  // Translate the description and make sure it remains correct.
  await doc.l10n.translateElements([desc]);
  Assert.equal(
    desc.textContent,
    newExpectedDesc,
    "New Search Suggestions description correct after forcing translation"
  );

  // Clean up and reset the prefs.
  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(EXPERIMENT_PREF);
  if (originalExperimentPrefDefaultBranchValue === undefined) {
    Services.prefs.deleteBranch(EXPERIMENT_PREF);
  } else {
    Services.prefs
      .getDefaultBranch(EXPERIMENT_PREF)
      .setBoolPref("", originalExperimentPrefDefaultBranchValue);
  }
}

// Checks how the Quick Suggest pref and checkbox react to updates to
// URLBAR_PREF and MAIN_PREF.
add_task(async function superiorPrefInteraction() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [MAIN_PREF, true],
      [URLBAR_PREF, true],
      [EXPERIMENT_PREF, true],
      [QUICK_SUGGEST_PREF, true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  Assert.ok(
    !BrowserTestUtils.is_hidden(container),
    "The container is visible initially"
  );

  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.ok(checkbox.checked, "Checkbox should be checked");
  Assert.ok(!checkbox.disabled, "Checkbox should be enabled");

  // Two superior prefs control the Quick Suggest pref: URLBAR_PREF and
  // MAIN_PREF.  Toggle each and make sure the Quick Suggest checkbox reacts
  // appropriately.
  for (let superiorPref of [URLBAR_PREF, MAIN_PREF]) {
    info(`Testing superior pref ${superiorPref}`);

    // Set the superior pref to false.
    Services.prefs.setBoolPref(superiorPref, false);

    // The pref should remain true.
    Assert.ok(
      Services.prefs.getBoolPref(QUICK_SUGGEST_PREF),
      "Pref should remain true"
    );

    // The checkbox should have become unchecked and disabled.
    Assert.ok(
      !checkbox.checked,
      "Checkbox should become unchecked after disabling urlbar suggestions"
    );
    Assert.ok(
      checkbox.disabled,
      "Checkbox should become disabled after disabling urlbar suggestions"
    );

    // Set the superior pref to true.
    Services.prefs.setBoolPref(superiorPref, true);

    // The pref should remain true.
    Assert.ok(
      Services.prefs.getBoolPref(QUICK_SUGGEST_PREF),
      "Pref should remain true"
    );

    // The checkbox should have become checked and enabled again.
    Assert.ok(
      checkbox.checked,
      "Checkbox should become checked after re-enabling urlbar suggestions"
    );
    Assert.ok(
      !checkbox.disabled,
      "Checkbox should become enabled after re-enabling urlbar suggestions"
    );

    // Set the pref to false.
    Services.prefs.setBoolPref(QUICK_SUGGEST_PREF, false);

    // The checkbox should have become unchecked.
    Assert.ok(
      !checkbox.checked,
      "Checkbox should become unchecked after setting pref to false"
    );
    Assert.ok(
      !checkbox.disabled,
      "Checkbox should remain enabled after setting pref to false"
    );

    // Set the superior pref to false again.
    Services.prefs.setBoolPref(superiorPref, false);

    // The pref should remain false.
    Assert.ok(
      !Services.prefs.getBoolPref(QUICK_SUGGEST_PREF),
      "Pref should remain false"
    );

    // The checkbox should remain unchecked and become disabled.
    Assert.ok(
      !checkbox.checked,
      "Checkbox should remain unchecked after disabling urlbar suggestions"
    );
    Assert.ok(
      checkbox.disabled,
      "Checkbox should become disabled after disabling urlbar suggestions"
    );

    // Set the superior pref to true.
    Services.prefs.setBoolPref(superiorPref, true);

    // The pref should remain false.
    Assert.ok(
      !Services.prefs.getBoolPref(QUICK_SUGGEST_PREF),
      "Pref should remain false"
    );

    // The checkbox should remain unchecked and become enabled.
    Assert.ok(
      !checkbox.checked,
      "Checkbox should remain unchecked after re-enabling urlbar suggestions"
    );
    Assert.ok(
      !checkbox.disabled,
      "Checkbox should become enabled after re-enabling urlbar suggestions"
    );

    // Finally, set the pref back to true.
    Services.prefs.setBoolPref(QUICK_SUGGEST_PREF, true);

    // The checkbox should have become checked.
    Assert.ok(
      checkbox.checked,
      "Checkbox should become checked after setting pref back to true"
    );
    Assert.ok(
      !checkbox.disabled,
      "Checkbox should remain enabled after setting pref back to true"
    );
  }

  // Clean up.
  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(QUICK_SUGGEST_PREF);
  Services.prefs.clearUserPref(URLBAR_PREF);
  Services.prefs.clearUserPref(MAIN_PREF);

  await SpecialPowers.popPrefEnv();
});

// Clicks the checkbox and makes sure the pref is updated, and updates the pref
// and makes sure the checkbox is updated.
add_task(async function toggle() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [MAIN_PREF, true],
      [URLBAR_PREF, true],
      [EXPERIMENT_PREF, true],
      [QUICK_SUGGEST_PREF, true],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let checkbox = doc.getElementById(CHECKBOX_ID);
  checkbox.scrollIntoView();

  Assert.ok(!BrowserTestUtils.is_hidden(checkbox), "The checkbox is visible");
  Assert.ok(checkbox.checked, "The checkbox is checked initially");

  // Click the checkbox.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + checkbox.id,
    {},
    gBrowser.selectedBrowser
  );
  Assert.ok(!checkbox.checked, "The checkbox is not checked after first click");
  Assert.ok(
    !Services.prefs.getBoolPref(QUICK_SUGGEST_PREF),
    "The pref is false after first click"
  );

  // Click again.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + checkbox.id,
    {},
    gBrowser.selectedBrowser
  );
  Assert.ok(checkbox.checked, "The checkbox is checked after second click");
  Assert.ok(
    Services.prefs.getBoolPref(QUICK_SUGGEST_PREF),
    "The pref is true after second click"
  );

  // Toggle the pref.
  await SpecialPowers.pushPrefEnv({
    set: [[QUICK_SUGGEST_PREF, false]],
  });
  Assert.ok(
    !checkbox.checked,
    "The checkbox is not checked after setting the pref to false"
  );

  // Toggle the pref again.
  await SpecialPowers.popPrefEnv();
  Assert.ok(
    checkbox.checked,
    "The checkbox is checked after setting the pref to true again"
  );

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});
