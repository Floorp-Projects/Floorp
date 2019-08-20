/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that a few of aspects of autofill are correctly
// preserved:
//
// * Autofill should preserve the user's case.  If you type ExA, it should be
//   autofilled to ExAmple.com/, not example.com/.
// * When you key down and then back up to the autofill result, autofill should
//   be restored, with the text selection and the user's case both preserved.
// * When you key down/up so that no result is selected, the value that the
//   user typed to trigger autofill should be restored in the input.

"use strict";

add_task(async function init() {
  await cleanUp();
});

add_task(async function origin() {
  await PlacesTestUtils.addVisits([
    "http://example.com/",
    "http://mozilla.org/example",
  ]);
  await promiseAutocompleteResultPopup("ExA", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "ExAmple.com/");
  Assert.equal(gURLBar.selectionStart, "ExA".length);
  Assert.equal(gURLBar.selectionEnd, "ExAmple.com/".length);
  checkKeys([
    ["KEY_ArrowDown", "http://mozilla.org/example", 1],
    ["KEY_ArrowDown", "ExA", -1],
    ["KEY_ArrowUp", "http://mozilla.org/example", 1],
    ["KEY_ArrowUp", "ExAmple.com/", 0],
    ["KEY_ArrowUp", "ExA", -1],
    ["KEY_ArrowDown", "ExAmple.com/", 0],
  ]);
  await cleanUp();
});

add_task(async function originPort() {
  await PlacesTestUtils.addVisits([
    "http://example.com:8888/",
    "http://mozilla.org/example",
  ]);
  await promiseAutocompleteResultPopup("ExA", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "ExAmple.com:8888/");
  Assert.equal(gURLBar.selectionStart, "ExA".length);
  Assert.equal(gURLBar.selectionEnd, "ExAmple.com:8888/".length);
  checkKeys([
    ["KEY_ArrowDown", "http://mozilla.org/example", 1],
    ["KEY_ArrowDown", "ExA", -1],
    ["KEY_ArrowUp", "http://mozilla.org/example", 1],
    ["KEY_ArrowUp", "ExAmple.com:8888/", 0],
    ["KEY_ArrowUp", "ExA", -1],
    ["KEY_ArrowDown", "ExAmple.com:8888/", 0],
  ]);
  await cleanUp();
});

add_task(async function originScheme() {
  await PlacesTestUtils.addVisits([
    "http://example.com/",
    "http://mozilla.org/example",
  ]);
  await promiseAutocompleteResultPopup("http://ExA", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "http://ExAmple.com/");
  Assert.equal(gURLBar.selectionStart, "http://ExA".length);
  Assert.equal(gURLBar.selectionEnd, "http://ExAmple.com/".length);
  checkKeys([
    ["KEY_ArrowDown", "http://mozilla.org/example", 1],
    ["KEY_ArrowDown", "http://ExA", -1],
    ["KEY_ArrowUp", "http://mozilla.org/example", 1],
    ["KEY_ArrowUp", "http://ExAmple.com/", 0],
    ["KEY_ArrowUp", "http://ExA", -1],
    ["KEY_ArrowDown", "http://ExAmple.com/", 0],
  ]);
  await cleanUp();
});

add_task(async function originPortScheme() {
  await PlacesTestUtils.addVisits([
    "http://example.com:8888/",
    "http://mozilla.org/example",
  ]);
  await promiseAutocompleteResultPopup("http://ExA", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "http://ExAmple.com:8888/");
  Assert.equal(gURLBar.selectionStart, "http://ExA".length);
  Assert.equal(gURLBar.selectionEnd, "http://ExAmple.com:8888/".length);
  checkKeys([
    ["KEY_ArrowDown", "http://mozilla.org/example", 1],
    ["KEY_ArrowDown", "http://ExA", -1],
    ["KEY_ArrowUp", "http://mozilla.org/example", 1],
    ["KEY_ArrowUp", "http://ExAmple.com:8888/", 0],
    ["KEY_ArrowUp", "http://ExA", -1],
    ["KEY_ArrowDown", "http://ExAmple.com:8888/", 0],
  ]);
  await cleanUp();
});

add_task(async function url() {
  await PlacesTestUtils.addVisits([
    "http://example.com/foo",
    "http://example.com/foo",
    "http://example.com/fff",
  ]);
  await promiseAutocompleteResultPopup("ExAmple.com/f", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "ExAmple.com/foo");
  Assert.equal(gURLBar.selectionStart, "ExAmple.com/f".length);
  Assert.equal(gURLBar.selectionEnd, "ExAmple.com/foo".length);
  checkKeys([
    ["KEY_ArrowDown", "http://example.com/fff", 1],
    ["KEY_ArrowDown", "ExAmple.com/f", -1],
    ["KEY_ArrowUp", "http://example.com/fff", 1],
    ["KEY_ArrowUp", "ExAmple.com/foo", 0],
    ["KEY_ArrowUp", "ExAmple.com/f", -1],
    ["KEY_ArrowDown", "ExAmple.com/foo", 0],
  ]);
  await cleanUp();
});

add_task(async function urlPort() {
  await PlacesTestUtils.addVisits([
    "http://example.com:8888/foo",
    "http://example.com:8888/foo",
    "http://example.com:8888/fff",
  ]);
  await promiseAutocompleteResultPopup("ExAmple.com:8888/f", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "ExAmple.com:8888/foo");
  Assert.equal(gURLBar.selectionStart, "ExAmple.com:8888/f".length);
  Assert.equal(gURLBar.selectionEnd, "ExAmple.com:8888/foo".length);
  checkKeys([
    ["KEY_ArrowDown", "http://example.com:8888/fff", 1],
    ["KEY_ArrowDown", "ExAmple.com:8888/f", -1],
    ["KEY_ArrowUp", "http://example.com:8888/fff", 1],
    ["KEY_ArrowUp", "ExAmple.com:8888/foo", 0],
    ["KEY_ArrowUp", "ExAmple.com:8888/f", -1],
    ["KEY_ArrowDown", "ExAmple.com:8888/foo", 0],
  ]);
  await cleanUp();
});

add_task(async function tokenAlias() {
  await Services.search.addEngineWithDetails("Test", {
    alias: "@example",
    template: "http://example.com/?search={searchTerms}",
  });
  registerCleanupFunction(async function() {
    let engine = Services.search.getEngineByName("Test");
    await Services.search.removeEngine(engine);
  });
  await promiseAutocompleteResultPopup("@ExA", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "@ExAmple ");
  Assert.equal(gURLBar.selectionStart, "@ExA".length);
  Assert.equal(gURLBar.selectionEnd, "@ExAmple ".length);
  // Token aliases (1) hide the one-off buttons and (2) show only a single
  // result, the "Search with" result for the alias's engine, so there's no way
  // to key up/down to change the selection, so this task doesn't check key
  // presses like the others do.
  await cleanUp();
});

// This test is a little different from the others.  It backspaces over the
// autofilled substring and checks that autofill is *not* preserved.
add_task(async function backspaceNoAutofill() {
  await PlacesTestUtils.addVisits([
    "http://example.com/",
    "http://example.com/",
    "http://mozilla.org/example",
  ]);
  await promiseAutocompleteResultPopup("ExA", window, true);
  let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(details.autofill);
  Assert.equal(gURLBar.value, "ExAmple.com/");
  Assert.equal(gURLBar.selectionStart, "ExA".length);
  Assert.equal(gURLBar.selectionEnd, "ExAmple.com/".length);

  EventUtils.synthesizeKey("KEY_Backspace");
  await UrlbarTestUtils.promiseSearchComplete(window);
  details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
  Assert.ok(!details.autofill);
  Assert.equal(gURLBar.value, "ExA");
  Assert.equal(gURLBar.selectionStart, "ExA".length);
  Assert.equal(gURLBar.selectionEnd, "ExA".length);

  let heuristicValue = "ExA";

  checkKeys([
    ["KEY_ArrowDown", "http://example.com/", 1],
    ["KEY_ArrowDown", "http://mozilla.org/example", 2],
    ["KEY_ArrowDown", "ExA", -1],
    ["KEY_ArrowUp", "http://mozilla.org/example", 2],
    ["KEY_ArrowUp", "http://example.com/", 1],
    ["KEY_ArrowUp", heuristicValue, 0],
    ["KEY_ArrowUp", "ExA", -1],
    ["KEY_ArrowDown", heuristicValue, 0],
  ]);

  await cleanUp();
});

function checkKeys(testTuples) {
  for (let [key, value, selectedIndex] of testTuples) {
    EventUtils.synthesizeKey(key);
    Assert.equal(UrlbarTestUtils.getSelectedIndex(window), selectedIndex);
    Assert.equal(gURLBar.untrimmedValue, value);
  }
}

async function cleanUp() {
  EventUtils.synthesizeKey("KEY_Escape");
  await UrlbarTestUtils.promisePopupClose(window);
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
