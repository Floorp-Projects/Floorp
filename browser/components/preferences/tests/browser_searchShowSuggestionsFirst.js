/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const MAIN_PREF = "browser.search.suggest.enabled";
const URLBAR_PREF = "browser.urlbar.suggest.searches";
const FIRST_PREF = "browser.urlbar.showSearchSuggestionsFirst";
const SEARCHBAR_PREF = "browser.search.widget.inNavBar";
const FIRST_CHECKBOX_ID = "showSearchSuggestionsFirstCheckbox";

add_setup(async function () {
  // Make sure the main and urlbar suggestion prefs are enabled.
  await SpecialPowers.pushPrefEnv({
    set: [
      [MAIN_PREF, true],
      [URLBAR_PREF, true],
    ],
  });
});

// Open preferences with search suggestions shown first (the default).
add_task(async function openWithSearchSuggestionsShownFirst() {
  // Initially the pref should be true so search suggestions are shown first.
  Assert.ok(
    Services.prefs.getBoolPref(FIRST_PREF),
    "Pref should be true initially"
  );

  // Open preferences.  The checkbox should be checked.
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let checkbox = doc.getElementById(FIRST_CHECKBOX_ID);
  Assert.ok(checkbox.checked, "Checkbox should be checked");
  Assert.ok(!checkbox.disabled, "Checkbox should be enabled");

  // Uncheck the checkbox.
  checkbox.checked = false;
  checkbox.doCommand();

  // The pref should now be false so that history is shown first.
  Assert.ok(
    !Services.prefs.getBoolPref(FIRST_PREF),
    "Pref should now be false to show history first"
  );

  // Make sure the checkbox state didn't change.
  Assert.ok(!checkbox.checked, "Checkbox should remain unchecked");
  Assert.ok(!checkbox.disabled, "Checkbox should remain enabled");

  // Clear the pref.
  Services.prefs.clearUserPref(FIRST_PREF);

  // The checkbox should have become checked again.
  Assert.ok(
    checkbox.checked,
    "Checkbox should become checked after clearing pref"
  );
  Assert.ok(
    !checkbox.disabled,
    "Checkbox should remain enabled after clearing pref"
  );

  // Clean up.
  gBrowser.removeCurrentTab();
});

// Open preferences with history shown first.
add_task(async function openWithHistoryShownFirst() {
  // Set the pref to show history first.
  Services.prefs.setBoolPref(FIRST_PREF, false);

  // Open preferences.  The checkbox should be unchecked.
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let checkbox = doc.getElementById(FIRST_CHECKBOX_ID);
  Assert.ok(!checkbox.checked, "Checkbox should be unchecked");
  Assert.ok(!checkbox.disabled, "Checkbox should be enabled");

  // Check the checkbox.
  checkbox.checked = true;
  checkbox.doCommand();

  // Make sure the checkbox state didn't change.
  Assert.ok(checkbox.checked, "Checkbox should remain checked");
  Assert.ok(!checkbox.disabled, "Checkbox should remain enabled");

  // The pref should now be true so that search suggestions are shown first.
  Assert.ok(
    Services.prefs.getBoolPref(FIRST_PREF),
    "Pref should now be true to show search suggestions first"
  );

  // Set the pref to false again.
  Services.prefs.setBoolPref(FIRST_PREF, false);

  // The checkbox should have become unchecked again.
  Assert.ok(
    !checkbox.checked,
    "Checkbox should become unchecked after setting pref to false"
  );
  Assert.ok(
    !checkbox.disabled,
    "Checkbox should remain enabled after setting pref to false"
  );

  // Clean up.
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref(FIRST_PREF);
});

// Checks how the show-suggestions-first pref and checkbox reacts to updates to
// URLBAR_PREF and MAIN_PREF.
add_task(async function superprefInteraction() {
  // Initially the pref should be true so search suggestions are shown first.
  Assert.ok(
    Services.prefs.getBoolPref(FIRST_PREF),
    "Pref should be true initially"
  );

  // Open preferences.  The checkbox should be checked.
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let checkbox = doc.getElementById(FIRST_CHECKBOX_ID);
  Assert.ok(checkbox.checked, "Checkbox should be checked");
  Assert.ok(!checkbox.disabled, "Checkbox should be enabled");

  Services.prefs.setBoolPref(SEARCHBAR_PREF, true);

  // Two superior prefs control the show-suggestion-first pref: URLBAR_PREF and
  // MAIN_PREF.  Toggle each and make sure the show-suggestion-first checkbox
  // reacts appropriately.
  for (let superiorPref of [URLBAR_PREF, MAIN_PREF]) {
    info(`Testing superior pref ${superiorPref}`);

    // Set the superior pref to false.
    Services.prefs.setBoolPref(superiorPref, false);

    // The pref should remain true.
    Assert.ok(
      Services.prefs.getBoolPref(FIRST_PREF),
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
      Services.prefs.getBoolPref(FIRST_PREF),
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
    Services.prefs.setBoolPref(FIRST_PREF, false);

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
      !Services.prefs.getBoolPref(FIRST_PREF),
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
      !Services.prefs.getBoolPref(FIRST_PREF),
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
    Services.prefs.setBoolPref(FIRST_PREF, true);

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

  Services.prefs.clearUserPref(FIRST_PREF);
  Services.prefs.clearUserPref(URLBAR_PREF);
  Services.prefs.clearUserPref(MAIN_PREF);
  Services.prefs.clearUserPref(SEARCHBAR_PREF);
});
