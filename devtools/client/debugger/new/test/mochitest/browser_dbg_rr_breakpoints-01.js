/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test basic breakpoint functionality in web replay.
async function test() {
  waitForExplicitFinish();

  let tab = gBrowser.addTab(null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  let toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_basic.html", 21);

  // Visit a lot of breakpoints so that we are sure we have crossed major
  // checkpoint boundaries.
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 10);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 9);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 8);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 7);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 6);
  await resumeToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 7);
  await resumeToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 8);
  await resumeToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 9);
  await resumeToLine(client, 21);
  await checkEvaluateInTopFrame(client, "number", 10);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
  finish();
}
