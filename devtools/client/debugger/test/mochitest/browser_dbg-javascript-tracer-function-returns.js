/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests tracing all function returns

"use strict";

add_task(async function testTracingFunctionReturn() {
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const jsCode = `async function foo() { nullReturn(); falseReturn(); await new Promise(r => setTimeout(r, 0)); return bar(); }; function nullReturn() { return null; } function falseReturn() { return false; } function bar() { return 42; }; function throwingFunction() { throw new Error("the exception") }`;
  const dbg = await initDebuggerWithAbsoluteURL(
    "data:text/html," +
      encodeURIComponent(`<script>${jsCode}</script><body></body>`)
  );

  await openContextMenuInDebugger(dbg, "trace");
  let toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_FUNCTION_RETURN"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-function-return`);
  await toggled;
  ok(true, "Toggled the trace of function returns");

  await clickElement(dbg, "trace");

  const topLevelThreadActorID =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");
  await hasConsoleMessage(dbg, "⟶ interpreter λ foo");
  await hasConsoleMessage(dbg, "⟶ interpreter λ bar");
  await hasConsoleMessage(dbg, "⟵ λ bar");
  await hasConsoleMessage(dbg, "⟵ λ foo");

  await clickElement(dbg, "trace");

  info("Wait for tracing to be disabled");
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  await openContextMenuInDebugger(dbg, "trace");
  toggled = waitForDispatch(dbg.store, "TOGGLE_JAVASCRIPT_TRACING_VALUES");
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-log-values`);
  await toggled;
  ok(true, "Toggled the log values setting");

  await clickElement(dbg, "trace");

  info("Wait for tracing to be re-enabled with logging of returned values");
  await waitForState(dbg, () => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  invokeInTab("foo");

  await hasConsoleMessage(dbg, "⟶ interpreter λ foo");
  await hasConsoleMessage(dbg, "⟶ interpreter λ bar");
  await hasConsoleMessage(dbg, "⟵ λ bar return 42");
  await hasConsoleMessage(dbg, "⟶ interpreter λ nullReturn");
  await hasConsoleMessage(dbg, "⟵ λ nullReturn return null");
  await hasConsoleMessage(dbg, "⟶ interpreter λ falseReturn");
  await hasConsoleMessage(dbg, "⟵ λ falseReturn return false");
  await hasConsoleMessage(
    dbg,
    `⟵ λ foo return \nPromise { <state>: "fulfilled", <value>: 42 }`
  );

  invokeInTab("throwingFunction").catch(() => {});
  await hasConsoleMessage(
    dbg,
    `⟵ λ throwingFunction throw \nError: the exception`
  );

  info("Stop tracing");
  await clickElement(dbg, "trace");
  await waitForState(dbg, () => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThreadActorID);
  });

  info("Toggle the two settings to the default value");
  await openContextMenuInDebugger(dbg, "trace");
  toggled = waitForDispatch(dbg.store, "TOGGLE_JAVASCRIPT_TRACING_VALUES");
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-log-values`);
  await toggled;

  await openContextMenuInDebugger(dbg, "trace");
  toggled = waitForDispatch(
    dbg.store,
    "TOGGLE_JAVASCRIPT_TRACING_FUNCTION_RETURN"
  );
  selectContextMenuItem(dbg, `#debugger-trace-menu-item-function-return`);
  await toggled;

  // Reset the trace on next interaction setting
  Services.prefs.clearUserPref(
    "devtools.debugger.javascript-tracing-on-next-interaction"
  );
});
