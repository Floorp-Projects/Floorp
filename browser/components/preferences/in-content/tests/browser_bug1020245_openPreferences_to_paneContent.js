/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Services.prefs.setBoolPref("browser.preferences.instantApply", true);

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.preferences.instantApply");
});

// Test opening to the differerent panes and subcategories in Preferences
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
  prefs = yield openPreferencesViaOpenPreferencesAPI("privacy-reports", {leaveOpen: true});
  is(prefs.selectedPane, "panePrivacy", "Privacy pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  ok(doc.querySelector("#locationBarGroup").hidden, "Location Bar prefs should be hidden when only Reports are requested");
  ok(!doc.querySelector("#header-privacy").hidden, "The header should be visible when a subcategory is requested");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
  prefs = yield openPreferencesViaOpenPreferencesAPI("general-search", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default");
  doc = gBrowser.contentDocument;
  is(doc.location.hash, "#general", "The subcategory should be removed from the URI");
  ok(doc.querySelector("#startupGroup").hidden, "Startup should be hidden when only Search is requested");
  ok(!doc.querySelector("#engineList").hidden, "The search engine list should be visible when Search is requested");
  yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

// Test opening Preferences with subcategory on an existing Preferences tab. See bug 1358475.
add_task(function*() {
  let prefs = yield openPreferencesViaOpenPreferencesAPI("general-search", {leaveOpen: true});
  is(prefs.selectedPane, "paneGeneral", "General pane is selected by default");
  let doc = gBrowser.contentDocument;
  is(doc.location.hash, "#general", "The subcategory should be removed from the URI");
  ok(doc.querySelector("#startupGroup").hidden, "Startup should be hidden when only Search is requested");
  ok(!doc.querySelector("#engineList").hidden, "The search engine list should be visible when Search is requested");
  // The reasons that here just call the `openPreferences` API without the helping function are
  //   - already opened one about:preferences tab up there and
  //   - the goal is to test on the existing tab and
  //   - using `openPreferencesViaOpenPreferencesAPI` would introduce more handling of additional about:blank and unneccessary event
  openPreferences("privacy-reports");
  let selectedPane = gBrowser.contentWindow.history.state;
  is(selectedPane, "panePrivacy", "Privacy pane should be selected");
  is(doc.location.hash, "#privacy", "The subcategory should be removed from the URI");
  ok(doc.querySelector("#locationBarGroup").hidden, "Location Bar prefs should be hidden when only Reports are requested");
  ok(!doc.querySelector("#header-privacy").hidden, "The header should be visible when a subcategory is requested");
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
