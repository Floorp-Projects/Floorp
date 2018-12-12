/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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
});
