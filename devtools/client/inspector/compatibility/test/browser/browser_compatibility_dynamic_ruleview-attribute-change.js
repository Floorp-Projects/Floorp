/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");

const {
  COMPATIBILITY_UPDATE_NODE_COMPLETE,
} = require("resource://devtools/client/inspector/compatibility/actions/index.js");

// Test the behavior rules are dynamically added

const ISSUE_OUTLINE_RADIUS = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "-moz-user-input",
  url: "https://developer.mozilla.org/docs/Web/CSS/-moz-user-input",
  deprecated: true,
  experimental: false,
};

const ISSUE_SCROLLBAR_COLOR = {
  type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
  property: "scrollbar-color",
  url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
  deprecated: false,
  experimental: false,
};

const TEST_URI = `
  <style>
    .issue {
      -moz-user-input: none;
    }
  </style>
  <body>
    <div class="test issue"></div>
  </body>
`;

add_task(async function () {
  info("Testing dynamic style change via the devtools inspector's rule view");
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

  const { allElementsPane, inspector, selectedElementPane } =
    await openCompatibilityView();

  info("Select the div to undergo mutation");
  const waitForCompatibilityListUpdate = waitForUpdateSelectedNodeAction(
    inspector.store
  );
  await selectNode(".test.issue", inspector);
  await waitForCompatibilityListUpdate;

  info("Check initial issues");
  await checkPanelIssues(selectedElementPane, allElementsPane, [
    ISSUE_OUTLINE_RADIUS,
  ]);

  await addNewRule(
    "scrollbar-color",
    "auto",
    inspector,
    selectedElementPane,
    allElementsPane,
    [ISSUE_OUTLINE_RADIUS, ISSUE_SCROLLBAR_COLOR]
  );

  info("Toggle the inline issue rendering it disable");
  await togglePropStatusOnRuleView(inspector, 0, 0);
  info("Check the issues listed in panel");
  await checkPanelIssues(selectedElementPane, allElementsPane, [
    ISSUE_OUTLINE_RADIUS,
  ]);

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
