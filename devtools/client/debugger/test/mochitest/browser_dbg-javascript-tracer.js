/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests the Javascript Tracing feature.

"use strict";

add_task(async function() {
  // This is preffed off for now, so ensure turning it on
  await pushPref("devtools.debugger.features.javascript-tracing", true);

  const dbg = await initDebugger("doc-scripts.html");

  info("Enable the tracing");
  await clickElement(dbg, "trace");

  const topLevelThread =
    dbg.toolbox.commands.targetCommand.targetFront.threadFront.actorID;
  info("Wait for tracing to be enabled");
  await waitForState(dbg, state => {
    return dbg.selectors.getIsThreadCurrentlyTracing(topLevelThread);
  });

  ok(
    dbg.toolbox.splitConsole,
    "Split console is automatically opened when tracing to the console"
  );

  invokeInTab("main");

  info("Wait for console messages for the whole trace");
  // `main` calls `foo` which calls `bar`
  await hasConsoleMessage(dbg, "λ main");
  await hasConsoleMessage(dbg, "λ foo");
  await hasConsoleMessage(dbg, "λ bar");

  const traceMessages = await findConsoleMessages(dbg.toolbox, "λ main");
  is(traceMessages.length, 1, "We got a unique trace for 'main' function call");
  const sourceLink = traceMessages[0].querySelector(".frame-link-source");
  sourceLink.click();
  info("Wait for the main function to be highlighted in the debugger");
  await waitForSelectedSource(dbg, "simple1.js");
  await waitForSelectedLocation(dbg, 1, 16);

  info("Disable the tracing");
  await clickElement(dbg, "trace");
  info("Wait for tracing to be disabled");
  await waitForState(dbg, state => {
    return !dbg.selectors.getIsThreadCurrentlyTracing(topLevelThread);
  });

  invokeInTab("inline_script2");

  // Let some time for the tracer to appear if we failed disabling the tracing
  await wait(1000);

  const messages = await findConsoleMessages(dbg.toolbox, "inline_script2");
  is(
    messages.length,
    0,
    "We stopped recording traces, an the function call isn't logged in the console"
  );
});

add_task(async function testPersitentLogMethod() {
  let dbg = await initDebugger("doc-scripts.html");
  is(
    dbg.selectors.getJavascriptTracingLogMethod(),
    "console",
    "By default traces are logged to the console"
  );

  info("Change the log method to stdout");
  dbg.actions.setJavascriptTracingLogMethod("stdout");

  await dbg.toolbox.closeToolbox();

  dbg = await initDebugger("doc-scripts.html");
  is(
    dbg.selectors.getJavascriptTracingLogMethod(),
    "stdout",
    "The new setting has been persisted"
  );

  info("Reset back to the default value");
  dbg.actions.setJavascriptTracingLogMethod("console");
});
