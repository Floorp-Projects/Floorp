/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test fixes for some simple stepping bugs.
async function test() {
  waitForExplicitFinish();

  let tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_basic.html", 22);
  await rewindToLine(client, 22);
  await stepInToLine(client, 25);
  await stepOverToLine(client, 26);
  await stepOverToLine(client, 27);
  await reverseStepInToLine(client, 33);
  await reverseStepOverToLine(client, 32);
  await reverseStepOutToLine(client, 26);
  await reverseStepOverToLine(client, 25);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
