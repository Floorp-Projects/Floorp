/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests reverse search features.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>Test reverse search`;
const isMacOS = AppConstants.platform === "macosx";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const jstermHistory = [
    `document`,
    `Dog = "Snoopy"`,
    `document
       .querySelectorAll("*")
       .forEach(console.log)`,
    `document`,
    `"a" + "😎"`,
  ];

  // We have to wait for the same message twice in order to wait for the evaluation line
  // as well as the result line
  const onLastMessage = waitForMessages({
    hud,
    messages: [{ text: `"a" + "😎"` }, { text: `"a😎"` }],
  });
  for (const input of jstermHistory) {
    execute(hud, input);
  }
  await onLastMessage;

  const initialValue = "initialValue";
  setInputValue(hud, initialValue);

  info("Check that the reverse search toolbar as the expected initial state");
  let reverseSearchElement = await openReverseSearch(hud);
  ok(
    reverseSearchElement,
    "Reverse search is displayed with a keyboard shortcut"
  );
  ok(
    !getReverseSearchInfoElement(hud),
    "The result info element is not displayed by default"
  );
  ok(
    !reverseSearchElement.querySelector(".search-result-button-prev") &&
      !reverseSearchElement.querySelector(".search-result-button-next"),
    "The results navigation buttons are not displayed by default"
  );
  is(
    getInputValue(hud),
    initialValue,
    "The jsterm value is not changed when opening reverse search"
  );
  is(isReverseSearchInputFocused(hud), true, "reverse search input is focused");

  EventUtils.sendString("d");
  let infoElement = await waitFor(() => getReverseSearchInfoElement(hud));
  is(
    infoElement.textContent,
    "3 of 3 results",
    "The reverse info has the expected text " +
      "— duplicated results (`document`) are coalesced"
  );

  const previousButton = reverseSearchElement.querySelector(
    ".search-result-button-prev"
  );
  const nextButton = reverseSearchElement.querySelector(
    ".search-result-button-next"
  );
  ok(previousButton, "Previous navigation button is now displayed");
  is(
    previousButton.title,
    `Previous result (${isMacOS ? "Ctrl + R" : "F9"})`,
    "Previous navigation button has expected title"
  );

  ok(nextButton, "Next navigation button is now displayed");
  is(
    nextButton.title,
    `Next result (${isMacOS ? "Ctrl + S" : "Shift + F9"})`,
    "Next navigation button has expected title"
  );
  is(getInputValue(hud), "document", "JsTerm has the expected input");
  is(
    hud.jsterm.autocompletePopup.isOpen,
    false,
    "Setting the input value did not trigger the autocompletion"
  );
  is(isReverseSearchInputFocused(hud), true, "reverse search input is focused");

  let onJsTermValueChanged = hud.jsterm.once("set-input-value");
  EventUtils.sendString("og");
  await onJsTermValueChanged;
  is(getInputValue(hud), `Dog = "Snoopy"`, "JsTerm input was updated");
  is(
    infoElement.textContent,
    "1 result",
    "The reverse info has the expected text"
  );
  ok(
    !reverseSearchElement.querySelector(".search-result-button-prev") &&
      !reverseSearchElement.querySelector(".search-result-button-next"),
    "The results navigation buttons are not displayed when there's only one result"
  );

  info("Check that the UI and results are updated when typing in the input");
  onJsTermValueChanged = hud.jsterm.once("set-input-value");
  EventUtils.sendString("g");
  await waitFor(() => reverseSearchElement.classList.contains("no-result"));
  is(
    getInputValue(hud),
    `Dog = "Snoopy"`,
    "JsTerm input was not updated since there's no results"
  );
  is(
    infoElement.textContent,
    "No results",
    "The reverse info has the expected text"
  );
  ok(
    !reverseSearchElement.querySelector(".search-result-button-prev") &&
      !reverseSearchElement.querySelector(".search-result-button-next"),
    "The results navigation buttons are not displayed when there's no result"
  );

  info("Check that Backspace updates the UI");
  EventUtils.synthesizeKey("KEY_Backspace");
  await waitFor(() => !reverseSearchElement.classList.contains("no-result"));
  is(
    infoElement.textContent,
    "1 result",
    "The reverse info has the expected text"
  );
  is(getInputValue(hud), `Dog = "Snoopy"`, "JsTerm kept its value");

  info("Check that Escape does not affect the jsterm value");
  EventUtils.synthesizeKey("KEY_Escape");
  await waitFor(() => !getReverseSearchElement(hud));
  is(
    getInputValue(hud),
    `Dog = "Snoopy"`,
    "Closing the input did not changed the JsTerm value"
  );
  is(isInputFocused(hud), true, "input is focused");

  info("Check that the search works with emojis");
  reverseSearchElement = await openReverseSearch(hud);
  onJsTermValueChanged = hud.jsterm.once("set-input-value");
  EventUtils.sendString("😎");
  infoElement = await waitFor(() => getReverseSearchInfoElement(hud));
  is(
    infoElement.textContent,
    "1 result",
    "The reverse info has the expected text"
  );

  info("Check that Enter evaluates the JsTerm and closes the UI");
  // We have to wait for the same message twice in order to wait for the evaluation line
  // as well as the result line
  const onMessage = waitForMessages({
    hud,
    messages: [{ text: `"a" + "😎"` }, { text: `"a😎"` }],
  });
  const onReverseSearchClose = waitFor(() => !getReverseSearchElement(hud));
  EventUtils.synthesizeKey("KEY_Enter");
  await Promise.all([onMessage, onReverseSearchClose]);
  ok(
    true,
    "Enter evaluates what's in the JsTerm and closes the reverse search UI"
  );
});
