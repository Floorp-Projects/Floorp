/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(function*() {
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                url: "http://example.com/",
                                                title: "test" });

  registerCleanupFunction(function* () {
    yield PlacesUtils.bookmarks.remove(bm);
  });

  // We do this test with both unifiedcomplete disabled and enabled.
  let ucpref = Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete");
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", ucpref);
  });

  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", false);
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, testDelete);

  Services.prefs.setBoolPref("browser.urlbar.unifiedcomplete", true);
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
  yield promiseAutocompleteResultPopup("exam");

  // move to the start.
  sendHome();
  // delete the first few chars - each delete should operate on the input field.
  sendDelete();
  Assert.equal(gURLBar.inputField.value, "xam");

  yield promisePopupShown(gURLBar.popup);

  sendDelete();
  Assert.equal(gURLBar.inputField.value, "am");
}
