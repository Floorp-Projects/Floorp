/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test basic logpoint functionality in web replay. When logpoints are added,
// new messages should appear in the correct order and allow time warping.
add_task(async function() {
  const dbg = await attachRecordingDebugger("doc_rr_basic.html", {
    waitForRecording: true,
  });

  const { tab, toolbox, threadClient, target } = dbg;
  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  const bp1 = await setBreakpoint(threadClient, "doc_rr_basic.html", 21, {
    logValue: `"Logpoint Number " + number`,
  });
  const bp2 = await setBreakpoint(threadClient, "doc_rr_basic.html", 6, {
    logValue: `"Logpoint Beginning"`,
  });
  const bp3 = await setBreakpoint(threadClient, "doc_rr_basic.html", 8, {
    logValue: `"Logpoint Ending"`,
  });

  const messages = await waitForMessageCount(hud, "Logpoint", 12);
  ok(messages[0].textContent.includes("Beginning"));
  for (let i = 1; i <= 10; i++) {
    ok(messages[i].textContent.includes("Number " + i));
  }
  ok(messages[11].textContent.includes("Ending"));

  await warpToMessage(hud, dbg, "Number 5");
  await threadClient.interrupt();

  await checkEvaluateInTopFrame(target, "number", 5);
  await reverseStepOverToLine(threadClient, 20);

  await threadClient.removeBreakpoint(bp1);
  await threadClient.removeBreakpoint(bp2);
  await threadClient.removeBreakpoint(bp3);
  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
