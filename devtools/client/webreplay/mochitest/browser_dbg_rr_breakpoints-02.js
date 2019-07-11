/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test unhandled divergence while evaluating at a breakpoint with Web Replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });
  const { threadFront, tab, toolbox, target } = dbg;

  const bp = await setBreakpoint(threadFront, "doc_rr_basic.html", 21);
  await rewindToLine(threadFront, 21);
  await checkEvaluateInTopFrame(target, "number", 10);
  await checkEvaluateInTopFrameThrows(target, "window.alert(3)");
  await checkEvaluateInTopFrame(target, "number", 10);
  await checkEvaluateInTopFrameThrows(target, "window.alert(3)");
  await checkEvaluateInTopFrame(target, "number", 10);
  await checkEvaluateInTopFrame(target, "testStepping2()", undefined);

  await threadFront.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
