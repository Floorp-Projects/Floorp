/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Pausing at a debugger statement on startup confuses the debugger.
PromiseTestUtils.whitelistRejectionsGlobally(/Unknown source actor/);

// Test interaction of breakpoints with debugger statements.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_debugger_statements.html");
  await resume(dbg);

  invokeInTab("foo");

  await waitForPaused(dbg);
  const pauseLine = getVisibleSelectedFrameLine(dbg);
  ok(pauseLine == 7, "Paused at first debugger statement");

  await addBreakpoint(dbg, "doc_debugger_statements.html", 8);
  await resumeToLine(dbg, 8);
  await resumeToLine(dbg, 9);
  await dbg.actions.removeAllBreakpoints(getContext(dbg));
  await rewindToLine(dbg, 7);
  await resumeToLine(dbg, 9);

  await shutdownDebugger(dbg);
});
