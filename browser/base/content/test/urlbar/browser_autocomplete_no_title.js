add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");

  let uri = NetUtil.newURI("http://bug1060642.example.com/beards/are/pretty/great");
  await PlacesTestUtils.addVisits([{uri, title: ""}]);

  await promiseAutocompleteResultPopup("bug1060642");
  ok(gURLBar.popup.richlistbox.children.length > 1, "Should get at least 2 results");
  let result = gURLBar.popup.richlistbox.children[1];
  is(result._titleText.textContent, "bug1060642.example.com", "Result title should be as expected");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
