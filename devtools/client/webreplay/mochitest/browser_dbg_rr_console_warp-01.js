/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic console time warping functionality in web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_error.html", {
    waitForRecording: true,
  });

  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  await warpToMessage(hud, dbg, "Number 5");

  await checkEvaluateInTopFrame(dbg, "number", 5);

  // Initially we are paused inside the 'new Error()' call on line 19. The
  // first reverse step takes us to the start of that line.
  await reverseStepOverToLine(dbg, 19);
  await reverseStepOverToLine(dbg, 18);
  await addBreakpoint(dbg, "doc_rr_error.html", 12);
  await rewindToLine(dbg, 12);
  await checkEvaluateInTopFrame(dbg, "number", 4);
  await resumeToLine(dbg, 12);
  await checkEvaluateInTopFrame(dbg, "number", 5);

  await shutdownDebugger(dbg);
});
