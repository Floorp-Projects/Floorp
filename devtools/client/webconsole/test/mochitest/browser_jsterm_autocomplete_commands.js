/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that console commands are autocompleted.

const TEST_URI = `data:text/html;charset=utf-8,Test command autocomplete`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  const onPopUpOpen = autocompletePopup.once("popup-opened");

  info(`Enter ":"`);
  jsterm.focus();
  EventUtils.sendString(":");

  await onPopUpOpen;

  const expectedCommands = [":help", ":screenshot"];
  is(
    getPopupItems(autocompletePopup).join("\n"),
    expectedCommands.join("\n"),
    "popup contains expected commands"
  );

  let onAutocompleUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("s");
  await onAutocompleUpdated;
  checkInputCompletionValue(
    hud,
    "creenshot",
    "completion node has expected :screenshot value"
  );

  onAutocompleUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("KEY_Tab");
  await onAutocompleUpdated;
  is(
    getInputValue(hud),
    ":screenshot",
    "Tab key correctly completed :screenshot"
  );

  ok(!autocompletePopup.isOpen, "popup is closed after Tab");

  info("Test :hel completion");
  await setInputValue(hud, ":he");
  onAutocompleUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("l");

  await onAutocompleUpdated;
  checkInputCompletionValue(
    hud,
    "p",
    "completion node has expected :help value"
  );

  EventUtils.synthesizeKey("KEY_Tab");
  is(getInputValue(hud), ":help", "Tab key correctly completes :help");
});

function getPopupItems(popup) {
  return popup.items.map(item => item.label);
}
