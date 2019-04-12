/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This test ensures that we display just the domain name when a URL result doesn't
 * have a title.
 */

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
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.displayed.title, "bug1060642.example.com",
    "Result title should be as expected");
});
