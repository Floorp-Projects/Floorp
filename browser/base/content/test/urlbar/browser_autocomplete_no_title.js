add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  await PlacesUtils.history.clear();
  const uri = "http://bug1060642.example.com/beards/are/pretty/great";
  await PlacesTestUtils.addVisits([{ uri, title: "" }]);
  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
    BrowserTestUtils.removeTab(tab);
  });

  await promiseAutocompleteResultPopup("bug1060642");
  let result = await waitForAutocompleteResultAt(1);
  is(result._titleText.textContent, "bug1060642.example.com", "Result title should be as expected");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
});
