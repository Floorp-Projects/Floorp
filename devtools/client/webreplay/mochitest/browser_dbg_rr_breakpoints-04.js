/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test navigating back to earlier breakpoints while recording, then resuming
// recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const { threadFront, target } = dbg;

  const bp = await setBreakpoint(threadFront, "doc_rr_continuous.html", 14);
  await resumeToLine(threadFront, 14);
  const value = await evaluateInTopFrame(target, "number");
  await resumeToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", value + 1);
  await rewindToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", value);
  await resumeToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", value + 1);
  await resumeToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", value + 2);
  await resumeToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", value + 3);
  await rewindToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", value + 2);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
