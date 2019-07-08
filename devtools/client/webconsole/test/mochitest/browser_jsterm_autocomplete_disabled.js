/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that disabling autocomplete for console

const TEST_URI = `data:text/html;charset=utf-8,Test command autocomplete`;

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

  let onPopUpOpen = popup.once("popup-opened");

  info("Check that Ctrl+Space opens the popup when preference is false");
  onPopUpOpen = popup.once("popup-opened");
  EventUtils.synthesizeKey(" ", { ctrlKey: true });
  await onPopUpOpen;

  ok(popup.isOpen, "popup opens on Ctrl+Space");

  const onUpdated = jsterm.once("autocomplete-updated");

  ok(popup.getItems().length > 0, "'w' gave a list of suggestions");

  EventUtils.synthesizeKey("in");
  await onUpdated;
  ok(popup.getItems().length == 2, "'win' gave a list of suggestions");
}
