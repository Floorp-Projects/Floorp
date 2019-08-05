/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test stepping back while recording, then resuming recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const { threadFront, target } = dbg;

  await threadFront.interrupt();
  const bp = await setBreakpoint(threadFront, "doc_rr_continuous.html", 13);
  await resumeToLine(threadFront, 13);
  const value = await evaluateInTopFrame(target, "number");
  await reverseStepOverToLine(threadFront, 12);
  await checkEvaluateInTopFrame(target, "number", value - 1);
  await resumeToLine(threadFront, 13);
  await resumeToLine(threadFront, 13);
  await checkEvaluateInTopFrame(target, "number", value + 1);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
