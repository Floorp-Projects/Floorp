/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test some issues when stepping around after hitting a breakpoint while recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const { threadFront, tab, toolbox } = dbg;

  await threadFront.interrupt();
  const bp1 = await setBreakpoint(threadFront, "doc_rr_continuous.html", 19);
  await resumeToLine(threadFront, 19);
  await reverseStepOverToLine(threadFront, 18);
  await stepInToLine(threadFront, 22);
  const bp2 = await setBreakpoint(threadFront, "doc_rr_continuous.html", 24);
  await resumeToLine(threadFront, 24);
  const bp3 = await setBreakpoint(threadFront, "doc_rr_continuous.html", 22);
  await rewindToLine(threadFront, 22);

  await threadFront.removeBreakpoint(bp1);
  await threadFront.removeBreakpoint(bp2);
  await threadFront.removeBreakpoint(bp3);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
