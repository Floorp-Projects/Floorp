var original = Services.prefs.getBoolPref("browser.search.suggest.enabled");

registerCleanupFunction(() => {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", original);
});

// Open with suggestions enabled
add_task(async function() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(!urlbarBox.disabled, "Checkbox should be enabled");

  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  ok(urlbarBox.disabled, "Checkbox should be disabled");

  gBrowser.removeCurrentTab();
});

// Open with suggestions disabled
add_task(async function() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", false);

  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let doc = gBrowser.selectedBrowser.contentDocument;
  let urlbarBox = doc.getElementById("urlBarSuggestion");
  ok(urlbarBox.disabled, "Checkbox should be disabled");

  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  ok(!urlbarBox.disabled, "Checkbox should be enabled");

  gBrowser.removeCurrentTab();
});

add_task(async function() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", original);
});
