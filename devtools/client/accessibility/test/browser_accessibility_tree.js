/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1>Top level header</h1>
    <p>This is a paragraph.</p>
  </body>
</html>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   action   {Function}  An optional action that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const tests = [{
  desc: "Test the initial accessibility tree state.",
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }]
  }
}, {
  desc: "Expand first tree node.",
  action: async ({ doc }) => toggleRow(doc, 0),
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }, {
      role: "heading",
      name: `"Top level header"`
    }, {
      role: "paragraph",
      name: `""`
    }]
  }
}, {
  desc: "Collapse first tree node.",
  action: async ({ doc }) => toggleRow(doc, 0),
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }]
  }
}];

/**
 * Simple test that checks content of the Accessibility panel tree.
 */
addA11yPanelTestsTask(tests, TEST_URI, "Test Accessibility panel tree.");
