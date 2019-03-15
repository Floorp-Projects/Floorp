/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test stepping back while recording, then resuming recording.
add_task(async function() {
  const tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_continuous.html", "current");

  const { toolbox, target } = await attachDebugger(tab);
  const client = toolbox.threadClient;
  await client.interrupt();
  const bp = await setBreakpoint(client, "doc_rr_continuous.html", 13);
  await resumeToLine(client, 13);
  const value = await evaluateInTopFrame(target, "number");
  await reverseStepOverToLine(client, 12);
  await checkEvaluateInTopFrame(target, "number", value - 1);
  await resumeToLine(client, 13);
  await resumeToLine(client, 13);
  await checkEvaluateInTopFrame(target, "number", value + 1);

  await client.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
