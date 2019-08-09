/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that code completion works properly on $_.

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<p>test code completion on $_`;

add_task(async function() {
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
  await executeAndWaitForMessage(
    hud,
    `Object.create(null, Object.getOwnPropertyDescriptors({
    x: 1,
    y: "hello"
  }))`,
    `Object { x: 1, y: "hello" }`
  );

  await setInputValueForAutocompletion(hud, "$_.");
  checkInputCompletionValue(hud, "x", "'$_.' completion (completeNode)");
  is(
    getAutocompletePopupLabels(autocompletePopup).join("|"),
    "x|y",
    "autocomplete popup has expected items"
  );
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");

  await setInputValueForAutocompletion(hud, "$_.x.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  is(
    getAutocompletePopupLabels(autocompletePopup).includes("toExponential"),
    true,
    "autocomplete popup has expected items"
  );

  await setInputValueForAutocompletion(hud, "$_.y.");
  is(autocompletePopup.isOpen, true, "autocomplete popup is open");
  is(
    getAutocompletePopupLabels(autocompletePopup).includes("trim"),
    true,
    "autocomplete popup has expected items"
  );
});

function getAutocompletePopupLabels(autocompletePopup) {
  return autocompletePopup.items.map(i => i.label);
}
