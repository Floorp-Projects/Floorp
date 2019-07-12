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

  const { toolbox } = await attachDebugger(tab);
  const front = toolbox.threadFront;
  await front.interrupt();
  const bp = await setBreakpoint(front, "doc_rr_basic.html", 22);
  await rewindToLine(front, 22);
  await stepInToLine(front, 25);
  await stepOverToLine(front, 26);
  await stepOverToLine(front, 27);
  await reverseStepOverToLine(front, 26);
  await stepInToLine(front, 30);
  await stepOverToLine(front, 31);
  await stepOverToLine(front, 32);
  await stepOverToLine(front, 33);
  await reverseStepOverToLine(front, 32);
  await stepOutToLine(front, 27);
  await reverseStepOverToLine(front, 26);
  await reverseStepOverToLine(front, 25);

  await front.removeBreakpoint(bp);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
