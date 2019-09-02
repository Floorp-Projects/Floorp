/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test some issues when stepping around after hitting a breakpoint while recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");

  await addBreakpoint(dbg, "doc_rr_continuous.html", 19);
  await resumeToLine(dbg, 19);
  await reverseStepOverToLine(dbg, 18);
  await stepInToLine(dbg, 22);
  await addBreakpoint(dbg, "doc_rr_continuous.html", 24);
  await resumeToLine(dbg, 24);
  await addBreakpoint(dbg, "doc_rr_continuous.html", 22);
  await rewindToLine(dbg, 22);

  await shutdownDebugger(dbg);
});
