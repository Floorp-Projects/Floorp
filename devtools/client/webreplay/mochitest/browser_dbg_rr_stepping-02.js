/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test fixes for some simple stepping bugs.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });
  const { threadFront } = dbg;

  await threadFront.interrupt();
  const bp = await setBreakpoint(threadFront, "doc_rr_basic.html", 22);
  await rewindToLine(threadFront, 22);
  await stepInToLine(threadFront, 25);
  await stepOverToLine(threadFront, 26);
  await stepOverToLine(threadFront, 27);
  await reverseStepOverToLine(threadFront, 26);
  await stepInToLine(threadFront, 30);
  await stepOverToLine(threadFront, 31);
  await stepOverToLine(threadFront, 32);

  // Check that the scopes pane shows the value of the local variable.
  await waitForPaused(dbg);
  for (let i = 1; ; i++) {
    if (getScopeLabel(dbg, i) == "c") {
      is("NaN", getScopeValue(dbg, i));
      break;
    }
  }

  await stepOverToLine(threadFront, 33);
  await reverseStepOverToLine(threadFront, 32);
  await stepOutToLine(threadFront, 27);
  await reverseStepOverToLine(threadFront, 26);
  await reverseStepOverToLine(threadFront, 25);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
