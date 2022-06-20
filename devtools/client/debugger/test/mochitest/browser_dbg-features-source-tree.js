/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * This test focuses on the SourceTree component, where we display all debuggable sources.
 *
 * The first two tests expand the tree via manual DOM events (first with clicks and second with keys).
 * `waitForSourcesInSourceTree()` is a key assertion method. Passing `{noExpand: true}`
 * is important to avoid automatically expand the source tree.
 *
 * The following tests depend on auto-expand and only assert all the sources possibly displayed
 */

"use strict";

requestLongerTimeout(5);

/* import-globals-from ../../../framework/browser-toolbox/test/helpers-browser-toolbox.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/framework/browser-toolbox/test/helpers-browser-toolbox.js",
  this
);

const testServer = createVersionizedHttpTestServer(
  "examples/sourcemaps-reload-uncompressed"
);
const TEST_URL = testServer.urlFor("index.html");

const INTEGRATION_TEST_PAGE_SOURCES = [
  "index.html",
  "iframe.html",
  "script.js",
  "onload.js",
  "test-functions.js",
  "query.js?x=1",
  "query.js?x=2",
  "bundle.js",
  "original.js",
  "bundle-with-another-original.js",
  "original-with-no-update.js",
  "replaced-bundle.js",
  "removed-original.js",
  "named-eval.js",
  // Webpack generated some extra sources:
  "bootstrap 3b1a221408fdde86aa49",
  "bootstrap a1ecee2f86e1d0ea3fb5",
  "bootstrap 6fda1f7ea9ecbc1a2d5b",
  // There is 3 occurences, one per target (main thread, worker and iframe).
  // But there is even more source actors (named evals and duplicated script tags).
  "same-url.sjs",
  "same-url.sjs",
];
// The iframe one is only available when fission is enabled, or EFT
if (isFissionEnabled() || isEveryFrameTargetEnabled()) {
  INTEGRATION_TEST_PAGE_SOURCES.push("same-url.sjs");
}

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
  dbg.toolbox.closeToolbox();
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
  dbg.toolbox.closeToolbox();
});

/**
 * Tests that the source tree works with all the various types of sources
 * coming from the integration test page.
 *
 * Also assert a few extra things on sources with query strings:
 *  - they can be pretty printed,
 *  - quick open matches them,
 *  - you can set breakpoint on them.
 */
add_task(async function testSourceTreeOnTheIntegrationTestPage() {
  // We open against a blank page and only then navigate to the test page
  // so that sources aren't GC-ed before opening the debugger.
  // When we (re)load a page while the debugger is opened, the debugger
  // will force all sources to be held in memory.
  const dbg = await initDebuggerWithAbsoluteURL("about:blank");

  await navigateToAbsoluteURL(
    dbg,
    TEST_URL,
    "index.html",
    "script.js",
    "test-functions.js",
    "query.js?x=1",
    "query.js?x=2",
    "bundle.js",
    "original.js",
    "replaced-bundle.js",
    "removed-original.js",
    "named-eval.js"
  );

  await waitForSourcesInSourceTree(dbg, INTEGRATION_TEST_PAGE_SOURCES);

  info(
    "Assert the number of sources and source actors for the same-url.sjs sources"
  );
  const mainThreadSameUrlSource = findSourceInThread(
    dbg,
    "same-url.sjs",
    "Main Thread"
  );
  ok(mainThreadSameUrlSource, "Found same-url.js in the main thread");
  is(
    dbg.selectors.getSourceActorsForSource(mainThreadSameUrlSource.id).length,
    // When EFT is disabled the iframe's source is meld into the main target
    isEveryFrameTargetEnabled() ? 3 : 4,
    "same-url.js is loaded 3 times in the main thread"
  );

  const iframeSameUrlSource = findSourceInThread(
    dbg,
    "same-url.sjs",
    testServer.urlFor("iframe.html")
  );
  if (isEveryFrameTargetEnabled()) {
    ok(iframeSameUrlSource, "Found same-url.js in the iframe thread");
    is(
      dbg.selectors.getSourceActorsForSource(iframeSameUrlSource.id).length,
      1,
      "same-url.js is loaded one time in the iframe thread"
    );
  } else {
    ok(
      !iframeSameUrlSource,
      "When EFT is off, the iframe source is into the main thread bucket"
    );
  }

  const workerSameUrlSource = findSourceInThread(
    dbg,
    "same-url.sjs",
    "same-url.sjs"
  );
  ok(workerSameUrlSource, "Found same-url.js in the worker thread");
  is(
    dbg.selectors.getSourceActorsForSource(workerSameUrlSource.id).length,
    1,
    "same-url.js is loaded one time in the worker thread"
  );

  info("Assert the content of the named eval");
  await selectSource(dbg, "named-eval.js");
  assertTextContentOnLine(dbg, 3, `console.log("named-eval");`);

  info("Assert that nameless eval don't show up in the source tree");
  invokeInTab("breakInEval");
  await waitForPaused(dbg);
  await waitForSourcesInSourceTree(dbg, INTEGRATION_TEST_PAGE_SOURCES);
  await resume(dbg);

  info("Assert the content of sources with query string");
  await selectSource(dbg, "query.js?x=1");
  const tab = findElement(dbg, "activeTab");
  is(tab.innerText, "query.js?x=1", "Tab label is query.js?x=1");
  assertTextContentOnLine(
    dbg,
    1,
    `function query() {console.log("query x=1");}`
  );
  await addBreakpoint(dbg, "query.js?x=1", 1);
  assertBreakpointHeading(dbg, "query.js?x=1", 0);

  // pretty print the source and check the tab text
  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "query.js?x=1:formatted");
  await waitForSelectedSource(dbg, "query.js?x=1:formatted");

  const prettyTab = findElement(dbg, "activeTab");
  is(prettyTab.innerText, "query.js?x=1", "Tab label is query.js?x=1");
  ok(prettyTab.querySelector(".img.prettyPrint"));
  assertBreakpointHeading(dbg, "query.js?x=1", 0);
  assertTextContentOnLine(dbg, 1, `function query() {`);
  // Note the replacements of " by ' here:
  assertTextContentOnLine(dbg, 2, `console.log('query x=1');`);

  // assert quick open works with queries
  pressKey(dbg, "quickOpen");
  type(dbg, "query.js?x");

  // There can be intermediate updates in the results,
  // so wait for the final expected value
  await waitFor(async () => {
    const resultItem = findElement(dbg, "resultItems");
    if (!resultItem) {
      return false;
    }
    return resultItem.innerText.includes("query.js?x=1");
  }, "Results include the source with the query string");
  dbg.toolbox.closeToolbox();
});

/**
 * Verify that Web Extension content scripts appear only when
 * devtools.chrome.enabled is set to true and that they get
 * automatically re-selected on page reload.
 */
add_task(async function testSourceTreeWithWebExtensionContentScript() {
  const extension = await installAndStartContentScriptExtension();

  info("Without the chrome preference, the content script doesn't show up");
  await pushPref("devtools.chrome.enabled", false);
  let dbg = await initDebugger("doc-content-script-sources.html");
  // Let some time for unexpected source to appear
  await wait(1000);
  await waitForSourcesInSourceTree(dbg, []);
  await dbg.toolbox.closeToolbox();

  info("With the chrome preference, the content script shows up");
  await pushPref("devtools.chrome.enabled", true);
  const toolbox = await openToolboxForTab(gBrowser.selectedTab, "jsdebugger");
  dbg = createDebuggerContext(toolbox);
  await waitForSourcesInSourceTree(dbg, ["content_script.js"]);
  await selectSource(dbg, "content_script.js");
  ok(
    findElementWithSelector(dbg, ".sources-list .focused"),
    "Source is focused"
  );
  for (let i = 1; i < 3; i++) {
    info(
      `Reloading tab (${i} time), the content script should always be reselected`
    );
    gBrowser.reloadTab(gBrowser.selectedTab);
    await waitForSelectedSource(dbg, "content_script.js");
    ok(
      findElementWithSelector(dbg, ".sources-list .focused"),
      "Source is focused"
    );
  }
  await dbg.toolbox.closeToolbox();

  await extension.unload();
});

// Test that the Web extension name is shown in source tree rather than
// the extensions internal UUID. This checks both the web toolbox and the
// browser toolbox.
add_task(async function testSourceTreeNamesForWebExtensions() {
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.browsertoolbox.fission", true);
  const extension = await installAndStartContentScriptExtension();

  const dbg = await initDebugger("doc-content-script-sources.html");
  await waitForSourcesInSourceTree(dbg, [], {
    noExpand: true,
  });

  is(
    getLabel(dbg, 2),
    "Test content script extension",
    "Test content script extension is labeled properly"
  );

  await dbg.toolbox.closeToolbox();
  await extension.unload();

  // Make sure the toolbox opens with the debugger selected.
  await pushPref("devtools.browsertoolbox.panel", "jsdebugger");

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({
    createDebuggerContext,
    waitUntil,
    findSourceNodeWithText,
    findAllElements,
    getSelector,
    findAllElementsWithSelector,
    assertSourceTreeNode,
  });

  await ToolboxTask.spawn(selectors, async _selectors => {
    this.selectors = _selectors;
  });

  await ToolboxTask.spawn(null, async () => {
    try {
      /* global gToolbox */
      // Wait for the debugger to finish loading.
      await gToolbox.getPanelWhenReady("jsdebugger");
      const dbgx = createDebuggerContext(gToolbox);
      let rootNodeForExtensions = null;
      await waitUntil(() => {
        rootNodeForExtensions = findSourceNodeWithText(dbgx, "extension");
        return !!rootNodeForExtensions;
      });
      // Find the root node for extensions and expand it if needed
      if (
        !!rootNodeForExtensions &&
        !rootNodeForExtensions.querySelector(".arrow.expanded")
      ) {
        rootNodeForExtensions.querySelector(".arrow").click();
      }

      // Assert that extensions are displayed in the source tree
      // with their extension name.
      await assertSourceTreeNode(dbgx, "Picture-In-Picture");
      await assertSourceTreeNode(dbgx, "Form Autofill");
    } catch (e) {
      console.log("Caught exception in spawn", e);
      throw e;
    }
  });

  await ToolboxTask.destroy();
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
 * Find and assert the source tree node with the specified text
 * exists on the source tree.
 *
 * @param {Object} dbg
 * @param {String} text The node text displayed
 */
async function assertSourceTreeNode(dbg, text) {
  let node = null;
  await waitUntil(() => {
    node = findSourceNodeWithText(dbg, text);
    return !!node;
  });
  ok(!!node, `Source tree node with text "${text}" exists`);
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
