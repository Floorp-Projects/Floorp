/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing only on next user interaction

"use strict";

add_task(async function testTracingOnNextInteraction() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  // Cover tracing only on next user interaction
  const jsCode = `function foo() {}; window.addEventListener("mousedown", function onmousedown(){}, { capture: true }); window.onclick = function onclick() {};`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," +
      encodeURIComponent(`<script>${jsCode}</script><body></body>`)
  );

  await openContextMenuInDebugger(dbg, "trace");
  const toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_ON_NEXT_INTERACTION"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-next-interaction`);
  await toggled;
  ok(true, "Toggled the trace on next interaction");

  await clickElement(dbg, "trace");

  const traceButton = findElement(dbg, "trace");
  // Wait for the trace button to be highlighted
  await waitFor(() => {
    return traceButton.classList.contains("pending");
  });
  ok(
    traceButton.classList.contains("pending"),
    "The tracer button is also highlighted as pending until the user interaction is triggered"
  );

  invokeInTab("foo");

  // Let a change to have the tracer to regress and log foo call
  await wait(500);

  is(
    (await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    0,
    "The tracer did not log the function call before trigerring the click event"
  );

  // We intentionally turn off this a11y check, because the following click
  // is send on an empty <body> to to test the click event tracer performance,
  // and not to activate any control, therefore this check can be ignored.
  AccessibilityUtils.setEnv({
    mustHaveAccessibleRule: false,
  });
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "body",
    {},
    gBrowser.selectedBrowser
  );
  AccessibilityUtils.resetEnv();

  let topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  await hasConsoleMessage(dbg, "λ onmousedown");
  await hasConsoleMessage(dbg, "λ onclick");

  ok(
    traceButton.classList.contains("active"),
    "The tracer button is still highlighted as active"
  );
  ok(
    !traceButton.classList.contains("pending"),
    "The tracer button is no longer pending after the user interaction"
  );

  is(
    (await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    0,
    "Even after the click, the code called before the click is still not logged"
  );

  // But if we call this code again, now it should be logged
  invokeInTab("foo");
  await hasConsoleMessage(dbg, "λ foo");
  ok(true, "foo was traced as expected");

  await clickElement(dbg, "trace");

  topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be disabled");
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  ok(
    !traceButton.classList.contains("active"),
    "The tracer button is no longer highlighted as active"
  );
  ok(
    !traceButton.classList.contains("pending"),
    "The tracer button is still not pending after disabling"
  );

  // Reset the trace on next interaction setting
  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-on-next-interaction"
  );
});

add_task(async function testInteractionBetweenDebuggerAndConsole() {
  const jsCode = `function foo() {};`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," + encodeURIComponent(`<script>${jsCode}</script>`)
  );

  info("Enable the tracing via the debugger button");
  await clickElement(dbg, "trace");

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "λ foo");

  info("Disable the tracing via a console command");
  const { hud } = await dbg.toolbox.getPanel("webconsole");
  let msg = await evaluateExpressionInConsole(hud, ":trace", "console-api");
  is(msg.textContent.trim(), "Stopped tracing");

  ok(
    !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID),
    "Tracing is also reported as disabled in the debugger"
  );

  info(
    "Clear the console output from the first tracing session started from the debugger"
  );
  hud.ui.clearOutput();
  await waitFor(
    async () => !(await findConsoleMessages(dbg.toolbox, "λ foo")).length,
    "Wait for console to be cleared"
  );

  info("Enable the tracing via a console command");
  msg = await evaluateExpressionInConsole(hud, ":trace", "console-api");
  is(msg.textContent.trim(), "Started tracing to Web Console");

  info("Wait for tracing to be also enabled in the debugger");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });
  ok(true, "Debugger also reports the tracing in progress");

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "λ foo");

  info("Disable the tracing via the debugger button");
  await clickElement(dbg, "trace");

  info("Wait for tracing to be disabled per debugger button");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  info("Also wait for stop message in the console");
  await hasConsoleMessage(dbg, "Stopped tracing");
});
