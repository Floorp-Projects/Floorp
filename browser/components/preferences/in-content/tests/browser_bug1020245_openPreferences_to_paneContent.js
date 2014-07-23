/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Services.prefs.setBoolPref("browser.preferences.inContent", true);
Services.prefs.setBoolPref("browser.preferences.instantApply", true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.inContent");
  Services.prefs.clearUserPref("browser.preferences.instantApply");
});

add_task(function() {
  let prefs = yield openPreferencesViaOpenPreferencesAPI("paneContent");
  is(prefs.selectedPane, "paneContent", "Content pane was selected");
  prefs = yield openPreferencesViaOpenPreferencesAPI("advanced", "updateTab");
  is(prefs.selectedPane, "paneAdvanced", "Advanced pane was selected");
  is(prefs.selectedAdvancedTab, "updateTab", "The update tab within the advanced prefs should be selected");
  prefs = yield openPreferencesViaHash("privacy");
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected when hash is 'privacy'");
  prefs = yield openPreferencesViaOpenPreferencesAPI("nonexistant-category");
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default when a nonexistant-category is requested");
  prefs = yield openPreferencesViaHash("nonexistant-category");
  is(prefs.selectedPane, "paneGeneral", "General pane is selected when hash is a nonexistant-category");
  prefs = yield openPreferencesViaHash();
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default");
});

function openPreferencesViaOpenPreferencesAPI(aPane, aAdvancedTab) {
  let deferred = Promise.defer();
  gBrowser.selectedTab = gBrowser.addTab("about:blank");
  openPreferences(aPane, aAdvancedTab ? {advancedTab: aAdvancedTab} : undefined);
  let newTabBrowser = gBrowser.selectedBrowser;

  newTabBrowser.addEventListener("Initialized", function PrefInit() {
    newTabBrowser.removeEventListener("Initialized", PrefInit, true);
    newTabBrowser.contentWindow.addEventListener("load", function prefLoad() {
      newTabBrowser.contentWindow.removeEventListener("load", prefLoad);
      let win = gBrowser.contentWindow;
      let selectedPane = win.history.state;
      let doc = win.document;
      let selectedAdvancedTab = aAdvancedTab && doc.getElementById("advancedPrefs").selectedTab.id;
      gBrowser.removeCurrentTab();
      deferred.resolve({selectedPane: selectedPane, selectedAdvancedTab: selectedAdvancedTab});
    });
  }, true);

  return deferred.promise;
}

function openPreferencesViaHash(aPane) {
  let deferred = Promise.defer();
  gBrowser.selectedTab = gBrowser.addTab("about:preferences" + (aPane ? "#" + aPane : ""));
  let newTabBrowser = gBrowser.selectedBrowser;

  newTabBrowser.addEventListener("Initialized", function PrefInit() {
    newTabBrowser.removeEventListener("Initialized", PrefInit, true);
    newTabBrowser.contentWindow.addEventListener("load", function prefLoad() {
      newTabBrowser.contentWindow.removeEventListener("load", prefLoad);
      let win = gBrowser.contentWindow;
      let selectedPane = win.history.state;
      gBrowser.removeCurrentTab();
      deferred.resolve({selectedPane: selectedPane});
    });
  }, true);

  return deferred.promise;
}
