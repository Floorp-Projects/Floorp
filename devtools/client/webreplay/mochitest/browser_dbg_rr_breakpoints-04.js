/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test navigating back to earlier breakpoints while recording, then resuming
// recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");

  await addBreakpoint(dbg, "doc_rr_continuous.html", 14);
  await resumeToLine(dbg, 14);
  const value = await evaluateInTopFrame(dbg, "number");
  await resumeToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", value + 1);
  await rewindToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", value);
  await resumeToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", value + 1);
  await resumeToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", value + 2);
  await resumeToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", value + 3);
  await rewindToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", value + 2);

  await shutdownDebugger(dbg);
});
