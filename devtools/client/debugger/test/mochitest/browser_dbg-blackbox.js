/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// This test covers all the blackboxing functionality relating to a selected
// source open in the debugger editor.

"use strict";

requestLongerTimeout(5);

const contextMenuItems = {
  ignoreSource: { selector: "#node-menu-blackbox", label: "Ignore source" },
  unignoreSource: { selector: "#node-menu-blackbox", label: "Unignore source" },
  ignoreLines: { selector: "#node-menu-blackbox-lines", label: "Ignore lines" },
  unignoreLines: {
    selector: "#node-menu-blackbox-lines",
    label: "Unignore lines",
  },
  ignoreLine: { selector: "#node-menu-blackbox-line", label: "Ignore line" },
  unignoreLine: {
    selector: "#node-menu-blackbox-line",
    label: "Unignore line",
  },
};

// Tests basic functionality for blackbox source and blackbox single and multiple lines
add_task(async function testAllBlackBox() {
  await pushPref("devtools.debugger.features.blackbox-lines", true);
  // using the doc-command-click.html as it has a simple js file we can use
  // testing.
  const file = "simple4.js";
  const dbg = await initDebugger("doc-command-click.html", file);

  const source = findSource(dbg, file);

  await selectSource(dbg, source);

  await addBreakpoint(dbg, file, 8);

  await testBlackBoxSource(dbg, source);
  await testBlackBoxMultipleLines(dbg, source);
  await testBlackBoxSingleLine(dbg, source);
});

// Test that the blackboxed lines are persisted accross reloads and still work accordingly.
add_task(async function testBlackBoxOnReload() {
  await pushPref("devtools.debugger.features.blackbox-lines", true);

  const file = "simple4.js";
  const dbg = await initDebugger("doc-command-click.html", file);

  const source = findSource(dbg, file);

  await selectSource(dbg, source);

  // Adding 2 breakpoints in funcB() and funcC() which
  // would be hit in order.
  await addBreakpoint(dbg, file, 8);
  await addBreakpoint(dbg, file, 12);

  // Lets reload without any blackboxing to make all necesary postions
  // are hit.
  const onReloaded = reload(dbg, file);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 12);
  await resumeAndWaitForPauseCounter(dbg);

  info("Wait for reload to complete after resume");
  await onReloaded;

  assertNotPaused(dbg);

  info("Ignoring line 2");
  await openContextMenu(dbg, "gutter", 2);
  await assertBlackBoxBoxContextMenuItems(dbg, [contextMenuItems.ignoreLine]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  info("Ignoring line 7 to 9");
  selectEditorLines(dbg, 7, 9);
  await openContextMenu(dbg, "CodeMirrorLines");
  await assertBlackBoxBoxContextMenuItems(dbg, [
    contextMenuItems.unignoreSource,
    contextMenuItems.ignoreLines,
  ]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  const onReloaded2 = reload(dbg, file);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 12);
  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded2;

  assertNotPaused(dbg);
});

add_task(async function testBlackBoxOnToolboxRestart() {
  await pushPref("devtools.debugger.features.blackbox-lines", true);

  const dbg = await initDebugger("doc-command-click.html", "simple4.js");
  const source = findSource(dbg, "simple4.js");

  await selectSource(dbg, source);

  const onReloaded = reload(dbg, "simple4.js");
  await waitForPaused(dbg);

  info("Assert it paused at the debugger statement");
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);
  await onReloaded;

  info("Ignoring line 2 using the gutter context menu");
  await openContextMenu(dbg, "gutter", 2);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  await reloadBrowser();
  // Wait a little bit incase of a pause
  await wait(1000);

  info("Assert that the debugger no longer pauses on the debugger statement");
  assertNotPaused(dbg);

  info("Close the toolbox");
  await dbg.toolbox.closeToolbox();

  info("Reopen the toolbox on the debugger");
  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  const dbg2 = createDebuggerContext(toolbox);
  await waitForSelectedSource(dbg2, findSource(dbg2, "simple4.js"));

  await reloadBrowser();
  // Wait a little incase of a pause
  await wait(1000);

  info("Assert that debbuger still does not pause on the debugger statement");
  assertNotPaused(dbg2);
});

async function testBlackBoxSource(dbg, source) {
  info("Start testing blackboxing the whole source");

  info("blackbox the whole simple4.js source file");
  await openContextMenu(dbg, "CodeMirrorLines");
  await assertBlackBoxBoxContextMenuItems(dbg, [
    contextMenuItems.ignoreSource,
    contextMenuItems.ignoreLine,
  ]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox");

  invokeInTab("funcA");

  info(
    "The debugger statement on line 2 and the breakpoint on line 8 should not be hit"
  );
  assertNotPaused(dbg);

  info("unblackbox the whole source");
  await openContextMenu(dbg, "CodeMirrorLines");
  await assertBlackBoxBoxContextMenuItems(dbg, [
    contextMenuItems.unignoreSource,
  ]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox");

  invokeInTab("funcA");

  info("assert the pause at the debugger statement on line 2");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  info("assert the pause at the breakpoint set on line 8");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resumeAndWaitForPauseCounter(dbg);

  assertNotPaused(dbg);
}

async function testBlackBoxMultipleLines(dbg, source) {
  info("Blackbox lines 7 to 13");
  selectEditorLines(dbg, 7, 13);
  await openContextMenu(dbg, "CodeMirrorLines");
  await assertBlackBoxBoxContextMenuItems(dbg, [
    contextMenuItems.ignoreSource,
    contextMenuItems.ignoreLines,
  ]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  invokeInTab("funcA");

  info("assert the pause at the debugger statement on line 2");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resumeAndWaitForPauseCounter(dbg);

  info(
    "The breakpoint set on line 8 should not get hit as its within the blackboxed range"
  );
  assertNotPaused(dbg);

  info("Unblackbox lines 7 to 13");
  selectEditorLines(dbg, 7, 13);
  await openContextMenu(dbg, "CodeMirrorLines");
  await assertBlackBoxBoxContextMenuItems(dbg, [
    contextMenuItems.unignoreSource,
    contextMenuItems.unignoreLines,
  ]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  invokeInTab("funcA");

  // assert the pause at the debugger statement on line 2
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  // assert the pause at the breakpoint set on line 8
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resumeAndWaitForPauseCounter(dbg);

  assertNotPaused(dbg);
}

async function testBlackBoxSingleLine(dbg, source) {
  info("Black box line 2 of funcA() with the debugger statement");
  await openContextMenu(dbg, "gutter", 2);
  await assertBlackBoxBoxContextMenuItems(dbg, [contextMenuItems.ignoreLine]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  invokeInTab("funcA");

  // assert the pause at the breakpoint set on line 8
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resumeAndWaitForPauseCounter(dbg);

  assertNotPaused(dbg);

  info("Un-blackbox line 2 of funcA()");
  selectEditorLines(dbg, 2, 2);
  await openContextMenu(dbg, "CodeMirrorLines");
  await assertBlackBoxBoxContextMenuItems(dbg, [
    contextMenuItems.unignoreSource,
    contextMenuItems.unignoreLine,
  ]);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  invokeInTab("funcA");

  // assert the pause at the debugger statement on line 2
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  // assert the pause at the breakpoint set on line 8
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resume(dbg);

  assertNotPaused(dbg);
}

// Resume and wait for the thread context's `pauseCounter` to get
// updated.
async function resumeAndWaitForPauseCounter(dbg) {
  const prevThreadPauseCounter = getThreadContext(dbg).pauseCounter;
  await resume(dbg);
  return waitFor(
    () => getThreadContext(dbg).pauseCounter > prevThreadPauseCounter
  );
}

/**
 * Asserts that the blackbox context menu items which are visible are correct
 * @params {Object} dbg
 * @params {Array} expectedContextMenuItems
 *                 The context menu items (related to blackboxing) which should be visible
 *                 e.g When the whole source is blackboxed, we should only see the "Unignore source"
 *                 context menu item.
 */
async function assertBlackBoxBoxContextMenuItems(
  dbg,
  expectedContextMenuItems
) {
  for (const item of expectedContextMenuItems) {
    await assertContextMenuLabel(dbg, item.selector, item.label);
  }
}

/**
 * Opens the debugger editor context menu in either codemirror or the
 * the debugger gutter.
 * @params {Object} dbg
 * @params {String} elementName
 *                  The element to select
 * @params {Number} line
 *                  The line to open the context menu on.
 */
async function openContextMenu(dbg, elementName, line) {
  const waitForOpen = waitForContextMenu(dbg);
  info(`Open ${elementName} context menu on line ${line || ""}`);
  rightClickElement(dbg, elementName, line);
  return waitForOpen;
}

/**
 * Selects the specific black box context menu item
 * @params {Object} dbg
 * @params {String} itemName
 *                  The name of the context menu item.
 */
async function selectBlackBoxContextMenuItem(dbg, itemName) {
  const wait = waitForDispatch(dbg.store, "BLACKBOX");
  info(`Select the ${itemName} context menu item`);
  selectContextMenuItem(dbg, `#node-menu-${itemName}`);
  return wait;
}

/**
 * Selects a range of lines
 * @params {Object} dbg
 * @params {Number} startLine
 * @params {Number} endLine
 */
function selectEditorLines(dbg, startLine, endLine) {
  getCM(dbg).setSelection(
    { line: startLine - 1, ch: 0 },
    { line: endLine - 1, ch: 0 }
  );
}
