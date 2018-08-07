/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test basic recording of a tab without any debugging.
async function test() {
  waitForExplicitFinish();

  var recordingTab = gBrowser.addTab(null, { recordExecution: "*" });
  gBrowser.selectedTab = recordingTab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  await gBrowser.removeTab(recordingTab);

  ok(true, "Finished");
  finish();
}
