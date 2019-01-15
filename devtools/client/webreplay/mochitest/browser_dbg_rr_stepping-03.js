/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// To disable all Web Replay tests, see browser.ini

// Test stepping back while recording, then resuming recording.
add_task(async function() {
  const tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_continuous.html", "current");

  const toolbox = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  await setBreakpoint(client, "doc_rr_continuous.html", 13);
  await resumeToLine(client, 13);
  const value = await evaluateInTopFrame(client, "number");
  await reverseStepOverToLine(client, 12);
  await checkEvaluateInTopFrame(client, "number", value - 1);
  await resumeToLine(client, 13);
  await resumeToLine(client, 13);
  await checkEvaluateInTopFrame(client, "number", value + 1);

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
