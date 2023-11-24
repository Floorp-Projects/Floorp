/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that console commands are autocompleted.

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>Test command autocomplete`;

const {
  WebConsoleCommandsManager,
} = require("resource://devtools/server/actors/webconsole/commands/manager.js");

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  const { autocompletePopup } = jsterm;

  info(`Enter ":"`);
  jsterm.focus();
  let onAutocompleUpdated = jsterm.once("autocomplete-updated");
  EventUtils.sendString(":");
  await onAutocompleUpdated;

  const expectedCommands =
    WebConsoleCommandsManager.getAllColonCommandNames().map(name => `:${name}`);
  ok(
    hasExactPopupLabels(autocompletePopup, expectedCommands),
    "popup contains expected commands"
  );

  onAutocompleUpdated = jsterm.once("autocomplete-updated");
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
