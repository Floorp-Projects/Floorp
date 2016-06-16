add_task(function*() {
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://bug1105244.example.com/",
                                                title: "test" });

  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.remove(bm);
  });

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, testDelete);
});

function sendHome() {
  // unclear why VK_HOME doesn't work on Mac, but it doesn't...
  if (Services.appinfo.OS == "Darwin") {
    EventUtils.synthesizeKey("VK_LEFT", { altKey: true });
  } else {
    EventUtils.synthesizeKey("VK_HOME", {});
  }
}

function sendDelete() {
  EventUtils.synthesizeKey("VK_DELETE", {});
}

function* testDelete() {
  yield promiseAutocompleteResultPopup("bug1105244");

  // move to the start.
  sendHome();
  // delete the first few chars - each delete should operate on the input field.
  sendDelete();
  Assert.equal(gURLBar.inputField.value, "ug1105244");

  yield promisePopupShown(gURLBar.popup);

  sendDelete();
  Assert.equal(gURLBar.inputField.value, "g1105244");
}
