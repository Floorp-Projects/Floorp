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
 *   setup    {Function}  An optional setup that needs to be performed before
 *                        the state of the tree and the sidebar can be checked.
 *   expected {JSON}      An expected states for the tree and the sidebar.
 * }
 */
const tests = [
  {
    desc: "Test the initial accessibility tree and sidebar states.",
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
      ],
      sidebar: {
        name: "Accessibility Panel Test",
        role: "document",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 2,
        indexInParent: 0,
        states: [
          // The focused state is an outdated state, since the toolbox should now
          // have the focus and not the content page. See Bug 1702709.
          "focused",
          "readonly",
          "focusable",
          "opaque",
          "enabled",
          "sensitive",
        ],
      },
    },
  },
  {
    desc: "Expand first tree node.",
    setup: async ({ doc }) => toggleRow(doc, 0),
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
        {
          role: "heading",
          name: `"Top level header"`,
        },
        {
          role: "paragraph",
          name: `""`,
        },
      ],
    },
  },
  {
    desc: "Expand second tree node.",
    setup: async ({ doc }) => toggleRow(doc, 1),
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
        {
          role: "heading",
          name: `"Top level header"`,
        },
        {
          role: "text leaf",
          name: `"Top level header"`,
        },
        {
          role: "paragraph",
          name: `""`,
        },
      ],
      sidebar: {
        name: "Top level header",
        role: "heading",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 1,
        indexInParent: 0,
        states: ["selectable text", "opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Select third tree node.",
    setup: ({ doc }) => selectRow(doc, 2),
    expected: {
      sidebar: {
        name: "Top level header",
        role: "text leaf",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 0,
        indexInParent: 0,
        states: ["opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Collapse first tree node.",
    setup: async ({ doc }) => toggleRow(doc, 0),
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
      ],
      sidebar: {
        name: "Accessibility Panel Test",
        role: "document",
        actions: [],
        value: "",
        description: "",
        keyboardShortcut: "",
        childCount: 2,
        indexInParent: 0,
        states: ["readonly", "focusable", "opaque", "enabled", "sensitive"],
      },
    },
  },
  {
    desc: "Expand first tree node again.",
    setup: async ({ doc }) => toggleRow(doc, 0),
    expected: {
      tree: [
        {
          role: "document",
          name: `"Accessibility Panel Test"`,
        },
        {
          role: "heading",
          name: `"Top level header"`,
        },
        {
          role: "text leaf",
          name: `"Top level header"`,
        },
        {
          role: "paragraph",
          name: `""`,
        },
      ],
    },
  },
];

/**
 * Check navigation within the tree.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test Accessibility panel tree navigation."
);
