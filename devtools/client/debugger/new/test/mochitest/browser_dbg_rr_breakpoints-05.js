/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test hitting breakpoints when rewinding past the point where the breakpoint
// script was created.
async function test() {
  waitForExplicitFinish();

  let tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();

  // Rewind to the beginning of the recording.
  await rewindToLine(client, undefined);

  await setBreakpoint(client, "doc_rr_basic.html", 21);
  await resumeToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 1);
  await resumeToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 2);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
