/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Basic test for saving a recording and then replaying it in a new tab.
add_task(async function() {
  const recordingFile = newRecordingFile();
  const recordingTab = BrowserTestUtils.addTab(gBrowser, null, {
    recordExecution: "*",
  });
  gBrowser.selectedTab = recordingTab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  const remoteTab = recordingTab.linkedBrowser.frameLoader.remoteTab;
  ok(remoteTab, "Found recording remote tab");
  ok(remoteTab.saveRecording(recordingFile), "Saved recording");
  await once(Services.ppmm, "SaveRecordingFinished");

  const replayingTab = BrowserTestUtils.addTab(gBrowser, null, {
    replayExecution: recordingFile,
  });
  gBrowser.selectedTab = replayingTab;
  await once(Services.ppmm, "HitRecordingEndpoint");

  const dbg = await attachDebugger(replayingTab);

  await addBreakpoint(dbg, "doc_rr_basic.html", 21);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);
  await rewindToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 9);
  await resumeToLine(dbg, 21);
  await checkEvaluateInTopFrame(dbg, "number", 10);

  await gBrowser.removeTab(recordingTab);
  await shutdownDebugger(dbg);
});
