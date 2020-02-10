/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test hitting breakpoints when rewinding past the point where the breakpoint
// script was created.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  // Rewind to the beginning of the recording.
  await rewindToLine(dbg, undefined);

  await addBreakpoint(dbg, "doc_rr_basic.html", 21);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 1);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 2);

  await shutdownDebugger(dbg);
});
