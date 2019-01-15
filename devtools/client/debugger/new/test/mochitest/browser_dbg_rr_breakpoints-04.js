/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test navigating back to earlier breakpoints while recording, then resuming
// recording.
async function test() {
  waitForExplicitFinish();

  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const {threadClient, tab, toolbox} = dbg;

  await setBreakpoint(threadClient, "doc_rr_continuous.html", 14);
  await resumeToLine(threadClient, 14);
  let value = await evaluateInTopFrame(threadClient, "number");
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(threadClient, "number", value + 1);
  await rewindToLine(threadClient, 14);
  await checkEvaluateInTopFrame(threadClient, "number", value);
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(threadClient, "number", value + 1);
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(threadClient, "number", value + 2);
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(threadClient, "number", value + 3);
  await rewindToLine(threadClient, 14);
  await checkEvaluateInTopFrame(threadClient, "number", value + 2);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
