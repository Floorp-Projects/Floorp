/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI_1 = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1>Top level header</h1>
    <p>This is a paragraph.</p>
  </body>
</html>`;

const TEST_URI_2 = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Navigation Accessibility Panel</title>
  </head>
  <body></body>
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
  desc: "Test the initial accessibility tree state after first row is expanded.",
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
    }],
    sidebar: {
      name: "Accessibility Panel Test",
      role: "document"
    }
  }
}, {
  desc: "Reload the page.",
  action: async ({ panel }) => reload(panel.target),
  expected: {
    tree: [{
      role: "document",
      name: `"Accessibility Panel Test"`
    }],
    sidebar: {
      name: "Accessibility Panel Test",
      role: "document"
    }
  }
}, {
  desc: "Navigate to a new page.",
  action: async ({ panel }) => navigate(panel.target, buildURL(TEST_URI_2)),
  expected: {
    tree: [{
      role: "document",
      name: `"Navigation Accessibility Panel"`
    }],
    sidebar: {
      name: "Navigation Accessibility Panel",
      role: "document"
    }
  }
}];

/**
 * Simple test that checks content of the Accessibility panel tree on reload.
 */
addA11yPanelTestsTask(tests, TEST_URI_1, "Test Accessibility panel tree on reload.");
