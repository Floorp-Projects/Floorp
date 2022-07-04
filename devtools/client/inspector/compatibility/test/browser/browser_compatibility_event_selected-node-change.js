/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the content of the issue list will be changed when the new node is selected.

const TEST_URI = `
  <style>
  body {
    ruby-align: center;
  }

  .has-issue {
    font-variant-alternates: historical-forms;
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
      { property: "font-variant-alternates" },
      { property: "user-modify" },
    ],
  },
  {
    selector: ".no-issue",
    expectedIssues: [],
  },
  {
    selector: "body",
    expectedIssues: [{ property: "ruby-align" }],
  },
];

const TEST_DATA_ALL = [
  { property: "ruby-align" },
  { property: "font-variant-alternates" },
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
