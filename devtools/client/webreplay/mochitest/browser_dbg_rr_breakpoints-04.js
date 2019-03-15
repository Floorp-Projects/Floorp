/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test navigating back to earlier breakpoints while recording, then resuming
// recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const {threadClient, tab, toolbox, target} = dbg;

  const bp = await setBreakpoint(threadClient, "doc_rr_continuous.html", 14);
  await resumeToLine(threadClient, 14);
  const value = await evaluateInTopFrame(target, "number");
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(target, "number", value + 1);
  await rewindToLine(threadClient, 14);
  await checkEvaluateInTopFrame(target, "number", value);
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(target, "number", value + 1);
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(target, "number", value + 2);
  await resumeToLine(threadClient, 14);
  await checkEvaluateInTopFrame(target, "number", value + 3);
  await rewindToLine(threadClient, 14);
  await checkEvaluateInTopFrame(target, "number", value + 2);

  await threadClient.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
