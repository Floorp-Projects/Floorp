/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the content of the issue list will be changed when the new node is selected.

const TEST_URI = `
  <style>
  body {
    overflow-anchor: auto;
  }

  .has-issue {
    scrollbar-color: auto;
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
      {
        property: "scrollbar-color",
        url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
      },
      {
        property: "user-modify",
        url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
      },
    ],
  },
  {
    selector: ".no-issue",
    expectedIssues: [],
  },
  {
    selector: "body",
    expectedIssues: [
      {
        property: "overflow-anchor",
        url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
      },
    ],
  },
];

const TEST_DATA_ALL = [
  {
    property: "overflow-anchor",
    url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
  },
  {
    property: "scrollbar-color",
    url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
  },
  {
    property: "user-modify",
    url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
  },
];

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { allElementsPane, inspector, selectedElementPane } =
    await openCompatibilityView();

  for (const { selector, expectedIssues } of TEST_DATA_SELECTED) {
    info(`Check the issue list for ${selector} node`);
    await selectNode(selector, inspector);
    await assertIssueList(selectedElementPane, expectedIssues);
    info("Check whether the issues on all elements pane are not changed");
    await assertIssueList(allElementsPane, TEST_DATA_ALL);
  }
});
