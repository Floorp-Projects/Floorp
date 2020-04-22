/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_NAME = "browser.urlbar.matchBuckets";
const HISTORY_FIRST_PREF_VALUE = "general:5,suggestion:Infinity";
const CHECKBOX_ID = "showSearchSuggestionsFirstCheckbox";

// Open preferences with search suggestions shown first (the default).
add_task(async function openWithSearchSuggestionsShownFirst() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.suggest.searches", true]],
  });

  // The pref should be cleared initially so that search suggestions are shown
  // first (the default).
  Assert.equal(
    Services.prefs.getCharPref(PREF_NAME, ""),
    "",
    "Pref should be cleared initially"
  );

  // Open preferences.  The checkbox should be checked.
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.equal(checkbox.checked, true, "Checkbox should be checked");

  // Uncheck the checkbox.
  checkbox.checked = false;
  checkbox.doCommand();

  // The pref should now be set so that history is shown first.
  Assert.equal(
    Services.prefs.getCharPref(PREF_NAME, ""),
    HISTORY_FIRST_PREF_VALUE,
    "Pref should now be set to show history first"
  );

  // Clear the pref.
  Services.prefs.clearUserPref(PREF_NAME);

  // The checkbox should have become checked again.
  Assert.equal(
    checkbox.checked,
    true,
    "Checkbox should become checked after clearing pref"
  );

  // Clean up.
  gBrowser.removeCurrentTab();
});

// Open preferences with history shown first.
add_task(async function openWithHistoryShownFirst() {
  // Set the pref to show history first.
  Services.prefs.setCharPref(PREF_NAME, HISTORY_FIRST_PREF_VALUE);

  // Open preferences.  The checkbox should be unchecked.
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });
  let doc = gBrowser.selectedBrowser.contentDocument;
  let checkbox = doc.getElementById(CHECKBOX_ID);
  Assert.equal(checkbox.checked, false, "Checkbox should be unchecked");

  // Check the checkbox.
  checkbox.checked = true;
  checkbox.doCommand();

  // The pref should now be cleared so that search suggestions are shown first.
  Assert.equal(
    Services.prefs.getCharPref(PREF_NAME, ""),
    "",
    "Pref should now be cleared to show search suggestions first"
  );

  // Set the pref again.
  Services.prefs.setCharPref(PREF_NAME, HISTORY_FIRST_PREF_VALUE);

  // The checkbox should have become unchecked again.
  Assert.equal(
    checkbox.checked,
    false,
    "Checkbox should become unchecked after setting pref"
  );

  // Clean up.
  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref(PREF_NAME);
});
