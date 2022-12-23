/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.resultMenu", true]],
  });
});

add_task(async function test_remove_history() {
  const TEST_URL = "https://remove.me/from_urlbar/";
  await PlacesTestUtils.addVisits(TEST_URL);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  let promiseVisitRemoved = PlacesTestUtils.waitForNotification(
    "page-removed",
    events => events[0].url === TEST_URL,
    "places"
  );

  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "from_urlbar",
  });

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
  Assert.equal(result.url, TEST_URL, "Found the expected result");

  let expectedResultCount = UrlbarTestUtils.getResultCount(window) - 1;

  let promiseMenuOpen = BrowserTestUtils.waitForEvent(
    gURLBar.view.resultMenu,
    "popupshown"
  );
  let promiseMenuClosed = BrowserTestUtils.waitForEvent(
    gURLBar.view.resultMenu,
    "popuphidden"
  );
  let menuButton = UrlbarTestUtils.getButtonForResultIndex(window, "menu", 1);
  await EventUtils.synthesizeMouseAtCenter(menuButton, {}, window);
  info("waiting for the menu to open");
  await promiseMenuOpen;
  info("pressing access key of the menu item to remove the result");
  EventUtils.synthesizeKey("R");
  info("waiting for the menu to close");
  await promiseMenuClosed;

  const removeEvents = await promiseVisitRemoved;
  Assert.ok(
    removeEvents[0].isRemovedFromStore,
    "isRemovedFromStore should be true"
  );

  await TestUtils.waitForCondition(
    () => UrlbarTestUtils.getResultCount(window) == expectedResultCount,
    "Waiting for the result to disappear"
  );

  for (let i = 0; i < expectedResultCount; i++) {
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    Assert.notEqual(
      details.url,
      TEST_URL,
      "Should not find the test URL in the remaining results"
    );
  }

  await UrlbarTestUtils.promisePopupClose(window);
});
