/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test for saving a recording and then replaying it in a new tab,
// with rewinding disabled.
add_task(async function() {
  await pushPref("devtools.recordreplay.enableRewinding", false);

  const recordingFile = newRecordingFile();
  const recordingTab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = recordingTab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  const remoteTab = recordingTab.linkedBrowser.frameLoader.remoteTab;
  ok(remoteTab, "Found recording remote tab");
  ok(remoteTab.saveRecording(recordingFile), "Saved recording");
  await once(Services.ppmm, "SaveRecordingFinished");

  const replayingTab = BrowserTestUtils.addTab(gBrowser, null,
                                               { replayExecution: recordingFile });
  gBrowser.selectedTab = replayingTab;
  await once(Services.ppmm, "HitRecordingEndpoint");

  ok(true, "Replayed to end of recording");

  await gBrowser.removeTab(recordingTab);
  await gBrowser.removeTab(replayingTab);
});
