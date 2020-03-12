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
  COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE,
} = require("devtools/client/inspector/compatibility/actions/index");

const {
  toCamelCase,
} = require("devtools/client/inspector/compatibility/utils/cases");

async function openCompatibilityView() {
  info("Open the compatibility view");
  await pushPref("devtools.inspector.compatibility.enabled", true);

  const { inspector } = await openInspectorSidebarTab("compatibilityview");
  await waitForUpdateSelectedNodeAction(inspector.store);
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

/**
 * Return a promise which waits for COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE action.
 *
 * @param {Object} store
 * @return {Promise}
 */
function waitForUpdateSelectedNodeAction(store) {
  return waitForDispatch(store, COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE);
}

/**
 * Return a promise which waits for given action type.
 *
 * @param {Object} store
 * @param {Object} type
 * @return {Promise}
 */
function waitForDispatch(store, type) {
  return new Promise(resolve => {
    store.dispatch({
      type: "@@service/waitUntil",
      predicate: action => action.type === type,
      run: (dispatch, getState, action) => {
        resolve(action);
      },
    });
  });
}
