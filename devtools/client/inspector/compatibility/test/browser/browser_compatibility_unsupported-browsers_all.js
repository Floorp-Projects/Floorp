/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the all of default browsers are unsupported.

const TEST_URI = `
  <style>
  body {
    user-modify: read-only;
  }
  </style>
  <body></body>
`;

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, selectedElementPane } = await openCompatibilityView();

  info("Get the taget browsers we set as default");
  const { targetBrowsers } = inspector.store.getState().compatibility;

  info("Check the content of the issue item");
  const expectedIssues = [
    {
      property: "user-modify",
      unsupportedBrowsers: targetBrowsers,
      url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
    },
  ];
  await assertIssueList(selectedElementPane, expectedIssues);
});
