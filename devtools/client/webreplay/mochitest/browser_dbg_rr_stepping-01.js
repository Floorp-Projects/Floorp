/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic step-over/back functionality in web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_basic.html", 21);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);
  await reverseStepOverToLine(dbg, 20);
  await checkEvaluateInTopFrame(dbg, "number", 9);
  await checkEvaluateInTopFrameThrows(dbg, "window.alert(3)");
  await stepOverToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);

  await shutdownDebugger(dbg);
});
