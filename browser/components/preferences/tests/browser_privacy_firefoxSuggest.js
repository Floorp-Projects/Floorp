/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the Privacy pane's Firefox Suggest UI.

"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarProviderQuickSuggest:
    "resource:///modules/UrlbarProviderQuickSuggest.jsm",
});

const CONTAINER_ID = "firefoxSuggestContainer";
const MAIN_CHECKBOX_ID = "firefoxSuggestSuggestion";
const SPONSORED_CHECKBOX_ID = "firefoxSuggestSponsoredSuggestion";

// Maps text element IDs to `{ enabled, disabled }`, where `enabled` is the
// expected l10n ID when the Firefox Suggest feature is enabled, and `disabled`
// is when disabled.
const EXPECTED_L10N_IDS = {
  locationBarGroupHeader: {
    enabled: "addressbar-header-firefox-suggest",
    disabled: "addressbar-header",
  },
  locationBarSuggestionLabel: {
    enabled: "addressbar-suggest-firefox-suggest",
    disabled: "addressbar-suggest",
  },
};

let originalExperimentPrefDefaultBranchValue;

// Allow more time for Mac machines so they don't time out in verify mode.
if (AppConstants.platform == "macosx") {
  requestLongerTimeout(3);
}

add_task(async function init() {
  // Get the original value of the experiment pref on the default branch so we
  // can reset it later.  For robustness, don't assume it exists, so try-catch.
  // eslint-disable-next-line mozilla/use-default-preference-values
  try {
    originalExperimentPrefDefaultBranchValue = Services.prefs
      .getDefaultBranch("browser.urlbar.quicksuggest.enabled")
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
 * Runs a test that checks the visibility of the Firefox Suggest preferences UI
 * based on the main rollout pref. To be thorough and to test all sensical
 * combinations of default-branch and user-branch prefs, this optionally sets
 * the pref on both the default and user branches. Any of the `value` params may
 * be undefined, and in that case the pref is not set on that particular branch.
 *
 * In detail, this initializes the pref to a given value on the default and user
 * branches, checks UI visibility based on that initial pref value, sets the
 * pref again to a new value on the default and user branches, and then checks
 * that the visibility is correctly updated.
 *
 * @param {boolean} [initialDefaultBranchValue]
 *   The initial value of the pref to set on the default branch.
 * @param {boolean} [initialUserBranchValue]
 *   The initial value of the pref to set on the user branch.
 * @param {boolean} initialExpectedVisibility
 *   True if the UI should be visible initially or false if not.
 * @param {boolean} [newDefaultBranchValue]
 *   The updated value of the pref to set on the default branch.
 * @param {boolean} [newUserBranchValue]
 *   The updated value of the pref to set on the user branch.
 * @param {boolean} newExpectedVisibility
 *   True if the UI should be visible after updating the pref or false if not.
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
      .getDefaultBranch("browser.urlbar.quicksuggest.enabled")
      .setBoolPref("", initialDefaultBranchValue);
  }
  if (initialUserBranchValue !== undefined) {
    Services.prefs.setBoolPref(
      "browser.urlbar.quicksuggest.enabled",
      initialUserBranchValue
    );
  }

  Assert.equal(
    Services.prefs.getBoolPref("browser.urlbar.quicksuggest.enabled", false),
    initialExpectedVisibility,
    "Pref getter returns expected initial value"
  );

  // Open prefs and check the initial visibility.
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  Assert.equal(
    BrowserTestUtils.is_visible(container),
    initialExpectedVisibility,
    "The container has the expected initial visibility"
  );

  // Check the text elements' l10n IDs.
  for (let [id, { enabled, disabled }] of Object.entries(EXPECTED_L10N_IDS)) {
    Assert.equal(
      doc.getElementById(id).dataset.l10nId,
      initialExpectedVisibility ? enabled : disabled,
      "Initial l10n ID for element with ID: " + id
    );
  }

  // Set the new pref values.
  if (newDefaultBranchValue !== undefined) {
    Services.prefs
      .getDefaultBranch("browser.urlbar.quicksuggest.enabled")
      .setBoolPref("", newDefaultBranchValue);
  }
  if (newUserBranchValue !== undefined) {
    Services.prefs.setBoolPref(
      "browser.urlbar.quicksuggest.enabled",
      newUserBranchValue
    );
  }

  Assert.equal(
    Services.prefs.getBoolPref("browser.urlbar.quicksuggest.enabled", false),
    newExpectedVisibility,
    "Pref getter returns expected value after setting prefs"
  );

  // Check visibility again.
  Assert.equal(
    BrowserTestUtils.is_visible(container),
    newExpectedVisibility,
    "The container has the expected visibility after setting prefs"
  );

  // Check the text elements' l10n IDs again.
  for (let [id, { enabled, disabled }] of Object.entries(EXPECTED_L10N_IDS)) {
    Assert.equal(
      doc.getElementById(id).dataset.l10nId,
      newExpectedVisibility ? enabled : disabled,
      "New l10n ID for element with ID: " + id
    );
  }

  // Clean up and reset the prefs.
  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref("browser.urlbar.quicksuggest.enabled");
  if (originalExperimentPrefDefaultBranchValue === undefined) {
    Services.prefs.deleteBranch("browser.urlbar.quicksuggest.enabled");
  } else {
    Services.prefs
      .getDefaultBranch("browser.urlbar.quicksuggest.enabled")
      .setBoolPref("", originalExperimentPrefDefaultBranchValue);
  }
}

// Opens the pane and verifies the initial state of the checkboxes when:
// browser.urlbar.suggest.quicksuggest = true
// browser.urlbar.suggest.quicksuggest.sponsored = true
add_task(async function checkboxes_initial_mainTrue_sponsoredTrue() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.quicksuggest", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: true },
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quicksuggest", false]],
  });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });

  await SpecialPowers.popPrefEnv();
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: true },
  });

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Opens the pane and verifies the initial state of the checkboxes when:
// browser.urlbar.suggest.quicksuggest = true
// browser.urlbar.suggest.quicksuggest.sponsored = false
add_task(async function checkboxes_initial_mainTrue_sponsoredFalse() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.quicksuggest", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: false },
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quicksuggest", false]],
  });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });

  await SpecialPowers.popPrefEnv();
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: false },
  });

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Opens the pane and verifies the initial state of the checkboxes when:
// browser.urlbar.suggest.quicksuggest = false
// browser.urlbar.suggest.quicksuggest.sponsored = true
add_task(async function checkboxes_initial_mainFalse_sponsoredTrue() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.quicksuggest", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quicksuggest", true]],
  });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: true },
  });

  await SpecialPowers.popPrefEnv();
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Opens the pane and verifies the initial state of the checkboxes when:
// browser.urlbar.suggest.quicksuggest = false
// browser.urlbar.suggest.quicksuggest.sponsored = false
add_task(async function checkboxes_initial_mainFalse_sponsoredFalse() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.quicksuggest", false],
      ["browser.urlbar.suggest.quicksuggest.sponsored", false],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });

  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.quicksuggest", true]],
  });
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: false },
  });

  await SpecialPowers.popPrefEnv();
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Clicks the checkboxes and makes sure the prefs are updated.
add_task(async function clickCheckboxes() {
  // Start with both prefs enabled so both checkboxes should be enabled and
  // checked.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.quicksuggest.enabled", true],
      ["browser.urlbar.suggest.quicksuggest", true],
      ["browser.urlbar.suggest.quicksuggest.sponsored", true],
    ],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  let sponsoredCheckbox = doc.getElementById(SPONSORED_CHECKBOX_ID);
  Assert.ok(
    BrowserTestUtils.is_visible(sponsoredCheckbox),
    "The sponsored checkbox is visible"
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: true },
  });

  // Click the sponsored checkbox.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + SPONSORED_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: false },
  });
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "The sponsored pref is false after checkbox click 1"
  );

  // Click it again.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + SPONSORED_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: true },
  });
  Assert.ok(
    Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest.sponsored"),
    "The sponsored pref is true after checkbox click 2"
  );

  // Click the main checkbox.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + MAIN_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });
  Assert.ok(
    !Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest"),
    "The main pref is false after checkbox click 1"
  );
  Assert.ok(
    Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest.sponsored"),
    "The sponsored pref remains true after main checkbox click 1"
  );

  // Click it again.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + MAIN_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: true },
  });
  Assert.ok(
    Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest"),
    "The main pref is true after checkbox click 2"
  );
  Assert.ok(
    Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest.sponsored"),
    "The sponsored pref remains true after main checkbox click 2"
  );

  // Click the sponsored checkbox again.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + SPONSORED_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: false },
  });
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "The sponsored pref is false after checkbox click 3"
  );

  // Click the main checkbox again.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + MAIN_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: false },
    [SPONSORED_CHECKBOX_ID]: { enabled: false, checked: false },
  });
  Assert.ok(
    !Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest"),
    "The main pref is false after checkbox click 3"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "The sponsored pref remains false after main checkbox click 3"
  );

  // Click it again.
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + MAIN_CHECKBOX_ID,
    {},
    gBrowser.selectedBrowser
  );
  assertCheckboxes({
    [MAIN_CHECKBOX_ID]: { enabled: true, checked: true },
    [SPONSORED_CHECKBOX_ID]: { enabled: true, checked: false },
  });
  Assert.ok(
    Services.prefs.getBoolPref("browser.urlbar.suggest.quicksuggest"),
    "The main pref is true after checkbox click 4"
  );
  Assert.ok(
    !Services.prefs.getBoolPref(
      "browser.urlbar.suggest.quicksuggest.sponsored"
    ),
    "The sponsored pref remains false after main checkbox click 4"
  );

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

// Clicks the learn-more link.
add_task(async function clickLearnMore() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.quicksuggest.enabled", true]],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let addressBarSection = doc.getElementById("locationBarGroup");
  addressBarSection.scrollIntoView();

  let learnMore = doc.getElementById("firefoxSuggestSuggestionLearnMore");
  Assert.ok(
    BrowserTestUtils.is_visible(learnMore),
    "The sponsored checkbox is visible"
  );

  let tabPromise = BrowserTestUtils.waitForNewTab(
    gBrowser,
    UrlbarProviderQuickSuggest.helpUrl
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#" + learnMore.id,
    {},
    gBrowser.selectedBrowser
  );
  info("Waiting for help page to load in a new tab");
  await tabPromise;
  gBrowser.removeCurrentTab();

  gBrowser.removeCurrentTab();
  await SpecialPowers.popPrefEnv();
});

/**
 * Verifies the enabled and checked status of checkboxes.
 *
 * @param {object} statesByElementID
 *   Maps checkbox element IDs to `{ enabled, checked }`, where `enabled` and
 *   checked are both booleans, the expected values.
 */
function assertCheckboxes(statesByElementID) {
  let doc = gBrowser.selectedBrowser.contentDocument;
  let container = doc.getElementById(CONTAINER_ID);
  Assert.ok(BrowserTestUtils.is_visible(container), "The container is visible");
  for (let [id, { enabled, checked }] of Object.entries(statesByElementID)) {
    let checkbox = doc.getElementById(id);
    Assert.equal(
      checkbox.disabled,
      !enabled,
      "Checkbox enabled status for ID: " + id
    );
    Assert.equal(
      checkbox.checked,
      checked,
      "Checkbox checked status for ID: " + id
    );
  }
}
