/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test hitting breakpoints when using tricky control flow constructs:
// catch, finally, generators, and async/await.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_control_flow.html", {
    waitForRecording: true,
  });
  const { threadFront, tab, toolbox } = dbg;

  const breakpoints = [];

  await rewindToBreakpoint(10);
  await resumeToBreakpoint(12);
  await resumeToBreakpoint(18);
  await resumeToBreakpoint(20);
  await resumeToBreakpoint(32);
  await resumeToBreakpoint(27);
  await resumeToLine(threadFront, 32);
  await resumeToLine(threadFront, 27);
  await resumeToBreakpoint(42);
  await resumeToBreakpoint(44);
  await resumeToBreakpoint(50);
  await resumeToBreakpoint(54);

  for (const bp of breakpoints) {
    await threadFront.removeBreakpoint(bp);
  }
  await toolbox.closeToolbox();
  await gBrowser.removeTab(tab);

  async function rewindToBreakpoint(line) {
    const bp = await setBreakpoint(threadFront, "doc_control_flow.html", line);
    breakpoints.push(bp);
    await rewindToLine(threadFront, line);
  }

  async function resumeToBreakpoint(line) {
    const bp = await setBreakpoint(threadFront, "doc_control_flow.html", line);
    breakpoints.push(bp);
    await resumeToLine(threadFront, line);
  }
});
