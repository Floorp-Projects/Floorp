/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This is a lightweight version of browser_jsterm_await.js to only ensure top-level await
// support in the Browser Console.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,Top-level await Browser Console test";

add_task(async function() {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  await addTab(TEST_URI);
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

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
    messagesText.includes("Promise {"),
    false,
    "The output does not contain a Promise"
  );
  ok(
    messagesText.includes(simpleAwait) &&
      messagesText.includes(`Array [ "await1" ]`),
    "The output contains the the expected messages"
  );

  info("Close the Browser console");
  await BrowserConsoleManager.toggleBrowserConsole();
});
