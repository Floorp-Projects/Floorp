/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the autocomplete input is being blurred and focused when selecting a value.
// This will help screen-readers notify users of the value that was set in the input.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<p>test code completion";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const jsterm = hud.jsterm;
  const input = jsterm.inputNode;

  info("Test that the console input is not treated as a live region");
  ok(!isElementInLiveRegion(input), "Console input is not treated as a live region");

  info("Type 'd' to open the autocomplete popup");
  await autocomplete(jsterm, "d");

  // Add listeners for focus and blur events.
  let wasBlurred = false;
  input.addEventListener("blur", () => {
    wasBlurred = true;
  }, {
    once: true
  });

  let wasFocused = false;
  input.addEventListener("focus", () => {
    ok(wasBlurred, "jsterm input received a blur event before received back the focus");
    wasFocused = true;
  }, {
    once: true
  });

  info("Close the autocomplete popup by simulating a TAB key event");
  const onPopupClosed = jsterm.autocompletePopup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Tab");

  info("Wait for the autocomplete popup to be closed");
  await onPopupClosed;

  ok(wasFocused, "jsterm input received a focus event");
});

async function autocomplete(jsterm, value) {
  const popup = jsterm.autocompletePopup;

  await new Promise(resolve => {
    jsterm.setInputValue(value);
    jsterm.complete(jsterm.COMPLETE_HINT_ONLY, resolve);
  });

  ok(popup.isOpen && popup.itemCount > 0,
    "Autocomplete popup is open and contains suggestions");
}

function isElementInLiveRegion(element) {
  if (!element) {
    return false;
  }

  if (element.hasAttribute("aria-live")) {
    return element.getAttribute("aria-live") !== "off";
  }

  return isElementInLiveRegion(element.parentNode);
}
