/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test whether the deprecated CSS property is shown as issue correctly or not.

const { COMPATIBILITY_ISSUE_TYPE } = require("devtools/shared/constants");

const TEST_URI = `
  <style>
  body {
    color: blue;
    font-variant-alternates: historical-forms;
    user-modify: read-only;
  }
  div {
    ruby-align: center;
  }
  </style>
  <body>
    <div>test</div>
  </body>
`;

const TEST_DATA_SELECTED = [
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
    property: "font-variant-alternates",
    url: "https://developer.mozilla.org/docs/Web/CSS/font-variant-alternates",
    deprecated: false,
    experimental: false,
  },
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
    property: "user-modify",
    url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
    aliases: ["user-modify"],
    deprecated: true,
    experimental: false,
  },
];

const TEST_DATA_ALL = [
  ...TEST_DATA_SELECTED,
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
    property: "ruby-align",
    url: "https://developer.mozilla.org/docs/Web/CSS/ruby-align",
    deprecated: false,
    experimental: true,
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const {
    allElementsPane,
    selectedElementPane,
  } = await openCompatibilityView();

  info("Check the content of the issue list on the selected element");
  await assertIssueList(selectedElementPane, TEST_DATA_SELECTED);

  info("Check the content of the issue list on all elements");
  await assertIssueList(allElementsPane, TEST_DATA_ALL);
});
