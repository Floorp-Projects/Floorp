/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the evaluation context selector reacts as expected when performing some
// inspector actions (selecting a node, "use in console" context menu entry, …).

const FILE_FOLDER = `browser/devtools/client/webconsole/test/browser`;
const TEST_URI = `http://example.com/${FILE_FOLDER}/test-console-evaluation-context-selector.html`;
const IFRAME_PATH = `${FILE_FOLDER}/test-console-evaluation-context-selector-child.html`;

requestLongerTimeout(2);

add_task(async function() {
  await pushPref("devtools.contenttoolbox.fission", true);
  await pushPref("devtools.contenttoolbox.webconsole.input.context", true);

  const hud = await openNewTabWithIframesAndConsole(TEST_URI, [
    `http://example.org/${IFRAME_PATH}?id=iframe-1`,
    `http://mochi.test:8888/${IFRAME_PATH}?id=iframe-2`,
  ]);

  const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
    ".webconsole-evaluation-selector-button"
  );

  setInputValue(hud, "document.location.host");
  await waitForEagerEvaluationResult(hud, `"example.com"`);

  info("Go to the inspector panel");
  const inspector = await openInspector();

  info("Expand all the nodes");
  await inspector.markup.expandAll();

  info("Open the split console");
  await hud.toolbox.openSplitConsole();

  info("Select the first iframe h2 element");
  await selectIframeContentElement(inspector, ".iframe-1", "h2");

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.org")
  );
  ok(true, "The context was set to the selected iframe document");

  await waitForEagerEvaluationResult(hud, `"example.org"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  info("Select the second iframe h2 element");
  await selectIframeContentElement(inspector, ".iframe-2", "h2");

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("mochi.test")
  );
  ok(true, "The context was set to the selected iframe document");

  await waitForEagerEvaluationResult(hud, `"mochi.test:8888"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  info("Select an element in the top document");
  const h1NodeFront = await inspector.walker.findNodeFront(["h1"]);
  inspector.selection.setNodeFront(null);
  inspector.selection.setNodeFront(h1NodeFront);

  await waitForEagerEvaluationResult(hud, `"example.com"`);
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );

  info(
    "Check that 'Use in console' works as expected for element in the first iframe"
  );
  await testUseInConsole(
    hud,
    inspector,
    ".iframe-1",
    "h2",
    "temp0",
    `<h2 id="iframe-1">`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.org")
  );
  ok(true, "The context selector was updated");

  info(
    "Check that 'Use in console' works as expected for element in the second iframe"
  );
  await testUseInConsole(
    hud,
    inspector,
    ".iframe-2",
    "h2",
    "temp0",
    `<h2 id="iframe-2">`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("mochi.test:8888")
  );
  ok(true, "The context selector was updated");

  info(
    "Check that 'Use in console' works as expected for element in the top frame"
  );
  await testUseInConsole(
    hud,
    inspector,
    ":root",
    "h1",
    "temp0",
    `<h1 id="top-level">`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );
  ok(true, "The context selector was updated");
});

async function selectIframeContentElement(
  inspector,
  iframeSelector,
  iframeContentSelector
) {
  inspector.selection.setNodeFront(null);
  const iframeNodeFront = await inspector.walker.findNodeFront([
    iframeSelector,
  ]);
  const childrenNodeFront = await iframeNodeFront
    .treeChildren()[0]
    .walkerFront.findNodeFront([iframeContentSelector]);
  inspector.selection.setNodeFront(childrenNodeFront);
  return childrenNodeFront;
}

async function testUseInConsole(
  hud,
  inspector,
  iframeSelector,
  iframeContentSelector,
  variableName,
  expectedTextResult
) {
  const nodeFront = await selectIframeContentElement(
    inspector,
    iframeSelector,
    iframeContentSelector
  );
  const container = inspector.markup.getContainer(nodeFront);

  const onConsoleReady = inspector.once("console-var-ready");
  const menu = inspector.markup.contextMenu._openMenu({
    target: container.tagLine,
  });
  const useInConsoleItem = menu.items.find(
    ({ id }) => id === "node-menu-useinconsole"
  );
  useInConsoleItem.click();
  await onConsoleReady;

  menu.clear();

  is(
    getInputValue(hud),
    variableName,
    "A variable with the expected name was created"
  );
  await waitForEagerEvaluationResult(hud, expectedTextResult);
  ok(true, "The eager evaluation display the expected result");

  await executeAndWaitForMessage(
    hud,
    variableName,
    expectedTextResult,
    ".result"
  );
  ok(true, "the expected variable was created with the expected value.");
}
