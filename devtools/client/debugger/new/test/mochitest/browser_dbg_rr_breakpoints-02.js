/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test unhandled divergence while evaluating at a breakpoint with Web Replay.
async function test() {
  waitForExplicitFinish();

  const dbg = await attachRecordingDebugger("doc_rr_basic.html", { waitForRecording: true });
  const {threadClient, tab, toolbox} = dbg;

  await setBreakpoint(threadClient, "doc_rr_basic.html", 21);
  await rewindToLine(threadClient, 21);
  await checkEvaluateInTopFrame(threadClient, "number", 10);
  await checkEvaluateInTopFrameThrows(threadClient, "window.alert(3)");
  await checkEvaluateInTopFrame(threadClient, "number", 10);
  await checkEvaluateInTopFrameThrows(threadClient, "window.alert(3)");
  await checkEvaluateInTopFrame(threadClient, "number", 10);
  await checkEvaluateInTopFrame(threadClient, "testStepping2()", undefined);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
