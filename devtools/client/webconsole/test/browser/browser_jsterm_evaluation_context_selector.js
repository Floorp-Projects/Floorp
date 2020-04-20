/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FILE_FOLDER = `browser/devtools/client/webconsole/test/browser`;
const TEST_URI = `http://example.com/${FILE_FOLDER}/test-console-evaluation-context-selector.html`;
const IFRAME_PATH = `${FILE_FOLDER}/test-console-evaluation-context-selector-child.html`;

requestLongerTimeout(2);

add_task(async function() {
  await pushPref("devtools.contenttoolbox.fission", true);
  await pushPref("devtools.contenttoolbox.webconsole.input.context", true);

  await addTab(TEST_URI);

  info("Create new iframes and add them to the page.");
  await ContentTask.spawn(gBrowser.selectedBrowser, IFRAME_PATH, async function(
    iframPath
  ) {
    const iframe1 = content.document.createElement("iframe");
    const iframe2 = content.document.createElement("iframe");
    iframe1.classList.add("iframe-1");
    iframe2.classList.add("iframe-2");
    content.document.body.append(iframe1, iframe2);

    const onLoadIframe1 = new Promise(resolve => {
      iframe1.addEventListener("load", resolve, { once: true });
    });
    const onLoadIframe2 = new Promise(resolve => {
      iframe2.addEventListener("load", resolve, { once: true });
    });

    iframe1.src = `http://example.org/${iframPath}?id=iframe-1`;
    iframe2.src = `http://mochi.test:8888/${iframPath}?id=iframe-2`;

    await Promise.all([onLoadIframe1, onLoadIframe2]);
  });

  const hud = await openConsole();

  const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
    ".webconsole-evaluation-selector-button"
  );

  ok(
    evaluationContextSelectorButton,
    "The evaluation context selector is visible"
  );
  is(
    evaluationContextSelectorButton.innerText,
    "Top",
    "The button has the expected 'Top' text"
  );
  is(
    evaluationContextSelectorButton.classList.contains(
      "webconsole-evaluation-selector-button-non-top"
    ),
    false,
    "The non-top class isn't applied"
  );

  const topLevelDocumentMessage = await executeAndWaitForMessage(
    hud,
    "document.location",
    "example.com",
    ".result"
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
  is(
    evaluationContextSelectorButton.classList.contains(
      "webconsole-evaluation-selector-button-non-top"
    ),
    true,
    "The non-top class is applied"
  );

  await waitForEagerEvaluationResult(hud, `"example.org"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  const iframe1DocumentMessage = await executeAndWaitForMessage(
    hud,
    "document.location",
    "example.org",
    ".result"
  );
  setInputValue(hud, "document.location.host");

  info("Select the second iframe h2 element");
  await selectIframeContentElement(inspector, ".iframe-2", "h2");

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("mochi.test")
  );
  ok(true, "The context was set to the selected iframe document");
  is(
    evaluationContextSelectorButton.classList.contains(
      "webconsole-evaluation-selector-button-non-top"
    ),
    true,
    "The non-top class is applied"
  );

  await waitForEagerEvaluationResult(hud, `"mochi.test:8888"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  const iframe2DocumentMessage = await executeAndWaitForMessage(
    hud,
    "document.location",
    "mochi.test",
    ".result"
  );
  setInputValue(hud, "document.location.host");

  info("Select an element in the top document");
  const h1NodeFront = await inspector.walker.findNodeFront(["h1"]);
  inspector.selection.setNodeFront(null);
  inspector.selection.setNodeFront(h1NodeFront);

  await waitForEagerEvaluationResult(hud, `"example.com"`);
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );
  is(
    evaluationContextSelectorButton.classList.contains(
      "webconsole-evaluation-selector-button-non-top"
    ),
    false,
    "The non-top class isn't applied"
  );

  await hud.toolbox.selectTool("webconsole");
  info("Check that 'Store as global variable' selects the right context");
  await testStoreAsGlobalVariable(
    hud,
    iframe1DocumentMessage,
    "temp0",
    "example.org"
  );
  await waitForEagerEvaluationResult(
    hud,
    `Location http://example.org/${IFRAME_PATH}?id=iframe-1`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.org")
  );
  ok(true, "The context was set to the selected iframe document");

  await testStoreAsGlobalVariable(
    hud,
    iframe2DocumentMessage,
    "temp0",
    "mochi.test:8888"
  );
  await waitForEagerEvaluationResult(
    hud,
    `Location http://mochi.test:8888/${IFRAME_PATH}?id=iframe-2`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("mochi.test")
  );
  ok(true, "The context was set to the selected iframe document");

  await testStoreAsGlobalVariable(
    hud,
    topLevelDocumentMessage,
    "temp0",
    "example.com"
  );
  await waitForEagerEvaluationResult(hud, `Location ${TEST_URI}`);
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );
  ok(true, "The context was set to the top document");

  info("Open the inspector again");
  await hud.toolbox.selectTool("inspector");

  info(
    "Check that 'Use in console' works as expected for element in the first iframe"
  );
  await testUseInConsole(
    hud,
    inspector,
    ".iframe-1",
    "h2",
    "temp1",
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
    "temp1",
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
    "temp1",
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

async function testStoreAsGlobalVariable(
  hud,
  msg,
  variableName,
  expectedTextResult
) {
  const menuPopup = await openContextMenu(
    hud,
    msg.node.querySelector(".objectBox")
  );
  const storeMenuItem = menuPopup.querySelector("#console-menu-store");
  const onceInputSet = hud.jsterm.once("set-input-value");
  storeMenuItem.click();

  info("Wait for console input to be updated with the temp variable");
  await onceInputSet;

  info("Wait for context menu to be hidden");
  await hideContextMenu(hud);

  is(getInputValue(hud), variableName, "Input was set");

  await executeAndWaitForMessage(
    hud,
    `${variableName}`,
    expectedTextResult,
    ".result"
  );
  ok(true, "Correct variable assigned into console.");
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
