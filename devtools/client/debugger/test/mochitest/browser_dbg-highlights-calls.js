/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks to see if command button highlights and unhighlights
// calls when debugger is paused.
add_task(async function() {
  await pushPref("devtools.debugger.features.command-click", true);
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
  pressKey(dbg, "commandKeyUp");
  const nocalls = dbg.win.document.querySelectorAll(".highlight-function-calls");
  is(nocalls.length, 0);
});