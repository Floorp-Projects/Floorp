/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test hitting breakpoints when using tricky control flow constructs:
// catch, finally, generators, and async/await.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_control_flow.html", {
    waitForRecording: true,
  });

  await rewindToBreakpoint(10);
  await resumeToBreakpoint(12);
  await resumeToBreakpoint(18);
  await resumeToBreakpoint(20);
  await resumeToBreakpoint(32);
  await resumeToBreakpoint(27);
  await resumeToLine(dbg, 32);
  await resumeToLine(dbg, 27);
  await resumeToBreakpoint(42);
  await resumeToBreakpoint(44);
  await resumeToBreakpoint(50);
  await resumeToBreakpoint(54);

  await shutdownDebugger(dbg);

  async function rewindToBreakpoint(line) {
    await addBreakpoint(dbg, "doc_control_flow.html", line);
    await rewindToLine(dbg, line);
  }

  async function resumeToBreakpoint(line) {
    await addBreakpoint(dbg, "doc_control_flow.html", line);
    await resumeToLine(dbg, line);
  }
});
