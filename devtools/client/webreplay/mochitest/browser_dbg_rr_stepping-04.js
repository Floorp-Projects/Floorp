/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Stepping past the beginning or end of a frame should act like a step-out.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  await addBreakpoint(dbg, "doc_rr_basic.html", 21);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);
  await waitForSelectedLocation(dbg, 21);
  await reverseStepOverToLine(dbg, 20);
  await reverseStepOverToLine(dbg, 12);

  // After reverse-stepping out of the topmost frame we should rewind to the
  // last breakpoint hit.
  await reverseStepOverToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 9);

  await stepOverToLine(dbg, 22);
  await stepOverToLine(dbg, 23);
  await stepOverToLine(dbg, 13);
  await stepOverToLine(dbg, 17);
  await stepOverToLine(dbg, 18);

  // After forward-stepping out of the topmost frame we should run forward to
  // the next breakpoint hit.
  await stepOverToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);

  await shutdownDebugger(dbg);
});
