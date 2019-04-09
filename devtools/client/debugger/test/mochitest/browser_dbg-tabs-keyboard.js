/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests removing tabs with keyboard shortcuts

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1", "simple2");

  await selectSource(dbg, "simple1");
  await selectSource(dbg, "simple2");
  is(countTabs(dbg), 2);
  
  pressKey(dbg, "close");
  waitForDispatch(dbg, "CLOSE_TAB");
  is(countTabs(dbg), 1);

  pressKey(dbg, "close");
  waitForDispatch(dbg, "CLOSE_TAB");
  is(countTabs(dbg), 0);
});
