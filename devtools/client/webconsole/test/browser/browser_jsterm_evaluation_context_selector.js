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

  const hud = await openNewTabWithIframesAndConsole(TEST_URI, [
    `http://example.org/${IFRAME_PATH}?id=iframe-1`,
    `http://mochi.test:8888/${IFRAME_PATH}?id=iframe-2`,
  ]);

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

  info("Check the context selector menu");
  const expectedTopItem = {
    label: "Top",
    tooltip: TEST_URI,
  };
  const expectedSeparatorItem = { separator: true };
  const expectedFirstIframeItem = {
    label: "iframe-1|example.org",
    tooltip: `http://example.org/${IFRAME_PATH}?id=iframe-1`,
  };
  const expectedSecondIframeItem = {
    label: "iframe-2|mochi.test:8888",
    tooltip: `http://mochi.test:8888/${IFRAME_PATH}?id=iframe-2`,
  };

  await checkContextSelectorMenu(hud, [
    {
      ...expectedTopItem,
      checked: true,
    },
    expectedSeparatorItem,
    {
      ...expectedFirstIframeItem,
      checked: false,
    },
    {
      ...expectedSecondIframeItem,
      checked: false,
    },
  ]);

  info("Select the first iframe");
  selectTargetInContextSelector(hud, expectedFirstIframeItem.label);

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

  info("Select the second iframe in the context selector menu");
  await checkContextSelectorMenu(hud, [
    {
      ...expectedTopItem,
      checked: false,
    },
    expectedSeparatorItem,
    {
      ...expectedFirstIframeItem,
      checked: true,
    },
    {
      ...expectedSecondIframeItem,
      checked: false,
    },
  ]);
  selectTargetInContextSelector(hud, expectedSecondIframeItem.label);

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

  info("Select the top frame in the context selector menu");
  await checkContextSelectorMenu(hud, [
    {
      ...expectedTopItem,
      checked: false,
    },
    expectedSeparatorItem,
    {
      ...expectedFirstIframeItem,
      checked: false,
    },
    {
      ...expectedSecondIframeItem,
      checked: true,
    },
  ]);
  selectTargetInContextSelector(hud, expectedTopItem.label);

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
});

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
