/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic breakpoint functionality in web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_basic.html", 21);

  // Visit a lot of breakpoints so that we are sure we have crossed major
  // checkpoint boundaries.
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 9);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 8);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 7);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 6);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 7);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 8);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 9);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);

  await shutdownDebugger(dbg);
});
