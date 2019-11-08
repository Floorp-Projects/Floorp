/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../../../test/head.js */

// Import the inspector's head.js first (which itself imports shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/head.js",
  this
);

const {
  toCamelCase,
} = require("devtools/client/inspector/compatibility/utils/cases");

async function openCompatibilityView() {
  info("Open the compatibility view");
  await pushPref("devtools.inspector.compatibility.enabled", true);

  const { inspector } = await openInspectorSidebarTab("compatibilityview");
  const panel = inspector.panelDoc.querySelector(
    "#compatibilityview-panel .inspector-tabpanel"
  );
  return { inspector, panel };
}

/**
 * Check whether the content of issue item element is matched with the expected values.
 *
 * @param {Element} panel
 *        The Compatibility panel container element
 * @param {Array} expectedIssues
 *        Array of the issue expected.
 *        For the structure of issue items, see types.js.
 */
async function assertIssueList(panel, expectedIssues) {
  info("Check the number of issues");
  await waitUntil(
    () =>
      panel.querySelectorAll("[data-qa-property]").length ===
      expectedIssues.length
  );
  ok(true, "The number of issues is correct");

  if (expectedIssues.length === 0) {
    // No issue.
    return;
  }

  const issueEls = panel.querySelectorAll("[data-qa-property]");

  for (let i = 0; i < expectedIssues.length; i++) {
    info(`Check an element at index[${i}]`);
    const issueEl = issueEls[i];
    const expectedIssue = expectedIssues[i];

    for (const [key, value] of Object.entries(expectedIssue)) {
      const datasetKey = toCamelCase(`qa-${key}`);
      is(
        issueEl.dataset[datasetKey],
        JSON.stringify(value),
        `The value of ${datasetKey} is correct`
      );
    }
  }
}
