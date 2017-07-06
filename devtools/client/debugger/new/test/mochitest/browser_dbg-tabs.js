/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests adding and removing tabs

function countTabs(dbg) {
  return findElement(dbg, "sourceTabs").children.length;
}

add_task(function*() {
  let dbg = yield initDebugger("doc-scripts.html");

  yield selectSource(dbg, "simple1");
  yield selectSource(dbg, "simple2");
  is(countTabs(dbg), 2);

  // Test reloading the debugger
  yield reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 2);
  yield waitForSelectedSource(dbg);

  // Test reloading the debuggee a second time
  yield reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 2);
  yield waitForSelectedSource(dbg);
});

add_task(function*() {
  let dbg = yield initDebugger("doc-scripts.html", "simple1", "simple2");

  yield selectSource(dbg, "simple1");
  yield selectSource(dbg, "simple2");
  closeTab(dbg, "simple1");
  closeTab(dbg, "simple2");

  // Test reloading the debugger
  yield reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 0);
});
