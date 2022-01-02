/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This source map does not have source contents, so it's fetched separately
add_task(async function() {
  // NOTE: the CORS call makes the test run times inconsistent
  const dbg = await initDebugger("doc-sourcemaps3.html");
  dbg.actions.toggleMapScopes();

  const {
    selectors: { getBreakpoint, getBreakpointCount },
    getState,
  } = dbg;

  await waitForSources(dbg, "bundle.js", "sorted.js", "test.js");

  const sortedSrc = findSource(dbg, "sorted.js");

  await selectSource(dbg, sortedSrc);

  // Show the source in source tree in primiany panel for blackBox icon check
  rightClickElement(dbg, "CodeMirrorLines");
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, "#node-menu-show-source");
  await waitForDispatch(dbg.store, "SHOW_SOURCE");

  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");

  const sourceTab = findElementWithSelector(dbg, ".source-tab.active");
  ok(
    sourceTab.querySelector(".img.blackBox"),
    "Source tab has a blackbox icon"
  );

  const treeItem = findElementWithSelector(dbg, ".tree-node.focused");
  ok(
    treeItem.querySelector(".img.blackBox"),
    "Source tree item has a blackbox icon"
  );

  // breakpoint at line 38 in sorted
  await addBreakpoint(dbg, sortedSrc, 38);
  // invoke test
  invokeInTab("test");
  // should not pause
  is(isPaused(dbg), false);

  // unblackbox
  await clickElement(dbg, "blackbox");
  await waitForDispatch(dbg.store, "BLACKBOX");

  // click on test
  invokeInTab("test");
  // should pause
  await waitForPaused(dbg);

  ok(true, "blackbox works");
});
