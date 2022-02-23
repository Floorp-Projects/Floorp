/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const FILE_FOLDER = `browser/devtools/client/webconsole/test/browser`;
const TEST_URI = `https://example.com/${FILE_FOLDER}/test-console-evaluation-context-selector.html`;
const IFRAME_PATH = `${FILE_FOLDER}/test-console-evaluation-context-selector-child.html`;

// Test that when a target is destroyed, it does not appear in the list anymore (and
// the context is set to the top one if the destroyed target was selected).

add_task(async function() {
  await pushPref("devtools.webconsole.input.context", true);

  const hud = await openNewTabWithIframesAndConsole(TEST_URI, [
    `https://example.net/${IFRAME_PATH}?id=iframe-1`,
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

  is(
    evaluationContextSelectorButton.innerText,
    "Top",
    "The button has the expected 'Top' text"
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
    label: "iframe-1|example.net",
    tooltip: `https://example.net/${IFRAME_PATH}?id=iframe-1`,
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
  ]);

  info("Add another iframe");
  ContentTask.spawn(gBrowser.selectedBrowser, [IFRAME_PATH], function(path) {
    const iframe = content.document.createElement("iframe");
    iframe.src = `https://test1.example.org/${path}?id=iframe-2`;
    content.document.body.append(iframe);
  });

  // Wait until the new iframe is rendered in the context selector.
  await waitFor(() => {
    const items = getContextSelectorItems(hud);
    return (
      items.length === 4 &&
      items.some(el =>
        el
          .querySelector(".label")
          ?.textContent.includes("iframe-2|test1.example.org")
      )
    );
  });

  const expectedSecondIframeItem = {
    label: `iframe-2|test1.example.org`,
    tooltip: `https://test1.example.org/${IFRAME_PATH}?id=iframe-2`,
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
    evaluationContextSelectorButton.innerText.includes("example.net")
  );
  await waitForEagerEvaluationResult(hud, `"example.net"`);
  ok(true, "The context was set to the selected iframe document");

  info("Remove the first iframe from the content document");
  ContentTask.spawn(gBrowser.selectedBrowser, [], function() {
    content.document.querySelector("iframe").remove();
  });

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("Top")
  );
  ok(
    true,
    "The context was set to Top frame after the selected iframe was removed"
  );
  await waitForEagerEvaluationResult(hud, `"example.com"`);
  ok(true, "Instant evaluation is done against the top frame context");

  await checkContextSelectorMenu(hud, [
    {
      ...expectedTopItem,
      checked: true,
    },
    expectedSeparatorItem,
    {
      ...expectedSecondIframeItem,
      checked: false,
    },
  ]);

  info("Select the remaining iframe");
  selectTargetInContextSelector(hud, expectedSecondIframeItem.label);

  await waitFor(() =>
    evaluationContextSelectorButton.innerText.includes("test1.example.org")
  );
  await waitForEagerEvaluationResult(hud, `"test1.example.org"`);
  ok(true, "The context was set to the selected iframe document");

  info("Remove the second iframe from the content document");
  ContentTask.spawn(gBrowser.selectedBrowser, [], function() {
    content.document.querySelector("iframe").remove();
  });

  await waitFor(
    () =>
      !hud.ui.outputNode.querySelector(".webconsole-evaluation-selector-button")
  );
  ok(
    true,
    "The evaluation context selector is hidden after last iframe was removed"
  );

  await waitForEagerEvaluationResult(hud, `"example.com"`);
  ok(true, "Instant evaluation is done against the top frame context");
});
