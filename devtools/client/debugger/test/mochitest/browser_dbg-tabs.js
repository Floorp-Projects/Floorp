/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests adding and removing tabs

// Debugger operations may still be in progress when we navigate.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/Page has navigated/);

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
