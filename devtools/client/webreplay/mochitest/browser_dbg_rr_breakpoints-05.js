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

  const { threadFront, target } = dbg;

  await threadFront.interrupt();

  // Rewind to the beginning of the recording.
  await rewindToLine(threadFront, undefined);

  const bp = await setBreakpoint(threadFront, "doc_rr_basic.html", 21);
  await resumeToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 1);
  await resumeToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 2);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
