/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic console time warping functionality in web replay.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_error.html", {
    waitForRecording: true,
  });

  const { tab, toolbox, threadClient, target } = dbg;
  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  await warpToMessage(hud, dbg, "Number 5");
  await threadClient.interrupt();

  await checkEvaluateInTopFrame(target, "number", 5);

  // Initially we are paused inside the 'new Error()' call on line 19. The
  // first reverse step takes us to the start of that line.
  await reverseStepOverToLine(threadClient, 19);
  await reverseStepOverToLine(threadClient, 18);
  const bp = await setBreakpoint(threadClient, "doc_rr_error.html", 12);
  await rewindToLine(threadClient, 12);
  await checkEvaluateInTopFrame(target, "number", 4);
  await resumeToLine(threadClient, 12);
  await checkEvaluateInTopFrame(target, "number", 5);

  await threadClient.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
