add_task(function*() {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
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
