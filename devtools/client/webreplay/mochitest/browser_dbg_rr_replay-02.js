/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test ending a recording at a breakpoint and then separately replaying to the end.
add_task(async function() {
  waitForExplicitFinish();

  const recordingFile = newRecordingFile();
  const recordingTab = BrowserTestUtils.addTab(gBrowser, null, {
    recordExecution: "*",
  });
  gBrowser.selectedTab = recordingTab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_continuous.html", "current");

  const firstTab = await attachDebugger(recordingTab);
  let toolbox = firstTab.toolbox;
  let target = firstTab.target;
  let threadFront = toolbox.threadFront;
  await threadFront.interrupt();
  let bp = await setBreakpoint(threadFront, "doc_rr_continuous.html", 14);
  await resumeToLine(threadFront, 14);
  await resumeToLine(threadFront, 14);
  await reverseStepOverToLine(threadFront, 13);
  const lastNumberValue = await evaluateInTopFrame(target, "number");

  const remoteTab = recordingTab.linkedBrowser.frameLoader.remoteTab;
  ok(remoteTab, "Found recording remote tab");
  ok(remoteTab.saveRecording(recordingFile), "Saved recording");
  await once(Services.ppmm, "SaveRecordingFinished");

  await threadFront.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(recordingTab);

  const replayingTab = BrowserTestUtils.addTab(gBrowser, null, {
    replayExecution: recordingFile,
  });
  gBrowser.selectedTab = replayingTab;
  await once(Services.ppmm, "HitRecordingEndpoint");

  const dbg = await attachDebugger(replayingTab);
  toolbox = dbg.toolbox;
  target = dbg.target;
  threadFront = toolbox.threadFront;
  await threadFront.interrupt();

  // The recording does not actually end at the point where we saved it, but
  // will do at the next checkpoint. Rewind to the point we are interested in.
  bp = await setBreakpoint(threadFront, "doc_rr_continuous.html", 14);
  await rewindToLine(threadFront, 14);

  await checkEvaluateInTopFrame(target, "number", lastNumberValue);
  await reverseStepOverToLine(threadFront, 13);
  await rewindToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", lastNumberValue - 1);
  await resumeToLine(threadFront, 14);
  await checkEvaluateInTopFrame(target, "number", lastNumberValue);

  await threadFront.removeBreakpoint(bp);
  await shutdownDebugger(dbg);
});
