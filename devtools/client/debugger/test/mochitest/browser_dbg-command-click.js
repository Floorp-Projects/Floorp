/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks to see if holding down the command key and clicking on function call
// will jump the debugger to that call.
add_task(async function() {
  await pushPref("devtools.debugger.features.command-click", true);
  info("Checking to see if command click will jump the debugger to another highlighted call.");
  const dbg = await initDebugger("doc-command-click.html", "simple4.js");

  const source = findSource(dbg, "simple4.js");

  await selectSource(dbg, source.url);
  await waitForSelectedSource(dbg, source.url);


  invokeInTab("funcA");
  await waitForPaused(dbg);
  await waitForInlinePreviews(dbg);

  pressKey(dbg, "commandKeyDown");
  await waitForDispatch(dbg.store, "HIGHLIGHT_CALLS");
  const calls = dbg.win.document.querySelectorAll(".highlight-function-calls");
  is(calls.length, 2);

  const funcB = calls[0];
  clickDOMElement(dbg, funcB);
  await waitForDispatch(dbg.store, "RESUME");
  await waitForPaused(dbg);
  assertDebugLine(dbg, 3, 2);
  const nocalls = dbg.win.document.querySelectorAll(".highlight-function-calls");
  is(nocalls.length, 0);
});
