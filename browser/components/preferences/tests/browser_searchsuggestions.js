const SUGGEST_PREF_NAME = "browser.search.suggest.enabled";
const URLBAR_SUGGEST_PREF_NAME = "browser.urlbar.suggest.searches";
const PRIVATE_PREF_NAME = "browser.search.suggest.enabled.private";
const SEARCHBAR_PREF_NAME = "browser.search.widget.inNavBar";

let initialUrlbarSuggestValue;
let initialSuggestionsInPrivateValue;

add_setup(async function () {
  const originalSuggest = Services.prefs.getBoolPref(SUGGEST_PREF_NAME);
  initialUrlbarSuggestValue = Services.prefs.getBoolPref(
    URLBAR_SUGGEST_PREF_NAME
  );
  initialSuggestionsInPrivateValue =
    Services.prefs.getBoolPref(PRIVATE_PREF_NAME);

  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(SUGGEST_PREF_NAME, originalSuggest);
    Services.prefs.setBoolPref(
      PRIVATE_PREF_NAME,
      initialSuggestionsInPrivateValue
    );
  });
});

async function toggleElement(
  id,
  prefName,
  element,
  initialCheckboxValue,
  initialPrefValue,
  descCheckbox,
  descPref,
  shouldUpdatePref = false
) {
  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${id}`,
    {},
    gBrowser.selectedBrowser
  );
  is(
    element.checked,
    !initialCheckboxValue,
    `Should have flipped the ${descCheckbox} checkbox`
  );
  let expectedPrefValue = shouldUpdatePref
    ? !initialPrefValue
    : initialPrefValue;
  let prefValue = Services.prefs.getBoolPref(prefName);

  is(
    prefValue,
    expectedPrefValue,
    `Should ${
      shouldUpdatePref ? "" : "not"
    } have updated the ${descPref} preference value`
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    `#${id}`,
    {},
    gBrowser.selectedBrowser
  );
  is(
    element.checked,
    initialCheckboxValue,
    `Should have flipped the ${descCheckbox} checkbox back to the original value`
  );
  prefValue = Services.prefs.getBoolPref(prefName);
  expectedPrefValue = shouldUpdatePref ? !expectedPrefValue : initialPrefValue;
  is(
    prefValue,
    expectedPrefValue,
    `Should ${
      shouldUpdatePref ? "" : "not"
    } have updated the ${descPref} preference value`
  );
}

// Open with suggestions enabled
add_task(async function test_suggestions_start_enabled() {
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, true);
  Services.prefs.setBoolPref(SEARCHBAR_PREF_NAME, true);

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  let privateBox = doc.getElementById("showSearchSuggestionsPrivateWindows");
  ok(!urlbarBox.disabled, "Should have enabled the urlbar checkbox");
  ok(
    !privateBox.disabled,
    "Should have enabled the private mode suggestions checkbox"
  );
  is(
    urlbarBox.checked,
    initialUrlbarSuggestValue,
    "Should have the correct value for the urlbar checkbox"
  );
  is(
    privateBox.checked,
    initialSuggestionsInPrivateValue,
    "Should have the correct value for the private mode suggestions checkbox"
  );

  await toggleElement(
    "urlBarSuggestion",
    URLBAR_SUGGEST_PREF_NAME,
    urlbarBox,
    initialUrlbarSuggestValue,
    initialUrlbarSuggestValue,
    "urlbar",
    "urlbar",
    true
  );

  await toggleElement(
    "showSearchSuggestionsPrivateWindows",
    PRIVATE_PREF_NAME,
    privateBox,
    initialSuggestionsInPrivateValue,
    initialSuggestionsInPrivateValue,
    "private suggestion",
    "private suggestion",
    true
  );

  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, false);
  ok(!urlbarBox.checked, "Should have unchecked the urlbar box");
  ok(urlbarBox.disabled, "Should have disabled the urlbar box");
  Services.prefs.setBoolPref(SEARCHBAR_PREF_NAME, false);
  ok(urlbarBox.hidden, "Should have hidden the urlbar box");
  ok(!privateBox.checked, "Should have unchecked the private suggestions box");
  ok(privateBox.disabled, "Should have disabled the private suggestions box");

  gBrowser.removeCurrentTab();
});

// Open with suggestions disabled
add_task(async function test_suggestions_start_disabled() {
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, false);

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(urlbarBox.disabled, "Should have the urlbar box disabled");
  let privateBox = doc.getElementById("showSearchSuggestionsPrivateWindows");
  ok(privateBox.disabled, "Should have the private suggestions box disabled");

  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, true);

  ok(!urlbarBox.disabled, "Should have enabled the urlbar box");
  ok(!privateBox.disabled, "Should have enabled the private suggestions box");

  gBrowser.removeCurrentTab();
});

add_task(async function test_sync_search_suggestions_prefs() {
  info("Adding the search bar to the toolbar");
  Services.prefs.setBoolPref(SEARCHBAR_PREF_NAME, true);
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, true);
  Services.prefs.setBoolPref(URLBAR_SUGGEST_PREF_NAME, false);
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let suggestionsInSearchFieldsCheckbox = doc.getElementById(
    "suggestionsInSearchFieldsCheckbox"
  );

  is(
    suggestionsInSearchFieldsCheckbox.checked,
    true,
    "Should have the correct value for the search suggestions checkbox"
  );

  is(
    Services.prefs.getBoolPref(SUGGEST_PREF_NAME),
    true,
    `${SUGGEST_PREF_NAME} should be enabled`
  );
  is(
    Services.prefs.getBoolPref(URLBAR_SUGGEST_PREF_NAME),
    false,
    `${URLBAR_SUGGEST_PREF_NAME} should be disabled`
  );

  await toggleElement(
    "suggestionsInSearchFieldsCheckbox",
    URLBAR_SUGGEST_PREF_NAME,
    suggestionsInSearchFieldsCheckbox,
    suggestionsInSearchFieldsCheckbox.checked,
    false,
    "search suggestion",
    "urlbar suggest"
  );

  info("Removing the search bar from the toolbar");
  Services.prefs.setBoolPref(SEARCHBAR_PREF_NAME, false);

  const suggestsPref = [true, false];
  const urlbarSuggestsPref = [true, false];

  for (let suggestState of suggestsPref) {
    for (let urlbarSuggestsState of urlbarSuggestsPref) {
      Services.prefs.setBoolPref(SUGGEST_PREF_NAME, suggestState);
      Services.prefs.setBoolPref(URLBAR_SUGGEST_PREF_NAME, urlbarSuggestsState);

      if (suggestState && urlbarSuggestsState) {
        ok(
          suggestionsInSearchFieldsCheckbox.checked,
          "Should have checked the suggestions checkbox"
        );
      } else {
        ok(
          !suggestionsInSearchFieldsCheckbox.checked,
          "Should have unchecked the suggestions checkbox"
        );
      }
      ok(
        !suggestionsInSearchFieldsCheckbox.disabled,
        "Should have the suggestions checkbox enabled"
      );
    }
  }

  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref(SEARCHBAR_PREF_NAME);
  Services.prefs.clearUserPref(URLBAR_SUGGEST_PREF_NAME);
  Services.prefs.clearUserPref(SUGGEST_PREF_NAME);
});
