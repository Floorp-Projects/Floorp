/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic step-over/back functionality in web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });
  const { threadFront, target } = dbg;

  await threadFront.interrupt();
  const bp = await setBreakpoint(threadFront, "doc_rr_basic.html", 21);
  await rewindToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 10);
  await reverseStepOverToLine(threadFront, 20);
  await checkEvaluateInTopFrame(target, "number", 9);
  await checkEvaluateInTopFrameThrows(target, "window.alert(3)");
  await stepOverToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 10);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
