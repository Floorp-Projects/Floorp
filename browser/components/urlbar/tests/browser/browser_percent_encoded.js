/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

 // Tests that searching history works for both encoded or decoded strings.

add_task(async function test() {
  const decoded = "日本";
  const TEST_URL = TEST_BASE_URL + "?" + decoded;
  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
  });

  // Visit url in a new tab, going through normal urlbar workflow.
  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  let promise = PlacesTestUtils.waitForNotification("page-visited", visits => {
    Assert.equal(visits.length, 1, "Was notified for the right number of visits.");
    let {url, transitionType} = visits[0];
    return url == encodeURI(TEST_URL) &&
           transitionType == PlacesUtils.history.TRANSITIONS.TYPED;
  }, "places");
  gURLBar.focus();
  gURLBar.value = TEST_URL;
  info("Visiting url");
  EventUtils.synthesizeKey("KEY_Enter");
  await promise;
  gBrowser.removeCurrentTab({ skipPermitUnload: true });


  info("Search for the decoded string.");
  await promiseAutocompleteResultPopup(decoded);
  Assert.equal(UrlbarTestUtils.getResultCount(window), 2,
               "Check number of results");
  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, encodeURI(TEST_URL), "Check result url");

  info("Search for the encoded string.");
  await promiseAutocompleteResultPopup(encodeURIComponent(decoded));
  Assert.equal(UrlbarTestUtils.getResultCount(window), 2,
               "Check number of results");
  result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, encodeURI(TEST_URL), "Check result url");
});
