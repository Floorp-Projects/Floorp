/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic recording of a tab without any debugging.
add_task(async function() {
  const recordingTab = BrowserTestUtils.addTab(gBrowser, null, {
    recordExecution: "*",
  });
  gBrowser.selectedTab = recordingTab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  await gBrowser.removeTab(recordingTab);

  ok(true, "Finished");
});
