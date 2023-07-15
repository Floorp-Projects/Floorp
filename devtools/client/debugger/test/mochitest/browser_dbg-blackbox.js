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

const SOURCE_IS_FULLY_IGNORED = "source";
const SOURCE_LINES_ARE_IGNORED = "line";
const SOURCE_IS_NOT_IGNORED = "none";

// Tests basic functionality for blackbox source and blackbox single and multiple lines
add_task(async function testAllBlackBox() {
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
  const file = "simple4.js";
  const dbg = await initDebugger("doc-command-click.html", file);

  const source = findSource(dbg, file);

  await selectSource(dbg, source);

  // Adding 2 breakpoints in funcB() and funcC() which
  // would be hit in order.
  await addBreakpoint(dbg, file, 8);
  await addBreakpoint(dbg, file, 12);

  info("Reload without any blackboxing to make all necesary postions are hit");
  const onReloaded = reload(dbg, file);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resume(dbg);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 12);
  await resume(dbg);

  info("Wait for reload to complete after resume");
  await onReloaded;

  assertNotPaused(dbg);

  info("Ignoring line 2 using the gutter context menu");
  await openContextMenuInDebugger(dbg, "gutter", 2);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  info("Ignoring line 7 to 9 using the editor context menu");
  await selectEditorLinesAndOpenContextMenu(dbg, { startLine: 7, endLine: 9 });
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  const onReloaded2 = reload(dbg, file);

  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 12);
  await resume(dbg);
  info("Wait for reload to complete after resume");
  await onReloaded2;

  assertNotPaused(dbg);

  info(
    "Check that the expected blackbox context menu state is correct across reload"
  );

  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 2,
    nonBlackBoxedLine: 4,
    blackBoxedLines: [7, 9],
    nonBlackBoxedLines: [3, 4],
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 2,
    nonBlackBoxedLine: 4,
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple4.js"),
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });
});

add_task(async function testBlackBoxOnToolboxRestart() {
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
  await openContextMenuInDebugger(dbg, "gutter", 2);
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

  info("Assert the blackbox context menu items before any blackboxing is done");
  // When the source is not blackboxed there are no blackboxed lines
  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackBoxedLines: null,
    nonBlackBoxedLines: [3, 4],
    blackboxedSourceState: SOURCE_IS_NOT_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackboxedSourceState: SOURCE_IS_NOT_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: null,
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple4.js"),
  });

  info(
    "Blackbox the whole simple4.js source file using the editor context menu"
  );
  await openContextMenuInDebugger(dbg, "CodeMirrorLines");
  await selectBlackBoxContextMenuItem(dbg, "blackbox");

  info("Assert that all lines in the source are styled correctly");
  assertIgnoredStyleInSourceLines(dbg, { hasBlackboxedLinesClass: true });

  info("Assert that the source tree for simple4.js has the ignored style");
  const node = findSourceNodeWithText(dbg, "simple4.js");
  ok(
    node.querySelector(".blackboxed"),
    "simple4.js node does not have the ignored style"
  );

  invokeInTab("funcA");

  info(
    "The debugger statement on line 2 and the breakpoint on line 8 should not be hit"
  );
  assertNotPaused(dbg);

  info("Assert the blackbox context menu items after blackboxing is done");
  // When the whole source is blackboxed there are no nonBlackboxed lines
  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 2,
    nonBlackBoxedLine: null,
    blackBoxedLines: [3, 5],
    nonBlackBoxedLines: null,
    blackboxedSourceState: SOURCE_IS_FULLY_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 2,
    nonBlackBoxedLine: null,
    blackboxedSourceState: SOURCE_IS_FULLY_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple4.js"),
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });

  info("Unblackbox the whole source using the sourcetree context menu");
  rightClickEl(dbg, findSourceNodeWithText(dbg, "simple4.js"));
  await waitForContextMenu(dbg);
  await selectBlackBoxContextMenuItem(dbg, "blackbox");

  info("Assert that all lines in the source are un-styled correctly");
  assertIgnoredStyleInSourceLines(dbg, { hasBlackboxedLinesClass: false });

  info(
    "Assert that the source tree for simple4.js does not have the ignored style"
  );
  const nodeAfterBlackbox = findSourceNodeWithText(dbg, "simple4.js");
  ok(
    !nodeAfterBlackbox.querySelector(".blackboxed"),
    "simple4.js node still has the ignored style"
  );

  invokeInTab("funcA");

  info("assert the pause at the debugger statement on line 2");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  info("assert the pause at the breakpoint set on line 8");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resume(dbg);

  assertNotPaused(dbg);

  // When the source is not blackboxed there are no blackboxed lines
  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackBoxedLines: null,
    nonBlackBoxedLines: [3, 4],
    blackboxedSourceState: SOURCE_IS_NOT_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackboxedSourceState: SOURCE_IS_NOT_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: null,
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });
}

async function testBlackBoxMultipleLines(dbg, source) {
  info("Blackbox lines 7 to 13 using the  editor content menu items");
  await selectEditorLinesAndOpenContextMenu(dbg, { startLine: 7, endLine: 13 });
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 3,
    blackBoxedLines: [7, 9],
    nonBlackBoxedLines: [3, 4],
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 3,
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple4.js"),
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });

  info("Assert that the ignored lines are styled correctly");
  assertIgnoredStyleInSourceLines(dbg, {
    lines: [7, 13],
    hasBlackboxedLinesClass: true,
  });

  info("Assert that the source tree for simple4.js has the ignored style");
  const node = findSourceNodeWithText(dbg, "simple4.js");

  ok(
    node.querySelector(".blackboxed"),
    "simple4.js node does not have the ignored style"
  );

  invokeInTab("funcA");

  info("assert the pause at the debugger statement on line 2");
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 2);
  await resume(dbg);

  info(
    "The breakpoint set on line 8 should not get hit as its within the blackboxed range"
  );
  assertNotPaused(dbg);

  info("Unblackbox lines 7 to 13");
  await selectEditorLinesAndOpenContextMenu(dbg, { startLine: 7, endLine: 13 });
  await selectBlackBoxContextMenuItem(dbg, "blackbox-lines");

  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackBoxedLines: null,
    nonBlackBoxedLines: [3, 4],
    blackboxedSourceState: SOURCE_IS_NOT_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackboxedSourceState: SOURCE_IS_NOT_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: null,
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });

  info("Assert that the un-ignored lines are no longer have the style");
  assertIgnoredStyleInSourceLines(dbg, {
    lines: [7, 13],
    hasBlackboxedLinesClass: false,
  });

  info(
    "Assert that the source tree for simple4.js does not have the ignored style"
  );
  const nodeAfterBlackbox = findSourceNodeWithText(dbg, "simple4.js");
  ok(
    !nodeAfterBlackbox.querySelector(".blackboxed"),
    "simple4.js still has the ignored style"
  );

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

async function testBlackBoxSingleLine(dbg, source) {
  info("Black box line 2 of funcA() with the debugger statement");
  await openContextMenuInDebugger(dbg, "gutter", 2);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 2,
    nonBlackBoxedLine: 4,
    blackBoxedLines: null,
    nonBlackBoxedLines: [3, 4],
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: null,
    nonBlackBoxedLine: 4,
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple4.js"),
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });

  info("Assert that the ignored line 2 is styled correctly");
  assertIgnoredStyleInSourceLines(dbg, {
    lines: [2],
    hasBlackboxedLinesClass: true,
  });

  info("Black box line 4 of funcC() with the debugger statement");
  await openContextMenuInDebugger(dbg, "gutter", 4);
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  info("Assert that the ignored line 4 is styled correctly");
  assertIgnoredStyleInSourceLines(dbg, {
    lines: [4],
    hasBlackboxedLinesClass: true,
  });

  invokeInTab("funcA");

  // assert the pause at the breakpoint set on line 8
  await waitForPaused(dbg);
  assertPausedAtSourceAndLine(dbg, source.id, 8);
  await resume(dbg);

  assertNotPaused(dbg);

  info("Un-blackbox line 2 of funcA()");
  selectEditorLines(dbg, 2, 2);
  await openContextMenuInDebugger(dbg, "CodeMirrorLines");
  await selectBlackBoxContextMenuItem(dbg, "blackbox-line");

  await assertEditorBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 4,
    nonBlackBoxedLine: 3,
    blackBoxedLines: null,
    nonBlackBoxedLines: [11, 12],
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertGutterBlackBoxBoxContextMenuItems(dbg, {
    blackboxedLine: 4,
    nonBlackBoxedLine: 3,
    blackboxedSourceState: SOURCE_LINES_ARE_IGNORED,
  });

  await assertSourceTreeBlackBoxBoxContextMenuItems(dbg, {
    blackBoxedSourceTreeNode: null,
    nonBlackBoxedSourceTreeNode: findSourceNodeWithText(dbg, "simple2.js"),
  });

  info("Assert that the un-ignored line 2 is styled correctly");
  assertIgnoredStyleInSourceLines(dbg, {
    lines: [2],
    hasBlackboxedLinesClass: false,
  });

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

async function assertContextMenuDisabled(dbg, selector, shouldBeDisabled) {
  const item = await waitFor(() => findContextMenu(dbg, selector));
  is(
    item.disabled,
    shouldBeDisabled,
    `The the context menu item is ${
      shouldBeDisabled ? "disabled" : "not disabled"
    }`
  );
}

/**
 * Asserts that the gutter blackbox context menu items which are visible are correct
 * @params {Object} dbg
 * @params {Array} testFixtures
 *                 Details needed for the assertion. Any blackboxed/nonBlackboxed lines
 *                 and any blackboxed/nonBlackboxed sources
 */
async function assertGutterBlackBoxBoxContextMenuItems(dbg, testFixtures) {
  const { blackboxedLine, nonBlackBoxedLine, blackboxedSourceState } =
    testFixtures;
  if (blackboxedLine) {
    info(
      "Asserts that the gutter context menu items when clicking on the gutter of a blackboxed line"
    );
    const popup = await openContextMenuInDebugger(
      dbg,
      "gutter",
      blackboxedLine
    );
    // When the whole source is blackboxed the the gutter visually shows `ignore line`
    // but it is disabled indicating that individual lines cannot be nonBlackboxed.
    const item =
      blackboxedSourceState == SOURCE_IS_FULLY_IGNORED
        ? contextMenuItems.ignoreLine
        : contextMenuItems.unignoreLine;

    await assertContextMenuLabel(dbg, item.selector, item.label);
    await assertContextMenuDisabled(
      dbg,
      item.selector,
      blackboxedSourceState == SOURCE_IS_FULLY_IGNORED
    );
    await closeContextMenu(dbg, popup);
  }

  if (nonBlackBoxedLine) {
    info(
      "Asserts that the gutter context menu items when clicking on the gutter of a nonBlackboxed line"
    );
    const popup = await openContextMenuInDebugger(
      dbg,
      "gutter",
      nonBlackBoxedLine
    );
    const item = contextMenuItems.ignoreLine;
    await assertContextMenuLabel(dbg, item.selector, item.label);
    await assertContextMenuDisabled(dbg, item.selector, false);
    await closeContextMenu(dbg, popup);
  }
}

/**
 * Asserts that the source tree blackbox context menu items which are visible are correct
 * @params {Object} dbg
 * @params {Array} testFixtures
 *                 Details needed for the assertion. Any blackboxed/nonBlackboxed sources
 */
async function assertSourceTreeBlackBoxBoxContextMenuItems(dbg, testFixtures) {
  const { blackBoxedSourceTreeNode, nonBlackBoxedSourceTreeNode } =
    testFixtures;
  if (blackBoxedSourceTreeNode) {
    info(
      "Asserts that the source tree blackbox context menu items when clicking on a blackboxed source tree node"
    );
    rightClickEl(dbg, blackBoxedSourceTreeNode);
    const popup = await waitForContextMenu(dbg);
    const item = contextMenuItems.unignoreSource;
    await assertContextMenuLabel(dbg, item.selector, item.label);
    await closeContextMenu(dbg, popup);
  }

  if (nonBlackBoxedSourceTreeNode) {
    info(
      "Asserts that the source tree blackbox context menu items when clicking on an un-blackboxed sorce tree node"
    );
    rightClickEl(dbg, nonBlackBoxedSourceTreeNode);
    const popup = await waitForContextMenu(dbg);
    const _item = contextMenuItems.ignoreSource;
    await assertContextMenuLabel(dbg, _item.selector, _item.label);
    await closeContextMenu(dbg, popup);
  }
}

/**
 * Asserts that the editor blackbox context menu items which are visible are correct
 * @params {Object} dbg
 * @params {Array} testFixtures
 *                 Details needed for the assertion. Any blackboxed/nonBlackboxed lines
 *                 and any blackboxed/nonBlackboxed sources
 */
async function assertEditorBlackBoxBoxContextMenuItems(dbg, testFixtures) {
  const {
    blackboxedLine,
    nonBlackBoxedLine,
    blackBoxedLines,
    nonBlackBoxedLines,
    blackboxedSourceState,
  } = testFixtures;

  if (blackboxedLine) {
    info(
      "Asserts the editor blackbox context menu items when right-clicking on a single blackboxed line"
    );
    const popup = await selectEditorLinesAndOpenContextMenu(dbg, {
      startLine: blackboxedLine,
    });

    const expectedContextMenuItems = [contextMenuItems.unignoreSource];

    if (blackboxedSourceState !== SOURCE_IS_FULLY_IGNORED) {
      expectedContextMenuItems.push(contextMenuItems.unignoreLine);
    }

    for (const expectedContextMenuItem of expectedContextMenuItems) {
      info(
        "Checking context menu item " +
          expectedContextMenuItem.selector +
          " with label " +
          expectedContextMenuItem.label
      );
      await assertContextMenuLabel(
        dbg,
        expectedContextMenuItem.selector,
        expectedContextMenuItem.label
      );
    }
    await closeContextMenu(dbg, popup);
  }

  if (nonBlackBoxedLine) {
    info(
      "Asserts the editor blackbox context menu items when right-clicking on a single non-blackboxed line"
    );
    const popup = await selectEditorLinesAndOpenContextMenu(dbg, {
      startLine: nonBlackBoxedLine,
    });

    const expectedContextMenuItems = [
      blackboxedSourceState == SOURCE_IS_NOT_IGNORED
        ? contextMenuItems.ignoreSource
        : contextMenuItems.unignoreSource,
      contextMenuItems.ignoreLine,
    ];

    for (const expectedContextMenuItem of expectedContextMenuItems) {
      info(
        "Checking context menu item " +
          expectedContextMenuItem.selector +
          " with label " +
          expectedContextMenuItem.label
      );
      await assertContextMenuLabel(
        dbg,
        expectedContextMenuItem.selector,
        expectedContextMenuItem.label
      );
    }
    await closeContextMenu(dbg, popup);
  }

  if (blackBoxedLines) {
    info(
      "Asserts the editor blackbox context menu items when right-clicking on multiple blackboxed lines"
    );
    const popup = await selectEditorLinesAndOpenContextMenu(dbg, {
      startLine: blackBoxedLines[0],
      endLine: blackBoxedLines[1],
    });

    const expectedContextMenuItems = [contextMenuItems.unignoreSource];

    if (blackboxedSourceState !== SOURCE_IS_FULLY_IGNORED) {
      expectedContextMenuItems.push(contextMenuItems.unignoreLines);
    }

    for (const expectedContextMenuItem of expectedContextMenuItems) {
      info(
        "Checking context menu item " +
          expectedContextMenuItem.selector +
          " with label " +
          expectedContextMenuItem.label
      );
      await assertContextMenuLabel(
        dbg,
        expectedContextMenuItem.selector,
        expectedContextMenuItem.label
      );
    }
    await closeContextMenu(dbg, popup);
  }

  if (nonBlackBoxedLines) {
    info(
      "Asserts the editor blackbox context menu items when right-clicking on multiple non-blackboxed lines"
    );
    const popup = await selectEditorLinesAndOpenContextMenu(dbg, {
      startLine: nonBlackBoxedLines[0],
      endLine: nonBlackBoxedLines[1],
    });

    const expectedContextMenuItems = [
      blackboxedSourceState == SOURCE_IS_NOT_IGNORED
        ? contextMenuItems.ignoreSource
        : contextMenuItems.unignoreSource,
    ];

    if (blackboxedSourceState !== SOURCE_IS_FULLY_IGNORED) {
      expectedContextMenuItems.push(contextMenuItems.ignoreLines);
    }

    for (const expectedContextMenuItem of expectedContextMenuItems) {
      info(
        "Checking context menu item " +
          expectedContextMenuItem.selector +
          " with label " +
          expectedContextMenuItem.label
      );
      await assertContextMenuLabel(
        dbg,
        expectedContextMenuItem.selector,
        expectedContextMenuItem.label
      );
    }
    await closeContextMenu(dbg, popup);
  }
}

/**
 * Selects a range of lines
 * @param {Object} dbg
 * @param {Number} startLine
 * @param {Number} endLine
 */
function selectEditorLines(dbg, startLine, endLine) {
  getCM(dbg).setSelection(
    { line: startLine - 1, ch: 0 },
    { line: endLine, ch: 0 }
  );
}
