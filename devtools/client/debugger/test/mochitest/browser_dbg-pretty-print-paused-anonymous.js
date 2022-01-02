/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that pretty-printing a file with no URL works while paused.
add_task(async function() {
  const dbg = await initDebugger("doc-minified.html");

  const debuggerDone = dbg.client.evaluate("debugger; var foo;");
  await waitForPaused(dbg);

  // This will throw if things fail to pretty-print and render properly.
  await prettyPrint(dbg);
});
