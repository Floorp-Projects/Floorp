/* eslint-disable mozilla/no-arbitrary-setTimeout */
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
  await promiseAutocompleteResultPopup("remove.me/from_urlbar");
  await BrowserTestUtils.waitForCondition(
    () => gURLBar.popup.richlistbox.children.length > 1 &&
          gURLBar.popup.richlistbox.children[1].getAttribute("ac-value") == TEST_URL,
    "Waiting for the result to appear");
  EventUtils.synthesizeKey("VK_DOWN", {});
  Assert.equal(gURLBar.popup.richlistbox.selectedIndex, 1);
  let options = AppConstants.platform == "macosx" ? { shiftKey: true } : {};
  EventUtils.synthesizeKey("VK_DELETE", options);
  await promiseVisitRemoved;
  await BrowserTestUtils.waitForCondition(
    () => !gURLBar.popup.richlistbox.children.some(c => !c.collapsed && c.getAttribute("ac-value") == TEST_URL),
    "Waiting for the result to disappear");

  gURLBar.popup.hidePopup();
  await promisePopupHidden(gURLBar.popup);
});
