add_task(async function() {
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://bug1105244.example.com/",
                                                title: "test" });

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(bm);
  });

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, testDelete);
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

async function testDelete() {
  await promiseAutocompleteResultPopup("bug1105244");

  // move to the start.
  sendHome();
  // delete the first few chars - each delete should operate on the input field.
  sendDelete();
  Assert.equal(gURLBar.inputField.value, "ug1105244");

  await promisePopupShown(gURLBar.popup);

  sendDelete();
  Assert.equal(gURLBar.inputField.value, "g1105244");
}
