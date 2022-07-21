/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FILE_FOLDER = `browser/devtools/client/webconsole/test/browser`;
const TEST_URI = `https://example.com/${FILE_FOLDER}/test-console-evaluation-context-selector.html`;
const IFRAME_PATH = `${FILE_FOLDER}/test-console-evaluation-context-selector-child.html`;

requestLongerTimeout(2);

add_task(async function() {
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
    evaluationContextSelectorButton.classList.contains("checked"),
    false,
    "The checked class isn't applied"
  );

  const topLevelDocumentMessage = await executeAndWaitForResultMessage(
    hud,
    "document.location",
    "example.com"
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
    tooltip: `https://example.org/${IFRAME_PATH}?id=iframe-1`,
  };
  const expectedSecondIframeItem = {
    label: "iframe-2|example.net",
    tooltip: `https://example.net/${IFRAME_PATH}?id=iframe-2`,
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
    evaluationContextSelectorButton.classList.contains("checked"),
    true,
    "The checked class is applied"
  );

  await waitForEagerEvaluationResult(hud, `"example.org"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  const iframe1DocumentMessage = await executeAndWaitForResultMessage(
    hud,
    "document.location",
    "example.org"
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
    evaluationContextSelectorButton.innerText.includes("example.net")
  );
  ok(true, "The context was set to the selected iframe document");
  is(
    evaluationContextSelectorButton.classList.contains("checked"),
    true,
    "The checked class is applied"
  );

  await waitForEagerEvaluationResult(hud, `"example.net"`);
  ok(true, "The instant evaluation result is updated in the iframe context");

  const iframe2DocumentMessage = await executeAndWaitForResultMessage(
    hud,
    "document.location",
    "example.net"
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
    evaluationContextSelectorButton.classList.contains("checked"),
    false,
    "The checked class isn't applied"
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
    `Location https://example.org/${IFRAME_PATH}?id=iframe-1`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.org")
  );
  ok(true, "The context was set to the selected iframe document");

  await testStoreAsGlobalVariable(
    hud,
    iframe2DocumentMessage,
    "temp0",
    "example.net"
  );
  await waitForEagerEvaluationResult(
    hud,
    `Location https://example.net/${IFRAME_PATH}?id=iframe-2`
  );
  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("example.net")
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

  await executeAndWaitForResultMessage(
    hud,
    `${variableName}`,
    expectedTextResult
  );
  ok(true, "Correct variable assigned into console.");
}
