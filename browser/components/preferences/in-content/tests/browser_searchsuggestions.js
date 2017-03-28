var original = Services.prefs.getBoolPref("browser.search.suggest.enabled");

registerCleanupFunction(() => {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", original);
});

// Open with suggestions enabled
add_task(function*() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  yield openPreferencesViaOpenPreferencesAPI("search", undefined, { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(!urlbarBox.disabled, "Checkbox should be enabled");

  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  ok(urlbarBox.disabled, "Checkbox should be disabled");

  gBrowser.removeCurrentTab();
});

// Open with suggestions disabled
add_task(function*() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  yield openPreferencesViaOpenPreferencesAPI("search", undefined, { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(urlbarBox.disabled, "Checkbox should be disabled");

  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  ok(!urlbarBox.disabled, "Checkbox should be enabled");

  gBrowser.removeCurrentTab();
});

add_task(function*() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", original);
});
