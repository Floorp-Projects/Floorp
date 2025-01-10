/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the content of the issue list will be changed when the rules are changed
// on the rule view.

const TEST_URI = `
  <style>
  .test-class {
    overflow-anchor: auto;
  }
  div {
    scrollbar-color: auto;
  }
  </style>
  <div class="test-class">test class</div>
  <div>test</div>
`;

const TEST_DATA_SELECTED = {
  fullRule: {
    expectedProperties: [
      {
        property: "overflow-anchor",
        url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
      },
      {
        property: "scrollbar-color",
        url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
      },
    ],
    expectedNodes: [
      {
        property: "overflow-anchor",
        nodes: [],
      },
      {
        property: "scrollbar-color",
        nodes: [],
      },
    ],
  },
  classRule: {
    expectedProperties: [
      {
        property: "overflow-anchor",
        url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
      },
    ],
    expectedNodes: [
      {
        property: "overflow-anchor",
        nodes: [],
      },
    ],
  },
  elementRule: {
    expectedProperties: [
      {
        property: "scrollbar-color",
        url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
      },
    ],
    expectedNodes: [
      {
        property: "scrollbar-color",
        nodes: [],
      },
    ],
  },
};

const TEST_DATA_ALL = {
  fullRule: {
    expectedProperties: [
      {
        property: "overflow-anchor",
        url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
      },
      {
        property: "scrollbar-color",
        url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
      },
    ],
    expectedNodes: [
      {
        property: "overflow-anchor",
        nodes: ["div.test-class"],
      },
      {
        property: "scrollbar-color",
        nodes: ["div.test-class", "div"],
      },
    ],
  },
  classRule: {
    expectedProperties: [
      {
        property: "overflow-anchor",
        url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
      },
    ],
    expectedNodes: [
      {
        property: "overflow-anchor",
        nodes: ["div.test-class"],
      },
    ],
  },
  elementRule: {
    expectedProperties: [
      {
        property: "scrollbar-color",
        url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
      },
    ],
    expectedNodes: [
      {
        property: "scrollbar-color",
        nodes: ["div.test-class", "div"],
      },
    ],
  },
};

const {
  COMPATIBILITY_UPDATE_NODES_COMPLETE,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

add_task(async function () {
  info("Enable 3 pane mode");
  await pushPref("devtools.inspector.three-pane-enabled", true);

  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { allElementsPane, inspector, selectedElementPane } =
    await openCompatibilityView();
  await selectNode(".test-class", inspector);

  info("Check the initial issue");
  await assertAll(selectedElementPane, TEST_DATA_SELECTED.fullRule);
  await assertAll(allElementsPane, TEST_DATA_ALL.fullRule);

  info("Check the issue after unchecking class rule");
  await _togglePropStatus(inspector, 1, 0);
  await assertAll(selectedElementPane, TEST_DATA_SELECTED.elementRule);
  await assertAll(allElementsPane, TEST_DATA_ALL.elementRule);

  info("Check the issue after unchecking div rule");
  await _togglePropStatus(inspector, 2, 0);
  await assertIssueList(selectedElementPane, []);
  await assertIssueList(allElementsPane, []);

  info("Check the issue after reverting class rule");
  await _togglePropStatus(inspector, 1, 0);
  await assertAll(selectedElementPane, TEST_DATA_SELECTED.classRule);
  await assertAll(allElementsPane, TEST_DATA_ALL.classRule);

  info("Check the issue after reverting div rule");
  await _togglePropStatus(inspector, 2, 0);
  await assertAll(selectedElementPane, TEST_DATA_SELECTED.fullRule);
  await assertAll(allElementsPane, TEST_DATA_ALL.fullRule);
});

async function assertAll(pane, { expectedProperties, expectedNodes }) {
  await assertIssueList(pane, expectedProperties);
  await assertNodeList(pane, expectedNodes);
}

async function _togglePropStatus(inspector, ruleIndex, propIndex) {
  const onNodesUpdated = waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_NODES_COMPLETE
  );
  await togglePropStatusOnRuleView(inspector, ruleIndex, propIndex);
  await onNodesUpdated;
}
