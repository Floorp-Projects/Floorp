/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test_remove_history() {
  const TEST_URL = "http://remove.me/from_urlbar/";
  await PlacesTestUtils.addVisits(TEST_URL);

  registerCleanupFunction(async function() {
    await PlacesUtils.history.clear();
  });

  let promiseVisitRemoved = PlacesTestUtils.waitForNotification(
    "onDeleteURI", uri => uri.spec == TEST_URL, "history");

  await promiseAutocompleteResultPopup("from_urlbar");
  let result = await waitForAutocompleteResultAt(1);
  Assert.equal(result.getAttribute("ac-value"), TEST_URL, "Found the expected result");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(gURLBar.popup.richlistbox.selectedIndex, 1);
  let options = AppConstants.platform == "macosx" ? { shiftKey: true } : {};
  EventUtils.synthesizeKey("KEY_Delete", options);
  await promiseVisitRemoved;
  await BrowserTestUtils.waitForCondition(
    () => !gURLBar.popup.richlistbox.itemChildren.some(c => !c.collapsed && c.getAttribute("ac-value") == TEST_URL),
    "Waiting for the result to disappear");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
});

// We shouldn't be able to remove a bookmark item.
add_task(async function test_remove_bookmark_doesnt() {
  const TEST_URL = "http://dont.remove.me/from_urlbar/";
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "test",
    url: TEST_URL,
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  await promiseAutocompleteResultPopup("from_urlbar");
  let result = await waitForAutocompleteResultAt(1);
  Assert.equal(result.getAttribute("ac-value"), TEST_URL, "Found the expected result");

  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(gURLBar.popup.richlistbox.selectedIndex, 1);
  let options = AppConstants.platform == "macosx" ? { shiftKey: true } : {};
  EventUtils.synthesizeKey("KEY_Delete", options);

  // We don't have an easy way of determining if the event was process or not,
  // so let any event queues clear before testing.
  await new Promise(resolve => setTimeout(resolve, 0));
  await PlacesTestUtils.promiseAsyncUpdates();

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);

  Assert.ok(await PlacesUtils.bookmarks.fetch({url: TEST_URL}),
    "Should still have the URL bookmarked.");
});
