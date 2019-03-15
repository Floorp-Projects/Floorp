/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic step-over/back functionality in web replay.
add_task(async function() {
  const tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  const { target, toolbox } = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  const bp = await setBreakpoint(client, "doc_rr_basic.html", 21);
  await rewindToLine(client, 21);
  await checkEvaluateInTopFrame(target, "number", 10);
  await reverseStepOverToLine(client, 20);
  await checkEvaluateInTopFrame(target, "number", 9);
  await checkEvaluateInTopFrameThrows(target, "window.alert(3)");
  await stepOverToLine(client, 21);
  await checkEvaluateInTopFrame(target, "number", 10);

  await client.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
