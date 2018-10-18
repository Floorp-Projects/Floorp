/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top-level await expression work as expected.

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,Web Console test top-level await";

add_task(async function() {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  // Run test with legacy JsTerm
  await pushPref("devtools.webconsole.jsterm.codeMirror", false);
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  const executeAndWaitForResultMessage = (input, expectedOutput) =>
    executeAndWaitForMessage(hud, input, expectedOutput, ".result");

  info("Evaluate a top-level await expression");
  const simpleAwait = `await new Promise(r => setTimeout(() => r(["await1"]), 500))`;
  await executeAndWaitForResultMessage(
    simpleAwait,
    `Array [ "await1" ]`,
  );

  // Check that the resulting promise of the async iife is not displayed.
  let messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  let messagesText = Array.from(messages).map(n => n.textContent).join(" - ");
  is(messagesText, `${simpleAwait} - Array [ "await1" ]`,
    "The output contains the the expected messages");

  // Check that the timestamp of the result is accurate
  const {
    visibleMessages,
    messagesById
  } = hud.ui.consoleOutput.getStore().getState().messages;
  const [commandId, resultId] = visibleMessages;
  const delta = messagesById.get(resultId).timeStamp -
    messagesById.get(commandId).timeStamp;
  ok(delta >= 500,
    `The result has a timestamp at least 500ms (${delta}ms) older than the command`);

  info("Check that assigning the result of a top-level await expression works");
  await executeAndWaitForResultMessage(
    `x = await new Promise(r => setTimeout(() => r("await2"), 500))`,
    `await2`,
  );

  let message = await executeAndWaitForResultMessage(
    `"-" + x + "-"`,
    `"-await2-"`,
  );
  ok(message.node, "`x` was assigned as expected");

  info("Check that concurrent await expression work fine");
  hud.ui.clearOutput();
  const delays = [1000, 500, 2000, 1500];
  const inputs = delays.map(delay => `await new Promise(
    r => setTimeout(() => r("await-concurrent-" + ${delay}), ${delay}))`);

  // Let's wait for the message that sould be displayed last.
  const onMessage = waitForMessage(hud, "await-concurrent-2000", ".message.result");
  for (const input of inputs) {
    jsterm.execute(input);
  }
  await onMessage;

  messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  messagesText = Array.from(messages).map(n => n.textContent);
  const expectedMessages = [
    ...inputs,
    `"await-concurrent-500"`,
    `"await-concurrent-1000"`,
    `"await-concurrent-1500"`,
    `"await-concurrent-2000"`,
  ];
  is(JSON.stringify(messagesText, null, 2), JSON.stringify(expectedMessages, null, 2),
    "The output contains the the expected messages, in the expected order");

  info("Check that a logged promise is still displayed as a promise");
  message = await executeAndWaitForResultMessage(
    `new Promise(r => setTimeout(() => r(1), 1000))`,
    `Promise {`,
  );
  ok(message, "Promise are displayed as expected");
}
