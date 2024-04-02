/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure keyboard navigation works in editor mode and does
// not trigger reader mode (See 1682340).

const TEST_URI = `http://example.com/browser/toolkit/components/reader/tests/browser/readerModeArticle.html`;
const isMacOS = AppConstants.platform === "macosx";

add_task(async function () {
  await pushPref("devtools.webconsole.input.editor", true);
  await pushPref("reader.parse-on-load.enabled", true);
  // Disable eager evaluation to avoid intermittent failures due to pending
  // requests to evaluateJSAsync.
  await pushPref("devtools.webconsole.input.eagerEvaluation", false);

  const readerModeButtonEl = document.querySelector("#reader-mode-button");

  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => !readerModeButtonEl.hidden,
    "wait for the reader mode button to be displayed"
  );

  const jstermHistory = [
    `document
         .querySelectorAll("*")
         .forEach(console.log)`,
    `Dog = "Snoopy"`,
  ];

  const onLastMessage = waitForMessageByType(hud, `"Snoopy"`, ".result");
  for (const input of jstermHistory) {
    execute(hud, input);
  }
  await onLastMessage;
  await openReverseSearch(hud);

  // Wait for a bit so reader mode would have some time to initialize.
  await wait(1000);
  is(
    readerModeButtonEl.getAttribute("readeractive"),
    null,
    "reader mode wasn't activated"
  );

  EventUtils.sendString("d");
  const infoElement = await waitFor(() => getReverseSearchInfoElement(hud));
  is(
    infoElement.textContent,
    "2 of 2 results",
    "The reverse info has the expected text"
  );

  is(getInputValue(hud), jstermHistory[1], "JsTerm has the expected input");

  await navigateResultsAndCheckState(hud, {
    direction: "previous",
    expectedInfoText: "1 of 2 results",
    expectedJsTermInputValue: jstermHistory[0],
  });

  await navigateResultsAndCheckState(hud, {
    direction: "next",
    expectedInfoText: "2 of 2 results",
    expectedJsTermInputValue: jstermHistory[1],
  });

  // Wait for a bit so reader mode would have some time to initialize.
  await wait(1000);
  is(
    readerModeButtonEl.getAttribute("readeractive"),
    null,
    "reader mode still wasn't activated"
  );

  await closeToolbox();
});

async function navigateResultsAndCheckState(
  hud,
  { direction, expectedInfoText, expectedJsTermInputValue }
) {
  const onJsTermValueChanged = hud.jsterm.once("set-input-value");
  if (direction === "previous") {
    triggerPreviousResultShortcut();
  } else {
    triggerNextResultShortcut();
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

function triggerPreviousResultShortcut() {
  if (isMacOS) {
    EventUtils.synthesizeKey("r", { ctrlKey: true });
  } else {
    EventUtils.synthesizeKey("VK_F9");
  }
}

function triggerNextResultShortcut() {
  if (isMacOS) {
    EventUtils.synthesizeKey("s", { ctrlKey: true });
  } else {
    EventUtils.synthesizeKey("VK_F9", { shiftKey: true });
  }
}
