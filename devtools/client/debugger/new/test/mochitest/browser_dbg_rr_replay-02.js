/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test ending a recording at a breakpoint and then separately replaying to the end.
async function test() {
  waitForExplicitFinish();

  let recordingFile = newRecordingFile();
  let recordingTab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = recordingTab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_continuous.html", "current");

  let toolbox = await attachDebugger(recordingTab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_continuous.html", 14);
  await resumeToLine(client, 14);
  await resumeToLine(client, 14);
  await reverseStepOverToLine(client, 13);
  let lastNumberValue = await evaluateInTopFrame(client, "number");

  let tabParent = recordingTab.linkedBrowser.frameLoader.tabParent;
  ok(tabParent, "Found recording tab parent");
  ok(tabParent.saveRecording(recordingFile), "Saved recording");
  await once(Services.ppmm, "SaveRecordingFinished");

  await toolbox.destroy();
  await gBrowser.removeTab(recordingTab);

  let replayingTab = BrowserTestUtils.addTab(gBrowser, null, { replayExecution: recordingFile });
  gBrowser.selectedTab = replayingTab;
  await once(Services.ppmm, "HitRecordingEndpoint");

  toolbox = await attachDebugger(replayingTab);
  client = toolbox.threadClient;
  await client.interrupt();
  await checkEvaluateInTopFrame(client, "number", lastNumberValue);
  await reverseStepOverToLine(client, 13);
  await setBreakpoint(client, "doc_rr_continuous.html", 14);
  await rewindToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", lastNumberValue - 1);
  await resumeToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", lastNumberValue);

  await toolbox.destroy();
  await gBrowser.removeTab(replayingTab);
  finish();
}
