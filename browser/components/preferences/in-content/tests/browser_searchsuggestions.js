const SUGGEST_PREF_NAME = "browser.search.suggest.enabled";
const URLBAR_SUGGEST_PREF_NAME = "browser.urlbar.suggest.searches";
var original = Services.prefs.getBoolPref(SUGGEST_PREF_NAME);

registerCleanupFunction(() => {
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, original);
});

// Open with suggestions enabled
add_task(async function() {
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, true);
  const INITIAL_URLBAR_SUGGEST_VALUE = Services.prefs.getBoolPref(
    URLBAR_SUGGEST_PREF_NAME
  );

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(!urlbarBox.disabled, "Checkbox should be enabled");
  is(
    urlbarBox.checked,
    INITIAL_URLBAR_SUGGEST_VALUE,
    "Checkbox should match initial pref value: " + INITIAL_URLBAR_SUGGEST_VALUE
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#urlBarSuggestion",
    {},
    gBrowser.selectedBrowser
  );
  is(
    urlbarBox.checked,
    !INITIAL_URLBAR_SUGGEST_VALUE,
    "Checkbox should be flipped after clicking it"
  );
  let prefValue = Services.prefs.getBoolPref(URLBAR_SUGGEST_PREF_NAME);
  is(
    prefValue,
    urlbarBox.checked,
    "Pref should match checkbox. Pref: " + prefValue
  );

  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#urlBarSuggestion",
    {},
    gBrowser.selectedBrowser
  );
  is(
    urlbarBox.checked,
    INITIAL_URLBAR_SUGGEST_VALUE,
    "Checkbox should be back to initial value after clicking it"
  );
  prefValue = Services.prefs.getBoolPref(URLBAR_SUGGEST_PREF_NAME);
  is(
    prefValue,
    urlbarBox.checked,
    "Pref should match checkbox. Pref: " + prefValue
  );

  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, false);
  ok(!urlbarBox.checked, "Checkbox should now be unchecked");
  ok(urlbarBox.disabled, "Checkbox should be disabled");

  gBrowser.removeCurrentTab();
});

// Open with suggestions disabled
add_task(async function() {
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, false);

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(urlbarBox.disabled, "Checkbox should be disabled");

  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, true);

  ok(!urlbarBox.disabled, "Checkbox should be enabled");

  gBrowser.removeCurrentTab();
});
