/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly on $_.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html><p>test code completion on $_`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info(
    "Test that there's no issue when trying to do an autocompletion without last " +
      "evaluation result"
  );
  await setInputValueForAutocompletion(hud, "$_.");
  is(autocompletePopup.items.length, 0, "autocomplete popup has no items");
  is(autocompletePopup.isOpen, false, "autocomplete popup is not open");

  info("Populate $_ by executing a command");
  await executeAndWaitForResultMessage(
    hud,
    `Object.create(null, Object.getOwnPropertyDescriptors({
    x: 1,
    y: "hello"
  }))`,
    `Object { x: 1, y: "hello" }`
  );

  await setInputValueForAutocompletion(hud, "$_.");
  checkInputCompletionValue(hud, "x", "'$_.' completion (completeNode)");
  ok(
    hasExactPopupLabels(autocompletePopup, ["x", "y"]),
    "autocomplete popup has expected items"
  );
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");

  await setInputValueForAutocompletion(hud, "$_.x.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  ok(
    hasPopupLabel(autocompletePopup, "toExponential"),
    "autocomplete popup has expected items"
  );

  await setInputValueForAutocompletion(hud, "$_.y.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  ok(
    hasPopupLabel(autocompletePopup, "trim"),
    "autocomplete popup has expected items"
  );

  info("Close autocomplete popup");
  await closeAutocompletePopup(hud);
});
