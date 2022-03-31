/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test focuses on the SourceTree component, where we display all debuggable sources.
 *
 * The first tests expand the tree via manual DOM events.
 * `waitForSourcesInSourceTree()` is a key assertion method. Passing `{noExpand: true}`
 * is important to avoid automatically expand the source tree.
 */

"use strict";

/**
 * This test opens the SourceTree manually via click events on the nested source,
 * and then adds a source dynamically and asserts it is visible.
 */
add_task(async function testSimpleSourcesWithManualClickExpand() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );

  // Expand nodes and make sure more sources appear.
  is(getLabel(dbg, 1), "Main Thread", "Main thread is labeled properly");
  info("Before interacting with the source tree, no source are displayed");
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  info(
    "After clicking on the directory, all sources but the nested ones are displayed"
  );
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );

  await clickElement(dbg, "sourceDirectoryLabel", 4);
  info(
    "After clicking on the nested directory, the nested source is also displayed"
  );
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
    ],
    { noExpand: true }
  );

  const selected = waitForDispatch(dbg.store, "SET_SELECTED_LOCATION");
  await clickElement(dbg, "sourceNode", 5);
  await selected;
  await waitForSelectedSource(dbg, "nested-source.js");

  // Ensure the source file clicked is now focused
  await waitForElementWithSelector(dbg, ".sources-list .focused");

  const selectedSource = dbg.selectors.getSelectedSource().url;
  ok(selectedSource.includes("nested-source.js"), "nested-source is selected");
  await assertNodeIsFocused(dbg, 5);

  // Make sure new sources appear in the list.
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const script = content.document.createElement("script");
    script.src = "math.min.js";
    content.document.body.appendChild(script);
  });

  info("After adding math.min.js, we got a new source displayed");
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
      "math.min.js",
    ],
    { noExpand: true }
  );
  is(
    getSourceNodeLabel(dbg, 8),
    "math.min.js",
    "math.min.js - The dynamic script exists"
  );

  info("Assert that nested-source.js is still the selected source");
  await assertNodeIsFocused(dbg, 5);
});

/**
 * Test keyboard arrow behaviour on the SourceTree with a nested folder
 * that we manually expand/collapse via arrow keys.
 */
add_task(async function testSimpleSourcesWithManualKeyShortcutsExpand() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );

  // Before clicking on the source label, no source is displayed
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  // Right after, all sources, but the nested one are displayed
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );

  // Right key on open dir
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 3);

  // Right key on closed dir
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 4);

  // Left key on a open dir
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 4);

  // Down key on a closed dir
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 4);

  // Right key on a source
  // We are focused on the nested source and up to this point we still display only the 4 initial sources
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );
  await pressKey(dbg, "Right");
  await assertNodeIsFocused(dbg, 4);
  // Now, the nested source is also displayed
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
    ],
    { noExpand: true }
  );

  // Down key on a source
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 5);

  // Go to bottom of tree and press down key
  await pressKey(dbg, "Down");
  await pressKey(dbg, "Down");
  await assertNodeIsFocused(dbg, 6);

  // Up key on a source
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 5);

  // Left key on a source
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 4);

  // Left key on a closed dir
  // We are about to close the nested folder, the nested source is about to disappear
  await waitForSourcesInSourceTree(
    dbg,
    [
      "doc-sources.html",
      "simple1.js",
      "simple2.js",
      "long.js",
      "nested-source.js",
    ],
    { noExpand: true }
  );
  await pressKey(dbg, "Left");
  // And it disappeared
  await waitForSourcesInSourceTree(
    dbg,
    ["doc-sources.html", "simple1.js", "simple2.js", "long.js"],
    { noExpand: true }
  );
  await pressKey(dbg, "Left");
  await assertNodeIsFocused(dbg, 3);

  // Up Key at the top of the source tree
  await pressKey(dbg, "Up");
  await assertNodeIsFocused(dbg, 2);
});

/**
 * Tests that the source tree works with sources having query strings.
 *
 * Also assert a few extra things about such source:
 *  - they can be pretty printed,
 *  - quick open matches them,
 *  - you can set breakpoint on them.
 */
add_task(async function testSourceTreeWithQueryStrings() {
  const dbg = await initDebugger(
    "doc-sources-querystring.html",
    "simple1.js?x=1",
    "simple1.js?x=2"
  );

  // Expand nodes and make sure the two sources appear.
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });
  await clickElement(dbg, "sourceDirectoryLabel", 3);
  await waitForSourcesInSourceTree(dbg, ["simple1.js?x=1", "simple1.js?x=2"], {
    noExpand: true,
  });

  is(getLabel(dbg, 4), "simple1.js?x=1", "simple1.js?x=1 exists");
  is(getLabel(dbg, 5), "simple1.js?x=2", "simple1.js?x=2 exists");

  await selectSource(dbg, "simple1.js?x=1");
  const tab = findElement(dbg, "activeTab");
  is(tab.innerText, "simple1.js?x=1", "Tab label is simple1.js?x=1");
  await addBreakpoint(dbg, "simple1.js?x=1", 6);
  assertBreakpointHeading(dbg, "simple1.js?x=1", 0);

  // pretty print the source and check the tab text
  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "simple1.js?x=1:formatted");

  const prettyTab = findElement(dbg, "activeTab");
  is(prettyTab.innerText, "simple1.js?x=1", "Tab label is simple1.js?x=1");
  ok(prettyTab.querySelector(".img.prettyPrint"));
  assertBreakpointHeading(dbg, "simple1.js?x=1", 0);

  // assert quick open works with queries
  pressKey(dbg, "quickOpen");
  type(dbg, "simple1.js?x");

  const resultItems = await waitForAllElements(dbg, "resultItems");
  ok(resultItems[0].innerText.includes("simple1.js?x=1"));
});

/**
 * Make sure that named eval sources appear in the Debugger,
 * and we show proper source text content
 */
add_task(async function testSourceTreeWithNamedEval() {
  const dbg = await initDebugger(
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js"
  );

  info("Assert that all page sources appear in the source tree");
  await waitForSourcesInSourceTree(dbg, [
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js",
  ]);

  info(`>>> contentTask: evaluate evaled.js`);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.eval(
      `window.evaledFunc = function() {};
//# sourceURL=evaled.js
`
    );
  });

  info("Assert that the evaled source appear in the source tree");
  await waitForSourcesInSourceTree(dbg, [
    "doc-sources.html",
    "simple1.js",
    "simple2.js",
    "nested-source.js",
    "long.js",
    "evaled.js",
  ]);

  info("Wait for the evaled source");
  await waitForSource(dbg, "evaled.js");
  await selectSource(dbg, "evaled.js");

  assertTextContentOnLine(dbg, 1, "window.evaledFunc = function() {};");
});

/**
 * Return the text content for a given line in the Source Tree.
 *
 * @param {Object} dbg
 * @param {Number} index
 *        Line number in the source tree
 */
function getLabel(dbg, index) {
  return (
    findElement(dbg, "sourceNode", index)
      .textContent.trim()
      // There is some special whitespace character which aren't removed by trim()
      .replace(/^[\s\u200b]*/g, "")
  );
}

/**
 * Assert the location displayed in the breakpoint list, in the right sidebar.
 *
 * @param {Object} dbg
 * @param {String} label
 *        The expected displayed location
 * @param {Number} index
 *        The position of the breakpoint in the list to verify
 */
function assertBreakpointHeading(dbg, label, index) {
  const breakpointHeading = findAllElements(dbg, "breakpointHeadings")[index]
    .innerText;
  is(breakpointHeading, label, `Breakpoint heading is ${label}`);
}
