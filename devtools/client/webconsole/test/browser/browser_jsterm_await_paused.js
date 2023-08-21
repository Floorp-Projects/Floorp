/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that top-level await expression work as expected when debugger is paused.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test top-level await when debugger paused`;

add_task(async function () {
  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);

  // Force the split console to be closed.
  await pushPref("devtools.toolbox.splitconsoleEnabled", false);
  const hud = await openNewTabAndConsole(TEST_URI);

  const pauseExpression = `(() => {
    var inPausedExpression = ["bar"];
    /* Will pause the script and open the debugger panel */
    debugger;
    return "pauseExpression-res";
  })()`;
  execute(hud, pauseExpression);

  // wait for the debugger to be opened and paused.
  const toolbox = gDevTools.getToolboxForTab(gBrowser.selectedTab);

  await waitFor(() => toolbox.getPanel("jsdebugger"));
  const dbg = createDebuggerContext(toolbox);
  await waitForPaused(dbg);

  await toolbox.openSplitConsole();

  const awaitExpression = `await new Promise(res => {
    const result = ["res", ...inPausedExpression];
    setTimeout(() => res(result), 2000);
    console.log("awaitExpression executed");
  })`;

  const onAwaitResultMessage = waitForMessageByType(
    hud,
    `[ "res", "bar" ]`,
    ".result"
  );
  const onAwaitExpressionExecuted = waitForMessageByType(
    hud,
    "awaitExpression executed",
    ".console-api"
  );
  execute(hud, awaitExpression);

  // We send an evaluation just after the await one to ensure the await evaluation was
  // done. We can't await on the previous execution because it waits for the result to
  // be send, which won't happen until we resume the debugger.
  await executeAndWaitForResultMessage(hud, `"smoke"`, `"smoke"`);

  // Give the engine some time to evaluate the await expression before resuming.
  // Otherwise the awaitExpression may be evaluate while the thread is already resumed!
  await onAwaitExpressionExecuted;

  // Click on the resume button to not be paused anymore.
  await resume(dbg);

  info("Wait for the paused expression result to be displayed");
  await waitFor(() => findEvaluationResultMessage(hud, "pauseExpression-res"));

  await onAwaitResultMessage;
  const messages = hud.ui.outputNode.querySelectorAll(
    ".message.result .message-body"
  );
  const messagesText = Array.from(messages).map(n => n.textContent);
  const expectedMessages = [
    // Result of "smoke"
    `"smoke"`,
    // The result of pauseExpression (after smoke since pauseExpression iife was paused)
    `"pauseExpression-res"`,
    // Result of await
    `Array [ "res", "bar" ]`,
  ];
  Assert.deepEqual(
    messagesText,
    expectedMessages,
    "The output contains the the expected messages, in the expected order"
  );
});
