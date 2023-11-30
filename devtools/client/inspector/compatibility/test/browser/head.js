/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import the rule view's head.js first (which itself imports inspector's head.js and shared-head.js).
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/rules/test/head.js",
  this
);

const {
  COMPATIBILITY_UPDATE_SELECTED_NODE_COMPLETE,
  COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

const {
  toCamelCase,
} = require("resource://devtools/client/inspector/compatibility/utils/cases.js");

async function openCompatibilityView() {
  info("Open the compatibility view");
  const { inspector } = await openInspectorSidebarTab("compatibilityview");
  await Promise.all([
    waitForUpdateSelectedNodeAction(inspector.store),
    waitForUpdateTopLevelTargetAction(inspector.store),
  ]);
  const panel = inspector.panelDoc.querySelector(
    "#compatibilityview-panel .inspector-tabpanel"
  );

  const selectedElementPane = panel.querySelector(
    "#compatibility-app--selected-element-pane"
  );

  const allElementsPane = panel.querySelector(
    "#compatibility-app--all-elements-pane"
  );

  return { allElementsPane, inspector, panel, selectedElementPane };
}

/**
 * Check whether the content of issue item element is matched with the expected values.
 *
 * @param {Element} panel
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

  const getFluentString = await getFluentStringHelper([
    "devtools/client/compatibility.ftl",
  ]);

  for (const expectedIssue of expectedIssues) {
    const property = expectedIssue.property;
    info(`Check an element for ${property}`);
    const issueEl = getIssueItem(property, panel);
    ok(issueEl, `Issue element for the ${property} is in the panel`);

    if (expectedIssue.unsupportedBrowsers) {
      // We only display a single icon per unsupported browser, so we need to
      // group the expected unsupported browsers (versions) by their browser id.
      const expectedUnsupportedBrowsersById = new Map();
      for (const unsupportedBrowser of expectedIssue.unsupportedBrowsers) {
        if (!expectedUnsupportedBrowsersById.has(unsupportedBrowser.id)) {
          expectedUnsupportedBrowsersById.set(unsupportedBrowser.id, []);
        }
        expectedUnsupportedBrowsersById
          .get(unsupportedBrowser.id)
          .push(unsupportedBrowser);
      }

      const unsupportedBrowserListEl = issueEl.querySelector(
        ".compatibility-unsupported-browser-list"
      );
      const unsupportedBrowsersEl =
        unsupportedBrowserListEl.querySelectorAll("li");

      is(
        unsupportedBrowsersEl.length,
        expectedUnsupportedBrowsersById.size,
        "The expected number of browser icons are displayed"
      );

      for (const unsupportedBrowserEl of unsupportedBrowsersEl) {
        const expectedUnsupportedBrowsers = expectedUnsupportedBrowsersById.get(
          unsupportedBrowserEl.getAttribute("data-browser-id")
        );

        ok(expectedUnsupportedBrowsers, "The expected browser is displayed");
        // debugger;
        is(
          unsupportedBrowserEl.querySelector(".compatibility-browser-version")
            .innerText,
          // If esr is not supported, but a newest version isn't as well, we don't display
          // the esr version number
          (
            expectedUnsupportedBrowsers.find(
              ({ status }) => status !== "esr"
            ) || expectedUnsupportedBrowsers[0]
          ).version,
          "The expected browser version is displayed"
        );

        is(
          unsupportedBrowserEl.getAttribute("title"),
          getFluentString("compatibility-issue-browsers-list", "title", {
            browsers: expectedUnsupportedBrowsers
              .map(
                ({ name, status, version }) =>
                  `${name} ${version}${status ? ` (${status})` : ""}`
              )
              .join("\n"),
          }),
          "The brower item has the expected title attribute"
        );
      }
    }

    for (const [key, value] of Object.entries(expectedIssue)) {
      const datasetKey = toCamelCase(`qa-${key}`);
      is(
        issueEl.dataset[datasetKey],
        JSON.stringify(value),
        `The value of ${datasetKey} is correct`
      );
    }

    const propertyEl = issueEl.querySelector(
      ".compatibility-issue-item__property"
    );
    const MDN_CLASSNAME = "compatibility-issue-item__mdn-link";
    const SPEC_CLASSNAME = "compatibility-issue-item__spec-link";

    is(
      propertyEl.textContent,
      property,
      "property name is displayed as expected"
    );

    is(
      propertyEl.classList.contains(MDN_CLASSNAME),
      !!expectedIssue.url,
      `${property} element ${
        expectedIssue.url ? "has" : "does not have"
      } mdn link class`
    );
    is(
      propertyEl.classList.contains(SPEC_CLASSNAME),
      !!expectedIssue.specUrl,
      `${property} element ${
        expectedIssue.specUrl ? "has" : "does not have"
      } spec link class`
    );

    if (expectedIssue.url || expectedIssue.specUrl) {
      is(
        propertyEl.nodeName.toLowerCase(),
        "a",
        `Link rendered for ${property}`
      );

      const expectedUrl = expectedIssue.url
        ? expectedIssue.url +
          "?utm_source=devtools&utm_medium=inspector-compatibility&utm_campaign=default"
        : expectedIssue.specUrl;
      const { link } = await simulateLinkClick(propertyEl);
      is(
        link,
        expectedUrl,
        `Click on ${property} link navigates user to expected url`
      );
    } else {
      is(
        propertyEl.nodeName.toLowerCase(),
        "span",
        `No link rendered for ${property}`
      );

      const { link } = await simulateLinkClick(propertyEl);
      is(link, null, `Click on ${property} does not navigate`);
    }
  }
}

/**
 * Check whether the content of node item element is matched with the expected values.
 *
 * @param {Element} panel
 * @param {Array} expectedNodes
 *        e.g.
 *        [{ property: "margin-inline-end", nodes: ["body", "div.classname"] },...]
 */
async function assertNodeList(panel, expectedNodes) {
  for (const { property, nodes } of expectedNodes) {
    info(`Check nodes for ${property}`);
    const issueEl = getIssueItem(property, panel);

    await waitUntil(
      () =>
        issueEl.querySelectorAll(".compatibility-node-item").length ===
        nodes.length
    );
    ok(true, "The number of nodes is correct");

    const nodeEls = [...issueEl.querySelectorAll(".compatibility-node-item")];
    for (const node of nodes) {
      const nodeEl = nodeEls.find(el => el.textContent === node);
      ok(nodeEl, "The text content of the node element is correct");
    }
  }
}

/**
 * Get IssueItem of given property from given element.
 *
 * @param {String} property
 * @param {Element} element
 * @return {Element}
 */
function getIssueItem(property, element) {
  return element.querySelector(`[data-qa-property=\"\\"${property}\\"\"]`);
}

/**
 * Toggle enable/disable checkbox of a specific property on rule view.
 *
 * @param {Inspector} inspector
 * @param {Number} ruleIndex
 * @param {Number} propIndex
 */
async function togglePropStatusOnRuleView(inspector, ruleIndex, propIndex) {
  const ruleView = inspector.getPanel("ruleview").view;
  const rule = getRuleViewRuleEditor(ruleView, ruleIndex).rule;
  // In case of inline style changes, we track the mutations via the
  // inspector's markupmutation event to react to dynamic style changes
  // which Resource Watcher doesn't cover yet.
  // If an inline style is applied to the element, we need to wait on the
  // markupmutation event
  const onMutation =
    ruleIndex === 0 ? inspector.once("markupmutation") : Promise.resolve();
  const textProp = rule.textProps[propIndex];
  const onRuleviewChanged = ruleView.once("ruleview-changed");
  textProp.editor.enable.click();
  await Promise.all([onRuleviewChanged, onMutation]);
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
 * Return a promise which waits for COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE action.
 *
 * @param {Object} store
 * @return {Promise}
 */
function waitForUpdateTopLevelTargetAction(store) {
  return waitForDispatch(store, COMPATIBILITY_UPDATE_TOP_LEVEL_TARGET_COMPLETE);
}
