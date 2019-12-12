/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the deprecated CSS property is shown as issue correctly or not.

const MDNCompatibility = require("devtools/client/inspector/compatibility/lib/MDNCompatibility");

const TEST_URI = `
  <style>
  body {
    color: blue;
    border-block-color: lime;
    user-modify: read-only;
    font-variant-alternates: historical-forms;
  }
  </style>
  <body></body>
`;

const TEST_DATA = [
  {
    type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
    property: "border-block-color",
    url: "https://developer.mozilla.org/docs/Web/CSS/border-block-color",
    deprecated: false,
    experimental: false,
  },
  {
    type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY,
    property: "font-variant-alternates",
    url: "https://developer.mozilla.org/docs/Web/CSS/font-variant-alternates",
    deprecated: true,
    experimental: true,
  },
  {
    type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY_ALIASES,
    property: "user-modify",
    url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
    aliases: ["user-modify"],
    deprecated: true,
    experimental: false,
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { panel } = await openCompatibilityView();

  info("Check the content of the issue list");
  await assertIssueList(panel, TEST_DATA);
});
