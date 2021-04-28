/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../debugger/test/mochitest/helpers.js */
/* import-globals-from ../../debugger/test/mochitest/helpers/context.js */

// The test can take a while to run
requestLongerTimeout(2);

const FILENAME = "doc_backward_forward_navigation.html";
const TEST_URI_ORG = `${URL_ROOT_ORG}${FILENAME}`;
const TEST_URI_COM = `${URL_ROOT_COM}${FILENAME}`;

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/helpers.js",
  this
);

add_task(async function() {
  // Start with a page without much activity so the toolbox gets initialized safely
  const tab = await addTab(`data:text/html,<meta charset=utf8>`);

  // Select the debugger so there will be more activity
  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const inspector = await toolbox.selectTool("inspector");

  info("Navigate to the ORG test page");
  await navigateTo(TEST_URI_ORG);

  info("And then navigate to a different origin");
  await navigateTo(TEST_URI_COM);

  info(
    "Navigate backward and forward multiple times between the two origins, with different delays"
  );
  await navigateBackAndForth(TEST_URI_ORG, TEST_URI_COM);

  // Navigate one last time to a document with less activity so we don't have to deal
  // with pending promises when we destroy the toolbox
  const onInspectorReloaded = inspector.once("reloaded");
  await navigateTo(`${TEST_URI_ORG}?no-mutation`);
  await onInspectorReloaded;
  await checkToolboxState(toolbox);
});

add_task(async function() {
  const DATA_URL = `data:text/html,<meta charset=utf8>`;
  const tab = await addTab(DATA_URL);

  // Select the debugger so there will be more activity
  const toolbox = await openToolboxForTab(tab, "jsdebugger");
  const inspector = await toolbox.selectTool("inspector");

  info("Navigate to a different origin");
  await navigateTo(TEST_URI_COM);

  info("Then navigate back, and forth immediatly");
  // We can't call goBack and right away goForward as goForward and even the call to navigateTo
  // a bit later might be ignored. So we wait at least for the location to change.
  await safelyGoBack(DATA_URL);
  await safelyGoForward(TEST_URI_COM);

  // Navigate one last time to a document with less activity so we don't have to deal
  // with pending promises when we destroy the toolbox
  const onInspectorReloaded = inspector.once("reloaded");
  await navigateTo(`${TEST_URI_ORG}?no-mutation`);
  await onInspectorReloaded;
  await checkToolboxState(toolbox);
});

async function checkToolboxState(toolbox) {
  info("Check that the toolbox toolbar is still visible");
  const toolboxTabsEl = toolbox.doc.querySelector(".toolbox-tabs");
  ok(toolboxTabsEl, "Toolbar is still visible");

  info(
    "Check that the markup view is rendered correctly and elements can be selected"
  );
  const inspector = await toolbox.selectTool("inspector");
  const h1NodeFront = await getNodeFront("ul.logs", inspector);
  ok(h1NodeFront, "the markup view is still rendered fine");
  await selectNode("ul.logs", inspector);

  info("Check that the debugger has some sources");
  const dbgPanel = await toolbox.selectTool("jsdebugger");
  const dbg = createDebuggerContext(toolbox);

  info(`Wait for ${FILENAME} to be displayed in the debugger source panel`);
  const rootNode = await waitFor(() =>
    dbgPanel.panelWin.document.querySelector(selectors.sourceTreeRootNode)
  );
  await expandAllSourceNodes(dbg, rootNode);
  const sourcesTreeScriptNode = await waitFor(() =>
    findSourceNodeWithText(dbg, FILENAME)
  );

  ok(
    sourcesTreeScriptNode.innerText.includes(FILENAME),
    "The debugger has the expected source"
  );
}

async function navigateBackAndForth(
  expectedUrlAfterBackwardNavigation,
  expectedUrlAfterForwardNavigation
) {
  const delays = [100, 0, 500];
  for (const delay of delays) {
    // For each delays, do 3 back/forth navigations
    for (let i = 0; i < 3; i++) {
      await safelyGoBack(expectedUrlAfterBackwardNavigation);
      await wait(delay);
      await safelyGoForward(expectedUrlAfterForwardNavigation);
      await wait(delay);
    }
  }
}

async function safelyGoBack(expectedUrl) {
  const onLocationChange = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    expectedUrl
  );
  gBrowser.goBack();
  await onLocationChange;
}

async function safelyGoForward(expectedUrl) {
  const onLocationChange = BrowserTestUtils.waitForLocationChange(
    gBrowser,
    expectedUrl
  );
  gBrowser.goForward();
  await onLocationChange;
}
