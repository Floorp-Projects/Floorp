/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test some issues when stepping around after hitting a breakpoint while recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const {threadClient, tab, toolbox} = dbg;

  await threadClient.interrupt();
  const bp1 = await setBreakpoint(threadClient, "doc_rr_continuous.html", 19);
  await resumeToLine(threadClient, 19);
  await reverseStepOverToLine(threadClient, 18);
  await stepInToLine(threadClient, 22);
  const bp2 = await setBreakpoint(threadClient, "doc_rr_continuous.html", 24);
  await resumeToLine(threadClient, 24);
  const bp3 = await setBreakpoint(threadClient, "doc_rr_continuous.html", 22);
  await rewindToLine(threadClient, 22);

  await threadClient.removeBreakpoint(bp1);
  await threadClient.removeBreakpoint(bp2);
  await threadClient.removeBreakpoint(bp3);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
