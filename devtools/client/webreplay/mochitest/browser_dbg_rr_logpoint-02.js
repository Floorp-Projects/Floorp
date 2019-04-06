/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-undef */

"use strict";

// Test that logpoints appear and disappear as expected as breakpoints are
// modified. Also test that conditional logpoints work.
add_task(async function() {
  const dbg = await attachRecordingDebugger(
    "doc_rr_basic.html",
    { waitForRecording: true }
  );

  const {tab, toolbox} = dbg;
  const console = await getDebuggerSplitConsole(dbg);
  const hud = console.hud;

  await selectSource(dbg, "doc_rr_basic.html");
  await addBreakpoint(dbg, "doc_rr_basic.html", 21, undefined,
                      { logValue: `"Logpoint Number " + number` });
  await addBreakpoint(dbg, "doc_rr_basic.html", 6, undefined,
                      { logValue: `"Logpoint Beginning"` });
  await addBreakpoint(dbg, "doc_rr_basic.html", 8, undefined,
                      { logValue: `"Logpoint Ending"` });
  await waitForMessageCount(hud, "Logpoint", 12);

  await disableBreakpoint(dbg, findSource(dbg, "doc_rr_basic.html"), 6);
  await waitForMessageCount(hud, "Logpoint", 11);

  await setBreakpointOptions(dbg, "doc_rr_basic.html", 21, undefined,
    { logValue: `"Logpoint Number " + number`, condition: `number % 2 == 0` });
  await waitForMessageCount(hud, "Logpoint", 6);

  await dbg.actions.removeAllBreakpoints(getContext(dbg));

  await toolbox.destroy();
  await gBrowser.removeTab(tab);
});
