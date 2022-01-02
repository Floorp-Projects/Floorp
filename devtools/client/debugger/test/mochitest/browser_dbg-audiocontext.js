/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

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
