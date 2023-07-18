/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  await cleanUp();
});

add_task(async function test_autoFill_clear_properly_on_accent_char() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "",
    url: "https://example.com",
  });

  await search({
    searchString: "e",
    valueBefore: "e",
    valueAfter: "example.com/",
    placeholderAfter: "example.com/",
  });

  // Simulate macos accent character insertion. First the character is selected and
  // then replaced by the accentuated character.
  window.gURLBar.selectionStart = 0;
  window.gURLBar.selectionEnd = 1;
  EventUtils.sendChar("è", window);

  await UrlbarTestUtils.promiseSearchComplete(window);

  is(window.gURLBar.value, "è", "No auto complete for accent char.");

  await cleanUp();
});

async function cleanUp() {
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}
