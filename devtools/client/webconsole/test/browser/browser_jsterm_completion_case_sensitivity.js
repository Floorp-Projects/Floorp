/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly in regards to case sensitivity.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><p>test case-sensitivity completion.
  <script>
    fooBar = Object.create(null, Object.getOwnPropertyDescriptors({
      Foo: 1,
      test: 2,
      Test: 3,
      TEST: 4,
    }));
    FooBar = true;
  </script>`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  const checkInput = (expected, assertionInfo) =>
    checkInputValueAndCursorPosition(hud, expected, assertionInfo);

  info("Check that lowercased input is case-insensitive");
  await setInputValueForAutocompletion(hud, "foob");

  ok(
    hasExactPopupLabels(autocompletePopup, ["fooBar", "FooBar"]),
    "popup has expected item, in expected order"
  );

  checkInputCompletionValue(hud, "ar", "completeNode has expected value");

  info("Check that filtering the autocomplete cache is also case insensitive");
  let onAutoCompleteUpdated = jsterm.once("autocomplete-updated");
  // Send "a" to make the input "fooba"
  EventUtils.sendString("a");
  await onAutoCompleteUpdated;

  checkInput("fooba|");
  ok(
    hasExactPopupLabels(autocompletePopup, ["fooBar", "FooBar"]),
    "popup cache filtering is also case-insensitive"
  );
  checkInputCompletionValue(hud, "r", "completeNode has expected value");

  info(
    "Check that accepting the completion value will change the input casing"
  );
  let onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkInput("fooBar|", "The input was completed with the correct casing");
  checkInputCompletionValue(hud, "", "completeNode is empty");

  info("Check that the popup is displayed with only 1 matching item");
  onAutoCompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(".f");
  await onAutoCompleteUpdated;
  ok(autocompletePopup.isOpen, "autocomplete popup is open");

  // Here we want to match "Foo", and since the completion text will only be "oo", we want
  // to display the popup so the user knows that we are matching "Foo" and not "foo".
  checkInput("fooBar.f|");
  ok(true, "The popup was opened even if there's 1 item matching");
  ok(
    hasExactPopupLabels(autocompletePopup, ["Foo"]),
    "popup has expected item"
  );
  checkInputCompletionValue(hud, "oo", "completeNode has expected value");

  onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkInput("fooBar.Foo|", "The input was completed with the correct casing");
  checkInputCompletionValue(hud, "", "completeNode is empty");

  info("Check that Javascript keywords are displayed first");
  await setInputValueForAutocompletion(hud, "func");

  ok(
    hasExactPopupLabels(autocompletePopup, ["function", "Function"]),
    "popup has expected item"
  );
  checkInputCompletionValue(hud, "tion", "completeNode has expected value");

  onPopupClose = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");
  await onPopupClose;
  checkInput("function|", "The input was completed as expected");
  checkInputCompletionValue(hud, "", "completeNode is empty");

  info("Check that filtering the cache works like on the server");
  await setInputValueForAutocompletion(hud, "fooBar.");
  ok(
    hasExactPopupLabels(autocompletePopup, ["test", "Foo", "Test", "TEST"]),
    "popup has expected items"
  );

  onAutoCompleteUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("T");
  await onAutoCompleteUpdated;
  ok(
    hasExactPopupLabels(autocompletePopup, ["Test", "TEST"]),
    "popup was filtered case-sensitively, as expected"
  );

  info("Close autocomplete popup");
  await closeAutocompletePopup(hud);
});
