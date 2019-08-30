/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test stepping back while recording, then resuming recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");

  await addBreakpoint(dbg, "doc_rr_continuous.html", 13);
  await resumeToLine(dbg, 13);
  const value = await evaluateInTopFrame(dbg, "number");
  await reverseStepOverToLine(dbg, 12);
  await checkEvaluateInTopFrame(dbg, "number", value - 1);
  await resumeToLine(dbg, 13);
  await resumeToLine(dbg, 13);
  await checkEvaluateInTopFrame(dbg, "number", value + 1);

  await shutdownDebugger(dbg);
});
