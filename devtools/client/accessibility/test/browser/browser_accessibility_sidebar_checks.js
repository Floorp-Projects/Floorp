/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <p style="color: red;">Red</p>
    <p style="color: blue;">Blue</p>
    <p style="color: gray; background: linear-gradient(#e66465, #9198e5);">Gray</p>
  </body>
</html>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   setup    {Function}  An optional setup that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const tests = [{
  desc: "Test the initial accessibility audit state.",
  expected: {
    audit: { CONTRAST: null },
  },
}, {
  desc: "Check accessible representing text node in red.",
  setup: async ({ doc }) => {
    await toggleRow(doc, 0);
    await toggleRow(doc, 1);
    await selectRow(doc, 2);
  },
  expected: {
    audit: {
      "CONTRAST": {
        "value": 4.00,
        "color": [255, 0, 0, 1],
        "backgroundColor": [255, 255, 255, 1],
        "isLargeText": false,
        "score": "fail",
      },
    },
  },
}, {
  desc: "Check accessible representing text node in blue.",
  setup: async ({ doc }) => {
    await toggleRow(doc, 3);
    await selectRow(doc, 4);
  },
  expected: {
    audit: {
      "CONTRAST": {
        "value": 8.59,
        "color": [0, 0, 255, 1],
        "backgroundColor": [255, 255, 255, 1],
        "isLargeText": false,
        "score": "AAA",
      },
    },
  },
}];

/**
 * Test that checks the Accessibility panel sidebar.
 */
addA11yPanelTestsTask(tests, TEST_URI, "Test Accessibility panel sidebar.");
