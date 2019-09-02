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

  let dbg = await attachDebugger(recordingTab);

  await addBreakpoint(dbg, "doc_rr_continuous.html", 14);
  await resumeToLine(dbg, 14);
  await resumeToLine(dbg, 14);
  await reverseStepOverToLine(dbg, 13);
  const lastNumberValue = await evaluateInTopFrame(dbg, "number");

  const remoteTab = recordingTab.linkedBrowser.frameLoader.remoteTab;
  ok(remoteTab, "Found recording remote tab");
  ok(remoteTab.saveRecording(recordingFile), "Saved recording");
  await once(Services.ppmm, "SaveRecordingFinished");

  await shutdownDebugger(dbg);

  const replayingTab = BrowserTestUtils.addTab(gBrowser, null, {
    replayExecution: recordingFile,
  });
  gBrowser.selectedTab = replayingTab;
  await once(Services.ppmm, "HitRecordingEndpoint");

  dbg = await attachDebugger(replayingTab);

  // The recording does not actually end at the point where we saved it, but
  // will do at the next checkpoint. Rewind to the point we are interested in.
  await addBreakpoint(dbg, "doc_rr_continuous.html", 14);
  await rewindToLine(dbg, 14);

  await checkEvaluateInTopFrame(dbg, "number", lastNumberValue);
  await reverseStepOverToLine(dbg, 13);
  await rewindToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", lastNumberValue - 1);
  await resumeToLine(dbg, 14);
  await checkEvaluateInTopFrame(dbg, "number", lastNumberValue);

  await shutdownDebugger(dbg);
});
