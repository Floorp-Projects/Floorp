/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether some of browsers are unsupported.

const {
  updateTargetBrowsers,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

const TEST_URI = `
  <style>
  body {
    border-block-color: lime;
  }
  </style>
  <body></body>
`;

const TARGET_BROWSERS = [
  { id: "firefox", version: "1" },
  { id: "firefox", version: "70" },
  { id: "firefox_android", version: "1" },
  { id: "firefox_android", version: "70" },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, selectedElementPane } = await openCompatibilityView();

  info("Update the target browsers for this test");
  await inspector.store.dispatch(updateTargetBrowsers(TARGET_BROWSERS));

  info("Check the content of the issue item");
  const expectedIssues = [
    {
      property: "border-block-color",
      unsupportedBrowsers: [
        { id: "firefox", version: "1" },
        { id: "firefox_android", version: "1" },
      ],
    },
  ];
  await assertIssueList(selectedElementPane, expectedIssues);
});
