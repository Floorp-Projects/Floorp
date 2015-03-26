/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  let ucpref = Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete");
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", ucpref);
  });

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(tab);

  let uri = NetUtil.newURI("http://bug1060642.example.com/beards/are/pretty/great");
  yield PlacesTestUtils.addVisits([{uri: uri, title: ""}]);

  yield promiseAutocompleteResultPopup("bug1060642");
  ok(gURLBar.popup.richlistbox.children.length > 1, "Should get at least 2 results");
  let result = gURLBar.popup.richlistbox.children[1];
  is(result._title.textContent, "bug1060642.example.com", "Result title should be as expected");

  gURLBar.popup.hidePopup();
  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
