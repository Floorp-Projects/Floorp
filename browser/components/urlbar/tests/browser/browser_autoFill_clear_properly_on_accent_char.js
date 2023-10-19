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
  gURLBar.selectionStart = 0;
  gURLBar.selectionEnd = 1;
  EventUtils.sendChar("è", window);

  await UrlbarTestUtils.promiseSearchComplete(window);

  is(gURLBar.value, "è", "No auto complete for accent char.");

  await cleanUp();
});

add_task(async function dont_clear_placeholder_if_autofill_accepted() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "",
    url: "https://abc.yz",
  });

  let selectionChangedPromise = waitForSelectionChange({ times: 2 });
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "abc",
  });
  await UrlbarTestUtils.promiseSearchComplete(window);

  // PromiseAutoCompleteResultPopup fires one input event and two
  // selectionchange events. If we don't wait for them to be fired before
  // entering navigation keys, the selection gets messed up.
  await selectionChangedPromise;

  Assert.equal(gURLBar.value, "abc.yz/", "autofilled value is as expected");
  info("Synthesizing keys");
  await sendNavigationKey("KEY_ArrowRight");
  await sendNavigationKey("KEY_ArrowLeft");
  await sendNavigationKey("KEY_ArrowLeft");
  await sendNavigationKey("KEY_ArrowLeft");

  EventUtils.sendChar("x");
  is(gURLBar.value, "abc.xyz/", "No auto complete for accent char.");

  await cleanUp();
});

add_task(async function dont_clear_placeholder_after_selection_change() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "",
    url: "https://mozilla.org/",
  });

  let userTypedValue = "mo";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: userTypedValue,
  });

  Assert.equal(
    gURLBar.value,
    "mozilla.org/",
    "autofilled value is as expected"
  );

  info("Simulate mouse click to change caret position.");
  let selectionChangedPromise = waitForSelectionChange();
  is(
    gURLBar.selectionStart,
    userTypedValue.length,
    " SelectionStart at the beginning of the placeholder"
  );
  is(
    gURLBar.selectionEnd,
    gURLBar.value.length,
    " Selection at the end of the placeholder"
  );
  gURLBar.selectionStart = 1;
  gURLBar.selectionEnd = 1;

  await selectionChangedPromise;
  await UrlbarTestUtils.promiseSearchComplete(window);

  EventUtils.sendChar("o", window);

  await UrlbarTestUtils.promiseSearchComplete(window);

  is(
    gURLBar.value,
    "moozilla.org/",
    "Autofill was not cleared and new character was inserted."
  );

  await cleanUp();
});

add_task(async function modify_autofilled_selection() {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "",
    url: "https://developer.mozilla.org/en-US/",
  });

  let userTypedValue = "d";
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: userTypedValue,
  });

  Assert.equal(
    gURLBar.value,
    "developer.mozilla.org/",
    "autofilled value is as expected"
  );
  await sendNavigationKey("KEY_ArrowDown");

  let selectionChangedPromise = waitForSelectionChange();
  gURLBar.selectionStart = gURLBar.value.length - 6;
  gURLBar.selectionEnd = gURLBar.value.length - 1;

  await selectionChangedPromise;
  await UrlbarTestUtils.promiseSearchComplete(window);

  EventUtils.sendChar("j", window);

  await UrlbarTestUtils.promiseSearchComplete(window);
  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL("https://developer.mozilla.org/j/"),
    "gURLBar contains correct modified autofilled value"
  );
});

async function cleanUp() {
  await UrlbarTestUtils.promisePopupClose(window, () => gURLBar.blur());
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
}

async function sendNavigationKey(key) {
  let selectionChangePromise = waitForSelectionChange();
  EventUtils.synthesizeKey(key);
  await selectionChangePromise;
}

async function waitForSelectionChange(options = { times: 1 }) {
  let observedSelectionChanges = 0;

  function handler(event, resolve) {
    observedSelectionChanges += 1;
    if (observedSelectionChanges == options.times) {
      resolve();
    }
  }

  await new Promise(resolve => {
    gURLBar.addEventListener("selectionchange", event =>
      handler(event, resolve)
    );
  });

  gURLBar.removeEventListener("selectionchange", handler);
}
