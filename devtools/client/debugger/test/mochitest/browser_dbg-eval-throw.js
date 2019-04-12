/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that exceptions thrown while evaluating code will pause at the point the
// exception was generated when pausing on uncaught exceptions.
add_task(async function() {
  const dbg = await initDebugger("doc-eval-throw.html");
  await togglePauseOnExceptions(dbg, true, true);

  invokeInTab("evaluateException");
  await waitForPaused(dbg);
  const source = dbg.selectors.getSelectedSource();
  ok(!source.url, "Selected source should not have a URL");
});
