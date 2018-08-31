/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function invokeUsingCtrlD(phase) {
  switch (phase) {
  case 1:
    EventUtils.synthesizeKey("d", {accelKey: true});
    break;
  case 2:
  case 4:
    EventUtils.synthesizeKey("KEY_Escape");
    break;
  case 3:
    EventUtils.synthesizeKey("d", {accelKey: true});
    EventUtils.synthesizeKey("d", {accelKey: true});
    break;
  }
}

function invokeUsingStarButton(phase) {
  switch (phase) {
  case 1:
     EventUtils.synthesizeMouseAtCenter(BookmarkingUI.star, {});
    break;
  case 2:
  case 4:
    EventUtils.synthesizeKey("KEY_Escape");
    break;
  case 3:
     EventUtils.synthesizeMouseAtCenter(BookmarkingUI.star,
                                        { clickCount: 2 });
    break;
  }
}

add_task(async function() {
  const TEST_URL = "data:text/plain,Content";

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  registerCleanupFunction(async () => {
    await BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.eraseEverything();
  });

  // Changing the location causes the star to asynchronously update, thus wait
  // for it to be in a stable state before proceeding.
  await TestUtils.waitForCondition(
    () => BookmarkingUI.status == BookmarkingUI.STATUS_UNSTARRED
  );

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: TEST_URL,
    title: "Bug 432599 Test",
  });
  Assert.equal(BookmarkingUI.status, BookmarkingUI.STATUS_STARRED,
               "The star state should be starred");

  for (let invoker of [invokeUsingStarButton, invokeUsingCtrlD]) {
    for (let phase = 1; phase < 5; ++phase) {
      let promise = checkBookmarksPanel(phase);
      invoker(phase);
      await promise;
      Assert.equal(BookmarkingUI.status, BookmarkingUI.STATUS_STARRED,
                   "The star state shouldn't change");
    }
  }
});

var initialValue;
var initialRemoveHidden;
function checkBookmarksPanel(phase) {
  let popupElement = document.getElementById("editBookmarkPanel");
  let titleElement = document.getElementById("editBookmarkPanelTitle");
  let removeElement = document.getElementById("editBookmarkPanelRemoveButton");
  switch (phase) {
  case 1:
  case 3:
    return promisePopupEvent(popupElement, "shown");
  case 2:
    initialValue = titleElement.value;
    initialRemoveHidden = removeElement.hidden;
    return promisePopupEvent(popupElement, "hidden");
  case 4:
    Assert.equal(titleElement.value, initialValue, "The bookmark panel's title should be the same");
    Assert.equal(removeElement.hidden, initialRemoveHidden, "The bookmark panel's visibility should not change");
    return promisePopupEvent(popupElement, "hidden");
  }
  return Promise.reject(new Error("Unknown phase"));
}
