/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing only on next page load

"use strict";

add_task(async function testTracingOnNextLoad() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  await pushPref(
    "devtools.debugger.features.javascript-tracing-log-method",
    "console"
  );
  // Cover tracing function argument values
  const jsCode = `function foo() {}; function bar() {}; foo(); dump("plop\\n")`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," +
      encodeURIComponent(`<script>${jsCode}</script><body></body>`)
  );

  const traceButton = findElement(dbg, "trace");

  await openContextMenuInDebugger(dbg, "trace");
  let toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_ON_NEXT_LOAD"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-next-load`);
  await toggled;
  ok(true, "Toggled the trace on next load");

  await clickElement(dbg, "trace");

  ok(
    !traceButton.classList.contains("pending"),
    "Before toggling the trace button, it has no particular state"
  );

  info(
    "Wait for the split console to be automatically displayed when toggling this setting"
  );
  await dbg.toolbox.getPanelWhenReady("webconsole");

  invokeInTab("bar");

  // Let some time for the call to be traced
  await wait(500);

  is(
    (await findConsoleMessages(dbg.toolbox, "λ bar")).length,
    0,
    "The code isn't logged as the tracer shouldn't be started yet"
  );

  // Wait for the trace button to be highlighted
  await waitFor(() => {
    return traceButton.classList.contains("pending");
  }, "The tracer button is pending before reloading the page");

  info("Add an iframe to trigger a target creation");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    const iframe = content.document.createElement("iframe");
    iframe.src = "data:text/html,iframe";
    content.document.body.appendChild(iframe);
  });

  // Let some time to regress and start the tracer
  await wait(500);

  ok(
    traceButton.classList.contains("pending"),
    "verify that the tracer is still pending after the iframe creation"
  );

  // Reload the page to trigger the tracer
  await reload(dbg);

  let topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled after page reload");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });
  ok(
    traceButton.classList.contains("active"),
    "The tracer button is now active after reload"
  );

  info("Wait for the split console to be displayed");
  await dbg.toolbox.getPanelWhenReady("webconsole");

  // Ensure that the very early code is traced
  await hasConsoleMessage(dbg, "λ foo");

  is(
    (await findConsoleMessages(dbg.toolbox, "λ bar")).length,
    0,
    "The code ran before the reload isn't logged"
  );

  await clickElement(dbg, "trace");

  topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be disabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });
  await waitFor(() => {
    return !traceButton.classList.contains("active");
  }, "The tracer button is no longer active after stop request");

  info("Toggle the setting back off");
  await openContextMenuInDebugger(dbg, "trace");
  toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_ON_NEXT_LOAD"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-next-load`);
  await toggled;
  await waitFor(() => {
    return !traceButton.classList.contains("pending");
  }, "The tracer button is no longer pending after toggling the setting");

  // Reset the trace on next interaction setting
  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-on-next-interaction"
  );
});
