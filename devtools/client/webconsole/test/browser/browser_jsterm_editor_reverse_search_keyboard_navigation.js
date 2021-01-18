/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure keyboard navigation works in editor mode and does
// not trigger reader mode (See 1682340).

const TEST_URI = `http://example.com/browser/toolkit/components/reader/test/readerModeArticle.html`;
/*
const TEST_URI = `http://example.com/document-builder.sjs?html=
  <html>
    <head>
      <meta name="description" content="this is the description">
      <title>Article title</title>
    </head>
    <body>
      <h1>Firefox DevTools Rock</h1>
      ${"<p>I shall not trigger reader mode".repeat(1000)}
    </body>
  </html>
  `;*/
const isMacOS = AppConstants.platform === "macosx";

add_task(async function() {
  await pushPref("devtools.webconsole.input.editor", true);
  await pushPref("reader.parse-on-load.enabled", true);

  const readerModeButtonEl = document.querySelector("#reader-mode-button");

  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(() => !readerModeButtonEl.hidden);

  const jstermHistory = [
    `document`,
    `document
         .querySelectorAll("*")
         .forEach(console.log)`,
    `Dog = "Snoopy"`,
  ];

  const onLastMessage = waitForMessage(hud, `"Snoopy"`, ".result");
  for (const input of jstermHistory) {
    execute(hud, input);
  }
  await onLastMessage;
  await openReverseSearch(hud);

  // Wait for a bit so reader mode would have some time to initialize.
  await wait(1000);
  is(
    readerModeButtonEl.getAttribute("readeractive"),
    "",
    "reader mode wasn't activated"
  );

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

  info(
    "Check that trying to navigate when there's only 1 result does not throw"
  );
  EventUtils.sendString("og");
  await waitFor(
    () => getReverseSearchInfoElement(hud).textContent === "1 result"
  );
  triggerPreviousResultShortcut();
  triggerNextResultShortcut();

  info("Check that trying to navigate when there's no result does not throw");
  EventUtils.sendString("g");
  await waitFor(
    () => getReverseSearchInfoElement(hud).textContent === "No results"
  );
  triggerPreviousResultShortcut();
  triggerNextResultShortcut();

  is(
    readerModeButtonEl.getAttribute("readeractive"),
    "",
    "reader mode still wasn't activated"
  );
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
