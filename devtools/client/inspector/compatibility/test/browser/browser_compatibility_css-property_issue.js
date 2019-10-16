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
    experimental: true,
    unsupportedBrowsers: [],
  },
  {
    type: MDNCompatibility.ISSUE_TYPE.CSS_PROPERTY_ALIASES,
    property: "user-modify",
    url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
    aliases: ["user-modify"],
    deprecated: true,
    experimental: false,
    unsupportedBrowsers: [],
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { panel } = await openCompatibilityView();

  info("Check the content of the issue item");
  for (const testData of TEST_DATA) {
    await waitUntil(() =>
      panel.querySelector(`[data-qa-property=${testData.property}]`)
    );
    const issueEl = panel.querySelector(
      `[data-qa-property=${testData.property}]`
    );
    ok(issueEl, `The element for ${testData.property} is displayed`);
    for (const [key, value] of Object.entries(testData)) {
      const fieldEl = issueEl.querySelector(`[data-qa-key=${key}]`);
      ok(fieldEl, `The element for ${key} is displayed`);
      is(fieldEl.dataset.qaValue, `${value}`, "The value is correct");
    }
  }
});
