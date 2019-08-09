/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top-level await expressions work as expected.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Web Console test top-level await";

add_task(async function() {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  const executeAndWaitForResultMessage = (input, expectedOutput) =>
    executeAndWaitForMessage(hud, input, expectedOutput, ".result");

  info("Evaluate a top-level await expression");
  const simpleAwait = `await new Promise(r => setTimeout(() => r(["await1"]), 500))`;
  await executeAndWaitForResultMessage(simpleAwait, `Array [ "await1" ]`);

  // Check that the resulting promise of the async iife is not displayed.
  const messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  const messagesText = Array.from(messages)
    .map(n => n.textContent)
    .join(" - ");
  is(
    messagesText,
    `${simpleAwait} - Array [ "await1" ]`,
    "The output contains the the expected messages"
  );

  // Check that the timestamp of the result is accurate
  const {
    visibleMessages,
    messagesById,
  } = hud.ui.wrapper.getStore().getState().messages;
  const [commandId, resultId] = visibleMessages;
  const delta =
    messagesById.get(resultId).timeStamp -
    messagesById.get(commandId).timeStamp;
  ok(
    delta >= 500,
    `The result has a timestamp at least 500ms (${delta}ms) older than the command`
  );

  info("Check that assigning the result of a top-level await expression works");
  await executeAndWaitForResultMessage(
    `x = await new Promise(r => setTimeout(() => r("await2"), 500))`,
    `await2`
  );

  let message = await executeAndWaitForResultMessage(
    `"-" + x + "-"`,
    `"-await2-"`
  );
  ok(message.node, "`x` was assigned as expected");

  info("Check that a logged promise is still displayed as a promise");
  message = await executeAndWaitForResultMessage(
    `new Promise(r => setTimeout(() => r(1), 1000))`,
    `Promise {`
  );
  ok(message, "Promise are displayed as expected");
});
