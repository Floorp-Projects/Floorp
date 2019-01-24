// This test makes sure that when the user starts typing origins and URLs, the
// case of the user's search string is preserved inside the origin part of the
// autofilled string.

"use strict";

add_task(async function init() {
  await cleanUp();
});

add_task(async function origin() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/",
  }]);
  await promiseAutocompleteResultPopup("ExA");
  await waitForAutocompleteResultAt(0);
  Assert.equal(gURLBar.value, "ExAmple.com/");
  await cleanUp();
});

add_task(async function originPort() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/",
  }]);
  await promiseAutocompleteResultPopup("ExA");
  await waitForAutocompleteResultAt(0);
  Assert.equal(gURLBar.value, "ExAmple.com:8888/");
  await cleanUp();
});

add_task(async function originScheme() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/",
  }]);
  await promiseAutocompleteResultPopup("http://ExA");
  await waitForAutocompleteResultAt(0);
  Assert.equal(gURLBar.value, "http://ExAmple.com/");
  await cleanUp();
});

add_task(async function originPortScheme() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/",
  }]);
  await promiseAutocompleteResultPopup("http://ExA");
  await waitForAutocompleteResultAt(0);
  Assert.equal(gURLBar.value, "http://ExAmple.com:8888/");
  await cleanUp();
});

add_task(async function url() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com/foo",
  }]);
  await promiseAutocompleteResultPopup("ExAmple.com/f");
  Assert.equal(gURLBar.value, "ExAmple.com/foo");
  await cleanUp();
});

add_task(async function urlPort() {
  await PlacesTestUtils.addVisits([{
    uri: "http://example.com:8888/foo",
  }]);
  await promiseAutocompleteResultPopup("ExAmple.com:8888/f");
  Assert.equal(gURLBar.value, "ExAmple.com:8888/foo");
  await cleanUp();
});

async function cleanUp() {
  EventUtils.synthesizeKey("KEY_Escape");
  await promisePopupHidden(gURLBar.popup);
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
