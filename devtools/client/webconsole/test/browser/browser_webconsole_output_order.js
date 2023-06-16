/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that any output created from calls to the console API comes before the
// echoed JavaScript.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const evaluationResultMessage = await executeAndWaitForResultMessage(
    hud,
    `for (let i = 0; i < 5; i++) { console.log("item-" + i); }`,
    "undefined"
  );

  info("Wait for all the log messages to be displayed");
  // Console messages are batched by the Resource watcher API and might be rendered after
  // the result message.
  const logMessages = await waitFor(() => {
    const messages = findConsoleAPIMessages(hud, "item-", ".log");
    return messages.length === 5 ? messages : null;
  });

  const commandMessage = findMessageByType(hud, "", ".command");
  is(
    commandMessage.nextElementSibling,
    logMessages[0],
    `the command message is followed by the first log message ( Got "${commandMessage.nextElementSibling.textContent}")`
  );

  for (let i = 0; i < logMessages.length; i++) {
    ok(
      logMessages[i].textContent.includes(`item-${i}`),
      `The log message item-${i} is at the expected position ( Got "${logMessages[i].textContent}")`
    );
  }

  is(
    logMessages[logMessages.length - 1].nextElementSibling,
    evaluationResultMessage.node,
    "The evaluation result is after the last log message"
  );
});
