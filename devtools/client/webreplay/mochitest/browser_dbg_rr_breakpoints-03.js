/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// To disable all Web Replay tests, see browser.ini

// Test some issues when stepping around after hitting a breakpoint while recording.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_continuous.html");
  const {threadClient, tab, toolbox} = dbg;

  await threadClient.interrupt();
  await setBreakpoint(threadClient, "doc_rr_continuous.html", 19);
  await resumeToLine(threadClient, 19);
  await reverseStepOverToLine(threadClient, 18);
  await checkEvaluateInTopFrame(threadClient,
    "SpecialPowers.Cu.recordReplayDirective(/* AlwaysTakeTemporarySnapshots */ 3)",
    undefined);
  await stepInToLine(threadClient, 22);
  await setBreakpoint(threadClient, "doc_rr_continuous.html", 24);
  await resumeToLine(threadClient, 24);
  await setBreakpoint(threadClient, "doc_rr_continuous.html", 22);
  await rewindToLine(threadClient, 22);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
