/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the content of the issue list will be changed when the new node is selected.

const TEST_URI = `
  <style>
  body {
    border-block-color: lime;
  }

  .has-issue {
    border-inline-color: lime;
    user-modify: read-only;
  }

  .no-issue {
    color: black;
  }
  </style>
  <body>
    <div class="has-issue">has issue</div>
    <div class="no-issue">no issue</div>
  </body>
`;

const TEST_DATA_SELECTED = [
  {
    selector: ".has-issue",
    expectedIssues: [
      { property: "border-inline-color" },
      { property: "user-modify" },
    ],
  },
  {
    selector: ".no-issue",
    expectedIssues: [],
  },
  {
    selector: "body",
    expectedIssues: [{ property: "border-block-color" }],
  },
];

const TEST_DATA_ALL = [
  { property: "border-block-color" },
  { property: "border-inline-color" },
  { property: "user-modify" },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const {
    allElementsPane,
    inspector,
    selectedElementPane,
  } = await openCompatibilityView();

  for (const { selector, expectedIssues } of TEST_DATA_SELECTED) {
    info(`Check the issue list for ${selector} node`);
    await selectNode(selector, inspector);
    await assertIssueList(selectedElementPane, expectedIssues);
    info("Check whether the issues on all elements pane are not changed");
    await assertIssueList(allElementsPane, TEST_DATA_ALL);
  }
});
