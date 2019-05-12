/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test fixes for some simple stepping bugs.
add_task(async function() {
  const tab = BrowserTestUtils.addTab(gBrowser, null, { recordExecution: "*" });
  gBrowser.selectedTab = tab;
  openTrustedLinkIn(EXAMPLE_URL + "doc_rr_basic.html", "current");
  await once(Services.ppmm, "RecordingFinished");

  const { toolbox } = await attachDebugger(tab), client = toolbox.threadClient;
  await client.interrupt();
  const bp = await setBreakpoint(client, "doc_rr_basic.html", 22);
  await rewindToLine(client, 22);
  await stepInToLine(client, 25);
  await stepOverToLine(client, 26);
  await stepOverToLine(client, 27);
  await reverseStepOverToLine(client, 26);
  await stepInToLine(client, 30);
  await stepOverToLine(client, 31);
  await stepOverToLine(client, 32);
  await stepOverToLine(client, 33);
  await reverseStepOverToLine(client, 32);
  await stepOutToLine(client, 27);
  await reverseStepOverToLine(client, 26);
  await reverseStepOverToLine(client, 25);

  await client.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
