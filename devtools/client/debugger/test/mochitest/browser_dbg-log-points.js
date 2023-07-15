/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/*
 * Tests that log points are correctly logged to the console
 */

"use strict";

add_task(async function () {
  Services.prefs.setBoolPref("devtools.toolbox.splitconsoleEnabled", true);
  const dbg = await initDebugger(
    "doc-script-switching.html",
    "script-switching-01.js"
  );

  const source = findSource(dbg, "script-switching-01.js");
  await selectSource(dbg, "script-switching-01.js");

  await getDebuggerSplitConsole(dbg);

  info(
    `Add a first log breakpoint with no argument, which will log "display name", i.e. firstCall`
  );
  await altClickElement(dbg, "gutter", 7);
  await waitForBreakpoint(dbg, "script-switching-01.js", 7);

  info("Add another log breakpoint with static arguments");
  await dbg.actions.addBreakpoint(createLocation({ line: 8, source }), {
    logValue: "'a', 'b', 'c'",
  });

  invokeInTab("firstCall");
  await waitForPaused(dbg);

  info("Wait for the two log breakpoints");
  await hasConsoleMessage(dbg, "firstCall");
  await hasConsoleMessage(dbg, "a b c");

  const { link, value } = await findConsoleMessage(dbg, "a b c");
  is(link, "script-switching-01.js:8:2", "logs should have the relevant link");
  is(value, "a b c", "logs should have multiple values");
  await removeBreakpoint(dbg, source.id, 7);
  await removeBreakpoint(dbg, source.id, 8);

  await resume(dbg);

  info(
    "Now set a log point calling a method with a debugger statement and a breakpoint, it shouldn't pause on the log point"
  );
  await addBreakpoint(dbg, "script-switching-01.js", 8);
  await addBreakpoint(dbg, "script-switching-01.js", 7);
  await dbg.actions.addBreakpoint(createLocation({ line: 7, source }), {
    logValue: "'second call', secondCall()",
  });

  invokeInTab("firstCall");
  await waitForPaused(dbg);
  // We aren't pausing on secondCall's debugger statement,
  // called by the condition, but only on the breakpoint we set on firstCall, line 8
  assertPausedAtSourceAndLine(dbg, source.id, 8);

  // The log point is visible, even if it had a debugger statement in it.
  await hasConsoleMessage(dbg, "second call 44");
  await removeBreakpoint(dbg, source.id, 7);
  await removeBreakpoint(dbg, source.id, 8);

  await resume(dbg);
  // Resume a second time as we are hittin the debugger statement as firstCall calls secondCall
  await resume(dbg);

  info(
    "Set a log point throwing an exception and ensure the exception is displayed"
  );
  await dbg.actions.addBreakpoint(createLocation({ line: 7, source }), {
    logValue: "jsWithError(",
  });
  invokeInTab("firstCall");
  await waitForPaused(dbg);
  // Exceptions in conditional breakpoint would not trigger a pause,
  // So we end up pausing on the debugger statement in the other script.
  assertPausedAtSourceAndLine(
    dbg,
    findSource(dbg, "script-switching-02.js").id,
    6
  );

  // But verify that the exception message is visible even if we didn't pause.
  // As the logValue is evaled within an array `[${logValue}]`,
  // the exception message is a bit cryptic...
  await hasConsoleMessage(dbg, "expected expression, got ']'");
});
