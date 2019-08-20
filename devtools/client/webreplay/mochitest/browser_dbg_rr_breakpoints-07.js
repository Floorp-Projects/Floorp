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
  const { threadFront } = dbg;

  await waitForPaused(dbg);
  const { frames } = await threadFront.getFrames(0, 1);
  ok(frames[0].where.line == 6, "Paused at first debugger statement");

  const bp = await setBreakpoint(
    threadFront,
    "doc_debugger_statements.html",
    7
  );
  await resumeToLine(threadFront, 7);
  await resumeToLine(threadFront, 8);
  await threadFront.removeBreakpoint(bp);
  await rewindToLine(threadFront, 6);
  await resumeToLine(threadFront, 8);

  await shutdownDebugger(dbg);
});
