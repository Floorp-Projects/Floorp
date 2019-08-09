/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console API calls in the content page appear in the browser console.

"use strict";

const contentArgs = {
  log: "MyLog",
  warn: "MyWarn",
  error: "MyError",
  info: "MyInfo",
  debug: "MyDebug",
  counterName: "MyCounter",
  timerName: "MyTimer",
};

const TEST_URI = `data:text/html,<meta charset=utf8>console API calls<script>
  console.log("${contentArgs.log}");
  console.warn("${contentArgs.warn}");
  console.error("${contentArgs.error}");
  console.info("${contentArgs.info}");
  console.debug("${contentArgs.debug}");
  console.count("${contentArgs.counterName}");
  console.time("${contentArgs.timerName}");
  console.timeLog("${contentArgs.timerName}");
  console.timeEnd("${contentArgs.timerName}");
  console.trace();
  console.assert(false, "err");
</script>`;

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);

  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  hud.ui.clearOutput();

  await addTab(TEST_URI);

  const expectedMessages = [
    contentArgs.log,
    contentArgs.warn,
    contentArgs.error,
    contentArgs.info,
    contentArgs.debug,
    `${contentArgs.counterName}: 1`,
    `${contentArgs.timerName}:`,
    `timer ended`,
    `console.trace`,
    `Assertion failed`,
  ];
  await waitFor(() =>
    expectedMessages.every(expectedMessage => findMessage(hud, expectedMessage))
  );

  ok(true, "Expected messages are displayed in the browser console");

  info("Uncheck the Show content messages checkbox");
  const onContentMessagesHidden = waitFor(
    () => !findMessage(hud, contentArgs.log)
  );
  const checkbox = hud.ui.outputNode.querySelector(
    ".webconsole-filterbar-primary .filter-checkbox"
  );
  checkbox.click();
  await onContentMessagesHidden;

  for (const expectedMessage of expectedMessages) {
    ok(!findMessage(hud, expectedMessage), `"${expectedMessage}" is hidden`);
  }

  info("Check the Show content messages checkbox");
  const onContentMessagesDisplayed = waitFor(() =>
    expectedMessages.every(expectedMessage => findMessage(hud, expectedMessage))
  );
  checkbox.click();
  await onContentMessagesDisplayed;

  for (const expectedMessage of expectedMessages) {
    ok(findMessage(hud, expectedMessage), `"${expectedMessage}" is visible`);
  }
});
