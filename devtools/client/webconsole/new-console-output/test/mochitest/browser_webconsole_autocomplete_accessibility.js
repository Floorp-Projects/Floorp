/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the autocomplete input is being blurred and focused when selecting a value.
// This will help screen-readers notify users of the value that was set in the input.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test code completion";

add_task(function* () {
  yield loadTab(TEST_URI);

  let hud = yield openConsole();

  let jsterm = hud.jsterm;
  let input = jsterm.inputNode;

  info("Type 'd' to open the autocomplete popup");
  yield autocomplete(jsterm, "d");

  // Add listeners for focus and blur events.
  let wasBlurred = false;
  input.addEventListener("blur", () => {
    wasBlurred = true;
  }, {
    once: true
  });

  let wasFocused = false;
  input.addEventListener("blur", () => {
    ok(wasBlurred, "jsterm input received a blur event before received back the focus");
    wasFocused = true;
  }, {
    once: true
  });

  info("Close the autocomplete popup by simulating a TAB key event");
  let onPopupClosed = jsterm.autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("VK_TAB", {});

  info("Wait for the autocomplete popup to be closed");
  yield onPopupClosed;

  ok(wasFocused, "jsterm input received a focus event");
});

function* autocomplete(jsterm, value) {
  let popup = jsterm.autocompletePopup;

  yield new Promise(resolve => {
    jsterm.setInputValue(value);
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  ok(popup.isOpen && popup.itemCount > 0,
    "Autocomplete popup is open and contains suggestions");
}
