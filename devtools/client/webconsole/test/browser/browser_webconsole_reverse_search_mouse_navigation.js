/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests reverse search results mouse navigation.

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8>Test reverse search`;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const jstermHistory = [
    `document`,
    `document
       .querySelectorAll("span")
       .forEach(console.log)`,
    `Dog = "Snoopy"`,
  ];

  const onLastMessage = waitForMessage(hud, `"Snoopy"`);
  for (const input of jstermHistory) {
    execute(hud, input);
  }
  await onLastMessage;

  await openReverseSearch(hud);
  EventUtils.sendString("d");
  const infoElement = await waitFor(() => getReverseSearchInfoElement(hud));
  is(
    infoElement.textContent,
    "3 of 3 results",
    "The reverse info has the expected text"
  );

  is(getInputValue(hud), jstermHistory[2], "JsTerm has the expected input");
  is(
    hud.jsterm.autocompletePopup.isOpen,
    false,
    "Setting the input value did not trigger the autocompletion"
  );

  await navigateResultsAndCheckState(hud, {
    direction: "previous",
    expectedInfoText: "2 of 3 results",
    expectedJsTermInputValue: jstermHistory[1],
  });

  await navigateResultsAndCheckState(hud, {
    direction: "previous",
    expectedInfoText: "1 of 3 results",
    expectedJsTermInputValue: jstermHistory[0],
  });

  info(
    "Check that we go back to the last matching item if we were at the first"
  );
  await navigateResultsAndCheckState(hud, {
    direction: "previous",
    expectedInfoText: "3 of 3 results",
    expectedJsTermInputValue: jstermHistory[2],
  });

  await navigateResultsAndCheckState(hud, {
    direction: "next",
    expectedInfoText: "1 of 3 results",
    expectedJsTermInputValue: jstermHistory[0],
  });

  await navigateResultsAndCheckState(hud, {
    direction: "next",
    expectedInfoText: "2 of 3 results",
    expectedJsTermInputValue: jstermHistory[1],
  });

  await navigateResultsAndCheckState(hud, {
    direction: "next",
    expectedInfoText: "3 of 3 results",
    expectedJsTermInputValue: jstermHistory[2],
  });
});

async function navigateResultsAndCheckState(
  hud,
  { direction, expectedInfoText, expectedJsTermInputValue }
) {
  const onJsTermValueChanged = hud.jsterm.once("set-input-value");
  if (direction === "previous") {
    clickPreviousButton(hud);
  } else {
    clickNextButton(hud);
  }
  await onJsTermValueChanged;

  is(getInputValue(hud), expectedJsTermInputValue, "JsTerm has expected value");

  const infoElement = getReverseSearchInfoElement(hud);
  is(
    infoElement.textContent,
    expectedInfoText,
    "The reverse info has the expected text"
  );
  is(
    isReverseSearchInputFocused(hud),
    true,
    "reverse search input is still focused"
  );
}

function clickPreviousButton(hud) {
  const reverseSearchElement = getReverseSearchElement(hud);
  if (!reverseSearchElement) {
    return;
  }
  const button = reverseSearchElement.querySelector(
    ".search-result-button-prev"
  );
  if (!button) {
    return;
  }

  button.click();
}

function clickNextButton(hud) {
  const reverseSearchElement = getReverseSearchElement(hud);
  if (!reverseSearchElement) {
    return;
  }
  const button = reverseSearchElement.querySelector(
    ".search-result-button-next"
  );
  if (!button) {
    return;
  }

  button.click();
}
