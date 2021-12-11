/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks 'Blackbox' context menu in the Sources Panel.
// Checks if submenu works correctly for options:
// - 'Un/Blackbox files in this group'
// - 'Un/Blackbox files outside this group'
// - 'Un/Blackbox files in this directory'
// - 'Un/Blackbox files outside this directory'

const SOURCE_FILES = {
  nestedSource: "nested-source.js",
  codeReload1: "code_reload_1.js",
};

const NODE_SELECTORS = {
  nodeBlackBoxAll: "#node-blackbox-all",
  nodeBlackBoxAllInside: "#node-blackbox-all-inside",
  nodeUnBlackBoxAllInside: "#node-unblackbox-all-inside",
  nodeBlackBoxAllOutside: "#node-blackbox-all-outside",
  nodeUnBlackBoxAllOutside: "#node-unblackbox-all-outside",
};

add_task(async function() {
  const dbg = await initDebugger("doc-blackbox-all.html");

  info("Loads the source file and sets a breakpoint at line 2.");
  await waitForSources(dbg, SOURCE_FILES.nestedSource, SOURCE_FILES.codeReload1);
  await selectSource(dbg, SOURCE_FILES.nestedSource);
  await addBreakpoint(dbg, SOURCE_FILES.nestedSource, 2);

  info("Expands the whole source tree.");
  rightClickElement(dbg, "sourceTreeRootNode");
  await waitForContextMenu(dbg);
  selectContextMenuItem(dbg, "#node-menu-expand-all");
  await waitForAllElements(dbg, "sourceTreeFolderNode", 3);
  const sourceTreeFolderNodeEls = findAllElements(dbg, "sourceTreeFolderNode");
  const sourceTreeRootNodeEl = findElement(dbg, "sourceTreeRootNode");

  info("Blackbox files in this directory.");
  rightClickEl(dbg, sourceTreeFolderNodeEls[1]);
  await waitForContextMenu(dbg);
  await openContextMenuSubmenu(dbg, NODE_SELECTORS.nodeBlackBoxAll);
  await assertContextMenuLabel(dbg, NODE_SELECTORS.nodeBlackBoxAllInside, "Ignore files in this directory");
  await assertContextMenuLabel(dbg, NODE_SELECTORS.nodeBlackBoxAllOutside, "Ignore files outside this directory");
  selectContextMenuItem(dbg, NODE_SELECTORS.nodeBlackBoxAllInside);
  await waitForBlackboxCount(dbg, 1);
  await waitForRequestsToSettle(dbg);

  is(findSource(dbg, SOURCE_FILES.nestedSource).isBlackBoxed, true, "nested-source.js is blackboxed");
  is(findSource(dbg, SOURCE_FILES.codeReload1).isBlackBoxed, false, "code_reload_1.js is not blackboxed");

  info("The invoked function is blackboxed and the debugger does not pause.");
  invokeInTab("computeSomething");
  assertNotPaused(dbg);

  info("Unblackbox files outside this directory.");
  rightClickEl(dbg, sourceTreeFolderNodeEls[2]);
  await waitForContextMenu(dbg);
  await openContextMenuSubmenu(dbg, NODE_SELECTORS.nodeBlackBoxAll);
  await assertContextMenuLabel(dbg, NODE_SELECTORS.nodeBlackBoxAllInside, "Ignore files in this directory");
  await assertContextMenuLabel(dbg, NODE_SELECTORS.nodeUnBlackBoxAllOutside, "Unignore files outside this directory");
  selectContextMenuItem(dbg, NODE_SELECTORS.nodeUnBlackBoxAllOutside);
  await waitForBlackboxCount(dbg, 0);
  await waitForRequestsToSettle(dbg);

  is(findSource(dbg, SOURCE_FILES.nestedSource).isBlackBoxed, false, "nested-source.js is not blackboxed");
  is(findSource(dbg, SOURCE_FILES.codeReload1).isBlackBoxed, false, "code_reload_1.js is not blackboxed");

  info("All sources are unblackboxed and the debugger pauses on line 2.");
  invokeInTab("computeSomething");
  await waitForPaused(dbg);
  await resume(dbg);

  info("Blackbox files in this group.");
  rightClickEl(dbg, sourceTreeRootNodeEl);
  await waitForContextMenu(dbg);
  await assertContextMenuLabel(dbg, NODE_SELECTORS.nodeBlackBoxAllInside, "Ignore files in this group");
  selectContextMenuItem(dbg, NODE_SELECTORS.nodeBlackBoxAllInside);
  await waitForBlackboxCount(dbg, 2);
  await waitForRequestsToSettle(dbg);

  is(findSource(dbg, SOURCE_FILES.nestedSource).isBlackBoxed, true, "nested-source.js is blackboxed");
  is(findSource(dbg, SOURCE_FILES.codeReload1).isBlackBoxed, true, "code_reload_1.js is blackboxed");
 
  info("Unblackbox files in this group.");
  rightClickEl(dbg, sourceTreeRootNodeEl);
  await waitForContextMenu(dbg);
  await assertContextMenuLabel(dbg, NODE_SELECTORS.nodeUnBlackBoxAllInside, "Unignore files in this group");
  selectContextMenuItem(dbg, NODE_SELECTORS.nodeUnBlackBoxAllInside);
  await waitForBlackboxCount(dbg, 0);
  await waitForRequestsToSettle(dbg);

  is(findSource(dbg, SOURCE_FILES.nestedSource).isBlackBoxed, false, "nested-source.js is not blackboxed");
  is(findSource(dbg, SOURCE_FILES.codeReload1).isBlackBoxed, false, "code_reload_1.js is not blackboxed");
});

function waitForBlackboxCount(dbg, count) {
  return waitForState(
    dbg,
    state => dbg.selectors.getBlackBoxList().length === count
  );
}
