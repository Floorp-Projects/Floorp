/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests adding and removing tabs

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1", "simple2");

  await selectSource(dbg, "simple1");
  await selectSource(dbg, "simple2");
  is(countTabs(dbg), 2);

  info("Test reloading the debugger");
  await reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 2);
  await waitForSelectedSource(dbg);

  info("Test reloading the debuggee a second time");
  await reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 2);
  await waitForSelectedSource(dbg);
});

add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html", "simple1", "simple2");

  await selectSource(dbg, "simple1");
  await selectSource(dbg, "simple2");
  await closeTab(dbg, "simple1");
  await closeTab(dbg, "simple2");

  info("Test reloading the debugger");
  await reload(dbg, "simple1", "simple2");
  is(countTabs(dbg), 0);
});
