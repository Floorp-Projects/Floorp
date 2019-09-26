/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that the debugger doesn't break when workers are killed.
add_task(async function() {
  const dbg = await initDebugger("doc-windowless-workers.html");
  const mainThread = dbg.toolbox.threadFront.actor;

  // Make sure a simple-worker.js source is visible in the tree.
  await assertSourceCount(dbg, 7);
  await clickElement(dbg, "sourceDirectoryLabel", 6);
  await assertSourceCount(dbg, 8);

  // Terminate all workers.
  invokeInTab("sayGoodbye");

  // Make sure the debugger isn't broken by checking that we can pause in the
  // main thread.
  assertNotPaused(dbg);
  await dbg.actions.breakOnNext(getThreadContext(dbg));
  await waitForPaused(dbg, "doc-windowless-workers.html");
});
