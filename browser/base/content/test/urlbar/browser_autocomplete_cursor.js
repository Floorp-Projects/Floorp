add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  let ucpref = Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete");
  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", ucpref);
  });

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(tab);
  yield promiseAutocompleteResultPopup("www.mozilla.org");

  gURLBar.selectTextRange(4, 4);

  is(gURLBar.popup.state, "open", "Popup should be open");
  is(gURLBar.popup.richlistbox.selectedIndex, 0, "Should have selected something");

  EventUtils.synthesizeKey("VK_RIGHT", {});
  yield promisePopupHidden(gURLBar.popup);

  is(gURLBar.selectionStart, 5, "Should have moved the cursor");
  is(gURLBar.selectionEnd, 5, "And not selected anything");

  gBrowser.removeTab(tab);
});
