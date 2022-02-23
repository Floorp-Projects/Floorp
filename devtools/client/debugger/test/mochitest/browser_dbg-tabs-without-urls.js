/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that URL-less sources have tabs added to the UI but
// do not persist upon reload
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1", "simple2");

  await selectSource(dbg, "simple1");
  await selectSource(dbg, "simple2");

  is(countTabs(dbg), 2);

  invokeInTab("doEval");
  await waitForPaused(dbg);
  await resume(dbg);
  is(countTabs(dbg), 3);

  invokeInTab("doEval");
  await waitForPaused(dbg);
  await resume(dbg);
  is(countTabs(dbg), 4);

  // Test reloading the debugger
  await reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 2);

  // TODO: This is here to make this test less flakey because otherwise the
  // test will end while the files are still loading, which will stop all
  // in-progress requests causing uncaught rejections when querying
  // 'getBreakpointPositionsCompressed'.
  await selectSource(dbg, "simple1");
  await selectSource(dbg, "simple2");
});
