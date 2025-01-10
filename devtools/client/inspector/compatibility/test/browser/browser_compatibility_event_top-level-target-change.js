/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the issues after navigating to another page.

const TEST_DATA_ISSUES = {
  uri: `
    <style>
    body {
      overflow-anchor: auto;
    }
    div {
      scrollbar-color: auto;
    }
    </style>
    <body>
      <div>test</div>
    </body>
  `,
  expectedIssuesOnSelected: [
    {
      property: "overflow-anchor",
      url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
    },
  ],
  expectedIssuesOnAll: [
    {
      property: "overflow-anchor",
      url: "https://developer.mozilla.org/docs/Web/CSS/overflow-anchor",
    },
    {
      property: "scrollbar-color",
      url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-color",
    },
  ],
};

const TEST_DATA_NO_ISSUES = {
  uri: "<body></body>",
  expectedIssuesOnSelected: [],
  expectedIssuesOnAll: [],
};

add_task(async function () {
  const tab = await addTab(
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_DATA_ISSUES.uri)
  );

  const { allElementsPane, inspector, selectedElementPane } =
    await openCompatibilityView();

  info("Check issues at initial");
  await assertIssues(selectedElementPane, allElementsPane, TEST_DATA_ISSUES);

  info("Navigate to next uri that has no issues");
  await navigateTo(TEST_DATA_NO_ISSUES.uri, tab, inspector);
  info("Check issues after navigating");
  await assertIssues(selectedElementPane, allElementsPane, TEST_DATA_NO_ISSUES);

  info("Revert the uri");
  await navigateTo(TEST_DATA_ISSUES.uri, tab, inspector);
  info("Check issues after reverting");
  await assertIssues(selectedElementPane, allElementsPane, TEST_DATA_ISSUES);
});

async function assertIssues(
  selectedElementPane,
  allElementsPane,
  { expectedIssuesOnSelected, expectedIssuesOnAll }
) {
  await assertIssueList(selectedElementPane, expectedIssuesOnSelected);
  await assertIssueList(allElementsPane, expectedIssuesOnAll);
}

async function navigateTo(uri, tab, { store }) {
  uri = "data:text/html;charset=utf-8," + encodeURIComponent(uri);
  const onSelectedNodeUpdated = waitForUpdateSelectedNodeAction(store);
  const onTopLevelTargetUpdated = waitForUpdateTopLevelTargetAction(store);
  const onLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, uri);
  await Promise.all([onLoaded, onSelectedNodeUpdated, onTopLevelTargetUpdated]);
}
