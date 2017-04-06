/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Services.prefs.setBoolPref("browser.preferences.instantApply", true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.instantApply");
});

add_task(function*() {
  let prefs = yield openPreferencesViaOpenPreferencesAPI("panePrivacy");
  is(prefs.selectedPane, "panePrivacy", "Privacy pane was selected");
  prefs = yield openPreferencesViaOpenPreferencesAPI("advanced");
  is(prefs.selectedPane, "paneAdvanced", "Advanced pane was selected");
  prefs = yield openPreferencesViaHash("privacy");
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected when hash is 'privacy'");
  prefs = yield openPreferencesViaOpenPreferencesAPI("nonexistant-category");
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default when a nonexistant-category is requested");
  prefs = yield openPreferencesViaHash("nonexistant-category");
  is(prefs.selectedPane, "paneGeneral", "General pane is selected when hash is a nonexistant-category");
  prefs = yield openPreferencesViaHash();
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default");
  prefs = yield openPreferencesViaOpenPreferencesAPI("advanced-reports", {leaveOpen: true});
  is(prefs.selectedPane, "paneAdvanced", "Advanced pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#advanced", "The subcategory should be removed from the URI");
  ok(doc.querySelector("#updateOthers").hidden, "Search Updates should be hidden when only Reports are requested");
  ok(!doc.querySelector("#header-advanced").hidden, "The header should be visible when a subcategory is requested");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function openPreferencesViaHash(aPane) {
  let deferred = Promise.defer();
  gBrowser.selectedTab = gBrowser.addTab("about:preferences" + (aPane ? "#" + aPane : ""));
  let newTabBrowser = gBrowser.selectedBrowser;

  newTabBrowser.addEventListener("Initialized", function() {
    newTabBrowser.contentWindow.addEventListener("load", function() {
      let win = gBrowser.contentWindow;
      let selectedPane = win.history.state;
      gBrowser.removeCurrentTab();
      deferred.resolve({selectedPane});
    }, {once: true});
  }, {capture: true, once: true});

  return deferred.promise;
}
