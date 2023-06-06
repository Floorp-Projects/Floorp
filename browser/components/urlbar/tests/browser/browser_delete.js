/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test deleting the start of urls works correctly.
 */

add_task(async function () {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: "http://bug1105244.example.com/",
    title: "test",
  });

  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
  });

  await BrowserTestUtils.withNewTab("about:blank", testDelete);
});

function sendHome() {
  // unclear why VK_HOME doesn't work on Mac, but it doesn't...
  if (AppConstants.platform == "macosx") {
    EventUtils.synthesizeKey("KEY_ArrowLeft", { metaKey: true });
  } else {
    EventUtils.synthesizeKey("KEY_Home");
  }
}

function sendDelete() {
  EventUtils.synthesizeKey("KEY_Delete");
}

async function testDelete() {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "bug1105244",
  });

  // move to the start.
  sendHome();

  // delete the first few chars - each delete should operate on the input field.
  await UrlbarTestUtils.promisePopupOpen(window, sendDelete);
  Assert.equal(gURLBar.value, "ug1105244.example.com/");
  sendDelete();
  Assert.equal(gURLBar.value, "g1105244.example.com/");
}
