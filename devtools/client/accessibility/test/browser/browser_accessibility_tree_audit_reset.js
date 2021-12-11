/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global toggleMenuItem, selectAccessibleForNode, TREE_FILTERS_MENU_ID */

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Panel Test</title>
  </head>
  <body>
    <h1 style="color:rgba(255,0,0,0.1); background-color:rgba(255,255,255,1);">
      Top level header
    </h1>
    <h2 style="color:rgba(0,255,0,0.1); background-color:rgba(255,255,255,1);">
      Second level header
    </h2>
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
const tests = [
  {
    desc: "Check initial state.",
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
          selected: true,
        },
      ],
    },
  },
  {
    desc: "Run an audit from a11y panel toolbar by activating a filter.",
    setup: async ({ doc, toolbox }) => {
      await toggleMenuItem(doc, toolbox.doc, TREE_FILTERS_MENU_ID, 1);
    },
    expected: {
      tree: [
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
          selected: true,
        },
        {
          role: "text leaf",
          name: `"Second level header "contrast`,
          badges: ["contrast"],
        },
      ],
    },
  },
  {
    desc: "Select an accessible object.",
    setup: async env => {
      await selectAccessibleForNode(env, "h1");
    },
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
        {
          role: "heading",
          name: `"Top level header"`,
          selected: true,
        },
        {
          role: "text leaf",
          name: `"Top level header "contrast`,
          badges: ["contrast"],
        },
        {
          role: "heading",
          name: `"Second level header"`,
        },
        {
          role: "text leaf",
          name: `"Second level header "contrast`,
          badges: ["contrast"],
        },
      ],
    },
  },
];

/**
 * Simple test that checks content of the Accessibility panel tree when the
 * audit is activated via the panel's toolbar.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel tree with contrast filter audit activation."
);
