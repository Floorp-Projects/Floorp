/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that IDB transactions are not processed at microtask checkpoints
// introduced by debugger hooks.
add_task(async function() {
  const dbg = await initDebugger("doc-idb-run-to-completion.html");
  invokeInTab("test", "doc-xhr-run-to-completion.html");
  await waitForPaused(dbg);
  ok(true, "paused after successfully processing IDB transaction");
});
