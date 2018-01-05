/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test to make sure that web console commands can fire while paused at a
// breakpoint that was triggered from a JS call.  Relies on asynchronous js
// evaluation over the protocol - see Bug 1088861.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "new-console-output/test/mochitest/test-eval-in-stackframe.html";

add_task(async function () {
  // Force the old debugger UI since it's directly used (see Bug 1301705).
  await pushPref("devtools.debugger.new-debugger-frontend", false);

  info("open the console");
  const hud = await openNewTabAndConsole(TEST_URI);
  const {jsterm} = hud;

  info("open the debugger");
  let {panel} = await openDebugger();
  let {activeThread} = panel.panelWin.DebuggerController;

  const onFirstCallFramesAdded = activeThread.addOneTimeListener("framesadded");
  // firstCall calls secondCall, which has a debugger statement, so we'll be paused.
  const onFirstCallMessageReceived = waitForMessage(hud, "undefined");

  const unresolvedSymbol = Symbol();
  let firstCallEvaluationResult = unresolvedSymbol;
  onFirstCallMessageReceived.then(message => {
    firstCallEvaluationResult = message;
  });
  jsterm.execute("firstCall()");

  info("Waiting for a frame to be added");
  await onFirstCallFramesAdded;

  info("frames added, select the console again");
  await openConsole();

  info("Executing basic command while paused");
  let onMessageReceived = waitForMessage(hud, "3");
  jsterm.execute("1 + 2");
  let message = await onMessageReceived;
  ok(message, "`1 + 2` was evaluated whith debugger paused");

  info("Executing command using scoped variables while paused");
  onMessageReceived = waitForMessage(hud, `"globalFooBug783499foo2SecondCall"`);
  jsterm.execute("foo + foo2");
  message = await onMessageReceived;
  ok(message, "`foo + foo2` was evaluated as expected with debugger paused");

  info("Checking the first command, which is the last to resolve since it paused");
  ok(firstCallEvaluationResult === unresolvedSymbol, "firstCall was not evaluated yet");

  info("Resuming the thread");
  activeThread.resume();

  message = await onFirstCallMessageReceived;
  ok(firstCallEvaluationResult !== unresolvedSymbol,
    "firstCall() returned correct value");
});
