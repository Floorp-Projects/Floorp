/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that pressing Enter quickly after a letter that makes the input exactly match the
// item in the autocomplete popup does not insert unwanted character. See Bug 1595068.

const TEST_URI = `data:text/html;charset=utf-8,<script>
  var uvwxyz = "zyxwvu";
</script>Autocomplete race on Enter`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info(`Enter "scre" and wait for the autocomplete popup to be displayed`);
  let onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "scre");
  await onPopupOpened;
  checkInputCompletionValue(hud, "en", "completeNode has expected value");

  info(`Type "n" and quickly after, "Enter"`);
  let onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("e");
  await waitForTime(50);
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClosed;

  is(getInputValue(hud), "screen", "the input has the expected value");

  setInputValue(hud, "");

  info(
    "Check that it works when typed word match exactly the item in the popup"
  );
  onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "wind");
  await onPopupOpened;
  checkInputCompletionValue(hud, "ow", "completeNode has expected value");

  info(`Quickly type "o", "w" and "Enter"`);
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("o");
  await waitForTime(5);
  EventUtils.synthesizeKey("w");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClosed;

  is(getInputValue(hud), "window", "the input has the expected value");

  setInputValue(hud, "");

  info("Check that it works when there's no autocomplete popup");
  let onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  await setInputValueForAutocompletion(hud, "uvw");
  await onAutocompleteUpdated;
  checkInputCompletionValue(hud, "xyz", "completeNode has expected value");

  info(`Quickly type "x" and "Enter"`);
  EventUtils.synthesizeKey("x");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  await waitFor(
    () => getInputValue(hud) === "uvwxyz",
    "input has expected 'uvwxyz' value"
  );
  ok(true, "input has the expected value");

  setInputValue(hud, "");

  info(
    "Check that it works when there's no autocomplete popup and the whole word is typed"
  );
  onAutocompleteUpdated = jsterm.once("autocomplete-updated");
  await setInputValueForAutocompletion(hud, "uvw");
  await onAutocompleteUpdated;
  checkInputCompletionValue(hud, "xyz", "completeNode has expected value");

  info(`Quickly type "x", "y", "z" and "Enter"`);
  const onResultMessage = waitForMessage(hud, "zyxwvu", ".result");
  EventUtils.synthesizeKey("x");
  await waitForTime(5);
  EventUtils.synthesizeKey("y");
  await waitForTime(5);
  EventUtils.synthesizeKey("z");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  info("wait for result message");
  await onResultMessage;
  is(getInputValue(hud), "", "Expression was evaluated and input was cleared");

  setInputValue(hud, "");

  info("Check that it works when typed letter match another item in the popup");
  onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "[].so");
  await onPopupOpened;
  checkInputCompletionValue(hud, "me", "completeNode has expected value");
  is(
    autocompletePopup.items.map(({ label }) => label).join("|"),
    "some|sort",
    "autocomplete has expected items"
  );

  info(`Quickly type "m" and "Enter"`);
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("m");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClosed;
  is(getInputValue(hud), "[].some", "the input has the expected value");

  setInputValue(hud, "");

  info(
    "Hitting Enter quickly after a letter that should close the popup evaluates the expression"
  );
  onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "doc");
  await onPopupOpened;
  checkInputCompletionValue(hud, "ument", "completeNode has expected value");

  info(`Quickly type "x" and "Enter"`);
  onPopupClosed = autocompletePopup.once("popup-closed");
  const onMessage = waitForMessage(hud, "1", ".result");
  EventUtils.synthesizeKey("x");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  await Promise.all[(onPopupClosed, onMessage)];
  is(
    getInputValue(hud),
    "",
    "the input is empty and the expression was evaluated"
  );

  info(
    "Hitting Enter quickly after a letter that will make the expression exactly match another item than the selected one"
  );
  onPopupOpened = autocompletePopup.once("popup-opened");
  await setInputValueForAutocompletion(hud, "cons");
  await onPopupOpened;
  checkInputCompletionValue(hud, "ole", "completeNode has expected value");
  info(`Quickly type "t" and "Enter"`);
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("t");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClosed;
  is(getInputValue(hud), "const", "the input has the expected item");

  info(
    "Hitting Enter quickly after a letter when the expression has text after"
  );
  await setInputValueForAutocompletion(hud, "f(und");
  ok(
    hasExactPopupLabels(autocompletePopup, ["undefined"]),
    `the popup has the "undefined" item`
  );
  info(`Quickly type "e" and "Enter"`);
  onPopupClosed = autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("e");
  await waitForTime(5);
  EventUtils.synthesizeKey("KEY_Enter");
  await onPopupClosed;
  is(getInputValue(hud), "f(undefined)", "the input has the expected item");
});
