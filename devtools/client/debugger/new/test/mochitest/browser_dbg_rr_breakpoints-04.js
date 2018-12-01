/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test navigating back to earlier breakpoints while recording, then resuming
// recording.
async function test() {
  waitForExplicitFinish();

  let tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_continuous.html", "current");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_continuous.html", 14);
  await resumeToLine(client, 14);
  let value = await evaluateInTopFrame(client, "number");
  await resumeToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", value + 1);
  await rewindToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", value);
  await resumeToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", value + 1);
  await resumeToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", value + 2);
  await resumeToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", value + 3);
  await rewindToLine(client, 14);
  await checkEvaluateInTopFrame(client, "number", value + 2);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
