/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This test makes sure that autofill works as expected when typing, character
// by character.

"use strict";

add_task(async function init() {
  await cleanUp();
});

add_task(async function origin() {
  await PlacesTestUtils.addVisits(["http://example.com/"]);
  // all lowercase
  await typeAndCheck([
    ["e", "example.com/"],
    ["x", "example.com/"],
    ["a", "example.com/"],
    ["m", "example.com/"],
    ["p", "example.com/"],
    ["l", "example.com/"],
    ["e", "example.com/"],
    [".", "example.com/"],
    ["c", "example.com/"],
    ["o", "example.com/"],
    ["m", "example.com/"],
    ["/", "example.com/"],
  ]);
  gURLBar.value = "";
  // mixed case
  await typeAndCheck([
    ["E", "Example.com/"],
    ["x", "Example.com/"],
    ["A", "ExAmple.com/"],
    ["m", "ExAmple.com/"],
    ["P", "ExAmPle.com/"],
    ["L", "ExAmPLe.com/"],
    ["e", "ExAmPLe.com/"],
    [".", "ExAmPLe.com/"],
    ["C", "ExAmPLe.Com/"],
    ["o", "ExAmPLe.Com/"],
    ["M", "ExAmPLe.CoM/"],
    ["/", "ExAmPLe.CoM/"],
  ]);
  await cleanUp();
});

add_task(async function url() {
  await PlacesTestUtils.addVisits(["http://example.com/foo/bar"]);
  // all lowercase
  await typeAndCheck([
    ["e", "example.com/"],
    ["x", "example.com/"],
    ["a", "example.com/"],
    ["m", "example.com/"],
    ["p", "example.com/"],
    ["l", "example.com/"],
    ["e", "example.com/"],
    [".", "example.com/"],
    ["c", "example.com/"],
    ["o", "example.com/"],
    ["m", "example.com/"],
    ["/", "example.com/"],
    ["f", "example.com/foo/"],
    ["o", "example.com/foo/"],
    ["o", "example.com/foo/"],
    ["/", "example.com/foo/"],
    ["b", "example.com/foo/bar"],
    ["a", "example.com/foo/bar"],
    ["r", "example.com/foo/bar"],
  ]);
  gURLBar.value = "";
  // mixed case
  await typeAndCheck([
    ["E", "Example.com/"],
    ["x", "Example.com/"],
    ["A", "ExAmple.com/"],
    ["m", "ExAmple.com/"],
    ["P", "ExAmPle.com/"],
    ["L", "ExAmPLe.com/"],
    ["e", "ExAmPLe.com/"],
    [".", "ExAmPLe.com/"],
    ["C", "ExAmPLe.Com/"],
    ["o", "ExAmPLe.Com/"],
    ["M", "ExAmPLe.CoM/"],
    ["/", "ExAmPLe.CoM/"],
    ["f", "ExAmPLe.CoM/foo/"],
    ["o", "ExAmPLe.CoM/foo/"],
    ["o", "ExAmPLe.CoM/foo/"],
    ["/", "ExAmPLe.CoM/foo/"],
    ["b", "ExAmPLe.CoM/foo/bar"],
    ["a", "ExAmPLe.CoM/foo/bar"],
    ["r", "ExAmPLe.CoM/foo/bar"],
  ]);
  await cleanUp();
});

add_task(async function tokenAlias() {
  // We have built-in engine aliases that may conflict with the one we choose
  // here in terms of autofill, so be careful and choose a weird alias.
  await Services.search.addEngineWithDetails("Test", {
    alias: "@__example",
    template: "http://example.com/?search={searchTerms}",
  });
  registerCleanupFunction(async function() {
    let engine = Services.search.getEngineByName("Test");
    await Services.search.removeEngine(engine);
  });
  // all lowercase
  await typeAndCheck([
    ["@", "@"],
    ["_", "@__example "],
    ["_", "@__example "],
    ["e", "@__example "],
    ["x", "@__example "],
    ["a", "@__example "],
    ["m", "@__example "],
    ["p", "@__example "],
    ["l", "@__example "],
    ["e", "@__example "],
  ]);
  gURLBar.value = "";
  // mixed case
  await typeAndCheck([
    ["@", "@"],
    ["_", "@__example "],
    ["_", "@__example "],
    ["E", "@__Example "],
    ["x", "@__Example "],
    ["A", "@__ExAmple "],
    ["m", "@__ExAmple "],
    ["P", "@__ExAmPle "],
    ["L", "@__ExAmPLe "],
    ["e", "@__ExAmPLe "],
  ]);
  await cleanUp();
});

async function typeAndCheck(values) {
  gURLBar.focus();
  for (let i = 0; i < values.length; i++) {
    let [char, expectedInputValue] = values[i];
    info(
      `Typing: i=${i} char=${char} ` +
        `substring="${expectedInputValue.substring(0, i + 1)}"`
    );
    EventUtils.synthesizeKey(char);
    if (i == 0 && char == "@") {
      // A single "@" doesn't trigger autofill, so skip the checks below.  (It
      // shows all the @ aliases.)
      continue;
    }
    await UrlbarTestUtils.promiseSearchComplete(window);
    let restIsSpaces = !expectedInputValue.substring(i + 1).trim();
    Assert.equal(gURLBar.value, expectedInputValue);
    Assert.equal(gURLBar.selectionStart, i + 1);
    Assert.equal(gURLBar.selectionEnd, expectedInputValue.length);
    if (restIsSpaces) {
      // Autofilled @ aliases have a trailing space.  We should check that the
      // space is autofilled when each preceding character is typed, but once
      // the final non-space char is typed, autofill actually stops and the
      // trailing space is not autofilled.  (Which is maybe not the way it
      // should work...)  Skip the check below.
      continue;
    }
    let details = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
    Assert.ok(details.autofill);
  }
}

async function cleanUp() {
  gURLBar.value = "";
  await UrlbarTestUtils.promisePopupClose(window, () => {
    EventUtils.synthesizeKey("KEY_Escape");
  });
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
