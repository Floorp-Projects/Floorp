/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that disabling autocomplete for console

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>Test command autocomplete`;

add_task(async function() {
  // Run with autocomplete preference as false
  await pushPref("devtools.webconsole.input.autocomplete", false);
  await performTests_false();
});

async function performTests_false() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;
  info("web console opened");

  const { autocompletePopup: popup } = hud.jsterm;

  info(`Enter "w"`);
  jsterm.focus();
  EventUtils.sendString("w");
  // delay of 2 seconds.
  await wait(2000);
  ok(!popup.isOpen, "popup is not open");

  info("Check that Ctrl+Space opens the popup when preference is false");
  let onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await onUpdated;

  ok(popup.isOpen, "popup opens on Ctrl+Space");
  ok(!!popup.getItems().length, "'w' gave a list of suggestions");

  onUpdated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey("in");
  await onUpdated;
  ok(popup.getItems().length == 2, "'win' gave a list of suggestions");

  info("Check that the completion text is updated when it was displayed");
  await setInputValue(hud, "");
  EventUtils.sendString("deb");
  let updated = jsterm.once("autocomplete-updated");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await updated;

  ok(!popup.isOpen, "popup is not open");
  is(
    jsterm.getAutoCompletionText(),
    "ugger",
    "completion text has expected value"
  );

  updated = jsterm.once("autocomplete-updated");
  EventUtils.sendString("u");
  await updated;
  is(
    jsterm.getAutoCompletionText(),
    "gger",
    "completion text has expected value"
  );
}
