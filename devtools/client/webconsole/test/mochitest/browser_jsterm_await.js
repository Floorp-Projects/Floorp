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

  info("Evaluate a top-level await expression");
  let onMessage = waitForMessage(hud, "await1");
  // We use "await" + 1 to not match the evaluated command message.
  const simpleAwait = `await new Promise(r => setTimeout(() => r("await" + 1), 1000))`;
  jsterm.execute(simpleAwait);
  await onMessage;
  // Check that the resulting promise of the async iife is not displayed.
  let messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  let messagesText = Array.from(messages).map(n => n.textContent).join(" - ");
  is(messagesText, `${simpleAwait} - await1`,
    "The output contains the the expected messages");

  info("Check that assigning the result of a top-level await expression works");
  onMessage = waitForMessage(hud, "await2");
  jsterm.execute(`x = await new Promise(r => setTimeout(() => r("await" + 2), 1000))`);
  await onMessage;

  onMessage = waitForMessage(hud, `"-await2-"`);
  jsterm.execute(`"-" + x + "-"`);
  let message = await onMessage;
  ok(message.node, "`x` was assigned as expected");

  info("Check that awaiting for a rejecting promise displays an error");
  onMessage = waitForMessage(hud, "await-rej", ".message.error");
  jsterm.execute(`x = await new Promise((resolve,reject) =>
    setTimeout(() => reject("await-" + "rej"), 1000))`);
  message = await onMessage;
  ok(message.node, "awaiting for a rejecting promise displays an error message");

  info("Check that concurrent await expression work fine");
  hud.ui.clearOutput();
  const delays = [2000, 500, 1000, 1500];
  const inputs = delays.map(delay => `await new Promise(
    r => setTimeout(() => r("await-concurrent-" + ${delay}), ${delay}))`);

  // Let's wait for the message that sould be displayed last.
  onMessage = waitForMessage(hud, "await-concurrent-2000");
  for (const input of inputs) {
    jsterm.execute(input);
  }
  await onMessage;

  messages = hud.ui.outputNode.querySelectorAll(".message .message-body");
  messagesText = Array.from(messages).map(n => n.textContent);
  const expectedMessages = [
    ...inputs,
    "await-concurrent-500",
    "await-concurrent-1000",
    "await-concurrent-1500",
    "await-concurrent-2000",
  ];
  is(JSON.stringify(messagesText, null, 2), JSON.stringify(expectedMessages, null, 2),
    "The output contains the the expected messages, in the expected order");

  info("Check that a logged promise is still displayed as a promise");
  onMessage = waitForMessage(hud, "Promise {");
  jsterm.execute(`new Promise(r => setTimeout(() => r(1), 1000))`);
  message = await onMessage;
  ok(message, "Promise are displayed as expected");
}
