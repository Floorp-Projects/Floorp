/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test unhandled divergence while evaluating at a breakpoint with Web Replay.
async function test() {
  waitForExplicitFinish();

  let tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_basic.html", 21);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 10);
  await checkEvaluateInTopFrameThrows(client, "window.alert(3)");
  await checkEvaluateInTopFrame(client, "number", 10);
  await checkEvaluateInTopFrameThrows(client, "window.alert(3)");
  await checkEvaluateInTopFrame(client, "number", 10);
  await checkEvaluateInTopFrame(client, "testStepping2()", undefined);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
