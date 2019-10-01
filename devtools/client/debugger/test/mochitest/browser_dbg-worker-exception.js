/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that we can pause on exceptions in newly created workers.
add_task(async function() {
  const dbg = await initDebugger("doc-worker-exception.html");

  await togglePauseOnExceptions(dbg, true, false);

  invokeInTab("startWorker");
  invokeInTab("messageWorker");

  await waitForPaused(dbg);

  const source = findSource(dbg, "worker-exception.js");
  assertPausedAtSourceAndLine(dbg, source.id, 2);
});
