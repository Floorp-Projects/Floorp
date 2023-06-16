/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Test that the evaluation context selector reacts as expected when performing some
// inspector actions (selecting a node, "use in console" context menu entry, …).

const FILE_FOLDER = `browser/devtools/client/webconsole/test/browser`;
const TEST_URI = `https://example.com/${FILE_FOLDER}/test-console-evaluation-context-selector.html`;
const IFRAME_PATH = `${FILE_FOLDER}/test-console-evaluation-context-selector-child.html`;

// Import helpers for the inspector
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

requestLongerTimeout(2);

add_task(async function () {
  await pushPref("devtools.webconsole.input.context", true);

  const hud = await openNewTabWithIframesAndConsole(TEST_URI, [
    `https://example.org/${IFRAME_PATH}?id=iframe-1`,
    `https://example.net/${IFRAME_PATH}?id=iframe-2`,
  ]);

  const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
    ".webconsole-evaluation-selector-button"
  );

  if (!isFissionEnabled() && !isEveryFrameTargetEnabled()) {
    is(
      evaluationContextSelectorButton,
      null,
      "context selector is only displayed when Fission or EFT is enabled"
    );
    return;
  }

  setInputValue(hud, "document.location.host");
  await waitForEagerEvaluationResult(hud, `"example.com"`);

  info("Go to the inspector panel");
  const inspector = await hud.toolbox.selectTool("inspector");

  info("Expand all the nodes");
  await inspector.markup.expandAll();

  info("Open the split console");
  await hud.toolbox.openSplitConsole();

  info("Select the first iframe h2 element");
  await selectNodeInFrames([".iframe-1", "h2"], inspector);

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.org")
  );
  ok(true, "The context was set to the selected iframe document");

  await waitForEagerEvaluationResult(hud, `"example.org"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  info("Select the top document via the context selector");
  // This should take the lead over the currently selected element in the inspector
  selectTargetInContextSelector(hud, "Top");

  await waitForEagerEvaluationResult(hud, `"example.com"`);
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );

  info("Select the second iframe h2 element");
  await selectNodeInFrames([".iframe-2", "h2"], inspector);

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.net")
  );
  ok(true, "The context was set to the selected iframe document");

  await waitForEagerEvaluationResult(hud, `"example.net"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  info("Select an element in the top document");
  await selectNodeInFrames(["h1"], inspector);

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
    [".iframe-1", "h2"],
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
    [".iframe-2", "h2"],
    "temp0",
    `<h2 id="iframe-2">`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.net")
  );
  ok(true, "The context selector was updated");

  info(
    "Check that 'Use in console' works as expected for element in the top frame"
  );
  await testUseInConsole(
    hud,
    inspector,
    ["h1"],
    "temp0",
    `<h1 id="top-level">`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );
  ok(true, "The context selector was updated");
});

async function testUseInConsole(
  hud,
  inspector,
  selectors,
  variableName,
  expectedTextResult
) {
  const nodeFront = await selectNodeInFrames(selectors, inspector);
  const container = inspector.markup.getContainer(nodeFront);

  // Clear the input before clicking on "Use in Console" to workaround an bug
  // with eager-evaluation, which will be skipped if the console input didn't
  // change. See https://bugzilla.mozilla.org/show_bug.cgi?id=1668916#c1.
  // TODO: Should be removed when Bug 1669151 is fixed.
  setInputValue(hud, "");
  // Also need to wait in order to avoid batching.
  await wait(100);

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

  await executeAndWaitForResultMessage(hud, variableName, expectedTextResult);
  ok(true, "the expected variable was created with the expected value.");
}
