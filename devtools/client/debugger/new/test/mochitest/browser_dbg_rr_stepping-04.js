/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Stepping past the beginning or end of a frame should act like a step-out.
async function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab(null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_basic.html", 21);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 10);
  await reverseStepOverToLine(client, 20);
  await reverseStepOverToLine(client, 12);

  // After reverse-stepping out of the topmost frame we should rewind to the
  // last breakpoint hit.
  await reverseStepOverToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 9);

  await stepOverToLine(client, 22);
  await stepOverToLine(client, 23);
  // Line 13 seems like it should be the next stepping point, but the column
  // numbers reported by the JS engine and required by the pause points do not
  // match, and we don't stop here.
  //await stepOverToLine(client, 13);
  await stepOverToLine(client, 17);
  await stepOverToLine(client, 18);

  // After forward-stepping out of the topmost frame we should run forward to
  // the next breakpoint hit.
  await stepOverToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 10);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
