/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the AudioContext are paused and resume appropriately when using the
// debugger.

add_task(async function() {
  const dbg = await initDebugger("doc-audiocontext.html");

  await invokeInTab("myFunction");
  await invokeInTab("suspendAC");
  invokeInTab("debuggerStatement");
  await waitForPaused(dbg);
  await resume(dbg);
  await invokeInTab("checkACState");
  ok(true, "No AudioContext state transition are caused by the debugger")
});
