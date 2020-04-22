const SUGGEST_PREF_NAME = "browser.search.suggest.enabled";
const URLBAR_SUGGEST_PREF_NAME = "browser.urlbar.suggest.searches";
const PRIVATE_PREF_NAME = "browser.search.suggest.enabled.private";

let initialUrlbarSuggestValue;
let initialSuggestionsInPrivateValue;

add_task(async function setup() {
  const originalSuggest = Services.prefs.getBoolPref(SUGGEST_PREF_NAME);
  initialUrlbarSuggestValue = Services.prefs.getBoolPref(
    URLBAR_SUGGEST_PREF_NAME
  );
  initialSuggestionsInPrivateValue = Services.prefs.getBoolPref(
    PRIVATE_PREF_NAME
  );

  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(SUGGEST_PREF_NAME, originalSuggest);
    Services.prefs.setBoolPref(
      PRIVATE_PREF_NAME,
      initialSuggestionsInPrivateValue
    );
  });
});

// Open with suggestions enabled
add_task(async function test_suggestions_start_enabled() {
  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, true);

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

  async function toggleElement(id, prefName, element, initialValue, desc) {
    await BrowserTestUtils.synthesizeMouseAtCenter(
      `#${id}`,
      {},
      gBrowser.selectedBrowser
    );
    is(
      element.checked,
      !initialValue,
      `Should have flipped the ${desc} checkbox`
    );
    let prefValue = Services.prefs.getBoolPref(prefName);
    is(
      prefValue,
      !initialValue,
      `Should have updated the ${desc} preference value`
    );

    await BrowserTestUtils.synthesizeMouseAtCenter(
      `#${id}`,
      {},
      gBrowser.selectedBrowser
    );
    is(
      element.checked,
      initialValue,
      `Should have flipped the ${desc} checkbox back to the original value`
    );
    prefValue = Services.prefs.getBoolPref(prefName);
    is(
      prefValue,
      initialValue,
      `Should have updated the ${desc} preference back to the original value`
    );
  }

  await toggleElement(
    "urlBarSuggestion",
    URLBAR_SUGGEST_PREF_NAME,
    urlbarBox,
    initialUrlbarSuggestValue,
    "urlbar"
  );
  await toggleElement(
    "showSearchSuggestionsPrivateWindows",
    PRIVATE_PREF_NAME,
    privateBox,
    initialSuggestionsInPrivateValue,
    "private suggestion"
  );

  Services.prefs.setBoolPref(SUGGEST_PREF_NAME, false);
  ok(!urlbarBox.checked, "Should have unchecked the urlbar box");
  ok(urlbarBox.disabled, "Should have disabled the urlbar box");
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
