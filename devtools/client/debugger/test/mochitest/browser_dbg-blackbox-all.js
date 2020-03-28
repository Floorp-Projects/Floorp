/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test checks 'Blackbox' context menu in the Sources Panel.
// Checks if submenu works correctly for options:
// - 'Un/Blackbox files in this group'
// - 'Un/Blackbox files outside this group'
// - 'Un/Blackbox files in this directory'
// - 'Un/Blackbox files outside this directory'

const sourceFiles = {
  nestedSource: "nested-source.js",
  codeReload1: "code_reload_1.js",
  previewGetter: "preview-getter.js",
};

const nodeSelectors = {
  nodeBlackBoxAll: "#node-blackbox-all",
  nodeBlackBoxAllInside: "#node-blackbox-all-inside",
  nodeUnBlackBoxAllInside: "#node-unblackbox-all-inside",
  nodeBlackBoxAllOutside: "#node-blackbox-all-outside",
  nodeUnBlackBoxAllOutside: "#node-unblackbox-all-outside",
};

function waitForBlackboxCount(dbg, count) {
  return waitForState(
    dbg,
    state => dbg.selectors.getBlackBoxList().length === count
  );
}

add_task(async function() {
  const dbg = await initDebugger("doc-blackbox-all.html");
 
  info("Loads the source file and sets a breakpoint at line 14.");
  await waitForSources(dbg, sourceFiles.nestedSource, sourceFiles.codeReload1, sourceFiles.previewGetter);
  await loadAndAddBreakpoint(dbg, sourceFiles.previewGetter, 14, 2);

  info("Expands the whole source tree.");
  rightClickElement(dbg, "sourceTreeRootNode");
  selectContextMenuItem(dbg, "#node-menu-expand-all");
  await waitForAllElements(dbg, "sourceTreeFolderNode", 3);

  const sourceTreeFolderNodeEls = findAllElements(dbg, "sourceTreeFolderNode");
  const sourceTreeRootNodeEl = findElement(dbg, "sourceTreeRootNode");
  let sources;

  info("Blackbox files in this group.");
  rightClickEl(dbg, sourceTreeRootNodeEl);
  await assertContextMenuLabel(dbg, nodeSelectors.nodeBlackBoxAllInside, "Blackbox files in this group");
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAllInside);
  await waitForBlackboxCount(dbg, 3);

  info("Main Thread was selected, all sources are blackboxed.");
  sources = dbg.selectors.getSourceList();
  is(sources.every(source => source.isBlackBoxed), true, "All sources are blackboxed as expected.");
  
  info("The invoked function is blackboxed and the debugger does not pause as expected.");
  invokeInTab("funcA");
  assertNotPaused(dbg);

  info("Unblackbox files in this directory");
  rightClickEl(dbg, sourceTreeFolderNodeEls[0]);
  await assertContextMenuLabel(dbg, nodeSelectors.nodeUnBlackBoxAllInside, "Unblackbox files in this directory");
  selectContextMenuItem(dbg, nodeSelectors.nodeUnBlackBoxAllInside);
  await waitForBlackboxCount(dbg, 0);

  info("All sources inside the selected directory are unblackboxed.");
  sources = dbg.selectors.getSourceList();
  is(sources.every(source => !source.isBlackBoxed), true, "All sources are unblackboxed as expected.");

  info("All sources are unblackboxed and the debugger pauses on line 14.");
  await selectSource(dbg, sourceFiles.nestedSource);
  invokeInTab("funcA");
  await waitForPaused(dbg);
  await resume(dbg);

  info("Blackbox files outside this directory - folder 'nested'");
  rightClickEl(dbg, sourceTreeFolderNodeEls[1]);
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAll);
  await assertContextMenuLabel(dbg, nodeSelectors.nodeBlackBoxAllInside, "Blackbox files in this directory");
  await assertContextMenuLabel(dbg, nodeSelectors.nodeBlackBoxAllOutside, "Blackbox files outside this directory");
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAllOutside);
  await waitForBlackboxCount(dbg, 2);

  info("Only source inside the selected directory is not blackboxed.");
  is(findSource(dbg, sourceFiles.nestedSource).isBlackBoxed, false, "nested-source.js source file is not blackboxed");
  is(findSource(dbg, sourceFiles.codeReload1).isBlackBoxed, true, "code_reload_1.js source file is blackboxed");
  is(findSource(dbg, sourceFiles.previewGetter).isBlackBoxed, true, "preview-getter.js source file is blackboxed");

  info("The invoked function is blackboxed and the debugger does not pause as expected.");
  invokeInTab("funcA");
  assertNotPaused(dbg);

  info("Blackbox files outside this directory - folder 'reload'");
  rightClickEl(dbg, sourceTreeFolderNodeEls[2]);
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAll);
  await assertContextMenuLabel(dbg, nodeSelectors.nodeUnBlackBoxAllInside, "Unblackbox files in this directory");
  await assertContextMenuLabel(dbg, nodeSelectors.nodeBlackBoxAllOutside, "Blackbox files outside this directory");
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAllOutside);
  await waitForBlackboxCount(dbg, 3);

  info("Unblackbox files outside this directory - folder");
  rightClickEl(dbg, sourceTreeFolderNodeEls[2]);
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAll);
  await assertContextMenuLabel(dbg, nodeSelectors.nodeUnBlackBoxAllInside, "Unblackbox files in this directory");
  await assertContextMenuLabel(dbg, nodeSelectors.nodeUnBlackBoxAllOutside, "Unblackbox files outside this directory");
  selectContextMenuItem(dbg, nodeSelectors.nodeUnBlackBoxAllOutside);
  await waitForBlackboxCount(dbg, 1);

  info("Only source inside the selected directory is still blackboxed.");
  is(findSource(dbg, sourceFiles.nestedSource).isBlackBoxed, false, "nested-source.js source file is not blackboxed");
  is(findSource(dbg, sourceFiles.codeReload1).isBlackBoxed, true, "code_reload_1.js source file is blackboxed");
  is(findSource(dbg, sourceFiles.previewGetter).isBlackBoxed, false, "preview-getter.js source file is not blackboxed");

  info("The invoked function is not blackboxed and the debugger pauses on line 14.");
  await selectSource(dbg, sourceFiles.codeReload1);
  invokeInTab("funcA");
  await waitForPaused(dbg);
  await resume(dbg);

  info("Unblackbox files in this directory - folder 'reload'");
  rightClickEl(dbg, sourceTreeFolderNodeEls[2]);
  selectContextMenuItem(dbg, nodeSelectors.nodeBlackBoxAll);
  await assertContextMenuLabel(dbg, nodeSelectors.nodeUnBlackBoxAllInside, "Unblackbox files in this directory");
  await assertContextMenuLabel(dbg, nodeSelectors.nodeBlackBoxAllOutside, "Blackbox files outside this directory");
  selectContextMenuItem(dbg, nodeSelectors.nodeUnBlackBoxAllInside);
  await waitForBlackboxCount(dbg, 0);

  sources = dbg.selectors.getSourceList();
  is(sources.every(source => !source.isBlackBoxed), true, "All sources are unblackboxed as expected.");
});