/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { COMPATIBILITY_ISSUE_TYPE } = require("devtools/shared/constants");

const {
  COMPATIBILITY_UPDATE_NODE_COMPLETE,
} = require("devtools/client/inspector/compatibility/actions/index");

// Test the behavior rules are dynamically added

const ISSUE_CURSOR = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "cursor",
  url: "https://developer.mozilla.org/docs/Web/CSS/cursor",
  deprecated: false,
  experimental: false,
};

const ISSUE_HYPHENS = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
  aliases: ["hyphens"],
  property: "hyphens",
  url: "https://developer.mozilla.org/docs/Web/CSS/hyphens",
  deprecated: false,
  experimental: false,
};

const TEST_URI = `
  <style>
    .issue {
      cursor: grab;
    }
  </style>
  <body>
    <div class="test issue"></div>
  </body>
`;

add_task(async function() {
  info("Testing dynamic style change via the devtools inspector's rule view");
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const {
    allElementsPane,
    inspector,
    selectedElementPane,
  } = await openCompatibilityView();

  info("Select the div to undergo mutation");
  const waitForCompatibilityListUpdate = waitForUpdateSelectedNodeAction(
    inspector.store
  );
  await selectNode(".test.issue", inspector);
  await waitForCompatibilityListUpdate;

  info("Check initial issues");
  await checkPanelIssues(selectedElementPane, allElementsPane, [ISSUE_CURSOR]);

  await addNewRule(
    "hyphens",
    "none",
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_CURSOR, ISSUE_HYPHENS]
  );

  info("Toggle the inline issue rendering it disable");
  await togglePropStatusOnRuleView(inspector, 0, 0);
  info("Check the issues listed in panel");
  await checkPanelIssues(selectedElementPane, allElementsPane, [ISSUE_CURSOR]);

  info("Toggle the class rule rendering it disabled");
  await togglePropStatusOnRuleView(inspector, 1, 0);
  info("Check the panel issues listed in panel");
  await checkPanelIssues(selectedElementPane, allElementsPane, []);

  await removeTab(tab);
});

async function addNewRule(
  newDeclaration,
  value,
  inspector,
  selectedElementPane,
  allElementsPane,
  issue
) {
  const { view } = await openRuleView();
  const waitForCompatibilityListUpdate = waitForDispatch(
    inspector.store,
    COMPATIBILITY_UPDATE_NODE_COMPLETE
  );

  info("Add a new inline property");
  await addProperty(view, 0, newDeclaration, value);
  info("Wait for changes");
  await waitForCompatibilityListUpdate;

  info("Check issues list for element and the webpage");
  await checkPanelIssues(selectedElementPane, allElementsPane, issue);
}

async function checkPanelIssues(selectedElementPane, allElementsPane, issues) {
  info("Check selected element issues");
  await assertIssueList(selectedElementPane, issues);
  info("Check all panel issues");
  await assertIssueList(allElementsPane, issues);
}
