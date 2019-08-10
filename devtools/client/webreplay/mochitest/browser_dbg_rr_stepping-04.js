/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Stepping past the beginning or end of a frame should act like a step-out.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });
  const { threadFront, target } = dbg;

  await threadFront.interrupt();
  const bp = await setBreakpoint(threadFront, "doc_rr_basic.html", 21);
  await rewindToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 10);
  await waitForSelectedLocation(dbg, 21);
  await reverseStepOverToLine(threadFront, 20);
  await reverseStepOverToLine(threadFront, 12);

  // After reverse-stepping out of the topmost frame we should rewind to the
  // last breakpoint hit.
  await reverseStepOverToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 9);

  await stepOverToLine(threadFront, 22);
  await stepOverToLine(threadFront, 23);
  await stepOverToLine(threadFront, 13);
  await stepOverToLine(threadFront, 17);
  await stepOverToLine(threadFront, 18);

  // After forward-stepping out of the topmost frame we should run forward to
  // the next breakpoint hit.
  await stepOverToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 10);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
