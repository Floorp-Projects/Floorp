/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test basic recovery of crashed child processes in web replay.
async function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab(null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_recovery.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_recovery.html", 21);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "SpecialPowers.Cu.recordReplayDirective(/* CrashSoon */ 1)", undefined);
  await stepOverToLine(client, 22);
  await stepOverToLine(client, 23);
  await checkEvaluateInTopFrame(client, "SpecialPowers.Cu.recordReplayDirective(/* CrashSoon */ 1); " +
                                        "SpecialPowers.Cu.recordReplayDirective(/* MaybeCrash */ 2)", undefined);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
